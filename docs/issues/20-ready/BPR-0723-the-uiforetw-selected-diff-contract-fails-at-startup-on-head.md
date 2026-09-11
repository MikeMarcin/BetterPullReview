---
schema: "implement-rfc.card.v1"
id: "BPR-0723"
title: "The UIforETW selected-diff contract fails at startup on HEAD"
status: "Ready"
rfcs: ["RFC-BPR-0004", "RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"first_seen":"8ff3b664","found_during":"BPR-0719"}
---

# The UIforETW selected-diff contract fails at startup on HEAD

`application_uiforetw_selected_diff_contract` is red at `8ff3b664`, before
any change of the v003 follow-ups. It was found while running the
[BPR-0719](../30-in-progress/BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md)
focused batch and reproduced on a clean worktree build of that HEAD
(`cmake --preset dev`, then `ctest -R '^application_uiforetw_selected_diff_contract$'`):

```
UIforETW light capture failed (1)
stdout:
STARTUP_FAILED
```

The failure comes 0.5 seconds into the first (light) capture, before the
probe runs: the application prints `STARTUP_FAILED` with nothing on stderr.
The configuration check at `src/app/main.cpp` (around line 7868) is not
the site, because it prints `UIFORETW_SELECTED_DIFF_CONFIGURATION_FAILED`;
what remains is a `github_application` start failure other than
`owner_busy` (the branch around line 7995) or the `catch (...)` around
line 8116. A developer build reports a thrown `std::exception` on stderr
as `STARTUP_FAILED_CAUSE:`, and none was printed, so the failure is either
a returned start error or a non-`std::exception` throw. The same binary
starts for `application_diff_workbench_public_path`,
`application_composition_contract` and
`application_review_round_trip_composition_contract`, which passed in the
same batch; the difference is the `--epoch-fixture-root` /
`--epoch-fixture-phase single` / `--uiforetw-selected-diff-evidence` /
`--diff-evidence-appearance` arguments.

Running the contract's exact command by hand (the app from `build/dev`,
`QT_QPA_PLATFORM=offscreen`, a fresh fixture root) reproduces the
one-word failure.

## Scope

- Name the failing startup step and repair it, or, if the failure is
  environmental to this station, name the environmental condition and make
  the application say so instead of `STARTUP_FAILED`.
- The contract is part of the RFC-BPR-0004 diff evidence and the
  RFC-BPR-0028 strip contract (compact row at 1280 and the 18-pixel rails),
  so it must be green before a closeout leans on either.

## Acceptance

- `application_uiforetw_selected_diff_contract` passes in `build/dev`.
- The cause is recorded here with the commit that introduced it, or the
  environmental condition is named and the application reports it by
  name.
