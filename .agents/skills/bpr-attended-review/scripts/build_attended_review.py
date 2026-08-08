#!/usr/bin/env python3
"""Build and validate deterministic, offline attended review packages."""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import re
import shutil
import sys
from pathlib import Path
from typing import Any


INPUT_SCHEMA = "bpr.attended_review_batch_input"
OUTPUT_SCHEMA = "bpr.attended_review_batch"
VERDICT_SCHEMA = "bpr.attended_review_verdict"
SCHEMA_VERSION = 1
IMAGE_EXTENSIONS = {".gif", ".jpeg", ".jpg", ".png", ".webp"}
VIDEO_EXTENSIONS = {".mp4", ".webm"}
AUDIO_EXTENSIONS = {".flac", ".m4a", ".mp3", ".ogg", ".opus", ".wav"}
TOKEN_RE = re.compile(r"^[a-z0-9][a-z0-9_.-]{0,63}$")
CLAIM_RE = re.compile(r"^[a-z0-9][a-z0-9_]{0,63}$")
GROUP_ID_RE = re.compile(r"^[a-z0-9][a-z0-9_-]{0,31}$")
LAYOUTS = {"grid", "compare"}
DETAIL_KINDS = {"list", "definitions", "code"}
AUDIT_STATUSES = {"pass", "fail"}


class ReviewError(ValueError):
    """A user-correctable review package error."""


def canonical_bytes(value: Any) -> bytes:
    return json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")


def sha256_bytes(data: bytes) -> str:
    return f"sha256:{hashlib.sha256(data).hexdigest()}"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return f"sha256:{digest.hexdigest()}"


