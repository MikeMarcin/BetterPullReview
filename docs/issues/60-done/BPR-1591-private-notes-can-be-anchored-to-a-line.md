---
schema: "implement-rfc.card.v1"
id: "BPR-1591"
title: "Private notes can be anchored to a line"
status: "Done"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1587"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "notes-margin", "criteria": ["AC05"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
acceptance: [{"criterion":"AC05","outcome":"pass","revision":"467d325ad","evidence":{"kind":"observation","detail":"Data half of AC05 (BPR-1591; the attended half is BPR-1592's). core_note_placement: a note hangs at its line where written and again after restart; a new head whose side is the same blob keeps the line, a changed side outdates it (content_changed), a deleted side outdates it (side_absent), the unchanged base side holds (negative control); pull-request, whole-file and other-file notes are not judged by line; no anchor past a side's last line or on an added file's empty side; a line kept without its blob holds only in its own comparison. sqlite_review_ledger_storage test_note_lines_migrate_and_round_trip: schema 9 migrates to 10 with an empty private_note_line, the old note unchanged; a note is written with its line, an edit keeps it, an edit that moves it is refused as a conflict, an invalid blob is refused; the line survives restart and core::place_note places, keeps and outdates it across a new head; deleting a note deletes its line only. sqlite_private_review_storage (shared v8 fixture), core_review_ledger, core_private_review, core_agent_finding*, application_review_ledger_host, application_private_review_host/native/file, qt_private_notes_panel, application_agent_finding_composition_contract, application_review_ledger_composition_contract, application_composition_contract, application_review_round_trip_composition_contract, architecture_boundaries and architecture_boundary_contract (45 rows), core-only core_note_placement and architecture_boundaries, coding_format_check, documentation gate."}}]
---
# BPR-1591: Private notes can be anchored to a line

The data half of RFC-BPR-0085 D3: a private note may record a side and line
against the comparison it was written on, in addition to today's file and
pull-request anchors.

## Acceptance

- Core decides placement (placed, outdated with its reason) from the note's
  anchor and the current comparison; the ledger migrates forward with old
  notes unchanged; restart and a new head keep or outdate the anchor as
  RFC-BPR-0085 AC05 says; architecture boundaries pass.

## Boundaries

Core, application host and SQLite ledger adapter; no Qt change.
