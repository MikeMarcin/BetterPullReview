from __future__ import annotations
import copy
import hashlib
import io
import json
import stat
import sys
import unittest
import zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from reference_contract import (ContractError, strict_loads, canonical, digest,
    byte_record, checked_bytes, safe_path, validate_round, apply_exact, fenced,
    render_markdown, validate_response, record_response, overlay_selected, validate_bundle)


class CanonicalTests(unittest.TestCase):
    def test_key_order_stable(self):
        self.assertEqual(canonical({'b':2,'a':1}),b'{"a":1,"b":2}')
    def test_array_order_matters(self):
        self.assertNotEqual(digest([1,2]),digest([2,1]))
    def test_unicode_not_normalized(self):
        self.assertNotEqual(digest('é'),digest('e\u0301'))
    def test_duplicate_json_refused(self):
        with self.assertRaises(ContractError):strict_loads('{"x":1,"x":2}')
    def test_float_refused(self):
        with self.assertRaises(ContractError):canonical({'x':1.0})
    def test_nonfinite_refused(self):
        with self.assertRaises(ContractError):strict_loads('{"x":NaN}')
    def test_surrogate_refused(self):
        with self.assertRaises(ContractError):canonical('\ud800')
    def test_huge_integer_refused(self):
        with self.assertRaises(ContractError):canonical(2**60)
    def test_depth_limit(self):
        x=[]
        for _ in range(40):x=[x]
        with self.assertRaises(ContractError):canonical(x)


class ByteAndEditTests(unittest.TestCase):
    def test_tabs_crlf_roundtrip(self):
        raw=b'\tfoo();\r\n'; self.assertEqual(checked_bytes(byte_record(raw)),raw)
    def test_empty_replacement(self):
        raw=b'abc';self.assertEqual(apply_exact(raw,hashlib.sha256(raw).hexdigest(),[(1,2,b'b',b'')]),b'ac')
    def test_insert(self):
        raw=b'ab';self.assertEqual(apply_exact(raw,hashlib.sha256(raw).hexdigest(),[(1,1,b'',b'XY')]),b'aXYb')
    def test_two_edits_original_coordinates(self):
        raw=b'abcdef';h=hashlib.sha256(raw).hexdigest()
        self.assertEqual(apply_exact(raw,h,[(0,1,b'a',b'LONG'),(4,6,b'ef',b'!')]),b'LONGbcd!')
    def test_adjacent_replacements_allowed(self):
        raw=b'ab';h=hashlib.sha256(raw).hexdigest()
        self.assertEqual(apply_exact(raw,h,[(0,1,b'a',b'x'),(1,2,b'b',b'y')]),b'xy')
    def test_overlap_refused(self):
        raw=b'abcd';h=hashlib.sha256(raw).hexdigest()
        with self.assertRaises(ContractError):apply_exact(raw,h,[(0,3,b'abc',b'x'),(2,4,b'cd',b'y')])
    def test_same_boundary_insertions_refused(self):
        raw=b'a';h=hashlib.sha256(raw).hexdigest()
        with self.assertRaises(ContractError):apply_exact(raw,h,[(0,0,b'',b'x'),(0,0,b'',b'y')])
    def test_insertion_at_replacement_edge_refused(self):
        raw=b'ab';h=hashlib.sha256(raw).hexdigest()
        with self.assertRaises(ContractError):apply_exact(raw,h,[(0,1,b'a',b'x'),(1,1,b'',b'y')])
    def test_stale_whole_file_refused(self):
        with self.assertRaises(ContractError):apply_exact(b'abcX',hashlib.sha256(b'abc').hexdigest(),[(0,1,b'a',b'x')])
    def test_wrong_original_slice_refused(self):
        raw=b'abc'
        with self.assertRaises(ContractError):apply_exact(raw,hashlib.sha256(raw).hexdigest(),[(0,1,b'z',b'x')])
    def test_hash_mismatch_refused(self):
        rec=byte_record(b'x');rec['sha256']='0'*64
        with self.assertRaises(ContractError):checked_bytes(rec)
    def test_invalid_base64_refused(self):
        rec=byte_record(b'x');rec['base64']='eA==\n'
        with self.assertRaises(ContractError):checked_bytes(rec)
    def test_bool_is_not_a_length(self):
        rec=byte_record(b'x');rec['length']=True
        with self.assertRaises(ContractError):checked_bytes(rec)
    def test_byte_budget(self):
        with self.assertRaises(ContractError):checked_bytes(byte_record(b'ab'),1)


