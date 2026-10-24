"""The exact probe must reject an executable with no allocator override."""
import os
import subprocess
import sys

disabled = '--disabled-redirect' in sys.argv[2:]
environment = {**os.environ, 'BPR_MIMALLOC_PROBE': '1'}
if disabled:
    environment['MIMALLOC_DISABLE_REDIRECT'] = '1'
result = subprocess.run([sys.argv[1]], env=environment,
                        text=True, capture_output=True, timeout=20)
assert result.returncode == 1, result
expected = 'malloc' if disabled else 'mi_is_in_heap_region'
assert f'MIMALLOC_PROBE_FAIL {expected}' in result.stderr, result.stderr
print('mimalloc-missing-override:pass (probe refused unattached executable)')
