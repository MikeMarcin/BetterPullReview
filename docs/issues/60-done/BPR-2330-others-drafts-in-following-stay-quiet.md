---
schema: "implement-rfc.card.v1"
id: "BPR-2330"
title: "Another author's draft waiting in Following sends no notification"
status: "Done"
rfcs: []
depends_on: ["BPR-2326"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","found_by":"owner, 2026-11-20, relayed by the coordinator, answering whether a draft in Following should still notify: \"Yes, stay quiet.\"","reservation":"docs/issues/_reservations/BPR-2330--BPR-2330--a64322720637.json","related_cards":["BPR-2326","BPR-1683"],"acceptance":["AC1","AC2","AC3","AC4"],"disk_note":"free disk fell to 3.6 GB after the core/application build (a freeze was also building); app sources were syntax-checked only and the build tree removed"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "4eef754eb", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(core_review_queue_attention|application_review_queue_notification)$'", "detail": "Core test_a_draft_waiting_in_following_is_quiet: another author's draft waits quietly with Following, not with Needs me, not once ready, not with an unread status, not when it is mine. Application test_a_draft_waiting_in_following_stays_quiet: for every bucket, notification_for and notification_active refuse the draft with Following; three refreshes of a draft review request make nothing due and nothing active."}}, {"criterion": "AC2", "outcome": "pass", "revision": "4eef754eb", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^application_review_queue_notification$'", "detail": "Marked ready (Draft=false), step_queue_notifications makes the pending review request due once and notification_for builds its request; three later refreshes in the same state make nothing due. The profile runtime (profile_runtime.cpp publish) now delivers exactly step.Due; it was syntax-checked with its build flags (clang -fsyntax-only, rc 0) but not linked or run: disk fell under 5 GB."}}, {"criterion": "AC3", "outcome": "pass", "revision": "4eef754eb", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(core_review_queue_attention|application_review_queue_notification)$'", "detail": "With Needs me every bucket notifies exactly as warrants_notification says and the draft request is due at once; comments on my own draft still notify; an ignored row stays quiet. Existing notification cases unchanged and passing."}}, {"criterion": "AC4", "outcome": "pass", "revision": "4eef754eb", "evidence": {"kind": "command", "command": "cmake -DBPR_ROOT=. -P cmake/VerifyArchitectureBoundaries.cmake; _coding-standard/scripts/format.py --dry-run (clang-format 22.1.8); python3 scripts/verify_documentation.py --root .; python3 tests/scripts/qualification_case_inventory_tests.py", "detail": "Boundaries verified; format clean; documentation gate on main after landing; qualification_case_inventory_contract passes after re-pinning composed_github_tests.cpp (BPR-2308 and BPR-2319 changes outside its two pinned cases) and counting 264 occurrences (BPR-2326's workbench case). Not run: the composed app and the end-to-end queue notification course (queue_composition_public_path), which need the full application build."}}]
---

# BPR-2330: Another author's draft waiting in Following sends no notification

## Asked

[[BPR-2326]] moved another author's draft from Needs me to Following but
left its notification eligibility alone, so a review request on a draft
still interrupted. The owner, 2026-11-20: "Yes, stay quiet." A follow-up
card because BPR-2326 is Done.

## Acceptance

- AC1 While another author's pull request is read as a draft and Settings ›
  Review › Home › Others' drafts is Following (the default), it sends no
  notification: not for a review request, comments, commits since my review
  or watched activity.
- AC2 When a read says it is ready for review, the usual rules apply: a
  pending review request (or other notifying state) notifies then, once, and
  not again on later refreshes while it stays in the same state.
- AC3 With Others' drafts on Needs me, notifications are as before. My own
  drafts and ignored rows are unchanged.
- AC4 Tests: core/application policy (a draft's request does not notify;
  ready notifies once; Needs me notifies), architecture boundaries, format
  and the documentation gate.

## Outcome

`core::draft_waits_quietly` decides when another author's draft is held
quiet; `application::notification_active` and
`application::step_queue_notifications` apply it, so the profile runtime
notifies such a row only once a read says it is ready, and then once.

Not verified: the composed application course for queue notifications and a
live BPR-Test run; the app sources were syntax-checked only.
