"""Explicit-PID passive attribution of the normal whdata XML input archive.

Attach only for an authorized user-driven load. Does not mount archives,
register guides, invoke host lookups or enable movement. Empty/skipped lookups
do not establish a loose file source. Arbitrary paths/pointers are never logged.
"""
import native_probe

native_probe.__doc__ = __doc__
native_probe.BUILD_ROOT = native_probe.ROOT / '.local/native/guide-source-probe'
native_probe.DEFAULT_BUILD = 'build-v023'
native_probe.DLL = native_probe.BUILD_ROOT / 'build-v023/GuideSourceProbe.dll'
native_probe.EXPECTED_PROBE = ('guide-source', 1)
native_probe.EXPECTED_HOOKS = [
    {'name': 'source', 'rva': '0x170ff94', 'prologue': '48895c240848897c2418554156415748'},
    {'name': 'lookup', 'rva': '0xf41730', 'prologue': '4883ec28488b91080100004885d2742b'},
]
native_probe.STATUS_RVA = 0x170ff94
native_probe.STATUS_BYTES = bytes.fromhex('48895c240848897c2418554156415748')
native_probe.STATUS_KEY = 'original_guide_source_prologue_restored'

if __name__ == '__main__':
    native_probe.main()
