"""Build a private Scaleform adapter from pinned, locally extracted Thief art.

Proprietary input/output stays in .local or the ignored build PAK. This tool
copies selected vector definitions, freezes the two intro states, embeds the
one required bitmap and adds only our AS2 adapter. It never
loads donor ActionScript, imported movies, focus/health gauges or smoke classes.
"""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import xml.etree.ElementTree as ET
import zlib
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DONOR = ROOT / '.local/research/20261021-hud-donor'
OUT = ROOT / '.local/assets/stealth'
FFDEC = ROOT / '.local/tools/ffdec-26.4.0/ffdec-cli.jar'
JAVA = Path(r'C:\Program Files\Java\jdk-21\bin\java.exe')
ART = {29, 30, *range(37, 47), 54, 55}
GEM_SHA = '28119b1a21a59d57357cf8a6eebd35aa18d02eec0747a3242bfa43a2292325d3'
ATLAS_SHA = '33a5354f81f5ce3acfacb176d168e9a4f783690537ab7be1fd0b4a8b1e7a419d'
FFDEC_SHA = '84573b0e260f2288ddb12afd54cdba853e1df77ca2514d6cff0c3a875f1e70cd'
ARCHIVE = ROOT / '.local/tools/downloads/ffdec_26.4.0.zip'
ARCHIVE_SHA = '56ce960aeafc7caf034e2052ba49f878a883f7c7ca690da6eeffca9587b5e4b9'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Bits:
    def __init__(self):
        self.bits = ''

    def u(self, value, count):
        assert 0 <= value < (1 << count)
        self.bits += format(value, '0%db' % count)
        return self

    def s(self, value, count):
        assert -(1 << (count - 1)) <= value < (1 << (count - 1))
        return self.u(value & ((1 << count) - 1), count)

    def data(self):
        padded = self.bits + '0' * (-len(self.bits) % 8)
        return bytes(int(padded[i:i + 8], 2) for i in range(0, len(padded), 8))


def signed_width(values):
    return max(1, max(abs(v).bit_length() + 1 for v in values))


def rect(x0, x1, y0, y1):
    n = signed_width((x0, x1, y0, y1))
    b = Bits().u(n, 5)
    for v in (x0, x1, y0, y1):
        b.s(v, n)
    return b.data()


def tag(code, body=b''):
    n = len(body)
    return struct.pack('<H', code << 6 | min(n, 63)) + (struct.pack('<I', n) if n >= 63 else b'') + body


def matrix(x=0, y=0):
    n = signed_width((x, y))
    return Bits().u(0, 1).u(0, 1).u(n, 5).s(x, n).s(y, n).data()


def place(char, depth, name=None, x=0, y=0, alpha=None):
    flags = (0x26 if name else 0x06) | (0x08 if alpha is not None else 0)
    color = b''
    if alpha is not None:
        color = Bits().u(0, 1).u(1, 1).u(10, 4)
        for value in (256, 256, 256, alpha):
            color.s(value, 10)
        color = color.data()
    return tag(26, bytes([flags]) + struct.pack('<HH', depth, char)
               + matrix(x, y) + color + (name.encode('ascii') + b'\0' if name else b''))


def sprite(char, children):
    return tag(39, struct.pack('<HH', char, 1) + b''.join(children) + tag(1) + tag(0))


def read_swf(data):
    assert data[:3] in (b'FWS', b'CWS'), 'Expected standard SWF'
    body = data[8:] if data[:3] == b'FWS' else zlib.decompress(data[8:])
    assert len(body) + 8 == struct.unpack_from('<I', data, 4)[0]
    pos = (5 + (body[0] >> 3) * 4 + 7) // 8 + 4
    tags = []
    while pos < len(body):
        word, = struct.unpack_from('<H', body, pos)
        pos += 2
        code, n = word >> 6, word & 63
        if n == 63:
            n, = struct.unpack_from('<I', body, pos)
            pos += 4
        payload = body[pos:pos + n]
        assert len(payload) == n
        tags.append((code, payload))
        pos += n
        if code == 0:
            assert pos == len(body)
            break
    return tags


