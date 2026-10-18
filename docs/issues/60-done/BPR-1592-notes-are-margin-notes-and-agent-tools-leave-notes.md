---
schema: "implement-rfc.card.v1"
id: "BPR-1592"
title: "Notes are margin notes, and agent tools leave Notes"
status: "Done"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1591"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "notes-margin", "criteria": ["AC05"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
acceptance: [{"criterion":"AC05","outcome":"pass","revision":"c2487b258","evidence":{"kind":"observation","detail":"Owner Accept, no note, of the frozen build frozen-c2487b258-v001 (executable b0371a5c...f694) in attended session 23a47c6b542ce5e92f47a335594b3a42, candidate BPR-1592-notes-margin-v001 (fingerprint 8a34ecb4...), decision 1d30aced-1d77-4ee1-a30f-3c4f29e90d2c, actor human: 'Private notes sit beside their lines like threads.' Automated on c2487b258 (main had not moved since BPR-1591's 4d0e58539): qt_private_notes_panel in Light and Dark (notes at their lines without overlap, outdated notes after the placed ones with their reason, pull-request/whole-file/other-file chips, line chips for notes out of view, open in place with the blue edge, Cmd+Return save with revision and line, refusal and refresh keep text, Esc once warns and twice discards, Draft comment offered at the note's line and not for an outdated note, pull-request note waits for Comment's target, delete, other file's note goes to its file, N on the selected line keeps side, line and blob, N without a line says why, Up/Down/Return, scope change resets; drawer principles hold), qt_workbench_window_scale_100/125/200 and the whole window binary (Notes shows no agent tools; the Agent tab holds Private history and Agent tools; tray header Comments, Notes, Tour, Agent on one row; icons only below 340 pixels; reachability at 280), qt_hide_drawers_native_menu_key, qt_drawer_parts_scale_*, application_review_submission_composition_public_path and scales (real app: pull-request note and a line note written with N, its path and side/line in the ledger, refusal, discard, delete; Draft comment opens the exact local draft at the note's line with zero GitHub mutation; restart keeps the note with its line), application_start_review_contract, core_note_placement, sqlite_review_ledger_storage, every application composition contract (round-trip resealed), architecture_boundaries, coding_format_check (25 rows); documentation gate."}}]
---
# BPR-1592: Notes are margin notes, and agent tools leave Notes

RFC-BPR-0085 D3 in Qt: notes hang at their lines like threads, dashed with a
lock; edge chips for pull-request and other-file notes; N for a new note; the
promote action, labelled **Draft comment** (the owner's choice at
[[BPR-1588]]'s review, 2026-10-17). The agent panels move unchanged into a
provisional Agent tab until [[BPR-1594]] rebuilds it.

## Acceptance

- RFC-BPR-0085 AC05.
- Owner, attended: "Private notes sit beside their lines like threads."

## Boundaries

Qt presentation over BPR-1591's anchors; promotion's semantics unchanged.
