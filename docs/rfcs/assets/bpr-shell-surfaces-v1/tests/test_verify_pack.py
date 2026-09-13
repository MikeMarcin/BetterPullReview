"""Pack-level integrity and semantic negative checks; no BPR or native test claims."""
from __future__ import annotations
import hashlib
import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('verify_pack', ROOT / 'scripts/verify_pack.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def reseal(root: Path) -> None:
    names = sorted(mod.payload_names(root))
    (root / 'SHA256SUMS').write_text(''.join(hashlib.sha256((root / n).read_bytes()).hexdigest() + '  ' + n + '\n' for n in names))


class PackTests(unittest.TestCase):
    def test_exact_pack(self):
        self.assertEqual(mod.verify(ROOT)['status'], 'passed')

    def copy(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        p = Path(tmp.name) / 'pack'
        shutil.copytree(ROOT, p, ignore=shutil.ignore_patterns('__pycache__'))
        return p

    def test_modified_file_is_rejected(self):
        p = self.copy(); f = p / 'design/shell-surfaces-v1.html'
        f.write_bytes(f.read_bytes() + b'<!-- x -->')
        with self.assertRaisesRegex(mod.PackError, 'Checksum mismatch'):
            mod.verify(p)

    def test_missing_criterion_even_with_new_hashes(self):
        # A2 is covered by the sign-in card and by the closeout card; drop both.
        p = self.copy(); f = p / 'cards/PACK-SHELL-04.md'
        f.write_text(f.read_text().replace('"acceptance":["A2"]', '"acceptance":[]'))
        f = p / 'cards/PACK-SHELL-12.md'
        f.write_text(f.read_text().replace('"A1","A2",', '"A1",')); reseal(p)
        with self.assertRaisesRegex(mod.PackError, 'coverage mismatch'):
            mod.verify(p)

    def test_closeout_drift_even_with_new_hashes(self):
        p = self.copy(); f = p / 'cards/PACK-SHELL-13.md'
        f.write_text(f.read_text().replace('**B1 — A close is a close.**', '**B1 — A close.**')); reseal(p)
        with self.assertRaisesRegex(mod.PackError, 'Closeout checklist differs'):
            mod.verify(p)

    def test_plan_cycle_even_with_new_hashes(self):
        p = self.copy(); f = p / 'milestone-plan.json'; d = json.loads(f.read_text())
        d['milestones'][0]['depends_on'] = ['verification-closeout']; f.write_text(json.dumps(d)); reseal(p)
        with self.assertRaisesRegex(mod.PackError, 'Dependency cycle'):
            mod.verify(p)

    def test_unsafe_ledger_path(self):
        p = self.copy(); (p / 'SHA256SUMS').write_text('0' * 64 + '  ../outside.txt\n')
        with self.assertRaisesRegex(mod.PackError, 'Unsafe relative path'):
            mod.verify(p)


if __name__ == '__main__':
    unittest.main()
