# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **bpr1561-linux-cpack-fixture** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## linux-cpack-fixture-repair

The tiny native Linux fixture configures the current production install graph and preserves baseline and default-generator verification.

**Work:** `BPR-1561`

**Prerequisites:** None

**Completion:**

- The native fixture declares the required xdiff shared target without changing production install guards or existing baseline and CPack assertions.
- Exact-candidate Ubuntu 26.04 native verification passes the Linux CPack test and package runtime contract, with a failing missing-target regression control.
- Focused local checks and the documentation gate pass, the issue slice is committed, and task-owned commands and writers are quiescent.

**Boundary:** Unversioned readiness for the next coordinated release only. No version selection or bump, release build, production package or installation, signing or unlock, enrollment, tag, push, publication or desktop control; RFC0038/BPR0961/BPR0962 remain open.
