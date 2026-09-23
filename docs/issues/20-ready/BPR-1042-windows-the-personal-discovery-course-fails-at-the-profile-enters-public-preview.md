---
schema: "implement-rfc.card.v1"
id: "BPR-1042"
title: "Windows: application_personal_discovery_public_path fails at BPR-1017's step \"the profile enters public preview\""
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-win rows after BPR-1017","found_at":"4fd95fe5","reproduced_at":["4fd95fe5","364c4f37"],"regression_from":"bfd927e7","related_cards":["BPR-1017"],"windows_specific":false,"fix_owner":"mac","run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1038--BPR-1042--5b0f0a679128.json"}
---

# BPR-1042: Windows: application_personal_discovery_public_path fails at BPR-1017's step "the profile enters public preview"

## Verdict: not Windows-specific

The failure does not depend on Windows code. It depends on a
per-platform build selection, and Linux has the same selection as Windows.

[[BPR-1017]] (`bfd927e7`) added a step to the course. It calls
`owner->activate_public_preview()` and then waits for
`status().PublicPreviewSignedOut`. The course does not force public preview.
It relies on the shipped selection,
`shipped_public_preview_capability()` (`src/app/src/github_application.cpp:2254`).
That selection is `Available = true` only under `__APPLE__`. It is `false`
under `__linux__` and `_WIN32`. Where it is false, the composition gives the
host no anonymous authority. The host refuses the activation with
`public_preview_unavailable` and never enters public preview. The wait then
runs out its 20 s.

`bfd927e7` was verified on macOS, where the selection is available, so the
step passed there. Windows fails at every run. Linux should fail at the same
step for the same reason. That is inferred from the source; this station has
not observed it.

The fix belongs in the course, so it is the Mac's. Other composed courses
that need public preview force the fixture's authority with
`options.PublicPreviewEnabledCourse = true` (for example
`device_binding_restart_tests.cpp:197` and
`setup_destination_public_path_tests.cpp:669`). The alternative is to
assert the public-preview variant only where the shipped selection is
available, and say so. Either way, [[BPR-1017]]'s AC2 (the signed-out and
public-preview windows present as before) must still be checked on the
platforms that can run it.

## Windows evidence

JaegAW, Windows 11 10.0.26200, `build/dev` Debug (MSVC 19.51, Qt 6.11.2),
offscreen and uncontained as registered. The records are station logs under
`build/logs/{mac-batch2-main-win,bpr-1042-main-win}/` and are not committed.

- At `4fd95fe5`: fails in all 4 runs (two sweeps and `rerun-1017-{1,2}`),
  each at `FAILED: the profile enters public preview`.
- At `364c4f37`: fails 3 of 3 (`ctest -j1 -V`). Each run takes 22.3 s, 20 s of
  which is the step's wait. The output before the failure is:

  ```text
  signed-out sibling: status=0 landing=0 queue shown=0 account=Sign in
  FAILED: the profile enters public preview
  ```

  The signed-out sibling step passes: `no_profile`, the sign-in landing, no
  queue, "Sign in".
- A local diagnostic build of the course, not committed, printed the owner's
  status on both sides of `activate_public_preview()`:

  | Run | Before | 1 s after |
  | --- | --- | --- |
  | As committed | `PublicPreviewComposed=0`, not signed out | `PublicPreviewUnavailable=1`, `PublicPreviewSignedOut=0` |
  | With the signed-out sibling skipped | the same | the same |

  So the new sibling step is not the cause. The activation is refused
  because public preview was never composed.
- Every run also prints `AGENT_WORKSPACE_RECOVERY_REFUSED:…:repair-required`.
  This is not related. Windows courses whose data root sits directly in a
  `QTemporaryDir` print it, including ones that pass (see [[BPR-1027]]'s
  "Offscreen" section).

## Acceptance

- AC1 The course reaches its public-preview step, or deliberately skips it
  with a stated reason, on platforms whose shipped selection has no public
  preview. The signed-out and public-preview sibling checks still run
  wherever public preview can be entered.
- AC2 `application_personal_discovery_public_path` passes on Windows N of N
  (N ≥ 3) and on macOS. Linux runs it and it passes.
