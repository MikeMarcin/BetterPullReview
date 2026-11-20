---
schema: "implement-rfc.card.v1"
id: "BPR-2325"
title: "A finding whose draft was deleted still says Drafted and cannot be drafted again"
status: "In Progress"
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
