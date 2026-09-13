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
import sys
from pathlib import Path, PurePosixPath

class PackError(ValueError):
    """A bounded handoff integrity or consistency failure."""


def ensure(value: bool, message: str) -> None:
    if not value:
        raise PackError(message)


def within(root: Path, name: str) -> Path:
    p = PurePosixPath(name)
    ensure(bool(name) and not p.is_absolute() and '..' not in p.parts and '\\' not in name,
           'Unsafe relative path: ' + name)
    path = root.joinpath(*p.parts)
    current = root
    for component in p.parts:
        current = current / component
        ensure(not current.is_symlink(), 'Symbolic link is not a pack file: ' + name)
    ensure(path.resolve().is_relative_to(root.resolve()), 'Path escapes pack: ' + name)
    return path


def payload_names(root: Path) -> set[str]:
    return {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()
            and '__pycache__' not in p.parts and p.name != 'SHA256SUMS'}


def load(root: Path, name: str) -> dict:
    data = json.loads(within(root, name).read_text(encoding='utf-8'))
    ensure(isinstance(data, dict), 'JSON object required: ' + name)
    return data


def frontmatter(path: Path) -> dict:
    text = path.read_text(encoding='utf-8')
    ensure(text.startswith('---\n'), 'Missing frontmatter: ' + path.name)
    end = text.find('\n---\n', 4)
    ensure(end >= 0, 'Unclosed frontmatter: ' + path.name)
    out = {}
    for line in text[4:end].splitlines():
        key, sep, value = line.partition(':')
        ensure(bool(sep) and key not in out, 'Invalid/duplicate frontmatter field: ' + path.name)
        out[key] = json.loads(value)
    return out


def acceptance_bullets(path: Path) -> list[str]:
    """The same parse the repository documentation gate applies to an RFC."""
    lines = path.read_text(encoding='utf-8').splitlines()
    start = next((i for i, l in enumerate(lines) if re.fullmatch(r'## (?:\d+\.\s+)?Acceptance criteria\s*', l)), None)
    ensure(start is not None, 'Missing acceptance heading: ' + path.name)
    out, cur = [], None
    for line in lines[start + 1:]:
        if line.startswith('## '):
            break
        m = re.fullmatch(r'- (?:\[[ xX]\] )?(.*)', line)
        if m:
            if cur is not None:
                out.append(' '.join(cur))
            cur = [m.group(1)]
        elif cur is not None and line[:1].isspace() and line.strip():
            cur.append(line.strip())
    if cur is not None:
        out.append(' '.join(cur))
    ensure(bool(out), 'Empty acceptance section: ' + path.name)
    return [re.sub(r'\s+', ' ', c).strip() for c in out]


def closeout_rows(path: Path) -> list[str]:
    text = path.read_text(encoding='utf-8')
    ensure(text.count('## Closeout checklist') == 1, 'Expected one closeout checklist: ' + path.name)
    ensure(len(re.findall(r'^RFC base: `[0-9a-f]{40}`\.$', text, re.M)) == 1, 'Expected one RFC base marker: ' + path.name)
    section = text.split('## Closeout checklist', 1)[1]
    nxt = re.search(r'^## ', section, re.M)
    if nxt:
        section = section[:nxt.start()]
    rows = [l for l in section.splitlines() if l.startswith('|')][2:]
    out = []
    for row in rows:
        cells = [c.strip() for c in row.strip().strip('|').split('|')]
        ensure(len(cells) == 3 and all(cells), 'Malformed checklist row: ' + path.name)
        out.append(re.sub(r'\s+', ' ', cells[0]))
    return out


def ancestors(nodes: dict[str, dict], key: str, stack: tuple[str, ...] = ()) -> set[str]:
    ensure(key in nodes, 'Unknown dependency: ' + key)
    ensure(key not in stack, 'Dependency cycle: ' + ' -> '.join(stack + (key,)))
    result = set()
    for dep in nodes[key]['depends_on']:
        result.add(dep)
        result.update(ancestors(nodes, dep, stack + (key,)))
    return result


