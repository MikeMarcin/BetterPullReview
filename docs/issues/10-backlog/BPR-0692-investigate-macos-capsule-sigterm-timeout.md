---
schema: "implement-rfc.card.v1"
id: "BPR-0692"
title: "Investigate macOS capsule SIGTERM cleanup timeout"
status: "Backlog"
rfcs: ["RFC-BPR-0016"]
depends_on: []
platforms: ["mac"]
---

# Investigate macOS capsule SIGTERM cleanup timeout

## Scope

Investigate the existing native capsule cancellation contract failure observed
while running the required core-only UX integration checks. This is independent
of profile/Settings source changes; it is not a reason to claim a green full
suite or to stop independent UX implementation.

## Evidence

At `2b40da44` plus ongoing UX changes, core-only ran 128 tests in445.72s.
125 passed; two URL-layer boundary failures were corrected and passed narrow
rechecks. The remaining failure is
`CapsuleImageTests.test_sigterm_reaps_blocked_child_group_and_detaches_real_image`
in `tests/scripts/designated_live_macos_capsule_tests.py`: child
`stage-zero --probe-launch` did not complete within30s after SIGTERM.
The full capsule suite ran44 cases in114.239s with this one error.

Direct isolated reproduction:
`python3 tests/scripts/designated_live_macos_capsule_tests.py CapsuleImageTests.test_sigterm_reaps_blocked_child_group_and_detaches_real_image`
failed with the same timeout in62.714s. No related production/script source was
changed by the UX work. Both attempts' named child processes were gone and
`hdiutil info` showed no test capsule mounts after completion. Root cause is
unresolved; neither timing noise nor a production defect is asserted.

## Acceptance

Find the blocked cleanup operation using fixture-only observation. Preserve
process-group reap and image detach requirements; do not merely raise timeouts
or suppress the case. Run the focused failing case and relevant capsule cleanup
checks after correction; record actual native result and cleanup. No live
credential, designated repository mutation or UX change is authorized here.
