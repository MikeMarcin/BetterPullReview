#!/usr/bin/env python3
"""Validate issue cards, report RFC queues, and move one card with its live links.

Uses the portable project config. Mutations require explicit bookkeeping policy.
No staging, commits, dependency edits, or acceptance judgments are performed.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from dataclasses import asdict
import fnmatch
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile
from urllib.parse import quote, unquote, urlsplit, urlunsplit

if sys.version_info < (3, 10):
    sys.exit(f'issue_board.py requires Python 3.10+; found {sys.version.split()[0]} at {sys.executable}')

sys.path.insert(0, str(Path(__file__).resolve().parent))
import board_model as board
import project_config

# Match inline links and reference definitions, retaining offsets into the
# original text. Code fences and inline code are masked before searching.
LINK_RE = re.compile(r'!?\[[^\]\n]*\]\(\s*(?P<inline><[^>\n]+>|[^\s()]+)'
                     r'(?:\s+["\'][^\n]*?["\'])?\s*\)'
                     r'|^ {0,3}\[[^\]\n]+\]:\s*(?P<reference><[^>\n]+>|\S+)', re.M)
def mask_code(text: str) -> str:
    """Keep character offsets while hiding fenced, indented, and inline code."""
    def blank(value: str) -> str:
        return ''.join('\n' if char == '\n' else ' ' for char in value)

    fence = None
    lines = []
    for line in text.splitlines(keepends=True):
        marker = re.match(r'^ {0,3}(`{3,}|~{3,})(.*)', line)
        if fence is not None:
            lines.append(blank(line))
            if marker and marker[1][0] == fence[0] and len(marker[1]) >= len(fence) and not marker[2].strip():
                fence = None
        elif marker:
            fence = marker[1]
            lines.append(blank(line))
        elif line.startswith(('    ', '\t')):
            lines.append(blank(line))
        else:
            lines.append(line)
    masked = ''.join(lines)
    return re.sub(r'(?<!`)(`+)(?!`)([\s\S]*?)(?<!`)\1(?!`)', lambda m: blank(m[0]), masked)


def require_valid(root: Path, config: dict, overrides: dict | None = None) -> dict:
    cards, errors = board.validate_board(root, config, overrides)
    if errors:
        raise board.ValidationError(root, errors)
    return cards


def queue(root: Path, config: dict, rfc: str, platform: str, include: list[str], all_cards: bool) -> dict:
    cards = require_valid(root, config)
    if not board.RFC_RE.fullmatch(rfc) or len(list((root / config['rfc_root']).glob(rfc + '-*.md'))) != 1:
        raise board.BoardError(root, 'rfc', f'unknown or ambiguous RFC {rfc}')
    for card_id in include:
        if card_id not in cards:
            raise board.BoardError(root, 'include', f'unknown card {card_id}')
    selected = sorted((c for c in cards.values() if rfc in c.rfcs or c.id in include),
                      key=lambda c: (int(c.id.rsplit('-', 1)[1]), c.id))
    lanes = config['lanes']
    done = lanes['done']
    startable = {lanes['ready'], lanes['platform_ready'].get(platform)}
    rows = []
    for card in selected:
        unmet = [dep for dep in card.deps if cards[dep].lane != done]
        if card.lane == done:
            disposition = 'done'
        elif card.lane == lanes['in_progress']:
            disposition = 'in_progress'
        elif card.lane == lanes['parked']:
            disposition = 'parked'
        elif card.lane == lanes['blocked']:
            disposition = 'blocked'
        elif card.lane in lanes['platform_ready'].values() and card.lane not in startable:
            disposition = 'deferred_platform'
        elif unmet:
            disposition = 'waiting_dependencies'
        elif card.lane == lanes['backlog']:
            disposition = 'backlog_candidate'
        else:
            disposition = 'ready'
        rows.append({'id': card.id, 'title': card.fields['title'],
                     'path': card.path.relative_to(root).as_posix(), 'lane': card.lane,
                     'status': card.fields['status'], 'unmet_dependencies': unmet,
                     'disposition': disposition,
                     'reason': card.fields.get('parked_on', card.fields.get('blocked_by'))})
    active = [row['id'] for row in rows if row['disposition'] == 'in_progress']
    ready = [row['id'] for row in rows if row['disposition'] == 'ready']
    matcher = project_config.closeout_card_matcher(config)
    return {'schema': 'implement-rfc.queue.v1', 'rfc': rfc, 'platform': platform,
            'next': None if len(active) > 1 else next(iter(active or ready), None),
            'ambiguous_in_progress': active if len(active) > 1 else [],
            'closeout_paths': [c.path.relative_to(root).as_posix() for c in selected if matcher(c.path, c.fields)],
            'cards': [row for row in rows if all_cards or row['disposition'] != 'done']}


def markdown_paths(root: Path, rules: dict) -> list[Path]:
    paths = set()
    for relative in rules['link_roots']:
        path = board.safe_path(root, root / relative)
        if path.is_file() and path.suffix == '.md':
            paths.add(path)
        elif path.is_dir():
            paths.update(path.rglob('*.md'))
    return sorted(board.safe_path(root, path) for path in paths)


def protected(root: Path, path: Path, rules: dict) -> bool:
    relative = path.relative_to(root).as_posix()
    return (any(fnmatch.fnmatchcase(relative, pattern) for pattern in rules['protected_globs'])
            and not any(fnmatch.fnmatchcase(relative, pattern) for pattern in rules['editable_globs']))


def rewrite_links(data: bytes, path: Path, new_path: Path, source: Path, destination: Path, root: Path) -> bytes:
    if source == destination and path == new_path:
        return data
    text = data.decode('utf-8')
    masked = mask_code(text)
    edits = []
    for match in LINK_RE.finditer(masked):
        group = 'inline' if match.group('inline') is not None else 'reference'
        begin, end = match.span(group)
        original = text[begin:end]
        angled = original.startswith('<')
        target = original[1:-1] if angled else original
        url = urlsplit(target)
        if url.scheme or url.netloc or not url.path:
            continue
        decoded = unquote(url.path)
        resolved = ((root / decoded.lstrip('/')) if decoded.startswith('/') else path.parent / decoded).resolve()
        if not resolved.is_relative_to(root):
            continue
        new_target = destination if resolved == source else resolved
        if resolved != source and path.parent == new_path.parent:
            continue
        if decoded.startswith('/'):
            rewritten = '/' + new_target.relative_to(root).as_posix()
        else:
            rewritten = Path(os.path.relpath(new_target, new_path.parent)).as_posix()
        if '%' in url.path or not angled:
            rewritten = quote(rewritten, safe='/._~-')
        rewritten = urlunsplit(('', '', rewritten, url.query, url.fragment))
        if angled:
            rewritten = '<' + rewritten + '>'
        if rewritten != original:
            edits.append((begin, end, rewritten))
    for begin, end, value in reversed(edits):
        text = text[:begin] + value + text[end:]
    return text.encode('utf-8')


def plan_move(root: Path, config: dict, card_id: str, lane: str,
              parked_on: str | None, blocked_by: str | None) -> tuple[dict, dict]:
    if 'bookkeeping' not in config:
        raise board.BoardError(root, 'bookkeeping', 'configure bookkeeping policy before moving cards')
    cards = require_valid(root, config)
    if card_id not in cards:
        raise board.BoardError(root, 'id', f'unknown card {card_id}')
    if lane not in project_config.lane_names(config):
        raise board.BoardError(root, 'lane', f'unknown lane {lane}')
    card = cards[card_id]
    lanes = config['lanes']
    if card.lane == lanes['done'] and lane != card.lane:
        raise board.BoardError(card.path, 'lane', 'Done is terminal; create follow-up work')
    for value, allowed, key in ((parked_on, lanes['parked'], 'parked_on'), (blocked_by, lanes['blocked'], 'blocked_by')):
        if value is not None and (lane != allowed or not board.TOKEN_RE.fullmatch(value)):
            raise board.BoardError(card.path, key, 'invalid token or destination lane')
    updates = {'status': config['lane_statuses'][lane],
               'parked_on': (parked_on or card.fields.get('parked_on')) if lane == lanes['parked'] else None,
               'blocked_by': (blocked_by or card.fields.get('blocked_by')) if lane == lanes['blocked'] else None}
    source = card.path
    destination = board.safe_path(root, source.parent.parent / lane / source.name)
    if source != destination and destination.exists():
        raise board.BoardError(destination, 'path', 'destination already exists')
    rules = config['bookkeeping']
    paths = set(markdown_paths(root, rules)) | {source}
    originals = {path: path.read_bytes() for path in paths}
    changes: dict[Path, bytes | None] = {}
    for path in sorted(paths):
        new_path = destination if path == source else path
        data = card.matter.updated(updates) if path == source else originals[path]
        updated = rewrite_links(data, path, new_path, source, destination, root)
        if new_path != path or updated != originals[path]:
            if protected(root, path, rules) or protected(root, new_path, rules):
                raise board.BoardError(path, 'link', 'move would change protected content')
            changes[new_path] = updated
    if source != destination:
        changes[source] = None
        originals[destination] = None
    require_valid(root, config, changes)
    return changes, originals


@contextmanager
def board_lock(root: Path):
    result = subprocess.run(['git', '-C', str(root), 'rev-parse', '--absolute-git-dir'],
                            capture_output=True, text=True, check=True)
    path = Path(result.stdout.strip()) / 'implement-rfc-board.lock'
    try:
        descriptor = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    except FileExistsError:
        raise board.BoardError(path, 'lock', 'another move is active; after interruption confirm its process ended before removing this lock') from None
    try:
        with os.fdopen(descriptor, 'w') as stream:
            stream.write(str(os.getpid()) + '\n')
        yield
    finally:
        path.unlink()


def write_file(path: Path, data: bytes, *, create: bool, mode: int = 0o644) -> None:
    descriptor, temporary = tempfile.mkstemp(prefix='.issue-board-', dir=path.parent)
    temporary_path = Path(temporary)
    try:
        with os.fdopen(descriptor, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(temporary_path, mode)
        if create:
            os.link(temporary_path, path)  # Atomic publication that refuses overwrite.
        else:
            os.replace(temporary_path, path)
    finally:
        temporary_path.unlink(missing_ok=True)


def apply_changes(changes: dict, originals: dict) -> None:
    for path, original in originals.items():
        if path.is_symlink() or (path.read_bytes() if path.exists() else None) != original:
            raise board.BoardError(path, 'concurrency', 'file changed since preflight')
    applied = []
    created_dirs = []
    modes = {p: stat.S_IMODE(p.stat().st_mode) for p in changes if p.exists()}
    source_mode = next((modes[p] for p, data in changes.items() if data is None), 0o644)
    try:
        for path, data in sorted(changes.items(), key=lambda item: item[1] is None):
            original = originals.get(path)
            if (path.read_bytes() if path.exists() else None) != original:
                raise board.BoardError(path, 'concurrency', 'file changed during move')
            if not path.parent.exists():
                path.parent.mkdir()  # Only a configured lane may be absent.
                created_dirs.append(path.parent)
            applied.append(path)
            if data is None:
                path.unlink()
            else:
                write_file(path, data, create=original is None, mode=modes.get(path, source_mode))
    except (OSError, board.BoardError) as exc:
        failures = []
        for path in reversed(applied):
            try:
                current = path.read_bytes() if path.exists() else None
                original = originals.get(path)
                if current == original:
                    continue  # The failing operation did not publish its write.
                if current != changes[path]:
                    raise OSError('file changed externally; not overwritten by rollback')
                if original is None:
                    path.unlink()
                else:
                    write_file(path, original, create=not path.exists(), mode=modes.get(path, 0o644))
            except OSError as rollback_error:
                failures.append(f'{path}: {rollback_error}')
        for directory in reversed(created_dirs):
            try:
                directory.rmdir()
            except OSError as rollback_error:
                failures.append(f'{directory}: {rollback_error}')
        if failures:
            raise board.BoardError('move', 'rollback', f'{exc}; incomplete rollback: ' + '; '.join(failures)) from exc
        raise


def move(root: Path, config: dict, args) -> dict:
    def perform() -> dict:
        changes, originals = plan_move(root, config, args.id, args.to, args.parked_on, args.blocked_by)
        if not args.dry_run:
            apply_changes(changes, originals)
        return {'schema': 'implement-rfc.move.v1', 'id': args.id, 'lane': args.to,
                'dry_run': args.dry_run, 'changed_paths': sorted(p.relative_to(root).as_posix() for p in changes)}
    if args.dry_run:
        return perform()
    with board_lock(root):
        return perform()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo-root', type=Path, default=project_config.repo_root())
    parser.add_argument('--project-config', type=Path, default=project_config.PROJECT_PATH)
    commands = parser.add_subparsers(dest='command', required=True)
    validate_parser = commands.add_parser('validate', help='Validate card schema and board relationships')
    queue_parser = commands.add_parser('queue', help='Report explicit RFC-linked work; never promote cards')
    queue_parser.add_argument('--rfc', required=True)
    queue_parser.add_argument('--platform', choices=['mac', 'win', 'linux'],
                              default={'darwin': 'mac', 'win32': 'win'}.get(sys.platform, 'linux'))
    queue_parser.add_argument('--include', nargs='+', default=[])
    queue_parser.add_argument('--all', action='store_true')
    move_parser = commands.add_parser('move', help='Move one card and synchronize metadata and live links')
    move_parser.add_argument('id')
    move_parser.add_argument('--to', required=True)
    move_parser.add_argument('--parked-on')
    move_parser.add_argument('--blocked-by')
    move_parser.add_argument('--dry-run', action='store_true')
    for subparser in (validate_parser, queue_parser, move_parser):
        subparser.add_argument('--json', action='store_true')
    args = parser.parse_args(argv)
    try:
        root = args.repo_root.resolve()
        config = project_config.load(args.project_config)
        if args.command == 'validate':
            cards, errors = board.validate_board(root, config)
            result = {'schema': 'implement-rfc.validation.v1', 'valid': not errors,
                      'card_count': len(cards), 'errors': [asdict(e) for e in errors]}
            code = 1 if errors else 0
        elif args.command == 'queue':
            result = queue(root, config, args.rfc, args.platform, args.include, args.all)
            code = 0
        else:
            result = move(root, config, args)
            code = 0
    except (board.BoardError, project_config.ProjectConfigError, OSError, subprocess.CalledProcessError, ValueError) as exc:
        if isinstance(exc, board.ValidationError):
            diagnostics = [asdict(error) for error in exc.diagnostics]
        elif isinstance(exc, board.BoardError):
            diagnostics = [asdict(exc.diagnostic)]
        else:
            diagnostics = [{'path': '', 'field': 'command', 'reason': str(exc)}]
        schema = 'validation' if args.command == 'validate' else args.command
        result = {'schema': f'implement-rfc.{schema}.v1', 'errors': diagnostics}
        if args.command == 'validate':
            result['valid'] = False
        code = 1
    if args.json:
        print(json.dumps(result, indent=2))
    elif 'errors' in result and result['errors']:
        for error in result['errors']:
            print(f"{error['path']}: {error['field']}: {error['reason']}", file=sys.stderr)
    elif args.command == 'validate':
        print(f"BOARD_OK ({result['card_count']} cards)")
    elif args.command == 'queue':
        print(f"{result['rfc']} ({result['platform']}): next={result['next'] or 'none'}")
        if result['ambiguous_in_progress']:
            print('Ambiguous in-progress cards: ' + ', '.join(result['ambiguous_in_progress']))
        for path in result['closeout_paths']:
            print('Closeout: ' + path)
        for row in result['cards']:
            detail = ', '.join(row['unmet_dependencies']) or row['reason'] or ''
            print(f"{row['id']} {row['disposition']} {row['path']} {detail}".rstrip())
    else:
        print(('DRY RUN ' if result['dry_run'] else '') + f"{result['id']} -> {result['lane']}")
        print('\n'.join(result['changed_paths']) or 'No changes')
    return code


if __name__ == '__main__':
    sys.exit(main())
