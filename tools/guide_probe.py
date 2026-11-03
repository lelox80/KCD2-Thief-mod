"""Explicit-PID observation of normal native guide loading and indexing.

Start before a user-driven level/save load. Never registers data or enables
motion. Source context and typed arrays do not identify the selected archive.
"""
import native_probe

native_probe.__doc__ = __doc__
native_probe.BUILD_ROOT = native_probe.ROOT / '.local/native/guide-probe'
native_probe.DEFAULT_BUILD = 'build-v022'
native_probe.DLL = native_probe.BUILD_ROOT / 'build-v022/GuideProbe.dll'
native_probe.EXPECTED_PROBE = ('guide-load', 1)
native_probe.EXPECTED_HOOKS = [
    {'name': 'source', 'rva': '0x170ff94', 'prologue': '48895c240848897c2418554156415748'},
    {'name': 'body', 'rva': '0x1331224', 'prologue': '48895c240848897c242055488bec4883'},
    {'name': 'index', 'rva': '0x7e6a50', 'prologue': '405355565741544155415641574883ec'},
]
native_probe.STATUS_RVA = 0x170ff94
native_probe.STATUS_BYTES = bytes.fromhex('48895c240848897c2418554156415748')
native_probe.STATUS_KEY = 'original_guide_source_prologue_restored'

if __name__ == '__main__':
    native_probe.main()