class RoundAndResponseTests(unittest.TestCase):
    def setUp(self):
        self.e=strict_loads((ROOT/'examples/round.json').read_bytes())
        self.r=strict_loads((ROOT/'examples/response.json').read_bytes())
    def resigned(self): self.e['payload_sha256']=digest(self.e['payload'])
    def test_full_example(self):
        validate_round(self.e)
        self.assertEqual(len(self.e['payload']['items']),8)
        self.assertEqual(sum(len(i['suggestions']) for i in self.e['payload']['items']),2)
    def test_unannounced_edit(self):
        self.e['payload']['items'][0]['body']='changed'
        with self.assertRaises(ContractError):validate_round(self.e)
    def test_count_mismatch(self):
        self.e['payload']['selection']['item_count']=7;self.resigned()
        with self.assertRaises(ContractError):validate_round(self.e)
    def test_partial_instructions_refused(self):
        self.e['payload']['selection']['instruction_set_complete']=False;self.resigned()
        with self.assertRaises(ContractError):validate_round(self.e)
    def test_duplicate_item_refused(self):
        self.e['payload']['items'][1]['id']='I1';self.resigned()
        with self.assertRaises(ContractError):validate_round(self.e)
    def test_dangling_relation(self):
        self.e['payload']['items'][0]['related_item_ids']=['private-other-item'];self.resigned()
        with self.assertRaises(ContractError):validate_round(self.e)
    def test_changed_suggestion_original_refused(self):
        s=self.e['payload']['items'][0]['suggestions'][0];s['original']=byte_record(b'wrong');self.resigned()
        with self.assertRaises(ContractError):validate_round(self.e)
    def test_already_applied_needs_result(self):
        self.e['payload']['items'][0]['suggestions'][0]['state']='observed_already_applied';self.resigned()
        with self.assertRaises(ContractError):validate_round(self.e)
    def test_response_is_optional_data(self):
        validate_response(self.r,self.e)
        self.assertIsNone(self.r['result_source_revision_id'])
    def test_response_wrong_round(self):
        self.r['round_id']='F2'
        with self.assertRaises(ContractError):validate_response(self.r,self.e)
    def test_response_cannot_verify(self):
        self.r['items'][0]['disposition']='verified'
        with self.assertRaises(ContractError):validate_response(self.r,self.e)
    def test_response_cannot_grant(self):
        self.r['publish_authorized']=True
        with self.assertRaises(ContractError):validate_response(self.r,self.e)
    def test_response_idempotence(self):
        registry={};self.assertEqual(record_response(registry,self.r,self.e),'recorded')
        self.assertEqual(record_response(registry,self.r,self.e),'duplicate')
    def test_response_id_conflict(self):
        registry={};record_response(registry,self.r,self.e);self.r['items'][0]['body']='different'
        with self.assertRaises(ContractError):record_response(registry,self.r,self.e)
    def test_dynamic_fences(self):
        text='x\n````\n';out=fenced(text,'cpp');self.assertTrue(out.startswith('`````cpp\n'))
    def test_readable_projection_exact(self):
        out=render_markdown(self.e);self.assertEqual(out.encode(),(ROOT/'examples/review.md').read_bytes())
        self.assertIn('Rendered text is not an exact copy/paste patch',out)


class ScopeAndBundleTests(unittest.TestCase):
    def test_outside_scope_preserved(self):
        parent={'a':b'old','outside':b'keep'}
        self.assertEqual(overlay_selected(parent,{'a':b'new'},{'a'}),{'a':b'new','outside':b'keep'})
    def test_explicit_delete_only(self):
        self.assertEqual(overlay_selected({'a':b'x','b':b'y'},{'a':None},{'a'}),{'b':b'y'})
    def test_uncaptured_delete_refused(self):
        with self.assertRaises(ContractError):overlay_selected({'a':b'x'},{'a':None},set())
    def test_parent_immutable(self):
        p={'a':b'x'};overlay_selected(p,{'a':b'y'},{'a'});self.assertEqual(p,{'a':b'x'})
    def test_portable_path_refusals(self):
        for p in ('../x','/x','C:/x','a\\b','a/%2e%2e/x','a/%252e%252e/x','a//x','a/./x','NUL.txt','a.','a\x00b'):
            with self.subTest(path=p),self.assertRaises(ContractError):safe_path(p)
    def test_unicode_alias(self):
        self.assertEqual(safe_path('é.txt'),safe_path('e\u0301.txt'))
    def test_case_alias(self):
        self.assertEqual(safe_path('Dir/A.cpp'),safe_path('dir/a.cpp'))
    def test_real_bundle_fixture(self):
        members=validate_bundle((ROOT/'examples/example.bpr-review.zip').read_bytes())
        self.assertIn('round.json',members)
    def test_symlink_member_refused(self):
        bio=io.BytesIO()
        with zipfile.ZipFile(bio,'w') as z:
            info=zipfile.ZipInfo('link');info.create_system=3;info.external_attr=(stat.S_IFLNK|0o777)<<16
            z.writestr(info,'target')
        with self.assertRaises(ContractError):validate_bundle(bio.getvalue())
    def test_traversal_member_refused(self):
        bio=io.BytesIO()
        with zipfile.ZipFile(bio,'w') as z:z.writestr('../evil','x')
        with self.assertRaises(ContractError):validate_bundle(bio.getvalue())
    def test_case_duplicate_member_refused(self):
        bio=io.BytesIO()
        with zipfile.ZipFile(bio,'w') as z:z.writestr('A','x');z.writestr('a','y')
        with self.assertRaises(ContractError):validate_bundle(bio.getvalue())
    def test_bomb_ratio_refused(self):
        bio=io.BytesIO()
        with zipfile.ZipFile(bio,'w',compression=zipfile.ZIP_DEFLATED) as z:z.writestr('a',b'0'*200000)
        with self.assertRaises(ContractError):validate_bundle(bio.getvalue())


if __name__=='__main__':unittest.main()
