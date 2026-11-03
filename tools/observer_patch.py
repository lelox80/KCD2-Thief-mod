"""Generate a minimal observer branch from the user's exact installed retail asset."""
import hashlib
import xml.etree.ElementTree as ET

ASSET = 'AI/player/switch/switch.xml'
SOURCE_SHA256 = 'fd115777802e2a2ca1a7b0d3b11f1c2656c16e0c44d9bcc83377e01014d51d2a'
STATES = ('crouch', 'inAir', 'cutscene', 'interactiveScene', 'dialogue',
          'horse', 'minigame', 'actorAnim', 'onBed')


def branch():
    node = ET.Element('While', doFail='false', propagateChildFail='false', condition='true')
    sequence = ET.SubElement(node, 'Sequence')
    reads = ET.SubElement(ET.SubElement(sequence, 'SuppressFailure'), 'Sequence')
    def lua(parent, method, args=''):
        code = (f'if KuttenbergThief and KuttenbergThief.{method} then '
                f'pcall(KuttenbergThief.{method}{", " if args else ""}{args}) end')
        ET.SubElement(parent, 'ExecuteLua', code=code)
    lua(reads, 'BeginActorObservation')
    for state in STATES:
        check = ET.SubElement(reads, 'ActorStateCheck', Who='$__player', State=state,
                              FailWithSubtreeMissing='false', saveVersion='2')
        lua(ET.SubElement(check, 'Then', canSkip='1'), 'ObserveActorState', f"'{state}', true")
        lua(ET.SubElement(check, 'Else', canSkip='1'), 'ObserveActorState', f"'{state}', false")
    lua(reads, 'CommitActorObservation')
    ET.SubElement(sequence, 'Wait', duration="'100ms'", timeType='GameTime',
                  doFail='false', variation='', skipInLOD='false')
    return ET.tostring(node, encoding='ascii', xml_declaration=False)


def patch(original):
    if hashlib.sha256(original).hexdigest() != SOURCE_SHA256:
        raise RuntimeError('Player behavior-tree asset changed; observer patch refused.')
    root = ET.fromstring(original)
    parallel = root.find("./BehaviorTree[@name='root']/Root/Behavior/Parallel")
    if parallel is None or parallel.attrib != {'successMode': 'All', 'failureMode': 'Any'}:
        raise RuntimeError('Unexpected player behavior-tree structure.')
    # Preserve all original bytes and node order, including editor data.
    anchor = b'\t\t\t\t</Parallel>\r\n\t\t\t</Behavior>'
    if original.count(anchor) != 1:
        raise RuntimeError('Player behavior-tree insertion anchor is ambiguous.')
    insertion = b'\t\t\t\t\t<!-- KuttenbergThief read-only observer -->\r\n' + branch() + b'\r\n'
    result = original.replace(anchor, insertion+anchor, 1)
    assert result.replace(insertion, b'', 1) == original
    rebuilt = ET.fromstring(result).find("./BehaviorTree[@name='root']/Root/Behavior/Parallel")
    assert len(rebuilt) == len(parallel)+1
    for before, after in zip(parallel, rebuilt):
        before.tail = None
        after.tail = None
        assert ET.tostring(before) == ET.tostring(after)
    return result


def check_conflicts(mods, own_id, extra_assets=()):
    """Conservatively refuse another mod providing a member we intend to override."""
    import zipfile
    targets = {ASSET.lower(), *(name.lower() for name in extra_assets)}
    if not mods.exists():
        return
    for folder in mods.iterdir():
        if not folder.is_dir() or folder.name.lower() == own_id:
            continue
        for path in folder.rglob('*'):
            if not path.is_file():
                continue
            if path.suffix.lower() == '.pak':
                try:
                    with zipfile.ZipFile(path) as archive:
                        if targets.intersection(n.replace('\\', '/').lower() for n in archive.namelist()):
                            raise RuntimeError('Another mod overrides a required asset: '+folder.name)
                except zipfile.BadZipFile:
                    raise RuntimeError('Cannot check mod archive for observer conflicts: '+folder.name)
            elif any(path.as_posix().lower().endswith('/'+target) for target in targets):
                raise RuntimeError('Another mod has a loose override for a required asset: '+folder.name)
