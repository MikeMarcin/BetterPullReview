---
id: BPR-0391
title: The native appearance probe's dark-side measurement outruns the owner
status: In Progress
rfc: RFC-BPR-0006
depends_on: []
---

# The native appearance probe's dark-side measurement outruns the owner

Found by the owner attempting the v004 foreground packet twice. The probe's
Light baseline politely waits for owner focus — on cocoa it loops printing
`NATIVE_APPEARANCE_WAITING_FOR_OWNER_FOCUS` until the window is active with
a focused widget (`main.cpp:3962`). The Dark side never got the same
courtesy: once the local, global, and style-hint schemes all read Dark, the
appearance stage counts `SettleAttempts >= 50U` ticks (`main.cpp:4527`) —
around half a second at the observed ~90 ticks/second — and measures once,
requiring `isActiveWindow`, `activeWindow == &window`, and
`focusWidget == InitialFocus` at that instant (`main.cpp:956`).

A human operator necessarily surrenders frontmost to System Settings to
change the appearance, and cannot return focus to the exact initial widget
within a second of the scheme settling. The 2026-08-08 pass succeeded only
because the owner's standing authorization allowed scripting the scheme
change, so the app never lost focus. The attended procedure as the v004
packet writes it — a person in System Settings — is therefore humanly
impossible at these bytes: the owner's first attempt shows the exact
signature (`NATIVE_APPEARANCE_TRANSITION:Light->Dark` observed,
`NATIVE_APPEARANCE_ACTIVE_FOCUS_FAILED`, `NATIVE_APPEARANCE_STATE_FAILED`,
no rasters), preserved at
`docs/verification/reviews/appearance-scheme-foreground-v004/foreground-run-attempt-1.log`.

## Required final state

- After the schemes settle, the dark side waits for the owner's focus to
  return — the same window-active-and-focused condition the Light side
  already waits for, reported with its own waiting marker — bounded only by
  the run budget, before the one-shot measurement is taken.
- The geometry-rebuild capture semantics are unchanged: the instrumentation
  window still opens at the accepted Light baseline, so rebuilds caused by
  the transition are attributed whether or not the owner has returned yet.
- The offscreen contract proves the new wait cannot hang an automated run:
  on a platform where focus is synthesized the wait is satisfied
  immediately, and budget expiry through the wait still produces the
  complete marker record.
- The v004 packet is regenerated against repaired bytes with the procedure
  unchanged for the operator: click, switch, return, and the probe now
  waits for the return.

## Verification

```sh
ctest --preset dev -R '^(application_native_appearance_probe_contract|application_attended_launch_contract)$' --output-on-failure
```
