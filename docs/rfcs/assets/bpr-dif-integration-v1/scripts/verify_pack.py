#!/usr/bin/env python3
"""Verify offline RFC-pack integrity and local plan consistency, not the app.

The observed milestone-plan shape is checked here. The live MASK skill must still
validate the remapped plan against its installed schema during import.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path, PurePosixPath

EXPECTED_CLIPS = ('calm_idle','greet','rest','blink','compare_glance','consider',
                  'comment_ready','attention','acknowledge','blocked')

class PackError(ValueError):
    """A bounded handoff integrity or consistency failure."""


def ensure(value: bool, message: str) -> None:
    if not value:
        raise PackError(message)


def within(root: Path, name: str) -> Path:
    p=PurePosixPath(name)
    ensure(bool(name) and not p.is_absolute() and '..' not in p.parts and '\\' not in name,
           'Unsafe relative path: '+name)
    path=root.joinpath(*p.parts)
    current=root
    for component in p.parts:
        current=current/component
        ensure(not current.is_symlink(),'Symbolic link is not a pack file: '+name)
    ensure(path.resolve().is_relative_to(root.resolve()),'Path escapes pack: '+name)
    return path


def payload_names(root: Path) -> set[str]:
    return {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()
            and '__pycache__' not in p.parts and p.name!='SHA256SUMS'}


def load(root: Path, name: str) -> dict:
    data=json.loads(within(root,name).read_text(encoding='utf-8'))
    ensure(isinstance(data,dict),'JSON object required: '+name)
    return data


def frontmatter(path: Path) -> dict:
    text=path.read_text(encoding='utf-8')
    ensure(text.startswith('---\n'),'Missing frontmatter: '+path.name)
    end=text.find('\n---\n',4)
    ensure(end>=0,'Unclosed frontmatter: '+path.name)
    out={}
    for line in text[4:end].splitlines():
        key,sep,value=line.partition(':')
        ensure(bool(sep) and key not in out,'Invalid/duplicate frontmatter field: '+path.name)
        out[key]=json.loads(value)
    return out


def ancestors(nodes: dict[str,dict], key: str, stack: tuple[str,...]=()) -> set[str]:
    ensure(key in nodes,'Unknown dependency: '+key)
    ensure(key not in stack,'Dependency cycle: '+' -> '.join(stack+(key,)))
    result=set()
    for dep in nodes[key]['depends_on']:
        result.add(dep)
        result.update(ancestors(nodes,dep,stack+(key,)))
    return result


def verify(root: Path) -> dict:
    root=root.resolve()
    ledger={}
    for line in (root/'SHA256SUMS').read_text(encoding='utf-8').splitlines():
        h,sep,name=line.partition('  ')
        ensure(bool(sep) and re.fullmatch('[0-9a-f]{64}',h) is not None,'Malformed checksum line')
        ensure(name not in ledger and name!='SHA256SUMS','Duplicate/self checksum')
        p=within(root,name)
        ensure(p.is_file(),'Missing payload: '+name)
        ensure(hashlib.sha256(p.read_bytes()).hexdigest()==h,'Checksum mismatch: '+name)
        ledger[name]=h
    actual=payload_names(root)
    ensure(actual==set(ledger),'Payload inventory differs from checksum ledger')
    manifest=load(root,'PACK-MANIFEST.json')
    ensure(manifest['status']=='proposed_design_and_implementation_handoff','Unexpected pack status')
    ensure(manifest['no_app_implementation_or_native_verification_performed'] is True,'False runtime authoring claim')
    rfcs={}
    for name in manifest['rfc_paths']:
        f=frontmatter(within(root,name))
        ensure(f['status']=='Proposed','Unexpected RFC status')
        ensure(f['id'] not in rfcs,'Duplicate RFC')
        rfcs[f['id']]=f
    ensure(set(rfcs)==set(manifest['rfc_ids']),'RFC index mismatch')
    cards={}
    coverage=set()
    for name in manifest['card_paths']:
        card=frontmatter(within(root,name))
        ensure(card['schema']=='implement-rfc.card.v1','Wrong card schema')
        ensure(card['id'] not in cards,'Duplicate card')
        ensure(set(card['rfcs'])<=set(rfcs),'Unknown card RFC')
        ensure(card['status']=='Backlog','Pack aliases must remain unstarted')
        ensure(card['metadata']['alias_requires_live_allocation'] is True,'Missing live alias qualification')
        coverage.update(card['metadata']['acceptance'])
        cards[card['id']]=card
    ensure(set(cards)==set(manifest['issue_aliases']) and len(cards)==18,'Card inventory mismatch')
    ensure(coverage=={f'A{i}' for i in range(1,9)}|{f'B{i}' for i in range(1,13)},'Acceptance coverage mismatch')
    for key in cards:
        ancestors(cards,key)
    plan=load(root,manifest['authored_milestone_plan'])
    ensure(plan['schema']=='implement-rfc.milestone-plan.v1','Wrong plan schema')
    ensure(plan['coverage']=='complete' and set(plan['scope']['rfcs'])==set(rfcs),'Plan scope mismatch')
    ensure(plan.get('unplanned_work',[])==[],'Unexpected unplanned work')
    milestones={}
    owners={}
    for milestone in plan['milestones']:
        ensure(milestone['id'] not in milestones,'Duplicate milestone')
        ensure(bool(milestone['outcome']) and bool(milestone['exit_criteria']) and bool(milestone['boundary']),
               'Incomplete milestone')
        milestones[milestone['id']]=milestone
        for item in milestone['work']:
            ident=item['issue']
            ensure(ident in cards,'Plan references missing card')
            ensure(ident not in owners,'Card appears in multiple milestones')
            owners[ident]=milestone['id']
            ensure(cards[ident]['metadata']['milestone']==milestone['id'],'Card/plan milestone disagreement')
    ensure(len(milestones)==6 and set(owners)==set(cards),'Plan coverage incomplete')
    for ident,card in cards.items():
        allowed=ancestors(milestones,owners[ident])|{owners[ident]}
        ensure(all(owners[dep] in allowed for dep in card['depends_on']),'Card dependency crosses an unplanned milestone edge')
    inputs=load(root,manifest['asset_inputs'])
    contract=load(root,manifest['clip_contract'])
    cues=load(root,manifest['cue_map'])
    input_ids=[a['clip_id'] for a in inputs['assets']]
    clip_ids=[c['id'] for c in contract['clips']]
    cue_ids=[c['clip'] for c in cues['cues']]
    ensure(tuple(input_ids)==EXPECTED_CLIPS,'Wrong incoming clip inventory/order')
    ensure(set(clip_ids)==set(EXPECTED_CLIPS) and len(clip_ids)==10,'Wrong contract clip inventory')
    ensure(set(cue_ids)==set(EXPECTED_CLIPS) and len(cue_ids)==10,'Missing or duplicate cue mapping')
    by_id={c['id']:c for c in contract['clips']}
    for a in inputs['assets']:
        p=within(root,a['path']);raw=p.read_bytes()
        ensure(hashlib.sha256(raw).hexdigest()==a['sha256'] and len(raw)==a['bytes'],'Input metadata hash/size mismatch')
        ensure(raw[:8]==b'\x89PNG\r\n\x1a\n' and raw[12:16]==b'IHDR','Invalid PNG header')
        width,height=struct.unpack('>II',raw[16:24])
        ensure((width,height)==(a['width'],a['height']) and raw[25]==6,'PNG dimensions/RGBA mismatch')
        ensure(a['frame_count']==a['columns']*a['rows'],'Incoming layout/count mismatch')
        c=by_id[a['clip_id']]
        ensure(a['frame_count']==c['frame_count'] and a['frame_duration_ms']==c['frame_duration_ms'],'Source/contract mismatch')
        ensure(len(a['crop_proposals_half_open'])==a['frame_count'],'Missing proposed cuts')
        for rect in a['crop_proposals_half_open']:
            l,t,r,b=rect
            ensure(all(type(v) is int for v in rect) and 0<=l<r<=width and 0<=t<b<=height,'Invalid proposed crop')
        ensure(a['status']=='incoming_candidate_not_shipping_asset','Incoming art mislabeled production')
    frames=sum(c['frame_count'] for c in contract['clips'])
    ensure(frames==71,'Wrong target frame total')
    pixels=(frames+2*len(clip_ids))*256*256*4
    ensure(contract['counts']['complete_decoded_bytes']==pixels==23855104,'Complete pixel arithmetic mismatch')
    ensure(contract['counts']['content_decoded_bytes']==frames*256*256*4,'Content pixel arithmetic mismatch')
    limits=contract['limits']
    ensure(pixels<=limits['max_complete_decoded_bytes']==24*1024*1024,'Wrong complete pixel ceiling')
    ensure((limits['max_content_frames']+20)*256*256*4<=limits['max_complete_decoded_bytes'],'Headroom exceeds complete budget')
    for c in contract['clips']:
        ensure(len(c['frames'])==c['frame_count']<=24,'Choreography frame count mismatch')
        ensure(c['duration_ms']==c['frame_count']*c['frame_duration_ms'],'Clip duration mismatch')
        ensure(0<=c['representative_frame']<c['frame_count'],'Invalid representative frame')
        if c['kind']=='static':
            ensure(c['id']=='rest' and c['frame_count']==1 and c['frame_duration_ms']==0,'Bad static contract')
        else:
            ensure(c['frame_duration_ms']>0 and c['frame_duration_ms']*12>=1000,'Frame rate exceeds limit')
    audit=load(root,'verification/raw-asset-audit.json')
    ensure(audit['summary']['asset_count']==10 and audit['summary']['all_have_actual_alpha'],'Raw audit inconsistent')
    ensure(audit['summary']['fractional_grid_count']==5,'Wrong fractional grid count')
    return {'status':'passed','payload_files':len(ledger),'rfcs':len(rfcs),'cards':len(cards),
            'milestones':len(milestones),'input_images':len(input_ids),'target_frames':frames,
            'target_complete_pixel_bytes':pixels,
            'scope':'Pack integrity, source metadata and local plan/contract consistency only',
            'native_skill_import_validated':False,'app_runtime_verified':False,'art_production_approved':False}


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parent.parent)
    parser.add_argument('--json',action='store_true')
    args=parser.parse_args()
    try:
        result=verify(args.root)
        if args.json:
            print(json.dumps(result,indent=2))
        else:
            print(f"PASS — {result['rfcs']} RFCs, {result['cards']} cards, {result['milestones']} milestones; "
                  f"{result['input_images']} exact inputs, {result['target_frames']} target frames, 22.75 MiB complete pixels.")
            print('Checksums, plan DAG/coverage, clip/cue metadata and source headers agree.')
            print('This does not certify native skill import, BPR runtime behavior or production-art acceptance.')
        return 0
    except (PackError,OSError,ValueError,KeyError,TypeError,struct.error) as exc:
        if args.json:
            print(json.dumps({'status':'failed','error':str(exc)}))
        else:
            print('FAIL — '+str(exc),file=sys.stderr)
        return 1

if __name__=='__main__':
    raise SystemExit(main())
