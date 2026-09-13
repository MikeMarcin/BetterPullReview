"""Pack-level integrity/semantic negative checks; no BPR/native test claims."""
from __future__ import annotations
import hashlib
import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('verify_pack',ROOT/'scripts/verify_pack.py')
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

def reseal(root:Path)->None:
    names=sorted(mod.payload_names(root))
    (root/'SHA256SUMS').write_text(''.join(hashlib.sha256((root/n).read_bytes()).hexdigest()+'  '+n+'\n' for n in names))

class PackTests(unittest.TestCase):
    def test_exact_pack(self):
        self.assertEqual(mod.verify(ROOT)['status'],'passed')

    def copy(self):
        tmp=tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        p=Path(tmp.name)/'pack'
        shutil.copytree(ROOT,p,ignore=shutil.ignore_patterns('__pycache__'))
        return p

    def test_modified_asset_is_rejected(self):
        p=self.copy(); image=p/'assets/incoming/dif-greet.png'
        image.write_bytes(image.read_bytes()+b'x')
        with self.assertRaisesRegex(mod.PackError,'Checksum mismatch'):
            mod.verify(p)

    def test_semantic_missing_cue_even_with_new_hashes(self):
        p=self.copy();f=p/'specs/CUE-MAP.json';d=json.loads(f.read_text());d['cues'].pop();f.write_text(json.dumps(d));reseal(p)
        with self.assertRaisesRegex(mod.PackError,'cue mapping'):
            mod.verify(p)

    def test_semantic_plan_cycle_even_with_new_hashes(self):
        p=self.copy();f=p/'milestone-plan.json';d=json.loads(f.read_text());d['milestones'][0]['depends_on']=['verification-closeout'];f.write_text(json.dumps(d));reseal(p)
        with self.assertRaisesRegex(mod.PackError,'Dependency cycle'):
            mod.verify(p)

    def test_wrong_pixel_budget_even_with_new_hashes(self):
        p=self.copy();f=p/'specs/CLIP-CONTRACT.json';d=json.loads(f.read_text());d['counts']['complete_decoded_bytes']-=262144;f.write_text(json.dumps(d));reseal(p)
        with self.assertRaisesRegex(mod.PackError,'pixel arithmetic'):
            mod.verify(p)

    def test_unsafe_ledger_path(self):
        p=self.copy();f=p/'SHA256SUMS';f.write_text('0'*64+'  ../outside.txt\n')
        with self.assertRaisesRegex(mod.PackError,'Unsafe relative path'):
            mod.verify(p)

if __name__=='__main__':
    unittest.main()
