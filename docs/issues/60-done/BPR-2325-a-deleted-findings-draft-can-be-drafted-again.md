---
schema: "implement-rfc.card.v1"
id: "BPR-2325"
title: "A finding whose draft was deleted still says Drafted and cannot be drafted again"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2325--BPR-2325--51d9cf6303f8.json","origin":"owner, 2026-11-20","related_cards":["BPR-2219","BPR-2231","BPR-2232","BPR-2061","BPR-2322","BPR-2323"],"related_rfcs":["RFC-BPR-0128"]}
---

# BPR-2325: A finding whose draft was deleted still says Drafted and cannot be drafted again

## Owner (2026-11-20)

"If I delete a draft comment from a report, it looks like there's no way for
me to re-add it, which is something I want to do sometimes when I mess it
up. It still just says drafted."

## Cause

A finding's kept disposition (`core::agent_finding_card_record`, the
`agent_finding_cards` store) says `drafted` and nothing else: it does not
name the draft it made. Deleting that draft, from the draft editor, Threads,
Finish your review or Discard review, changes nothing the finding reads, so
the report's finding and its Agent tab card keep saying Drafted, offer only
Go to draft, and do so again after a restart. Only Draft all's own Undo
reopened its cards.

## Change

- A drafted finding keeps the identity of the draft it made, in the record's
  existing `note` column (the note Keep local made, or now the draft Draft
  comment made). No schema change.
- When that draft is deleted, by any path, the finding is open again: the
  record is forgotten (so a restart finds it open), the report's finding and
  its Agent tab card stop saying Drafted and offer Draft comment again,
  which drafts the finding's text as it is now (a side chat's update
  included).
- A published draft is not deleted: its finding keeps saying Drafted and is
  not offered again.
- Each cleared link is a Standard diagnostic event, counts only.

## Acceptance

- AC1. Core: a drafted record keeps its draft's identity (and a legacy one
  reads 0); a record of any other disposition keeps none; the records
  drafted with a given draft are found.
- AC2. Application: deleting a finding's draft forgets its record, so a
  restart reads it open; publishing it does not.
- AC3. Qt: the report's finding and the Agent tab card go back to Draft
  comment when their draft's link is cleared; a published one keeps
  Drafted.
- AC4. A Standard `agent.finding_card` line says how many links were cleared.
- AC5. `architecture_boundaries`, `coding-format-check` and
  `python3 scripts/verify_documentation.py --root .` pass.

## Decisions

- When a draft counts as deleted: the draft owner reads it discarded, or no
  longer holds it after this same owner was seen holding it
  (`reopen_deleted_finding_drafts`, run whenever the saved drafts are
  presented). A draft another pull request's owner holds, or one not loaded
  yet, changes nothing, so a finding is never reopened by mistake.
- Published drafts are not deleted: their findings keep Drafted and are not
  offered again, as before.
- A card drafted before this change kept no draft identity; it keeps
  Drafted.
- The log line is a new `agent.finding_card` operation
  (`action=finding_draft_cleared count=N`), counts only.

## Verification

macOS, dev preset, `build/dev` at `ed7928922` (this change on main `cb15fb45d`
plus BPR-2325's allocation). `coding-format` on every changed C++ file.

| Criterion | Check | Result |
|---|---|---|
| AC1 | `core_agent_finding_card_memory` (a drafted record keeps its draft, a dismissed one nothing; `drafted_as`; a report's Draft comment keeps its draft) | pass |
| AC2 | `qt_agent_finding_cards_restart` (through the SQLite store: a drafted card comes back after a restart with its draft; reopened, it is kept open); `application_follow_up_public_path` (a report finding drafted, its draft deleted from Comments with Delete and its confirmation: the report and the Agent tab card offer Draft comment again, which drafts the finding's updated text again; nothing published) | pass |
| AC3 | `qt_agent_report_view` (a finding whose record is forgotten offers Draft comment again; one still drafted keeps Drafted); `qt_agent_finding_cards_restart` (only a drafted card is reopened) | pass |
| AC4 | `core_operation_diagnosis` (the line's grammar; it cannot fail); `application_follow_up_public_path` (the log has `action=finding_draft_cleared ... count=1`); `application_operation_diagnostics`, `application_host_operation_diagnostics`, `application_release_maintenance`, `diagnostic_controls` | pass |
| AC5 | `architecture_boundaries`, `coding_format_check`, `verify_documentation.py` | pass |

Not this change:
- `sqlite_agent_setup_storage` stops at "its check inspects its own program
  and passes" (the custom agent check [[BPR-2320]] changed), before its
  finding-card section, so that section's new draft assertion did not run
  there. The same persistence is checked through the SQLite store by
  `qt_agent_finding_cards_restart`.
- `application_review_round_trip_composition_contract`: the production app
  digest is resealed and passes; the course then stops at
  `SEMANTIC_COMMENT_PROBE_FAILED phase=4`, which [[BPR-2269]] recorded as
  predating it.

No real agent and no live GitHub. No attended look was prepared.
