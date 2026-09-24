"""Offline pack validation tests; no product/runtime/live claims."""
from __future__ import annotations
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("verify_pack", ROOT / "scripts/verify_pack.py")
v = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)

def seal(root: Path) -> None:
    (root/"SHA256SUMS").write_text("".join(hashlib.sha256((root/n).read_bytes()).hexdigest()+"  "+n+"\n" for n in v.payload_names(root)))

class PackTests(unittest.TestCase):
    def clone(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        root = Path(tmp.name)/"pack"
        shutil.copytree(ROOT, root)
        return root
    def mutate(self, file, fn):
        root=self.clone();path=root/file
        obj=json.loads(path.read_text());fn(obj);path.write_text(json.dumps(obj));seal(root)
        return root
    def reject(self, root, pattern):
        with self.assertRaisesRegex(v.PackError,pattern):v.verify(root)
    def test_exact(self): self.assertEqual(v.verify(ROOT)["status"],"passed")
    def test_checksum_modified(self):
        p=self.clone();(p/"README.md").write_text("changed");self.reject(p,"Checksum mismatch")
    def test_untracked_file(self):
        p=self.clone();(p/"unexpected.txt").write_text("x");self.reject(p,"inventory")
    def test_parent_overlay(self):
        p=self.clone();(p/"repository-overlay/docs/rfcs/RFC-BPR-0039-old.md").write_text("old");seal(p);self.reject(p,"overlay")
    def test_parent_plan_scope(self):
        p=self.mutate("milestone-plan.json",lambda d:d["scope"]["rfcs"].append("RFC-BPR-0039"));self.reject(p,"parent scope")
    def test_milestone_cycle(self):
        p=self.mutate("milestone-plan.json",lambda d:d["milestones"][0]["depends_on"].append("verification-closeout"));self.reject(p,"Dependency cycle")
    def test_card_cycle(self):
        p=self.mutate("cards/CARD-INDEX.json",lambda d:d["cards"][0]["depends_on"].append("PACK-PRTOUR-20"));self.reject(p,"Dependency cycle")
    def test_missing_work(self):
        p=self.mutate("milestone-plan.json",lambda d:d["milestones"][1]["work"].pop());self.reject(p,"coverage")
    def test_duplicate_work(self):
        p=self.mutate("milestone-plan.json",lambda d:d["milestones"][1]["work"].append(d["milestones"][0]["work"][0]));self.reject(p,"Duplicate milestone work")
    def test_unknown_requirement(self):
        p=self.mutate("cards/CARD-INDEX.json",lambda d:d["cards"][0]["acceptance"].append("AC99-99"));self.reject(p,"Unknown requirement")
    def test_missing_owner(self):
        p=self.mutate("cards/CARD-INDEX.json",lambda d:d["cards"][1]["acceptance"].clear());self.reject(p,"ownership gap")
    def test_wrong_skill_name(self):
        p=self.clone();f=p/"skill-source/pr-tour/SKILL.md";f.write_text(f.read_text().replace("name: pr-tour","name: review-tour"));seal(p);self.reject(p,"Skill name")
    def test_skill_publisher_script(self):
        p=self.clone();(p/"skill-source/pr-tour/publisher.py").write_text("# no second publisher\n");seal(p);self.reject(p,"Executable code")
    def test_default_json(self):
        p=self.mutate("specs/command-catalog.json",lambda d:d.update(default_output="json"));self.reject(p,"Human default")
    def test_save_remote_write(self):
        def edit(d):next(x for x in d["commands"] if x["name"]=="edit")["remote_write"]=True
        p=self.mutate("specs/command-catalog.json",edit);self.reject(p,"Unexpected remote writer")
    def test_unknown_command(self):
        p=self.mutate("examples/cli/show-problem.json",lambda d:d.update(command="raw-api"));self.reject(p,"Envelope command")
    def test_numeric_large_identity(self):
        p=self.mutate("examples/cli/open-published.json",lambda d:d["data"]["identity"].update(repository_id=9007199254740993));self.reject(p,"Lossless identifier")
    def test_fabricated_install(self):
        p=self.mutate("PACK-MANIFEST.json",lambda d:d.update(skill_installed=True));self.reject(p,"implementation/installation claim")
    def test_fabricated_acceptance(self):
        p=self.mutate("verification/acceptance-matrix.json",lambda d:d["requirements"][0].update(status="Passed"));self.reject(p,"Fabricated product evidence")
    def test_broken_link(self):
        p=self.clone();f=p/"README.md";f.write_text(f.read_text()+"\n[Missing](missing.md)\n");seal(p);self.reject(p,"Broken local link")
    def test_outside_link(self):
        p=self.clone();f=p/"README.md";f.write_text(f.read_text()+"\n[Outside](../outside.md)\n");seal(p);self.reject(p,"Escaping local link")
    def test_model_target_directive(self):
        p=self.mutate("examples/assist/parent-rewrite-result.json",lambda d:d.update(target="another-field"));self.reject(p,"model result authority")
    def test_reference_stale_protects(self):
        c=next(x for x in v.load(ROOT,"examples/text-ranges.json")["cases"] if x["id"]=="stale-aba")
        self.assertEqual(v.reference_splice(c),("stale",c["before"]))
    def test_reference_unicode_protects(self):
        c=next(x for x in v.load(ROOT,"examples/text-ranges.json")["cases"] if x["id"]=="invalid-scalar-boundary")
        self.assertEqual(v.reference_splice(c),("invalid_range",c["before"]))
    def test_reference_noop(self):
        c=next(x for x in v.load(ROOT,"examples/text-ranges.json")["cases"] if x["id"]=="no-op")
        self.assertEqual(v.reference_splice(c),("no_op",c["before"]))
    def test_reference_outside_text_change(self):
        def edit(d):d["cases"][0]["expected"]+=" outside scope"
        p=self.mutate("examples/text-ranges.json",edit);self.reject(p,"Reference splice mismatch")
    def test_removed_source_seam(self):
        p=self.mutate("references/SOURCE-BASELINE.json",lambda d:d["source_snapshot"].update(source_entries=[]));self.reject(p,"Missing source seam")
    def test_manifest_not_extension(self):
        p=self.mutate("PACK-MANIFEST.json",lambda d:d.update(extension_only=False));self.reject(p,"Extension-only")

if __name__=="__main__":unittest.main()
