"""Make a small, address-preserving private REA input from a pinned PE region.

The manifest identifies the original and derived bytes separately. External
callees, constants, RTTI and unwind data are absent: correlate with the original
PE inspector before drawing conclusions from this deliberately incomplete view.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.local/tools/native_inspection/tooldeps'))
import pefile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('sha256')
    parser.add_argument('rva', type=lambda v: int(v, 0))
    parser.add_argument('length', type=lambda v: int(v, 0))
    parser.add_argument('output', type=Path)
    parser.add_argument('--pe', action='store_true', help='Wrap in a minimal analysis-only PE (Windows REA rejects raw images)')
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to((ROOT / '.local').resolve()):
        parser.error('Extracted proprietary bytes must stay in ignored .local')
    if not 1 <= args.length <= 256 * 1024:
        parser.error('Region must be 1..262144 bytes')
    original = args.binary.read_bytes()
    if hashlib.sha256(original).hexdigest() != args.sha256.lower():
        parser.error('Original SHA-256 mismatch')
    pe = pefile.PE(data=original, fast_load=True)
    section = pe.get_section_by_rva(args.rva)
    if section is None or args.rva + args.length > section.VirtualAddress + section.SizeOfRawData:
        parser.error('Region must fit in one file-backed section')
    region = pe.get_data(args.rva, args.length)
    output.parent.mkdir(parents=True, exist_ok=True)
    derived = region
    if args.pe:
        # A new analysis container, never an edited or runnable game executable.
        # Preserve every instruction address and displacement; include no imports.
        section_rva = args.rva & ~4095
        body = bytes(args.rva - section_rva) + region
        raw_length = (len(body) + 511) & ~511
        header = bytearray(512)
        header[:2] = b'MZ'
        struct.pack_into('<I', header, 0x3c, 0x80)
        header[0x80:0x84] = b'PE\0\0'
        struct.pack_into('<HHIIIHH', header, 0x84, 0x8664, 1, 0, 0, 0, 0xf0, 0x22)
        opt = 0x98
        struct.pack_into('<H', header, opt, 0x20b)
        struct.pack_into('<I', header, opt + 4, raw_length)
        struct.pack_into('<IIQ', header, opt + 16, args.rva, section_rva, pe.OPTIONAL_HEADER.ImageBase)
        struct.pack_into('<II', header, opt + 32, 4096, 512)
        struct.pack_into('<HH', header, opt + 40, 6, 0)
        struct.pack_into('<HH', header, opt + 48, 6, 0)
        struct.pack_into('<II', header, opt + 56, (section_rva + len(body) + 4095) & ~4095, 512)
        struct.pack_into('<H', header, opt + 68, 3)
        struct.pack_into('<QQQQ', header, opt + 72, 1048576, 4096, 1048576, 4096)
        struct.pack_into('<I', header, opt + 108, 16)
        section_at = opt + 0xf0
        struct.pack_into('<8sIIIIIIHHI', header, section_at, b'.text\0\0\0', len(body), section_rva,
                         raw_length, 512, 0, 0, 0, 0, 0x60000020)
        derived = bytes(header) + body.ljust(raw_length, b'\0')
    output.write_bytes(derived)
    manifest = {'source': str(args.binary.resolve()), 'source_sha256': args.sha256,
                'rva': hex(args.rva), 'load_address': hex(pe.OPTIONAL_HEADER.ImageBase + args.rva),
                'length': len(region), 'region_sha256': hashlib.sha256(region).hexdigest(),
                'container': 'analysis-only-pe' if args.pe else 'raw',
                'container_sha256': hashlib.sha256(derived).hexdigest(),
                'limitation': 'Derived raw region; external code/data and PE metadata absent.'}
    output.with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest))


if __name__ == '__main__':
    main()