def require_string(value: Any, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ReviewError(f"{field} must be a non-empty string")
    return value.strip()


def optional_string(value: Any, field: str) -> str:
    if value is None:
        return ""
    if not isinstance(value, str):
        raise ReviewError(f"{field} must be a string")
    return value.strip()


def string_list(value: Any, field: str) -> list[str]:
    if value is None:
        return []
    if not isinstance(value, list):
        raise ReviewError(f"{field} must be an array of strings")
    return [require_string(item, f"{field}[{index}]") for index, item in enumerate(value)]


def require_token(value: Any, field: str, pattern: re.Pattern[str]) -> str:
    token = require_string(value, field)
    if not pattern.match(token):
        raise ReviewError(f"{field} must match {pattern.pattern}")
    return token


def slug(value: str, fallback: str) -> str:
    result = re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")
    return (result or fallback)[:64]


def resolve_source(base: Path, value: Any, field: str) -> Path:
    source_text = require_string(value, field)
    source = Path(source_text).expanduser()
    if not source.is_absolute():
        source = base / source
    source = source.resolve()
    if not source.is_file():
        raise ReviewError(f"{field} does not name a file: {source_text}")
    return source


def prepare_files(
    items: Any,
    *,
    field: str,
    source_base: Path,
    destination_root: Path,
    relative_root: Path,
    previewable: bool,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    if not isinstance(items, list):
        raise ReviewError(f"{field} must be an array")
    previewable_extensions = IMAGE_EXTENSIONS | VIDEO_EXTENSIONS | AUDIO_EXTENSIONS
    output: list[dict[str, Any]] = []
    identity: list[dict[str, Any]] = []
    for index, raw in enumerate(items):
        item_field = f"{field}[{index}]"
        if not isinstance(raw, dict):
            raise ReviewError(f"{item_field} must be an object")
        label = require_string(raw.get("label"), f"{item_field}.label")
        source = resolve_source(source_base, raw.get("path"), f"{item_field}.path")
        extension = source.suffix.lower()
        if previewable and extension not in previewable_extensions:
            allowed = ", ".join(sorted(previewable_extensions))
            raise ReviewError(f"{item_field}.path must use a previewable extension: {allowed}")
        if extension in IMAGE_EXTENSIONS:
            media_kind = "image"
        elif extension in VIDEO_EXTENSIONS:
            media_kind = "video"
        elif extension in AUDIO_EXTENSIONS:
            media_kind = "audio"
        else:
            media_kind = "file"
        filename = f"{index + 1:02d}-{slug(label, 'item')}{extension}"
        destination = destination_root / filename
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        digest = sha256_file(destination)
        relative_path = (relative_root / filename).as_posix()
        record = {
            "label": label,
            "media_kind": media_kind,
            "path": relative_path,
            "sha256": digest,
        }
        output.append(record)
        identity.append(
            {
                "label": label,
                "media_kind": media_kind,
                "sha256": digest,
            }
        )
    return output, identity


def prepare_media_group(
    raw: Any,
    *,
    field: str,
    source_base: Path,
    subject_root: Path,
    relative_subject_root: Path,
) -> tuple[dict[str, Any], dict[str, Any]] | None:
    if not isinstance(raw, dict):
        raise ReviewError(f"{field} must be an object")
    group_id = require_token(raw.get("group_id"), f"{field}.group_id", GROUP_ID_RE)
    title = require_string(raw.get("title"), f"{field}.title")
    required = raw.get("required", True)
    if not isinstance(required, bool):
        raise ReviewError(f"{field}.required must be a boolean")
    layout = raw.get("layout", "grid")
    if layout not in LAYOUTS:
        raise ReviewError(f"{field}.layout must be one of: {', '.join(sorted(LAYOUTS))}")
    note = optional_string(raw.get("note"), f"{field}.note")
    items, item_identity = prepare_files(
        raw.get("items"),
        field=f"{field}.items",
        source_base=source_base,
        destination_root=subject_root / group_id,
        relative_root=relative_subject_root / group_id,
        previewable=True,
    )
    if not items:
        if required:
            raise ReviewError(f"{field}.items must contain at least one item")
        return None
    if layout == "compare" and len(items) < 2:
        raise ReviewError(f"{field}.layout compare requires at least two items")
    record = {
        "group_id": group_id,
        "title": title,
        "required": required,
        "layout": layout,
        "note": note,
        "items": items,
    }
    identity = {
        "group_id": group_id,
        "title": title,
        "required": required,
        "layout": layout,
        "note": note,
        "items": item_identity,
    }
    return record, identity


def prepare_detail_sections(raw: Any, field: str) -> list[dict[str, Any]]:
    if raw is None:
        return []
    if not isinstance(raw, list):
        raise ReviewError(f"{field} must be an array")
    sections: list[dict[str, Any]] = []
    for index, section in enumerate(raw):
        section_field = f"{field}[{index}]"
        if not isinstance(section, dict):
            raise ReviewError(f"{section_field} must be an object")
        heading = require_string(section.get("heading"), f"{section_field}.heading")
        kind = section.get("kind")
        if kind not in DETAIL_KINDS:
            raise ReviewError(f"{section_field}.kind must be one of: {', '.join(sorted(DETAIL_KINDS))}")
        raw_items = section.get("items")
        if not isinstance(raw_items, list) or not raw_items:
            raise ReviewError(f"{section_field}.items must be a non-empty array")
        items: list[Any]
        if kind == "definitions":
            items = []
            for item_index, item in enumerate(raw_items):
                item_field = f"{section_field}.items[{item_index}]"
                if not isinstance(item, dict):
                    raise ReviewError(f"{item_field} must be an object")
                term = require_string(item.get("term"), f"{item_field}.term")
                definition = optional_string(item.get("definition"), f"{item_field}.definition")
                if set(item) - {"term", "definition"}:
                    raise ReviewError(f"{item_field} may contain only term and definition")
                items.append({"term": term, "definition": definition})
        elif kind == "code":
            items = []
            for item_index, item in enumerate(raw_items):
                if not isinstance(item, str):
                    raise ReviewError(f"{section_field}.items[{item_index}] must be a string")
                items.append(item.rstrip())
        else:
            items = string_list(raw_items, f"{section_field}.items")
        sections.append({"heading": heading, "kind": kind, "items": items})
    return sections


def prepare_audit_verdict(raw: Any, field: str) -> dict[str, Any] | None:
    if raw is None:
        return None
    if not isinstance(raw, dict):
        raise ReviewError(f"{field} must be an object")
    status = raw.get("status")
    if status not in AUDIT_STATUSES:
        raise ReviewError(f"{field}.status must be one of: {', '.join(sorted(AUDIT_STATUSES))}")
    note = optional_string(raw.get("note"), f"{field}.note")
    if set(raw) - {"status", "note"}:
        raise ReviewError(f"{field} may contain only status and note")
    return {"status": status, "note": note}


def prepare_subject(
    raw: Any,
    *,
    index: int,
    source_base: Path,
    output_root: Path,
) -> dict[str, Any]:
    field = f"subjects[{index}]"
    if not isinstance(raw, dict):
        raise ReviewError(f"{field} must be an object")

    subject_id = require_string(raw.get("subject_id"), f"{field}.subject_id")
    subject_key = require_string(raw.get("subject_key"), f"{field}.subject_key")
    display_name = require_string(raw.get("display_name"), f"{field}.display_name")
    description = optional_string(raw.get("description"), f"{field}.description")
    acceptance_criteria = string_list(raw.get("acceptance_criteria"), f"{field}.acceptance_criteria")
    known_concerns = string_list(raw.get("known_concerns"), f"{field}.known_concerns")
    ab_group = optional_string(raw.get("ab_group"), f"{field}.ab_group")
    # Alternatives to one question, of which the reviewer picks exactly one.
    # `ab_group` marks subjects as related for presentation; this says they are
    # mutually exclusive answers, which is a claim the verdict can be checked
    # against. Without it an n-way choice has to be recorded as one accept and
    # n-1 rejects, and a candidate that merely lost is indistinguishable from
    # one that failed.
    choice_group = optional_string(raw.get("choice_group"), f"{field}.choice_group")
    if choice_group and ab_group:
        raise ReviewError(
            f"{field} may declare choice_group or ab_group, not both: a set of "
            "alternatives is either a choice or a presentation grouping"
        )
    selection_group = optional_string(raw.get("selection_group"), f"{field}.selection_group")
    if selection_group and (ab_group or choice_group):
        raise ReviewError(
            f"{field} may declare only one of selection_group, choice_group, or ab_group; "
            "selection_group is the general form and choice_group its choose-exactly-one shorthand"
        )
    audit_verdict = prepare_audit_verdict(raw.get("audit_verdict"), f"{field}.audit_verdict")
    detail_sections = prepare_detail_sections(raw.get("detail_sections"), f"{field}.detail_sections")

    subject_dir_name = f"{index + 1:03d}-{slug(subject_key, 'subject')}"
    subject_root = output_root / "subjects" / subject_dir_name
    relative_subject_root = Path("subjects") / subject_dir_name

    raw_groups = raw.get("media_groups")
    if not isinstance(raw_groups, list) or not raw_groups:
        raise ReviewError(f"{field}.media_groups must be a non-empty array")
    groups: list[dict[str, Any]] = []
    group_identities: list[dict[str, Any]] = []
    seen_group_ids: set[str] = set()
    for group_index, raw_group in enumerate(raw_groups):
        group_field = f"{field}.media_groups[{group_index}]"
        prepared = prepare_media_group(
            raw_group,
            field=group_field,
            source_base=source_base,
            subject_root=subject_root,
            relative_subject_root=relative_subject_root,
        )
        group_id_value = raw_group.get("group_id") if isinstance(raw_group, dict) else None
        if isinstance(group_id_value, str):
            if group_id_value in seen_group_ids:
                raise ReviewError(f"{group_field}.group_id must be unique within the subject")
            seen_group_ids.add(group_id_value)
        if prepared is None:
            continue
        record, identity = prepared
        groups.append(record)
        group_identities.append(identity)
    if not groups:
        raise ReviewError(f"{field} must retain at least one non-empty media group")

    identity_files_raw = raw.get("identity_files", [])
    identity_files, file_identity = prepare_files(
        identity_files_raw,
        field=f"{field}.identity_files",
        source_base=source_base,
        destination_root=subject_root / "identity",
        relative_root=relative_subject_root / "identity",
        previewable=False,
    )

    digest_input = {
        "subject_id": subject_id,
        "subject_key": subject_key,
        "display_name": display_name,
        "description": description,
        "acceptance_criteria": acceptance_criteria,
        "known_concerns": known_concerns,
        "ab_group": ab_group,
        "choice_group": choice_group,
        "audit_verdict": audit_verdict,
        "media_groups": group_identities,
        "detail_sections": detail_sections,
        "identity_files": file_identity,
    }
    record = {
        "subject_id": subject_id,
        "subject_key": subject_key,
        "display_name": display_name,
        "description": description,
        "acceptance_criteria": acceptance_criteria,
        "known_concerns": known_concerns,
        "ab_group": ab_group,
        "choice_group": choice_group,
        "audit_verdict": audit_verdict,
        "media_groups": groups,
        "detail_sections": detail_sections,
        "identity_files": identity_files,
    }
    if selection_group:
        digest_input["selection_group"] = selection_group
        record["selection_group"] = selection_group
    record["subject_digest"] = sha256_bytes(canonical_bytes(digest_input))
    return record


def prepare_selection_groups(raw: Any) -> dict[str, dict[str, Any]]:
    if raw is None:
        return {}
    if not isinstance(raw, list) or not raw:
        raise ReviewError("selection_groups must be a non-empty array when present")
    groups: dict[str, dict[str, Any]] = {}
    for index, entry in enumerate(raw):
        field = f"selection_groups[{index}]"
        if not isinstance(entry, dict):
            raise ReviewError(f"{field} must be an object")
        group_id = require_token(entry.get("group_id"), f"{field}.group_id", GROUP_ID_RE)
        if group_id in groups:
            raise ReviewError(f"{field}.group_id must be unique")
        title = require_string(entry.get("title"), f"{field}.title")
        note = optional_string(entry.get("note"), f"{field}.note")
        min_accept = entry.get("min_accept", 1)
        max_accept = entry.get("max_accept", 1)
        if not isinstance(min_accept, int) or isinstance(min_accept, bool) or min_accept < 0:
            raise ReviewError(f"{field}.min_accept must be a non-negative integer")
        if not isinstance(max_accept, int) or isinstance(max_accept, bool) or max_accept < 1:
            raise ReviewError(f"{field}.max_accept must be a positive integer")
        if min_accept > max_accept:
            raise ReviewError(f"{field}.min_accept must not exceed max_accept")
        if set(entry) - {"group_id", "title", "note", "min_accept", "max_accept"}:
            raise ReviewError(f"{field} contains unsupported keys")
        groups[group_id] = {
            "group_id": group_id,
            "title": title,
            "note": note,
            "min_accept": min_accept,
            "max_accept": max_accept,
        }
    return groups


def prepare_claims(raw: Any) -> dict[str, bool]:
    if not isinstance(raw, dict) or not raw:
        raise ReviewError("downstream_claims must be a non-empty object")
    claims: dict[str, bool] = {}
    for key, value in raw.items():
        if not isinstance(key, str) or not CLAIM_RE.match(key):
            raise ReviewError(f"downstream_claims key {key!r} must match {CLAIM_RE.pattern}")
        if value is not False:
            raise ReviewError(f"downstream_claims.{key} must be false; the page cannot grant claims")
        claims[key] = False
    return claims


def build_manifest(source: dict[str, Any], input_path: Path, output_root: Path) -> dict[str, Any]:
    if source.get("schema_name") != INPUT_SCHEMA:
        raise ReviewError(f"schema_name must be {INPUT_SCHEMA!r}")
    if source.get("schema_version") != SCHEMA_VERSION:
        raise ReviewError(f"schema_version must be {SCHEMA_VERSION}")
    project_id = require_string(source.get("project_id"), "project_id")
    title = require_string(source.get("title"), "title")
    review_gate = require_token(source.get("review_gate"), "review_gate", TOKEN_RE)
    review_scope = require_token(source.get("review_scope"), "review_scope", TOKEN_RE)
    downstream_claims = prepare_claims(source.get("downstream_claims"))
    selection_groups = prepare_selection_groups(source.get("selection_groups"))
    reference_disclaimer = optional_string(source.get("reference_disclaimer"), "reference_disclaimer")
    raw_subjects = source.get("subjects")
    if not isinstance(raw_subjects, list) or not raw_subjects:
        raise ReviewError("subjects must be a non-empty array")

    subjects = [
        prepare_subject(
            raw,
            index=index,
            source_base=input_path.parent,
            output_root=output_root,
        )
        for index, raw in enumerate(raw_subjects)
    ]
    subject_ids = [subject["subject_id"] for subject in subjects]
    if len(subject_ids) != len(set(subject_ids)):
        raise ReviewError("subject_id values must be unique within a batch")

    group_indexes: dict[str, list[int]] = {}
    for index, subject in enumerate(subjects):
        group = subject.get("selection_group", "")
        if group:
            if group not in selection_groups:
                raise ReviewError(
                    f"subjects[{index}].selection_group references undeclared group {group!r}"
                )
            group_indexes.setdefault(group, []).append(index)
    for group_id, group in selection_groups.items():
        indexes = group_indexes.get(group_id, [])
        if len(indexes) < 2:
            raise ReviewError(f"selection group {group_id!r} needs at least two member subjects")
        if indexes != list(range(indexes[0], indexes[0] + len(indexes))):
            raise ReviewError(
                f"selection group {group_id!r} members must be contiguous in subject order"
            )
        if group["max_accept"] > len(indexes):
            raise ReviewError(f"selection group {group_id!r} max_accept exceeds its member count")

    batch_identity = {
        "project_id": project_id,
        "title": title,
        "review_gate": review_gate,
        "review_scope": review_scope,
        "downstream_claims": downstream_claims,
        "reference_disclaimer": reference_disclaimer,
        "subjects": [
            {
                "subject_id": subject["subject_id"],
                "subject_digest": subject["subject_digest"],
            }
            for subject in subjects
        ],
    }
    if selection_groups:
        batch_identity["selection_groups"] = list(selection_groups.values())
    batch_hash = hashlib.sha256(canonical_bytes(batch_identity)).hexdigest()[:24]
    batch_id = f"bpr:/attended-review/{slug(project_id, 'project')}/{batch_hash}"
    manifest = {
        "schema_name": OUTPUT_SCHEMA,
        "schema_version": SCHEMA_VERSION,
        "batch_id": batch_id,
        "project_id": project_id,
        "title": title,
        "review_gate": review_gate,
        "review_scope": review_scope,
        "downstream_claims": downstream_claims,
        "reference_disclaimer": reference_disclaimer,
        "subjects": subjects,
    }
    if selection_groups:
        manifest["selection_groups"] = list(selection_groups.values())
    return manifest


def media_html(items: list[dict[str, Any]]) -> str:
    figures: list[str] = []
    for item in items:
        source = html.escape(item["path"], quote=True)
        label = html.escape(item["label"])
        if item["media_kind"] == "video":
            media = f'<video controls preload="metadata" src="{source}"></video>'
        elif item["media_kind"] == "audio":
            media = f'<audio controls preload="metadata" src="{source}"></audio>'
        else:
            # A reviewer cannot judge one-device-pixel detail in a grid-sized
            # thumbnail. Every image opens at native
            # resolution without leaving the page or disturbing decision state.
            media = (
                f'<button type="button" class="media-zoom" '
                f'data-full-source="{source}" data-full-label="{label}" '
                f'aria-label="View {label} at full resolution">'
                f'<img loading="lazy" src="{source}" alt="{label}">'
                f'<span class="media-zoom-hint" aria-hidden="true">Full size</span>'
                f"</button>"
            )
        figures.append(f"<figure>{media}<figcaption>{label}</figcaption></figure>")
    return "\n".join(figures)


def list_html(items: list[str], empty: str) -> str:
    if not items:
        return f'<p class="muted">{html.escape(empty)}</p>'
    return "<ul>" + "".join(f"<li>{html.escape(item)}</li>" for item in items) + "</ul>"


def detail_sections_html(sections: list[dict[str, Any]]) -> str:
    if not sections:
        return ""
    rendered: list[str] = []
    for section in sections:
        heading = html.escape(section["heading"])
        if section["kind"] == "definitions":
            body = "<dl>" + "".join(
                f"<dt>{html.escape(item['term'])}</dt><dd>{html.escape(item['definition']) or '&nbsp;'}</dd>"
                for item in section["items"]
            ) + "</dl>"
        elif section["kind"] == "code":
            code = html.escape("\n".join(section["items"]))
            body = f"<pre>{code}</pre>"
        else:
            body = list_html(section["items"], "No items recorded.")
        rendered.append(f"<section><h3>{heading}</h3>{body}</section>")
    return f'<div class="detail-sections">{"".join(rendered)}</div>'


def media_groups_html(groups: list[dict[str, Any]]) -> str:
    sections: list[str] = []
    for group in groups:
        classes = "media-grid compare" if group["layout"] == "compare" else "media-grid"
        note = f'<p class="muted">{html.escape(group["note"])}</p>' if group["note"] else ""
        sections.append(
            f'<section><h3>{html.escape(group["title"])}</h3>{note}'
            f'<div class="{classes}">{media_html(group["items"])}</div></section>'
        )
    return "\n".join(sections)


def cardinality_line(group: dict[str, Any]) -> str:
    low, high = group["min_accept"], group["max_accept"]
    if low == high:
        core = "Choose exactly one alternative." if high == 1 else f"Choose exactly {high} alternatives."
    elif low == 0:
        core = f"Choose up to {high} alternatives."
    else:
        core = f"Choose {low} to {high} alternatives."
    return (
        f"{core} Unchosen alternatives are recorded as not chosen, never as rejections; "
        "Reject marks an alternative genuinely defective."
    )


def selection_group_open_html(group: dict[str, Any]) -> str:
    group_id = html.escape(group["group_id"], quote=True)
    note = f'<p class="muted">{html.escape(group["note"])}</p>' if group["note"] else ""
    return (
        f'<section class="selection-group" data-selection-group-header="{group_id}">'
        f'<h2>{html.escape(group["title"])}</h2>'
        f'<p class="muted">{html.escape(cardinality_line(group))}</p>{note}'
        f'<p class="selection-group-status" data-selection-group-status="{group_id}">'
        'No alternatives chosen yet.</p><div class="selection-group-members">'
    )


def option_html(subject: dict[str, Any]) -> str:
    subject_id = html.escape(subject["subject_id"], quote=True)
    digest = html.escape(subject["subject_digest"], quote=True)
    selection_group = html.escape(subject.get("selection_group", ""), quote=True)
    audit = subject["audit_verdict"]
    if audit:
        audit_note = f" — {html.escape(audit['note'])}" if audit["note"] else ""
        audit_banner = (
            f'<p class="audit-banner" data-audit="{audit["status"]}">'
            f'Mechanical audit: {audit["status"].upper()}{audit_note}</p>'
        )
    else:
        audit_banner = ""
    accept_disabled = " disabled" if audit and audit["status"] == "fail" else ""
    description = (
        f'<p class="muted">{html.escape(subject["description"])}</p>'
        if subject["description"]
        else ""
    )
    extras: list[str] = []
    if subject["acceptance_criteria"]:
        extras.append(
            f'<section><h4>Acceptance criteria</h4>{list_html(subject["acceptance_criteria"], "")}</section>'
        )
    if subject["known_concerns"]:
        extras.append(
            f'<section><h4>Known concerns</h4>{list_html(subject["known_concerns"], "")}</section>'
        )
    detail = detail_sections_html(subject["detail_sections"])
    if detail:
        extras.append(detail)
    if subject["identity_files"]:
        links = "<ul>" + "".join(
            f'<li><a href="{html.escape(item["path"], quote=True)}">{html.escape(item["label"])}</a> '
            f'<span class="identity">{html.escape(item["sha256"])}</span></li>'
            for item in subject["identity_files"]
        ) + "</ul>"
        extras.append(f'<section><h4>Identity-bound files</h4>{links}</section>')
    extras.append(f'<p class="identity">{subject_id}<br>{digest}</p>')
    extras_html = (
        '<details class="option-extra"><summary>Details and identity</summary>'
        + "".join(extras)
        + "</details>"
    )
    return f"""<article class="subject option" data-subject-id="{subject_id}" data-subject-digest="{digest}" data-selection-group="{selection_group}" data-review-state="pending">
  <h3>{html.escape(subject["display_name"])}</h3>
  {description}
  {audit_banner}
  <div class="media-groups">
    {media_groups_html(subject["media_groups"])}
  </div>
  {extras_html}
  <div class="decision-row option-actions" role="group" aria-label="Decision for {html.escape(subject["display_name"], quote=True)}">
    <button type="button" class="choose" data-decision="accept" aria-pressed="false"{accept_disabled}>Choose</button>
    <button type="button" class="minor" data-decision="reject" aria-pressed="false">Defective</button>
  </div>
  <details class="notes-fold"><summary>Note</summary><label class="notes">Review notes<textarea data-review-notes placeholder="Optional note for this alternative"></textarea></label></details>
</article>"""


def subjects_html(manifest: dict[str, Any]) -> str:
    groups = {group["group_id"]: group for group in manifest.get("selection_groups", [])}
    parts: list[str] = []
    open_group = ""
    for subject in manifest["subjects"]:
        group_id = subject.get("selection_group", "")
        if open_group and group_id != open_group:
            parts.append("</div></section>")
            open_group = ""
        if group_id and group_id != open_group:
            parts.append(selection_group_open_html(groups[group_id]))
            open_group = group_id
        parts.append(option_html(subject) if group_id else subject_html(subject))
    if open_group:
        parts.append("</div></section>")
    return "\n".join(parts)


def subject_html(subject: dict[str, Any]) -> str:
    subject_id = html.escape(subject["subject_id"], quote=True)
    digest = html.escape(subject["subject_digest"], quote=True)
    ab_group_attr = ""
    if subject["choice_group"]:
        ab_group_attr = f' data-choice-group="{html.escape(subject["choice_group"], quote=True)}"'
    elif subject["ab_group"]:
        ab_group_attr = f' data-ab-group="{html.escape(subject["ab_group"], quote=True)}"'
    identity_files = subject["identity_files"]
    if identity_files:
        links = "<ul>" + "".join(
            f'<li><a href="{html.escape(item["path"], quote=True)}">{html.escape(item["label"])}</a> '
            f'<span class="identity">{html.escape(item["sha256"])}</span></li>'
            for item in identity_files
        ) + "</ul>"
    else:
        links = '<p class="muted">No additional identity files.</p>'
    description = html.escape(subject["description"]) or "No description supplied."
    audit = subject["audit_verdict"]
    if audit:
        audit_note = f" — {html.escape(audit['note'])}" if audit["note"] else ""
        audit_banner = (
            f'<p class="audit-banner" data-audit="{audit["status"]}">'
            f'Mechanical audit: {audit["status"].upper()}{audit_note}</p>'
        )
    else:
        audit_banner = ""
    audit_failed = bool(audit and audit["status"] == "fail")
    accept_disabled = " disabled" if audit_failed else ""
    audit_explanation = ""
    if audit_failed:
        audit_explanation = (
            '<p class="muted audit-note">Accept is unavailable: this subject failed its '
            "mechanical audit. Reject it, or fix the failure and rebuild the batch.</p>"
        )
    return f"""<article class="subject" data-subject-id="{subject_id}" data-subject-digest="{digest}"{ab_group_attr} data-review-state="pending">
  <div class="subject-head">
    <div>
      <h2>{html.escape(subject["display_name"])}</h2>
      <p>{description}</p>
    </div>
    <div class="identity">{subject_id}<br>{digest}</div>
  </div>
  {audit_banner}
  <div class="media-groups">
    {media_groups_html(subject["media_groups"])}
  </div>
  <div class="criteria">
    <section><h3>Acceptance criteria</h3>{list_html(subject["acceptance_criteria"], "No additional criteria supplied.")}</section>
    <section><h3>Known concerns</h3>{list_html(subject["known_concerns"], "No known concerns recorded.")}</section>
  </div>
  {detail_sections_html(subject["detail_sections"])}
  <section class="identity-files"><h3>Identity-bound files</h3>{links}</section>
  <div class="decision-row" role="group" aria-label="Decision for {html.escape(subject["display_name"], quote=True)}">
    <button type="button" data-decision="accept" aria-pressed="false"{accept_disabled}>{"Choose exact alternative" if subject["choice_group"] else "Accept exact subject"}</button>
    <button type="button" data-decision="reject" aria-pressed="false">{"Reject exact alternative" if subject["choice_group"] else "Reject exact subject"}</button>
    <button type="button" data-decision="clear" aria-pressed="false">Clear decision</button>
  </div>
  {audit_explanation}
  <label class="notes">Review notes<textarea data-review-notes placeholder="Optional subject-specific notes"></textarea></label>
</article>"""


def render_html(manifest: dict[str, Any], css: str, runtime: str) -> str:
    config_json = json.dumps(manifest, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    config_json = config_json.replace("<", "\\u003c").replace(">", "\\u003e").replace("&", "\\u0026")
    cards = subjects_html(manifest)
    claim_names = html.escape(", ".join(sorted(manifest["downstream_claims"])))
    disclaimer = ""
    if manifest["reference_disclaimer"]:
        disclaimer = (
            f'<p class="reference-disclaimer"><strong>Reference:</strong> '
            f'{html.escape(manifest["reference_disclaimer"])}</p>'
        )
    page = f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{html.escape(manifest["title"])}</title>
  <style>\n{css}\n</style>
</head>
<body>
  <header>
    <h1>{html.escape(manifest["title"])}</h1>
    <p class="lede">Project: {html.escape(manifest["project_id"])} · Gate: {html.escape(manifest["review_gate"])} · Scope: {html.escape(manifest["review_scope"])} · Batch: {html.escape(manifest["batch_id"])}</p>
    <p class="scope-warning"><strong>Scope:</strong> This page records attended human decisions for this exact batch and gate only. It stores choices in this browser and cannot write repository evidence. Acceptance asserts nothing beyond this gate; these claims remain false: {claim_names}. Export the complete verdict and return it to the requesting agent session for validation.</p>
    {disclaimer}
  </header>
  <main>
    <section class="review-controls" aria-label="Review progress and export">
      <div class="review-controls-row">
        <div class="review-summary">
          <span id="review-progress">0/{len(manifest["subjects"])} reviewed</span>
          <span id="review-totals">0 accepted · 0 rejected · {len(manifest["subjects"])} pending</span>
        </div>
        <div class="review-actions">
          <label><input id="pending-only" type="checkbox"> Show pending only</label>
          <button id="export-verdict" type="button" disabled>Copy the complete verdict</button>
        </div>
      </div>
      <p id="review-status" role="status" aria-live="polite" aria-atomic="true">Resolve every exact subject to unlock the final verdict.</p>
    </section>
    {cards}
    <section class="export-panel">
      <h2>Final result</h2>
      <p>The pinned copy button unlocks after every exact subject has Accept or Reject selected. Its result stays visible in the top review bar, and the complete JSON remains below as a manual-copy fallback.</p>
      <textarea id="verdict-output" readonly aria-label="Exported verdict JSON"></textarea>
    </section>
  </main>
  <script id="attended-review-config" type="application/json">{config_json}</script>
  <script>\n{runtime}\n</script>
</body>
</html>
"""
    return "\n".join(line.rstrip() if line and not line.strip() else line for line in page.splitlines()) + "\n"


def load_json(path: Path, label: str) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except OSError as error:
        raise ReviewError(f"cannot read {label}: {error}") from error
    except json.JSONDecodeError as error:
        raise ReviewError(f"{label} is not valid JSON: {error}") from error


def build(input_path: Path, output_root: Path) -> dict[str, Any]:
    input_path = input_path.resolve()
    output_root = output_root.resolve()
    if output_root.exists():
        if not output_root.is_dir():
            raise ReviewError(f"output path is not a directory: {output_root}")
        if any(output_root.iterdir()):
            raise ReviewError(f"output directory must be empty: {output_root}")
    else:
        output_root.mkdir(parents=True)
    source = load_json(input_path, "input manifest")
    if not isinstance(source, dict):
        raise ReviewError("input manifest must contain a JSON object")
    manifest = build_manifest(source, input_path, output_root)
    skill_root = Path(__file__).resolve().parent.parent
    css = (skill_root / "assets" / "attended_review.css").read_text(encoding="utf-8")
    runtime = (skill_root / "assets" / "attended_review_runtime.js").read_text(encoding="utf-8")
    (output_root / "attended-review-manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (output_root / "index.html").write_text(
        render_html(manifest, css, runtime), encoding="utf-8"
    )
    return manifest


def identity_tuple(value: Any, field: str) -> tuple[str, str]:
    if not isinstance(value, dict):
        raise ReviewError(f"{field} must be an object")
    subject_id = require_string(value.get("subject_id"), f"{field}.subject_id")
    subject_digest = require_string(value.get("subject_digest"), f"{field}.subject_digest")
    if set(value) != {"subject_id", "subject_digest"}:
        raise ReviewError(f"{field} may contain only subject_id and subject_digest")
    return subject_id, subject_digest


def validate_verdict(manifest: dict[str, Any], verdict: dict[str, Any]) -> None:
    if manifest.get("schema_name") != OUTPUT_SCHEMA or manifest.get("schema_version") != SCHEMA_VERSION:
        raise ReviewError("review manifest has an unsupported schema")
    expected_scalars = {
        "schema_name": VERDICT_SCHEMA,
        "schema_version": SCHEMA_VERSION,
        "batch_id": manifest.get("batch_id"),
        "review_gate": manifest.get("review_gate"),
        "review_scope": manifest.get("review_scope"),
    }
    for field, expected in expected_scalars.items():
        if verdict.get(field) != expected:
            raise ReviewError(f"verdict {field} must equal {expected!r}")

    manifest_subjects = manifest.get("subjects", [])
    manifest_order = {
        (subject["subject_id"], subject["subject_digest"]): index
        for index, subject in enumerate(manifest_subjects)
    }
    expected = set(manifest_order)
    decided: list[tuple[str, str]] = []
    for collection in ("accepted_subjects", "rejected_subjects", "not_chosen_subjects"):
        # not_chosen_subjects is optional: a batch with no choice group has
        # nothing to put in it, and every verdict recorded before choice groups
        # existed omits it. Absent means empty, not malformed.
        if collection == "not_chosen_subjects" and collection not in verdict:
            continue
        values = verdict.get(collection)
        if not isinstance(values, list):
            raise ReviewError(f"verdict {collection} must be an array")
        entries = [
            identity_tuple(value, f"{collection}[{index}]")
            for index, value in enumerate(values)
        ]
        known_positions = [manifest_order[entry] for entry in entries if entry in manifest_order]
        if known_positions != sorted(known_positions):
            raise ReviewError(f"verdict {collection} must preserve manifest subject order")
        decided.extend(entries)
    if len(decided) != len(set(decided)):
        raise ReviewError("verdict contains a duplicate subject identity")
    if set(decided) != expected:
        missing = expected - set(decided)
        extra = set(decided) - expected
        raise ReviewError(f"verdict subject partition is not exact; missing={sorted(missing)!r}, extra={sorted(extra)!r}")

    # Exactly one winner per choice group, and every other member recorded as
    # not chosen rather than rejected. A losing alternative is not a defect,
    # and a record that cannot tell the two apart forces the next batch to
    # re-litigate a decision that was already made.
    choice_groups: dict[str, set[tuple[str, str]]] = {}
    selection_defs = {
        group["group_id"]: group for group in manifest.get("selection_groups", [])
    }
    selection_members: dict[str, set[tuple[str, str]]] = {}
    for subject in manifest_subjects:
        identity = (subject["subject_id"], subject["subject_digest"])
        group = subject.get("choice_group")
        if group:
            choice_groups.setdefault(group, set()).add(identity)
        selection = subject.get("selection_group", "")
        if selection:
            if selection not in selection_defs:
                raise ReviewError(
                    f"manifest subject references undeclared selection group {selection!r}"
                )
            selection_members.setdefault(selection, set()).add(identity)
    accepted_set = {
        identity_tuple(value, f"accepted_subjects[{index}]")
        for index, value in enumerate(verdict.get("accepted_subjects", []))
    }
    not_chosen_set = {
        identity_tuple(value, f"not_chosen_subjects[{index}]")
        for index, value in enumerate(verdict.get("not_chosen_subjects", []))
    }
    for group, members in sorted(choice_groups.items()):
        chosen = members & accepted_set
        if len(chosen) != 1:
            raise ReviewError(
                f"choice group {group!r} must have exactly one accepted "
                f"alternative, found {len(chosen)}"
            )
        remainder = members - chosen
        if not remainder <= not_chosen_set:
            raise ReviewError(
                f"every unchosen alternative of choice group {group!r} must be "
                "recorded as not chosen rather than rejected"
            )
    grouped_identities = {
        identity for members in choice_groups.values() for identity in members
    } | {identity for members in selection_members.values() for identity in members}
    stray = not_chosen_set - grouped_identities
    if stray:
        raise ReviewError(
            "not_chosen_subjects may only contain members of a choice or selection group; "
            f"stray={sorted(stray)!r}"
        )

    rejected_set = {
        identity_tuple(value, f"rejected_subjects[{index}]")
        for index, value in enumerate(verdict.get("rejected_subjects", []))
    }
    for group_id, members in sorted(selection_members.items()):
        group = selection_defs[group_id]
        chosen = len(members & accepted_set)
        if chosen == 0 and members <= rejected_set:
            continue
        if not (group["min_accept"] <= chosen <= group["max_accept"]):
            raise ReviewError(
                f"selection group {group_id!r} requires between {group['min_accept']} and "
                f"{group['max_accept']} accepted alternatives, found {chosen}; rejecting "
                "every member is the only alternative to meeting that range"
            )

    audit_failed = {
        (subject["subject_id"], subject["subject_digest"])
        for subject in manifest_subjects
        if subject.get("audit_verdict") and subject["audit_verdict"].get("status") == "fail"
    }
    for index, value in enumerate(verdict.get("accepted_subjects", [])):
        entry = identity_tuple(value, f"accepted_subjects[{index}]")
        if entry in audit_failed:
            raise ReviewError(
                f"subject {entry[0]} failed its mechanical audit and cannot be accepted"
            )

    notes = verdict.get("notes_by_subject_identity")
    if not isinstance(notes, dict):
        raise ReviewError("verdict notes_by_subject_identity must be an object")
    valid_note_keys = {f"{subject_id}@{digest}" for subject_id, digest in expected}
    for key, value in notes.items():
        if key not in valid_note_keys or not isinstance(value, str):
            raise ReviewError(f"invalid subject note entry: {key!r}")
    if verdict.get("downstream_claims") != manifest.get("downstream_claims"):
        raise ReviewError("verdict downstream_claims must equal the manifest's declared false-only claim set")

    required_fields = set(expected_scalars) | {
        "accepted_subjects",
        "rejected_subjects",
        "notes_by_subject_identity",
        "downstream_claims",
    }
    if not required_fields <= set(verdict):
        raise ReviewError("verdict contains missing top-level fields")
    if not set(verdict) <= required_fields | {"not_chosen_subjects"}:
        raise ReviewError("verdict contains unsupported top-level fields")
    if (choice_groups or selection_members) and "not_chosen_subjects" not in verdict:
        raise ReviewError(
            "a batch with a choice or selection group needs not_chosen_subjects, so a losing "
            "alternative is recorded as unchosen rather than as a rejection"
        )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    build_parser = subparsers.add_parser("build", help="build an offline review package")
    build_parser.add_argument("--input", required=True, type=Path)
    build_parser.add_argument("--out", required=True, type=Path)
    validate_parser = subparsers.add_parser("validate-verdict", help="validate a complete exported verdict")
    validate_parser.add_argument("--manifest", required=True, type=Path)
    validate_parser.add_argument("--verdict", required=True, type=Path)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    try:
        if args.command == "build":
            manifest = build(args.input, args.out)
            print(f"built {manifest['batch_id']} at {args.out.resolve() / 'index.html'}")
        else:
            manifest = load_json(args.manifest.resolve(), "review manifest")
            verdict = load_json(args.verdict.resolve(), "verdict")
            if not isinstance(manifest, dict) or not isinstance(verdict, dict):
                raise ReviewError("manifest and verdict must contain JSON objects")
            validate_verdict(manifest, verdict)
            print(f"valid complete verdict for {manifest['batch_id']}")
        return 0
    except ReviewError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
