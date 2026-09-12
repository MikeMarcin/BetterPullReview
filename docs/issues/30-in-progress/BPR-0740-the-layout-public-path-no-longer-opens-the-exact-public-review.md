---
schema: "implement-rfc.card.v1"
id: "BPR-0740"
title: "The layout public path no longer opens the exact public review"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_at":"7c044de4","found_by":"attended-prep-2026-09-10 d16"}
---

# The layout public path no longer opens the exact public review

`application_profile_window_layout_public_path` is red at HEAD. Its
`open_review` helper (`tests/end_to_end/profile_window_layout_public_path_tests.cpp:50-66`)
types `https://github.com/octo-org/review-repository/pull/N` into the Home
open bar, triggers the open action, clicks `homeSetupContinue` when it
appears, and waits for the session panel to reach `pull_request_ready`;
the wait times out with "exact public review opens". Reproduced on
2026-09-11 at 7c044de4 with the [BPR-0737](../40-parked/BPR-0737-the-epoch-line-and-the-comparison-menu-are-one-full-width-row-under-the-strip.md)
edits stashed, so it is not that slice's. The test last changed at
ad3abf50 ([BPR-0708](../60-done/BPR-0708-window-layout-restoration.md));
the Home open path it drives changed under
[BPR-0732](../60-done/BPR-0732-a-signed-out-home-is-one-paste-field-that-opens-a-public-pull-request.md)
(the signed-out Home is one paste field that opens a public link in one
gesture, with no "Read without signing in" and no `homeSetupContinue`
step) and [BPR-0733](../60-done/BPR-0733-home-lists-recently-viewed-pull-requests-instead-of-restoring-the-last-one.md).
No focused selector of either card names this test, which is how it went
red unnoticed; the composed courses it shares a fixture with pass.

## Scope

- Diagnose whether the course or the product is wrong: the course may be
  waiting for a Continue button that BPR-0732 removed, or the
  `RegisteredDeviceCourse` fixture may no longer reach `pull_request_ready`
  from the paste field. Fix the one that is wrong; the accepted BPR-0732
  behaviour is not reopened by this card.
- Owned paths: `tests/end_to_end/profile_window_layout_public_path_tests.cpp`,
  and only if the product is at fault, `src/app/src/github_application.cpp`
  (with both application digests repinned) or the Qt Home surface.

## Acceptance

- `application_profile_window_layout_public_path` passes in `ctest --preset dev`.
- The course still drags the files handle and restores the independent
  window layouts it was written to pin (BPR-0708's criteria), not a
  weakened wait.