def write_swf(tags):
    body = rect(0, 1920 * 20, 0, 1080 * 20) + struct.pack('<HH', 30 * 256, 1)
    body += b''.join(tag(c, d) for c, d in tags)
    return b'FWS\x08' + struct.pack('<I', len(body) + 8) + body


def freeze(timeline, wanted, omit):
    """Resolve PlaceObject2 moves at the selected donor frame, omit smoke only."""
    display, current = {}, 1
    for item in timeline.find('subTags'):
        kind = item.get('type')
        if kind == 'ShowFrameTag':
            if current == wanted:
                break
            current += 1
        elif kind == 'PlaceObject2Tag':
            depth = int(item.get('depth'))
            if item.get('placeFlagMove') == 'true':
                old = deepcopy(display[depth])
                for key, value in item.attrib.items():
                    if key.startswith('placeFlagHas') and value == 'false':
                        continue
                    old.set(key, value)
                for child in item:
                    prev = old.find(child.tag)
                    if prev is not None:
                        old.remove(prev)
                    old.append(deepcopy(child))
                display[depth] = old
            else:
                display[depth] = deepcopy(item)
        elif kind == 'RemoveObject2Tag':
            display.pop(int(item.get('depth')), None)
        else:
            assert kind == 'FrameLabelTag', 'Unexpected donor timeline code: ' + kind
    result = deepcopy(timeline)
    result.set('frameCount', '1')
    sub = result.find('subTags')
    sub.clear()
    for depth, item in sorted(display.items()):
        if int(item.get('characterId')) in omit:
            continue
        item.set('placeFlagMove', 'false')
        sub.append(item)
    ET.SubElement(sub, 'item', type='ShowFrameTag', forceWriteAsLong='false')
    return result


def ffdec(*args):
    run = subprocess.run([str(JAVA), '-Xmx1024m', '-Djava.awt.headless=true', '-jar', str(FFDEC), *map(str, args)],
                         cwd=ROOT, capture_output=True, text=True, check=True)
    if 'ERROR' in run.stdout or 'SEVERE' in run.stderr:
        raise RuntimeError('FFDec conversion failed: ' + run.stdout[-1500:] + run.stderr[-1500:])
    return run.stdout


