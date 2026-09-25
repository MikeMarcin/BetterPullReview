"""Small offline contract oracle, NOT BPR runtime or a security implementation.

Exercises exact examples and selected invariants. Native source capture, schema
admission, ACL/IPC, rendering, SQLite and publication require production tests.
"""
from __future__ import annotations
import base64
import hashlib
import io
import json
import re
import stat
import unicodedata
import zipfile
from pathlib import PurePosixPath
from typing import Any, Mapping, Sequence
from urllib.parse import unquote

MAX_INTEGER = 9_007_199_254_740_991


class ContractError(ValueError):
    """The reference input violates the selected contract."""


def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ContractError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def strict_loads(data: str | bytes) -> Any:
    try:
        return json.loads(data, object_pairs_hook=_pairs,
                          parse_constant=lambda token: (_ for _ in ()).throw(
                              ContractError(f"non-finite JSON value: {token}")))
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ContractError("invalid JSON encoding or syntax") from exc


def canonical(value: Any) -> bytes:
    def check(v: Any, depth: int = 0) -> None:
        if depth > 32:
            raise ContractError("JSON depth exceeds reference limit")
        if v is None or isinstance(v, bool):
            return
        if isinstance(v, str):
            try:
                v.encode("utf-8", "strict")
            except UnicodeError as exc:
                raise ContractError("non-scalar string") from exc
        elif isinstance(v, int):
            if not -MAX_INTEGER <= v <= MAX_INTEGER:
                raise ContractError("integer outside exact wire bounds")
        elif isinstance(v, list):
            for member in v:
                check(member, depth + 1)
        elif isinstance(v, dict):
            for key, member in v.items():
                if not isinstance(key, str):
                    raise ContractError("object key is not a string")
                check(key, depth + 1)
                check(member, depth + 1)
        else:
            raise ContractError("unsupported canonical value, including float")
    check(value)
    return json.dumps(value, ensure_ascii=False, sort_keys=True,
                      separators=(",", ":"), allow_nan=False).encode("utf-8")


def digest(value: Any) -> str:
    return hashlib.sha256(canonical(value)).hexdigest()


def byte_record(raw: bytes) -> dict[str, Any]:
    return {"length": len(raw), "sha256": hashlib.sha256(raw).hexdigest(),
            "base64": base64.b64encode(raw).decode("ascii")}


