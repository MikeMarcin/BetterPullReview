---
schema: "implement-rfc.card.v1"
id: "BPR-0741"
title: "The multi-window public path course still waits for a sign-in stop"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_at":"65c47ec6","found_by":"attended-prep-2026-09-10 d17"}
---

# The multi-window public path course still waits for a sign-in stop

`application_profile_multi_window_public_path` is red at HEAD, in one
second: `submit(*windowA, 17)` pastes the link signed out and the course
then asserts `homeSetupContinue` visible ("first review offers explicit
sign in",
`tests/end_to_end/profile_multi_window_public_path_tests.cpp:190-191`).
Since [BPR-0732](../60-done/BPR-0732-a-signed-out-home-is-one-paste-field-that-opens-a-public-pull-request.md)
a pasted public link no longer stops at sign-in where preview is composed,
and on macOS `shipped_public_preview_capability()` composes it for every
github.com fixture course (`src/app/src/github_application.cpp:1965-1977`,
`:4654-4674`): the destination resolves to `public_preview`, the button is
hidden by design, and the read goes out anonymously. The `github_com`
fixture answers the anonymous read of pull 17 once and never pull 18
(`src/app/src/github_public_path_fixture.cpp:932`, `:1368-1374`), so the
course cannot pass on the anonymous path. Same cause as
[BPR-0740](../30-in-progress/BPR-0740-the-layout-public-path-no-longer-opens-the-exact-public-review.md),
whose course was converted to the account path in d17; this one is
outside that card's owned paths. On Linux and Windows the capability is
not shipped and the course still sees `sign_in`, so the red is macOS-only.

## Scope

- Convert the course to the account path the restoration course uses
  (`profile_window_review_restoration_public_path_tests.cpp:333-341`):
  sign in from the landing card's primary button, wait for
  `session_ready` and `!PublicPreviewSignedOut`, then paste; an account
  opens the destination without a stop, so the Continue assertion becomes
  "hidden". Keep every multi-window criterion the course pins.
- Owned paths: `tests/end_to_end/profile_multi_window_public_path_tests.cpp`
  only; the accepted BPR-0732 behaviour is not reopened.

## Acceptance

- `application_profile_multi_window_public_path` passes in `ctest --preset dev`
  on macOS, with the same windows, reads and restarts it pinned before.
- `application_profile_window_review_restoration_public_path` and
  `application_profile_window_layout_public_path` still pass.
