#!/usr/bin/env python3
"""Verify RFC-pack structure and exact examples using Python 3.10+ standard library.

Not an implement-rfc import, product build, native test, security certification,
model invocation or service integration check.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit
from reference_contract import (ContractError, strict_loads, validate_round,
                                validate_response, validate_bundle, render_markdown)

ROOT = Path(__file__).resolve().parents[1]


def load(root: Path, rel: str):
    return strict_loads((root / rel).read_bytes())


def frontmatter(path: Path) -> dict:
    text = path.read_text(encoding='utf-8')
    if not text.startswith('---\n') or '\n---\n' not in text[4:]:
        raise ValueError(f'missing front matter: {path.name}')
    result = {}
    for line in text.split('\n---\n', 1)[0].splitlines()[1:]:
        key, sep, value = line.partition(':')
        if not sep or key in result:
            raise ValueError(f'invalid/duplicate front matter: {path.name}')
        result[key] = strict_loads(value.strip())
    return result


def assert_dag(nodes: dict[str, list[str]], label: str) -> None:
    active, done = set(), set()
    def visit(n: str) -> None:
        if n in active:
            raise ValueError(f'{label} dependency cycle at {n}')
        if n in done:
            return
        if n not in nodes:
            raise ValueError(f'{label} unknown dependency {n}')
        active.add(n)
        for parent in nodes[n]:
            visit(parent)
        active.remove(n)
        done.add(n)
    for n in nodes:
        visit(n)


def markdown_links(root: Path) -> int:
    total = 0
    for path in root.rglob('*.md'):
        if 'examples/source' in path.as_posix():
            continue
        clean = []
        fence = None
        for line in path.read_text(encoding='utf-8').splitlines():
            m = re.match(r'^\s*(`{3,}|~{3,})', line)
            if fence is not None:
                if m and m.group(1)[0] == fence[0] and len(m.group(1)) >= len(fence):
                    fence = None
                continue
            if m:
                fence = m.group(1)
                continue
            clean.append(line)
        text = '\n'.join(clean)
        for target in re.findall(r'\[[^\]]*\]\(([^\s)]+)(?:\s+"[^"]*")?\)', text):
            target = target.strip('<>')
            if target.startswith('#') or urlsplit(target).scheme:
                continue
            rel = unquote(target.split('#', 1)[0])
            resolved = (path.parent / rel).resolve()
            if not resolved.is_relative_to(root.resolve()) or not resolved.exists():
                raise ValueError(f'broken/outside link in {path.relative_to(root)}: {target}')
            total += 1
    return total


def verify(root: Path = ROOT, integrity: bool = True) -> dict:
    counts = {}
    if integrity:
        manifest = load(root, 'PACK-MANIFEST.json')
        listed = {}
        for entry in manifest['files']:
            rel = entry['path']
            if rel in listed or rel == 'PACK-MANIFEST.json':
                raise ValueError('duplicate/self manifest member')
            p = (root / rel).resolve()
            if not p.is_relative_to(root.resolve()) or not p.is_file():
                raise ValueError(f'missing/outside member: {rel}')
            raw = p.read_bytes()
            if len(raw) != entry['bytes'] or hashlib.sha256(raw).hexdigest() != entry['sha256']:
                raise ValueError(f'integrity mismatch: {rel}')
            listed[rel] = entry
        actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()
                  and '__pycache__' not in p.parts and p.name != 'PACK-MANIFEST.json'}
        if actual != set(listed):
            raise ValueError(f'manifest inventory mismatch: {sorted(actual ^ set(listed))}')
        counts['manifest_files'] = len(listed)
    rfcs = list((root / 'repository-overlay/docs/rfcs').glob('RFC-BPR-*.md'))
    rmap = {frontmatter(p)['id']: frontmatter(p) for p in rfcs}
    if len(rmap) != 9 or set(rmap) != {f'RFC-BPR-{i:04d}' for i in range(53,62)}:
        raise ValueError('RFC inventory mismatch')
    for rid, fm in rmap.items():
        if fm['status'] != 'Proposed':
            raise ValueError(f'not a proposal: {rid}')
    assert_dag({r: [p for p in f['depends_on'] if p in rmap] for r, f in rmap.items()}, 'RFC')
    cards = load(root, 'cards/CARD-INDEX.json')['cards']
    cmap = {c['id']: c for c in cards}
    if len(cards) != 52 or len(cmap) != 52:
        raise ValueError('card inventory mismatch')
    for card in cards:
        fm = frontmatter(root / card['file'])
        for field in ('id','rfcs','depends_on'):
            if fm[field] != card[field]:
                raise ValueError(f'card index mismatch: {card["id"]}/{field}')
        if fm['schema'] != 'implement-rfc.card.v1' or fm['status'] != 'Backlog':
            raise ValueError('invalid card schema/status')
        if set(card['rfcs']) - set(rmap):
            raise ValueError('unknown RFC on card')
        if fm.get('closeout_for', []) != card['closeout_for']:
            raise ValueError('closeout mismatch')
    assert_dag({c['id']:c['depends_on'] for c in cards}, 'card')
    plan = load(root, 'milestone-plan.json')
    if plan['schema'] != 'implement-rfc.milestone-plan.v1' or plan['coverage'] != 'complete' or set(plan['scope']['rfcs']) != set(rmap):
        raise ValueError('invalid native plan shape/scope')
    milestones = {m['id']:m for m in plan['milestones']}
    if len(milestones) != 11:
        raise ValueError('milestone count mismatch')
    assert_dag({m['id']:m['depends_on'] for m in plan['milestones']}, 'milestone')
    assignment = {}
    positions = {}
    for mi,m in enumerate(plan['milestones']):
        for wi,w in enumerate(m['work']):
            cid = w['issue']
            if cid in assignment or cid not in cmap:
                raise ValueError('duplicate/unknown milestone work')
            if cmap[cid]['milestone'] != m['id']:
                raise ValueError('card milestone disagrees')
            assignment[cid] = m['id']
            positions[cid] = (mi,wi)
    if set(assignment) != set(cmap):
        raise ValueError('incomplete card milestone coverage')
    for c in cards:
        if any(positions[d] >= positions[c['id']] for d in c['depends_on']):
            raise ValueError(f'non-topological authored order: {c["id"]}')
    matrix = load(root, 'verification/acceptance-matrix.json')['requirements']
    reqs = {r['id']:r for r in matrix}
    if len(reqs) != 90 or len(matrix) != 90:
        raise ValueError('acceptance inventory mismatch')
    for r in matrix:
        expected = {c['id'] for c in cards if r['id'] in c['acceptance'] and not c['closeout_for']}
        if not expected or expected != set(r['implementation_cards']):
            raise ValueError(f'orphan/mismatched requirement: {r["id"]}')
        if r['status'] != 'Not run' or r['evidence']['result'] != 'Not run':
            raise ValueError('artifact masquerades as product evidence')
    for c in cards:
        if set(c['acceptance']) - set(reqs):
            raise ValueError('unknown card acceptance ID')
    for rid in rmap:
        closes = [c for c in cards if rid in c['closeout_for']]
        if len(closes) != 1:
            raise ValueError('one closeout required per RFC')
        if set(closes[0]['acceptance']) != {r['id'] for r in matrix if r['rfc'] == rid}:
            raise ValueError('incomplete closeout criteria')
    scenarios = load(root,'verification/SCENARIOS.json')['scenarios']
    if len({s['id'] for s in scenarios}) != len(scenarios):
        raise ValueError('duplicate scenario')
    for s in scenarios:
        if set(s['requirements']) - set(reqs) or s['status'] != 'Not run':
            raise ValueError('invalid scenario evidence/requirement')
    for path in (root/'schemas').glob('*.json'):
        schema = strict_loads(path.read_bytes())
        if schema.get('$schema') != 'https://json-schema.org/draft/2020-12/schema':
            raise ValueError('schema dialect mismatch')
        def refs(v):
            if isinstance(v,dict):
                if '$ref' in v:
                    ref = v['$ref']
                    if not ref.startswith('#/$defs/') or ref[8:] not in schema.get('$defs',{}):
                        raise ValueError(f'unknown local schema reference: {ref}')
                for x in v.values(): refs(x)
            elif isinstance(v,list):
                for x in v: refs(x)
        refs(schema)
    envelope = load(root,'examples/round.json')
    validate_round(envelope)
    validate_response(load(root,'examples/response.json'),envelope)
    if (root/'examples/review.md').read_bytes() != render_markdown(envelope).encode('utf-8'):
        raise ValueError('readable round differs from canonical example')
    validate_bundle((root/'examples/example.bpr-review.zip').read_bytes())
    skill = (root/'skill-source/bpr-cycle/SKILL.md').read_text(encoding='utf-8')
    if '\nname: bpr-cycle\n' not in skill or not skill.startswith('---\n') or len(skill.splitlines()) > 500:
        raise ValueError('skill identity/length mismatch')
    counts.update(rfcs=len(rfcs),cards=len(cards),milestones=len(milestones),
                  acceptance_requirements=len(matrix),production_scenarios=len(scenarios),
                  local_links=markdown_links(root),example_feedback_items=len(envelope['payload']['items']),
                  example_suggestions=sum(len(i['suggestions']) for i in envelope['payload']['items']))
    return counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json',action='store_true',help='emit machine result only')
    args = parser.parse_args()
    try:
        counts = verify()
        result = {'schema':'bpr.review-cycles.pack-validation.v1','status':'pass',
                  'scope':'artifact only; no product/native/import/service evidence','counts':counts}
        if args.json:
            print(json.dumps(result,indent=2))
        else:
            print('PASS — Review Cycles RFC archive validation (artifact only).')
            for key,value in counts.items(): print(f'  {key.replace("_"," ")}: {value}')
            print('Product acceptance: all 90 rows remain Not run.')
        return 0
    except (OSError,ValueError,KeyError,TypeError,ContractError) as exc:
        if args.json: print(json.dumps({'status':'fail','error':str(exc)}))
        else: print(f'FAIL — {exc}',file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