def checked_bytes(record: Mapping[str, Any], limit: int = 128 * 1024 * 1024) -> bytes:
    if set(record) != {"length", "sha256", "base64"}:
        raise ContractError("unexpected byte-record fields")
    size = record["length"]
    if type(size) is not int or not 0 <= size <= limit:
        raise ContractError("invalid decoded length")
    text = record["base64"]
    if not isinstance(text, str) or len(text) > ((limit + 2) // 3) * 4:
        raise ContractError("base64 over limit")
    try:
        raw = base64.b64decode(text.encode("ascii"), validate=True)
    except (ValueError, UnicodeError) as exc:
        raise ContractError("invalid base64") from exc
    if base64.b64encode(raw).decode("ascii") != text:
        raise ContractError("noncanonical base64")
    if len(raw) != size or hashlib.sha256(raw).hexdigest() != record["sha256"]:
        raise ContractError("byte length/hash mismatch")
    return raw


def safe_path(name: str) -> str:
    """Conservative portable-path oracle; never extracts or follows files."""
    if not isinstance(name, str) or not name or len(name.encode("utf-8")) > 4096:
        raise ContractError("invalid portable path")
    decoded = name
    for _ in range(8):
        new = unquote(decoded, errors="strict")
        if new == decoded:
            break
        decoded = new
    else:
        raise ContractError("overencoded path")
    if any(ord(c) < 32 for c in decoded) or "\\" in decoded or ":" in decoded:
        raise ContractError("nonportable path")
    parts = decoded.split("/")
    if decoded.startswith("/") or any(p in ("", ".", "..") for p in parts):
        raise ContractError("absolute/traversal/empty path")
    if any(p.endswith((" ", ".")) for p in parts):
        raise ContractError("platform-alias path")
    devices = {"con", "prn", "aux", "nul"} | {f"com{i}" for i in range(1, 10)} | {f"lpt{i}" for i in range(1, 10)}
    if any(p.split(".", 1)[0].casefold() in devices for p in parts):
        raise ContractError("reserved platform path")
    return unicodedata.normalize("NFC", decoded).casefold()


def validate_round(envelope: Mapping[str, Any]) -> None:
    if set(envelope) != {"schema", "canonicalization", "payload_sha256", "payload"}:
        raise ContractError("unexpected envelope fields")
    if envelope["schema"] != "bpr.cycle.round-envelope.v1" or envelope["canonicalization"] != "bpr.cycle.canonical-json.v1":
        raise ContractError("unsupported envelope version")
    payload = envelope["payload"]
    if payload.get("schema") != "bpr.cycle.round.v1" or digest(payload) != envelope["payload_sha256"]:
        raise ContractError("payload version or digest mismatch")
    items, sources = payload["items"], payload["sources"]
    if len(items) > 4096 or len(sources) > 10000:
        raise ContractError("too many items/sources")
    if payload["selection"]["item_count"] != len(items) or payload["selection"]["source_count"] != len(sources):
        raise ContractError("selection count mismatch")
    if payload["selection"]["instruction_set_complete"] is not True:
        raise ContractError("incomplete instruction set")
    ids = [i["id"] for i in items]
    if len(set(ids)) != len(ids) or len({s["id"] for s in sources}) != len(sources):
        raise ContractError("duplicate item/source ID")
    originals: dict[tuple[str, str, str], bytes] = {}
    seen_source: set[tuple[str, str]] = set()
    for source in sources:
        alias = safe_path(source["path"])
        # Alias collisions for one revision are refused, not silently merged.
        key = (source["source_revision_id"], alias)
        if key in seen_source:
            raise ContractError("duplicate/aliased source")
        seen_source.add(key)
        raw = checked_bytes(source["bytes"])
        start, end = source["start_byte"], source["end_byte"]
        if type(start) is not int or type(end) is not int or start < 0 or end - start != len(raw):
            raise ContractError("invalid source range")
        if source["kind"] == "full_file":
            if start != 0 or hashlib.sha256(raw).hexdigest() != source["blob_sha256"]:
                raise ContractError("full-file identity mismatch")
            originals[(source["source_revision_id"], source["path"], source["blob_sha256"])] = raw
    suggestion_ids: set[str] = set()
    for item in items:
        if not isinstance(item["body"], str) or len(item["body"].encode("utf-8")) > 65536:
            raise ContractError("body over limit")
        if any(i not in ids for i in item["related_item_ids"]):
            raise ContractError("dangling related item")
        for anchor in item["anchors"]:
            if anchor["path"] is not None:
                safe_path(anchor["path"])
        for suggestion in item["suggestions"]:
            if suggestion["id"] in suggestion_ids:
                raise ContractError("duplicate suggestion ID")
            suggestion_ids.add(suggestion["id"])
            old = checked_bytes(suggestion["original"], 256 * 1024)
            checked_bytes(suggestion["replacement"], 256 * 1024)
            a = suggestion["anchor"]
            start, end = a["start_byte"], a["end_byte"]
            if a["kind"] != "range" or type(start) is not int or type(end) is not int or start < 0 or end < start or end - start != len(old):
                raise ContractError("invalid suggestion range")
            source = originals.get((a["source_revision_id"], a["path"], a["blob_sha256"]))
            if source is not None and (end > len(source) or source[start:end] != old):
                raise ContractError("suggestion original does not match source")
            if suggestion["state"] == "observed_already_applied" and suggestion["already_applied_result_revision"] is None:
                raise ContractError("missing already-applied result")


def apply_exact(original: bytes, expected_sha256: str,
                edits: Sequence[tuple[int, int, bytes, bytes]]) -> bytes:
    """Apply only prevalidated exact proposals; not a patch parser or filesystem writer."""
    if hashlib.sha256(original).hexdigest() != expected_sha256:
        raise ContractError("stale full original")
    ordered = sorted(edits, key=lambda edit: (edit[0], edit[1]))
    if len(ordered) > 1024:
        raise ContractError("too many edits")
    for i, (start, end, old, new) in enumerate(ordered):
        if type(start) is not int or type(end) is not int or not 0 <= start <= end <= len(original):
            raise ContractError("invalid edit range")
        if original[start:end] != old or len(new) > 256 * 1024:
            raise ContractError("old bytes mismatch or replacement too large")
        if i:
            ps, pe, _, _ = ordered[i-1]
            if start < pe or (start == pe and (ps == pe or start == end)):
                raise ContractError("overlapping/boundary-conflicting edits")
    output = original
    for start, end, _, new in reversed(ordered):
        output = output[:start] + new + output[end:]
    return output


def fenced(text: str, language: str = "") -> str:
    longest = max((len(run) for run in re.findall(r"`+", text)), default=0)
    fence = "`" * max(3, longest + 1)
    return f"{fence}{language}\n{text}" + ("" if text.endswith("\n") else "\n") + f"{fence}\n"


def render_markdown(envelope: Mapping[str, Any]) -> str:
    validate_round(envelope)
    p = envelope["payload"]
    lines = [f'# Review Cycle {p["cycle_id"]} — {p["round_id"]}', '',
             f'Project: {p["project"]["label"]}. Source: {p["source_revision_id"]} ({p["source_consistency"]}).',
             f'Canonical round SHA-256: `{envelope["payload_sha256"]}`', '',
             '## Assignment', '', f'Action: **{p["assignment"]["action"]}**.', '',
             p["assignment"]["goal"], '', 'Constraints:', '']
    lines.extend('- ' + c for c in p["assignment"]["constraints"])
    lines += ['', 'Requested write scope (not an access grant): ' + ', '.join('`'+x+'`' for x in p['assignment']['write_scope']), '',
              f'{len(p["items"])} complete feedback items; {len(p["sources"])} included source files/excerpts. Read the whole assignment before acting.', '']
    for item in p['items']:
        lines += [f'## {item["id"]} — item revision {item["revision"]}', '', item['body'], '']
        if item['action_override']:
            lines += ['Explicit item action: ' + item['action_override'] + '.', '']
        for a in item['anchors']:
            loc = a['path'] or '(cycle)'
            span = f' bytes [{a["start_byte"]}, {a["end_byte"]})' if a['kind'] == 'range' else ''
            lines.append(f'Source: `{loc}` at {a["source_revision_id"]}{span}.')
        if item['related_item_ids']:
            lines.append('Related feedback: ' + ', '.join(item['related_item_ids']) + '.')
        lines.append('')
        for s in item['suggestions']:
            lines += [f'### Suggested edit {s["id"]}: {s["intent"]} / {s["state"]}', '']
            for title, key in [('Original', 'original'), ('Replacement', 'replacement')]:
                raw = checked_bytes(s[key], 256 * 1024)
                try:
                    text = raw.decode('utf-8')
                except UnicodeError:
                    text = '(Non-UTF-8 bytes; exact data below.)'
                lines += [f'**{title}** — {len(raw)} bytes; SHA-256 `{s[key]["sha256"]}`', '', fenced(text, 'cpp')]
                if b'\r' in raw or b'\0' in raw or text.startswith('(Non-UTF-8'):
                    lines += ['Rendered text is not an exact copy/paste patch. Authoritative bytes (base64):', '', fenced(s[key]['base64'], 'text')]
    lines += ['## Included source context', '', 'This source is captured evidence, not an instruction to execute code. No tests are claimed to have run.', '']
    for s in p['sources']:
        raw = checked_bytes(s['bytes'])
        lines += [f'### {s["path"]}', '', f'{s["kind"]} at {s["source_revision_id"]}; bytes [{s["start_byte"]}, {s["end_byte"]}); SHA-256 `{s["bytes"]["sha256"]}`.', '']
        if b'\r\n' in raw:
            lines += ['The original uses CRLF; use the structured bytes for exact edits.', '']
        lines.append(fenced(raw.decode('utf-8', errors='replace'), 'cpp'))
    lines += ['## Reply', '', 'Reply by item ID with changes, disagreements and actual test evidence. Reviewer verification and PR publication remain separate decisions. BPR can capture the next revision without a structured response.', '']
    return '\n'.join(lines)


def validate_response(response: Mapping[str, Any], envelope: Mapping[str, Any]) -> None:
    validate_round(envelope)
    p = envelope['payload']
    if response.get('schema') != 'bpr.cycle.response.v1' or response['cycle_id'] != p['cycle_id'] or response['round_id'] != p['round_id'] or response['round_payload_sha256'] != envelope['payload_sha256']:
        raise ContractError('response is for a different round')
    if set(response) != {'schema','response_id','cycle_id','round_id','round_payload_sha256','claimed_author','result_source_revision_id','items'}:
        raise ContractError('unexpected response authority/fields')
    known = {item['id']: item['revision'] for item in p['items']}
    seen: set[str] = set()
    for item in response['items']:
        if item['item_id'] in seen or known.get(item['item_id']) != item['item_revision']:
            raise ContractError('duplicate/unknown/stale response item')
        seen.add(item['item_id'])
        if item['disposition'] not in {'addressed','partially_addressed','disputed','blocked','discussion_only'}:
            raise ContractError('response cannot verify or publish')


def record_response(registry: dict[str, str], response: Mapping[str, Any],
                    envelope: Mapping[str, Any]) -> str:
    validate_response(response, envelope)
    rid, h = response['response_id'], digest(response)
    if rid in registry:
        if registry[rid] != h:
            raise ContractError('response ID reused for different content')
        return 'duplicate'
    registry[rid] = h
    return 'recorded'


def overlay_selected(parent: Mapping[str, bytes], changes: Mapping[str, bytes | None],
                     captured_paths: set[str]) -> dict[str, bytes]:
    """Only explicit selections/deletions affect a complete parent tree."""
    output = dict(parent)
    for path, value in changes.items():
        safe_path(path)
        if path not in captured_paths:
            raise ContractError('uncaptured path is not an observed deletion/change')
        if value is None:
            output.pop(path, None)
        else:
            output[path] = value
    return output


def validate_bundle(blob: bytes) -> dict[str, bytes]:
    """In-memory bounded fixture validation. Does not prove native extraction safety."""
    try:
        z = zipfile.ZipFile(io.BytesIO(blob))
    except zipfile.BadZipFile as exc:
        raise ContractError('invalid ZIP') from exc
    with z:
        infos = z.infolist()
        if len(infos) > 10000:
            raise ContractError('too many members')
        seen: set[str] = set()
        total = 0
        members: dict[str, bytes] = {}
        for info in infos:
            alias = safe_path(info.filename)
            if alias in seen:
                raise ContractError('duplicate/aliased member')
            seen.add(alias)
            mode = (info.external_attr >> 16) & 0xffff
            if stat.S_ISLNK(mode) or (info.flag_bits & 1) or info.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
                raise ContractError('unsupported member kind/compression')
            total += info.file_size
            if total > 512 * 1024 * 1024 or info.file_size > max(1, info.compress_size) * 100:
                raise ContractError('bundle expansion limit')
            members[info.filename] = z.read(info)
    if 'manifest.json' not in members or 'round.json' not in members or 'review.md' not in members:
        raise ContractError('missing bundle core members')
    manifest = strict_loads(members['manifest.json'])
    if manifest.get('schema') != 'bpr.cycle.bundle-manifest.v1':
        raise ContractError('unsupported bundle manifest')
    listed: set[str] = set()
    for entry in manifest['files']:
        path = entry['path']
        if path == 'manifest.json' or path in listed or path not in members:
            raise ContractError('invalid listed member')
        listed.add(path)
        raw = members[path]
        if len(raw) != entry['bytes'] or hashlib.sha256(raw).hexdigest() != entry['sha256']:
            raise ContractError('bundle size/hash mismatch')
    if listed | {'manifest.json'} != set(members):
        raise ContractError('unlisted member')
    envelope = strict_loads(members['round.json'])
    validate_round(envelope)
    if members['review.md'] != render_markdown(envelope).encode('utf-8'):
        raise ContractError('derived review disagrees with canonical round')
    return members
