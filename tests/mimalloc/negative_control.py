"""The exact probe must reject an executable with no allocator override."""
import os
import subprocess
import sys

result = subprocess.run([sys.argv[1]], env={**os.environ, 'BPR_MIMALLOC_PROBE': '1'},
                        text=True, capture_output=True, timeout=20)
assert result.returncode == 1, result
assert 'MIMALLOC_PROBE_FAIL mi_is_in_heap_region' in result.stderr, result.stderr
print('mimalloc-missing-override:pass (probe refused unattached executable)')
