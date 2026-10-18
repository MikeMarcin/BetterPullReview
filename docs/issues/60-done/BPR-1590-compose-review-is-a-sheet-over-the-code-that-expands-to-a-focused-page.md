---
schema: "implement-rfc.card.v1"
id: "BPR-1590"
title: "Compose review is a sheet over the code that expands to a focused page"
status: "Done"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1587"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "compose-sheet", "criteria": ["AC04"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
acceptance: [{"criterion":"AC04","outcome":"pass","revision":"59d2234e6","evidence":{"kind":"observation","detail":"Owner Accept of the frozen build frozen-f8d68d949-v003 (executable cf627958…52a3) in attended session 95b80a7a4bd9bf3e575d2338a8986edc, candidate BPR-1590-compose-sheet-v003 (fingerprint 8b1c229f…), decision 89f1f2df-f2bd-4cfb-b2ff-3e69cd4a4d5a, actor human, note: \"This open in review is a bit too much, i think it's fine without that. Otherwise I think it's great.\" The hover words were removed in 615be6e85 as the note asks, without a new review (coordinator). v001 rejected (decision 41b07e24: dead space, not the chosen direction, placeholder data); v002 rejected (session 88280030: rows not one line, no way to the real comment, summary start easy to miss). Automated on the tree merged with main 857f03ebb: qt_review_submission_editor_scale_* (one-line rows with a very long comment, single click opens nothing, double click, Shift, Return, Shift+Return, row menu with Show in Threads, no hover words), qt_workbench_window_scale_100/200 and the whole window binary (sheet at 540 px beside the code, Expand/Restore keep text, caret and scroll, drawer principles; pending rows open their drafts in Review in the drawer focused, leave the expanded page, Shift+Return to Discuss's composer), application_start_review_contract (the real app opens the row's draft in the drawer), review submission composition public path and scales, setup destination, discuss top, header source navigation, review navigation entry, source PR find, qt_conversation_finder_scale_* (shared Threads gestures), qt_feedback_label_threads, every application composition contract (round-trip resealed), architecture_boundaries, coding_format_check (32 rows); documentation gate."}}]
---
# BPR-1590: Compose review is a sheet over the code that expands to a focused page

RFC-BPR-0085 D6: Finish your review as a 540-pixel sheet over the current
surface, Expand (⌘⇧M) to the focused single-column page in Discuss, Restore
back; no thread list; BPR-1541's panel unchanged. Supersedes [[BPR-1212]] Q3
for Compose review only.

## Acceptance

- RFC-BPR-0085 AC04.
- Owner, attended: "Finish review is a sheet beside the code, and expands to a
  focused page."

## Boundaries

Qt presentation and the composition page's placement; [[BPR-1345]] and
[[BPR-1541]] behaviour unchanged. Uses [[BPR-1584]]'s reading width if landed.