def embedded_atlas():
    from PIL import Image
    im = Image.open(DONOR / 'Gem_I2C.png').convert('RGBA')
    assert im.size == (64, 64)
    pixels = bytearray()
    for r, g, b, a in im.getdata():
        # SWF Lossless2 requires premultiplied ARGB, unlike PNG straight RGBA.
        pixels.extend((a, (r * a + 127) // 255, (g * a + 127) // 255, (b * a + 127) // 255))
    return struct.pack('<HBHH', 44, 5, 64, 64) + zlib.compress(pixels, 9)


def inputs():
    files = [
        DONOR / 'Gem.gfx', DONOR / 'Gem_I2C.png',
        ROOT / 'src/UI/KTStealth.as', Path(__file__), ARCHIVE, FFDEC,
        *sorted((FFDEC.parent / 'lib').glob('*.jar'))]
    return {str(p.relative_to(ROOT)).replace('\\', '/'): sha(p) for p in files}


def verify_toolchain():
    assert sha(ARCHIVE) == ARCHIVE_SHA, 'Official FFDec archive pin changed'
    with zipfile.ZipFile(ARCHIVE) as archive:
        for file in [FFDEC, *sorted((FFDEC.parent / 'lib').glob('*.jar'))]:
            expected = hashlib.sha256(archive.read(file.relative_to(FFDEC.parent).as_posix())).hexdigest()
            assert sha(file) == expected, 'FFDec dependency differs from inspected release: ' + file.name


def build():
    assert sha(DONOR / 'Gem.gfx') == GEM_SHA, 'Original donor movie pin changed'
    assert sha(DONOR / 'Gem_I2C.png') == ATLAS_SHA, 'Original atlas pin changed'
    assert sha(FFDEC) == FFDEC_SHA, 'Inspected FFDec tool pin changed'
    verify_toolchain()
    pins = inputs()
    receipt_path = OUT / 'receipt.json'
    if receipt_path.is_file():
        receipt = json.loads(receipt_path.read_text())
        if receipt['inputs'] == pins and sha(OUT / 'KTStealth.swf') == receipt['sha256']:
            return OUT / 'KTStealth.swf'
    OUT.mkdir(parents=True, exist_ok=True)
    # Generate XML directly from the pinned movie rather than trusting an edited
    # XML derivative. FFDec's private dump is never copied into source control.
    ffdec('-swf2xml', DONOR / 'Gem.gfx', OUT / 'original.xml')
    donor = ET.parse(OUT / 'original.xml').getroot()
    movie = ET.Element('swf', type='SWF', charset='UTF-8', compression='NONE',
                       encrypted='false', frameCount='1', frameRate='30.0', gfx='false',
                       hasEndTag='true', version='8', _xmlExportMajor='4', _xmlExportMinor='2')
    movie.append(deepcopy(donor.find('displayRect')))
    tags = ET.SubElement(movie, 'tags')
    selected = set()
    for item in donor.find('tags'):
        char = item.get('shapeId', item.get('spriteId', item.get('characterID', '-1')))
        if int(char) not in ART:
            continue
        selected.add(int(char))
        if int(char) in (54, 55):
            item = freeze(item, 8, {52, 53})
        tags.append(deepcopy(item))
    assert selected == ART
    ET.SubElement(tags, 'item', type='ShowFrameTag', forceWriteAsLong='false')
    ET.ElementTree(movie).write(OUT / 'art.xml', encoding='utf-8', xml_declaration=True)
    ffdec('-xml2swf', OUT / 'art.xml', OUT / 'art.swf')
    defs = []
    for code, data in read_swf((OUT / 'art.swf').read_bytes()):
        if code in (0, 1):
            continue
        if code == 1009:  # Scaleform DefineExternalImage2 -> embedded standard bitmap.
            code, data = 36, embedded_atlas()
        assert code in (2, 22, 32, 36, 39, 83), 'Non-art donor tag refused: ' + str(code)
        defs.append((code, data))
    assert len(defs) == len(ART)
    # Preserve the donor container's ordering and base alpha (166/256).
    own = sprite(211, [place(41, 1, 'base', alpha=166), place(54, 2, 'dark'), place(55, 3, 'light')])
    own += sprite(212, [place(211, 1, 'gem')])
    # AS2 compile target and no opaque background; image-only noninteractive HUD.
    raw = write_swf([(69, b'\0\0\0\0'), *defs])
    # Append our binary tags with the same stage header.
    # Retail hud.gfx places bc at(960,1080), stats at y-60, health at
    #(-141.5,-25). Put the gem just left of that bottom-center group. XML uses
    #uniform dynamic fit, so a wide viewport cannot squash the circular art.
    body = raw[8:] + own + place(212, 1, 'panel', 760 * 20, 1015 * 20) + tag(12, b'\x07\0') + tag(1) + tag(0)
    base = b'FWS\x08' + struct.pack('<I', len(body) + 8) + body
    (OUT / 'base.swf').write_bytes(base)
    script_dir = OUT / 'scripts/frame_1'
    script_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / 'src/UI/KTStealth.as', script_dir / 'DoAction.as')
    ffdec('-importScript', OUT / 'base.swf', OUT / 'KTStealth.swf', OUT / 'scripts')
    final = read_swf((OUT / 'KTStealth.swf').read_bytes())
    assert [c for c, _ in final].count(12) == 1 and all(c < 1000 for c, _ in final)
    receipt = {'version': 15, 'inputs': pins, 'sha256': sha(OUT / 'KTStealth.swf'),
               'donor_characters': sorted(ART), 'frozen_intro_frame': 8,
               'omitted': ['shared SmokeTrail_a/b particles', 'all donor ActionScript', 'other HUD components'],
               'layout': {'design_size': [1920,1080], 'gem_center': [760,1015], 'anchor': 'bottom center', 'uniform_fit': True},
               'adapter': 'owned AS2, 350ms heartbeat expiry; aggregate visibility only, no noise display'}
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    return OUT / 'KTStealth.swf'


if __name__ == '__main__':
    print(build())
