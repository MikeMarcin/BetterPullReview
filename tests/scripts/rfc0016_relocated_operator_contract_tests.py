#!/usr/bin/env python3
"""Public replay of retained operator authority after skill-source relocation."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.contained_temporary_support import ContainedTemporaryDirectory

BASE = Path('scripts/verification/rfc0016_attended_contract')
OLD = Path('.agents/skills/bpr-attended-review/scripts/validate_operator_packet.py')
GATE = Path('synthetic-gate-v001')


class RelocatedOperatorTests(unittest.TestCase):
    def setUp(self):
        self.temp = ContainedTemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git('init', '--quiet')
        self.git('config', 'user.name', 'Fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        (self.root / GATE).mkdir()
        (self.root / OLD).parent.mkdir(parents=True)
        (self.root / OLD).write_bytes((ROOT / BASE / 'scripts/validate_operator_packet.py').read_bytes())
        subject = ('schema_name=bpr.operator_decision_subject\nschema_version=1\n'
                   'gate=synthetic-gate-v001\nfact_1=bounded fixture\n')
        statement = f'I approve the synthetic gate bound by decision subject\nsha256:{hashlib.sha256(subject.encode()).hexdigest()}.\n'
        artifacts = {'decision-subject.txt':subject,
                     'operator-response.md':statement,
                     'operator-packet.md':'# Fixture\n\nTo approve, reply with this exact statement:\n\n' + ''.join('> ' + line + '\n' for line in statement.splitlines()),
                     'claims-contract.json':json.dumps({'schema_name':'bpr.operator_claim_contract','schema_version':1,'gate':GATE.name,'approved_claim':'synthetic_gate_v001_approved','adjacent_false_claims':['adjacent_authority_approved']})}
        for name, text in artifacts.items():
            (self.root / GATE / name).write_text(text, encoding='utf-8', newline='')
        self.commit('fixture')
        reviewed = self.git('rev-parse','HEAD')
        result = subprocess.run([sys.executable, str(self.root / OLD), '--repository-root', str(self.root), '--commit', reviewed, '--packet', str(GATE/'operator-packet.md'), '--subject', str(GATE/'decision-subject.txt'), '--response', str(GATE/'operator-response.md'), '--claims-contract', str(GATE/'claims-contract.json'), '--recorded-at', '2026-09-05T12:00:00Z', '--output', str(GATE/'validation-record.json')], capture_output=True, text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.commit('record')
        self.record_commit = self.git('rev-parse','HEAD')
        self.record_bytes = (self.root / GATE / 'validation-record.json').read_bytes()
        shutil.copytree(ROOT / BASE, self.root / BASE)
        (self.root / OLD).unlink()
        self.commit('relocate contract source')

    def git(self,*args):
        return subprocess.check_output(['git','-C',str(self.root),*args],text=True,stderr=subprocess.PIPE).strip()

    def commit(self,message):
        self.git('add','.')
        self.git('commit','--quiet','-m',message)

    def replay(self,success):
        result = subprocess.run([sys.executable,'-B',str(self.root/BASE/'revalidate_operator_record.py'),'--repository-root',str(self.root),'--output',str(GATE/'validation-record.json'),'--validate-existing','--record-commit',self.record_commit],capture_output=True,text=True)
        self.assertEqual(result.returncode,0 if success else 1,result.stdout+result.stderr)

    def test_relocation_preserves_record_and_command_bytes(self):
        self.replay(True)
        self.assertEqual((self.root/GATE/'validation-record.json').read_bytes(),self.record_bytes)
        self.assertFalse((self.root/OLD).exists())
        self.assertEqual(self.git('status','--porcelain'),'')

    def test_relocated_source_tampering_refuses(self):
        path=self.root/BASE/'scripts/validate_operator_packet.py'
        path.write_bytes(path.read_bytes()+b'\n# changed\n')
        self.replay(False)

    def test_reviewed_artifact_tampering_refuses(self):
        (self.root/GATE/'operator-response.md').write_text('Changed decision\n')
        self.replay(False)

    def test_committed_command_tampering_refuses(self):
        path=self.root/GATE/'validation-record.json'
        record=json.loads(path.read_bytes());record['validation_command']='false'
        path.write_text(json.dumps(record),encoding='utf-8')
        self.commit('tamper command')
        self.record_commit=self.git('rev-parse','HEAD')
        self.replay(False)


if __name__ == '__main__':
    unittest.main()