def verify(root: Path) -> dict:
    root = root.resolve()
    ledger = {}
    for line in (root / 'SHA256SUMS').read_text(encoding='utf-8').splitlines():
        h, sep, name = line.partition('  ')
        ensure(bool(sep) and re.fullmatch('[0-9a-f]{64}', h) is not None, 'Malformed checksum line')
        ensure(name not in ledger and name != 'SHA256SUMS', 'Duplicate/self checksum')
        p = within(root, name)
        ensure(p.is_file(), 'Missing payload: ' + name)
        ensure(hashlib.sha256(p.read_bytes()).hexdigest() == h, 'Checksum mismatch: ' + name)
        ledger[name] = h
    ensure(payload_names(root) == set(ledger), 'Payload inventory differs from checksum ledger')
    manifest = load(root, 'PACK-MANIFEST.json')
    ensure(manifest['status'] == 'proposed_design_and_implementation_handoff', 'Unexpected pack status')
    ensure(manifest['no_app_implementation_or_native_verification_performed'] is True, 'False runtime authoring claim')
    ensure(manifest['design_reference_is_not_runtime_evidence'] is True, 'Design reference misdescribed')
    for name in ('design_reference', 'design_notes', 'source_audit'):
        ensure(within(root, manifest[name]).is_file(), 'Missing ' + name)
    rfcs, bullets = {}, {}
    for name in manifest['rfc_paths']:
        path = within(root, name)
        f = frontmatter(path)
        ensure(f['status'] == 'Proposed', 'Unexpected RFC status')
        ensure(f['id'] not in rfcs and path.name.startswith(f['id'] + '-'), 'RFC id/filename problem')
        rfcs[f['id']] = f
        bullets[f['id']] = acceptance_bullets(path)
        ids = manifest['acceptance_ids'][f['id']]
        ensure([re.match(r'\*\*([AB]\d+) ', b).group(1) for b in bullets[f['id']]] == ids,
               'Acceptance IDs disagree with manifest: ' + f['id'])
    ensure(set(rfcs) == set(manifest['rfc_ids']), 'RFC index mismatch')
    all_ids = {i for ids in manifest['acceptance_ids'].values() for i in ids}
    cards, coverage, closeouts = {}, set(), set()
    for name in manifest['card_paths']:
        path = within(root, name)
        card = frontmatter(path)
        ensure(card['schema'] == 'implement-rfc.card.v1', 'Wrong card schema')
        ensure(card['id'] not in cards and path.name == card['id'] + '.md', 'Card id/filename problem')
        ensure(set(card['rfcs']) <= set(rfcs), 'Unknown card RFC')
        ensure(card['status'] == 'Backlog', 'Pack aliases must remain unstarted')
        ensure(card['metadata']['alias_requires_live_allocation'] is True, 'Missing live alias qualification')
        ensure(set(card['metadata']['acceptance']) <= all_ids, 'Unknown acceptance id on ' + card['id'])
        coverage.update(card['metadata']['acceptance'])
        if 'closeout_for' in card:
            ensure(card['closeout_for'] == card['rfcs'] and len(card['rfcs']) == 1, 'Closeout card must name one RFC')
            ensure(card['title'] == 'Verify and close ' + card['rfcs'][0], 'Closeout title convention')
            ensure(closeout_rows(path) == bullets[card['rfcs'][0]], 'Closeout checklist differs from RFC criteria: ' + card['id'])
            closeouts.add(card['rfcs'][0])
        cards[card['id']] = card
    ensure(set(cards) == set(manifest['issue_aliases']), 'Card inventory mismatch')
    ensure(coverage == all_ids, 'Acceptance coverage mismatch')
    ensure(closeouts == set(rfcs), 'Every RFC needs one closeout card')
    for key in cards:
        ancestors(cards, key)
    plan = load(root, manifest['authored_milestone_plan'])
    ensure(plan['schema'] == 'implement-rfc.milestone-plan.v1', 'Wrong plan schema')
    ensure(plan['coverage'] == 'complete' and set(plan['scope']['rfcs']) == set(rfcs), 'Plan scope mismatch')
    ensure(plan.get('unplanned_work', []) == [], 'Unexpected unplanned work')
    milestones, owners = {}, {}
    for milestone in plan['milestones']:
        ensure(milestone['id'] not in milestones, 'Duplicate milestone')
        ensure(bool(milestone['outcome']) and bool(milestone['exit_criteria']) and bool(milestone['boundary']), 'Incomplete milestone')
        milestones[milestone['id']] = milestone
        for item in milestone['work']:
            ident = item['issue']
            ensure(ident in cards and ident not in owners, 'Plan work reference problem: ' + ident)
            owners[ident] = milestone['id']
            ensure(cards[ident]['metadata']['milestone'] == milestone['id'], 'Card/plan milestone disagreement')
    ensure(set(owners) == set(cards), 'Plan coverage incomplete')
    for ident, card in cards.items():
        allowed = ancestors(milestones, owners[ident]) | {owners[ident]}
        ensure(all(owners[dep] in allowed for dep in card['depends_on']), 'Card dependency crosses an unplanned milestone edge')
    return {'status': 'passed', 'payload_files': len(ledger), 'rfcs': len(rfcs), 'cards': len(cards),
            'milestones': len(milestones), 'criteria': len(all_ids),
            'scope': 'Pack integrity and local plan/criteria consistency only',
            'native_skill_import_validated': False, 'app_runtime_verified': False, 'owner_accepted': False}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    try:
        result = verify(args.root)
        if args.json:
            print(json.dumps(result, indent=2))
        else:
            print(f"PASS: {result['rfcs']} RFCs, {result['cards']} cards, {result['milestones']} milestones, "
                  f"{result['criteria']} criteria covered; checksums, plan DAG and closeout checklists agree.")
            print('This does not certify native skill import, BPR runtime behaviour or owner acceptance.')
        return 0
    except (PackError, OSError, ValueError, KeyError, TypeError, AttributeError) as exc:
        if args.json:
            print(json.dumps({'status': 'failed', 'error': str(exc)}))
        else:
            print('FAIL: ' + str(exc), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
