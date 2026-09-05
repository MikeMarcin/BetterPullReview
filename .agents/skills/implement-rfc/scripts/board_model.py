"""Strict, lossless card front matter and shared board validation.

The card schema describes decoded metadata. Its deliberately small vocabulary
is evaluated here without third-party packages. Front matter supports one
key/value per line, text scalars, and inline lists of text; it is not full YAML.
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path

import project_config

SCHEMA_PATH = Path(__file__).resolve().parents[1] / 'schemas/issue-card.schema.json'
KEY_RE = re.compile(r'^[a-z][a-z0-9_]*$')
ID_RE = re.compile(r'^[A-Z]{2,8}-\d{4}$')
RFC_RE = re.compile(r'^RFC-[A-Z]{2,8}(?:-[A-Z]{2,8})?-\d{4}$')
TOKEN_RE = re.compile(r'^[a-z0-9][a-z0-9-]{0,63}$')


@dataclass(frozen=True)
class Diagnostic:
    path: str
    field: str
    reason: str

    def __str__(self) -> str:
        return f'{self.path}: {self.field}: {self.reason}'


class BoardError(ValueError):
    def __init__(self, path: Path | str, field: str, reason: str):
        self.diagnostic = Diagnostic(str(path), field, reason)
        super().__init__(str(self.diagnostic))


class ValidationError(BoardError):
    def __init__(self, root: Path, diagnostics: list[Diagnostic]):
        self.diagnostics = diagnostics
        super().__init__(root, 'board', '\n'.join(map(str, diagnostics)))


def safe_path(root: Path, path: Path) -> Path:
    """Reject symlinks, including ancestors, rather than editing through aliases."""
    try:
        relative = path.relative_to(root)
    except ValueError:
        raise BoardError(path, 'path', 'outside repository') from None
    cursor = root
    for part in relative.parts:
        if part in ('.', '..'):
            raise BoardError(path, 'path', 'noncanonical path')
        cursor /= part
        if cursor.is_symlink():
            raise BoardError(path, 'path', 'symlinks are not supported')
    return path


def scalar(value: str, path: Path, key: str) -> str:
    if value.startswith('"'):
        try:
            parsed = json.loads(value)
            if isinstance(parsed, str) and '\n' not in parsed and '\r' not in parsed:
                return parsed
        except ValueError:
            pass
        raise BoardError(path, key, 'invalid quoted text; use JSON double quoting')
    if value.startswith("'"):
        if len(value) >= 2 and value.endswith("'") and "'" not in value[1:-1].replace("''", ''):
            return value[1:-1].replace("''", "'")
        raise BoardError(path, key, 'invalid single-quoted text')
    if (not value or value[0] in '!&*{}[]|>@`%#' or re.search(r':(?:\s|$)|(?:^|\s)#', value)):
        raise BoardError(path, key, 'unsupported plain text (including unquoted colon); quote the value')
    return value


def decode(value: str, path: Path, key: str) -> str | list[str]:
    if not value.startswith('['):
        return scalar(value, path, key)
    if not value.endswith(']'):
        raise BoardError(path, key, 'unterminated inline list')
    inside = value[1:-1].strip()
    if not inside:
        return []
    # Tokens may be quoted, but embedded commas/complex YAML are deliberately
    # unsupported. IDs cannot contain commas, so no card capability is lost.
    return [scalar(part.strip(), path, key) for part in inside.split(',')]


@dataclass
class Matter:
    raw: dict[str, str]
    values: dict[str, str | list[str]]
    lines: list[str]
    indices: dict[str, int]
    end: int

    def updated(self, updates: dict[str, str | None]) -> bytes:
        lines = list(self.lines)
        newline = '\r\n' if lines[0].endswith('\r\n') else '\n'
        for key, value in updates.items():
            if key in self.indices:
                index = self.indices[key]
                if value is None:
                    lines[index] = ''
                elif self.values[key] != value:
                    lines[index] = f'{key}: {value}{newline}'
        additions = [f'{key}: {value}{newline}' for key, value in updates.items()
                     if key not in self.indices and value is not None]
        lines[self.end:self.end] = additions
        return ''.join(lines).encode('utf-8')


def parse_matter(data: bytes, path: Path) -> Matter:
    try:
        lines = data.decode('utf-8').splitlines(keepends=True)
    except UnicodeDecodeError:
        raise BoardError(path, 'front_matter', 'invalid UTF-8') from None
    if not lines or lines[0].rstrip('\r\n') != '---':
        raise BoardError(path, 'front_matter', 'missing front matter')
    raw: dict[str, str] = {}
    values: dict[str, str | list[str]] = {}
    indices: dict[str, int] = {}
    for index, line in enumerate(lines[1:], 1):
        if line.rstrip('\r\n') == '---':
            return Matter(raw, values, lines, indices, index)
        key, separator, value = line.rstrip('\r\n').partition(':')
        if not separator or not KEY_RE.fullmatch(key):
            raise BoardError(path, 'front_matter', f'unsupported field syntax on line {index + 1}')
        if key in raw:
            raise BoardError(path, key, 'duplicate front-matter key')
        raw[key] = value.strip()
        values[key] = decode(value.strip(), path, key)
        indices[key] = index
    raise BoardError(path, 'front_matter', 'missing closing fence')


def schema_errors(values: dict, path: Path, config: dict) -> list[Diagnostic]:
    """Evaluate the checked-in flat card schema; reject unsupported keywords.

    This is intentionally not a general JSON Schema engine. Adding a schema
    feature requires adding its evaluator and tests in the same change.
    """
    schema = json.loads(SCHEMA_PATH.read_text(encoding='utf-8'))
    properties = schema['properties'].copy()
    link = config['rfc_link_field']
    properties[link] = properties.pop('rfc')
    if 'front_matter' in config['closeout_card']:
        marker = next(iter(config['closeout_card']['front_matter']))
        properties.setdefault(marker, {'type': 'string', 'minLength': 1})
    required = [link if key == 'rfc' else key for key in schema['required']]
    errors = []
    for key in required:
        if key not in values:
            errors.append(Diagnostic(str(path), key, 'required field missing'))
    for key, value in values.items():
        if key not in properties:
            errors.append(Diagnostic(str(path), key, 'unknown card field'))
            continue
        rule = properties[key]
        allowed = {'type', 'minLength', 'pattern', 'items', 'uniqueItems', 'description'}
        if set(rule) - allowed:
            raise RuntimeError(f'unsupported card schema keywords: {set(rule) - allowed}')
        valid_type = isinstance(value, str) if rule['type'] == 'string' else isinstance(value, list)
        reason = None
        if not valid_type:
            reason = f"expected {rule['type']}"
        elif isinstance(value, str):
            if len(value.strip()) < rule.get('minLength', 0):
                reason = 'must be nonempty'
            elif 'pattern' in rule and not re.fullmatch(rule['pattern'], value):
                reason = 'invalid value format'
        else:
            item_rule = rule['items']
            if any(not isinstance(item, str) or not re.fullmatch(item_rule['pattern'], item) for item in value):
                reason = 'invalid dependency id'
            elif rule.get('uniqueItems') and len(value) != len(set(value)):
                reason = 'duplicate dependency'
        if reason:
            errors.append(Diagnostic(str(path), key, reason))
    return errors


@dataclass
class Card:
    path: Path
    lane: str
    matter: Matter
    config: dict

    @property
    def id(self) -> str:
        return self.matter.values['id']

    @property
    def fields(self) -> dict:
        return self.matter.values

    @property
    def deps(self) -> list[str]:
        return self.fields['depends_on']

    @property
    def rfcs(self) -> list[str]:
        value = self.fields[self.config['rfc_link_field']]
        return [] if value == 'none' else [value]

    @property
    def body(self) -> str:
        return ''.join(self.matter.lines)


def policy(config: dict) -> dict:
    lanes = config['lanes']
    return config.get('bookkeeping', {
        'deferring_lanes': [lanes['backlog'], lanes['blocked'], *lanes['platform_ready'].values()],
        'parked_on': [], 'blocked_by_forbidden_prefixes': [],
        'link_roots': [], 'protected_globs': [], 'editable_globs': [],
    })


def read_cards(root: Path, config: dict, overrides: dict[Path, bytes | None] | None = None
               ) -> tuple[dict[str, Card], list[Diagnostic]]:
    overrides = overrides or {}
    board = safe_path(root, root / config['issue_root'])
    errors: list[Diagnostic] = []
    cards: dict[str, Card] = {}
    if not board.is_dir():
        return cards, [Diagnostic(str(board), 'path', 'issue root missing')]
    lanes = project_config.lane_names(config)
    for entry in sorted(board.iterdir()):
        if entry.is_dir() and not entry.name.startswith('_') and entry.name not in lanes:
            errors.append(Diagnostic(str(entry), 'lane', f'non-canonical board folder: {entry.name}'))
    for lane in lanes:
        directory = safe_path(root, board / lane)
        paths = set(directory.glob('*.md')) | {p for p in overrides if p.parent == directory}
        for path in sorted(paths):
            if path in overrides and overrides[path] is None:
                continue
            try:
                safe_path(root, path)
                data = overrides[path] if path in overrides else path.read_bytes()
                matter = parse_matter(data, path)
                found = schema_errors(matter.values, path, config)
                errors.extend(found)
                if found:
                    continue
                card = Card(path, lane, matter, config)
                if not path.name.startswith(card.id + '-'):
                    errors.append(Diagnostic(str(path), 'id', 'filename/id mismatch'))
                if card.id in cards:
                    errors.append(Diagnostic(str(path), 'id', f'duplicate card id {card.id}'))
                else:
                    cards[card.id] = card
            except BoardError as exc:
                errors.append(exc.diagnostic)
    return cards, errors


def validate_cards(cards: dict[str, Card], config: dict) -> list[Diagnostic]:
    errors: list[Diagnostic] = []
    lanes = config['lanes']
    rules = policy(config)
    done = lanes['done']
    for card in cards.values():
        def error(field: str, reason: str) -> None:
            errors.append(Diagnostic(str(card.path), field, reason))
        expected = config['lane_statuses'][card.lane]
        if card.fields['status'] != expected:
            error('status', f"status '{card.fields['status']}' != '{expected}'")
        for key, destination in (('parked_on', lanes['parked']), ('blocked_by', lanes['blocked'])):
            value = card.fields.get(key)
            if card.lane == destination:
                if not value:
                    error(key, f'{key} required in {destination}')
                elif key == 'parked_on' and rules['parked_on'] and value not in rules['parked_on']:
                    error(key, 'unknown parked_on token')
                elif key == 'blocked_by' and any(value.startswith(p) for p in rules['blocked_by_forbidden_prefixes']):
                    error(key, 'a platform is a lane, not a blocker')
            elif value is not None:
                error(key, f'{key} outside its lane')
        for dep in card.deps:
            if dep == card.id:
                error('depends_on', 'self dependency')
            elif dep not in cards:
                error('depends_on', f'depends_on {dep} not found')
            elif card.lane not in rules['deferring_lanes'] and cards[dep].lane != done:
                error('depends_on', f'{dep} is not Done ({cards[dep].lane})')
    # Iterative DFS handles large boards without Python recursion limits.
    visited: set[str] = set()
    for start in cards:
        if start in visited:
            continue
        stack = [(start, iter(cards[start].deps))]
        active = {start}
        while stack:
            node, children = stack[-1]
            child = next(children, None)
            if child is None:
                visited.add(node)
                active.remove(node)
                stack.pop()
            elif child in active:
                errors.append(Diagnostic(str(cards[node].path), 'depends_on', f'issue dependency cycle through {child}'))
            elif child in cards and child not in visited:
                active.add(child)
                stack.append((child, iter(cards[child].deps)))
    return errors


def validate_board(root: Path, config: dict, overrides: dict[Path, bytes | None] | None = None
                   ) -> tuple[dict[str, Card], list[Diagnostic]]:
    cards, errors = read_cards(root, config, overrides)
    errors.extend(validate_cards(cards, config))
    rfcs: dict[str, Path] = {}
    rfc_root = safe_path(root, root / config['rfc_root'])
    for path in sorted(rfc_root.glob('RFC-*.md')):
        if path.name == 'README.md':
            continue
        try:
            safe_path(root, path)
            metadata = parse_matter(path.read_bytes(), path).values
            rfc_id = metadata.get('id', '')
            if not isinstance(rfc_id, str) or not RFC_RE.fullmatch(rfc_id) or not path.name.startswith(rfc_id + '-'):
                raise BoardError(path, 'id', 'invalid RFC identity or filename')
            if rfc_id in rfcs:
                raise BoardError(path, 'id', f'duplicate RFC id {rfc_id}')
            rfcs[rfc_id] = path
        except BoardError as exc:
            errors.append(exc.diagnostic)
    for card in cards.values():
        for rfc in card.rfcs:
            if rfc not in rfcs:
                errors.append(Diagnostic(str(card.path), config['rfc_link_field'], f'linked {rfc} has no RFC file'))
    counter_path = safe_path(root, root / config['next_id']['path'])
    if not counter_path.is_file():
        errors.append(Diagnostic(str(counter_path), 'next_id', 'next-id file missing'))
    else:
        value = counter_path.read_text(encoding='utf-8').strip()
        if not ID_RE.fullmatch(value):
            errors.append(Diagnostic(str(counter_path), 'next_id', f"malformed value '{value}'"))
        else:
            prefix, number = value.rsplit('-', 1)
            highest = max((int(c.id.rsplit('-', 1)[1]) for c in cards.values()
                           if c.id.startswith(prefix + '-')), default=0)
            if int(number) <= highest:
                errors.append(Diagnostic(str(counter_path), 'next_id', f'next {value} <= highest used {prefix}-{highest:04d}'))
    return cards, errors
