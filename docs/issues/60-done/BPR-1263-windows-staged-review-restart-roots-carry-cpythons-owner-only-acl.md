---
schema: "implement-rfc.card.v1"
id: "BPR-1263"
title: "Windows: staged_review_restart's roots carry CPython's owner-only ACL, and the staged journal refuses them"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win BPR-0905 Windows full suite and red triage, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1263--BPR-1263--3bcc890db10d.json","related_cards":["BPR-0905","BPR-1233","BPR-0834","BPR-1237"]}
acceptance: [{"criterion":"staged_review_restart passes on Windows, every kill point included","outcome":"pass","revision":"e86dafa19","evidence":{"kind":"command","detail":"bpr-main-win, ctest --preset windows-dev -j1 -V -R ^staged_review_restart$ with this change: passes in 6.24 s and prints all 24 markers, 15 staged-kill:<step>:<boundary>:pass and 9 maintenance-kill:<step>:<boundary>:pass; staged_review_session passes in 3.06 s. Before, it failed alone in 0.13 s at 'FAILED: production staged journal opens'."}},{"criterion":"POSIX behavior is unchanged","outcome":"pass","revision":"e86dafa19","evidence":{"kind":"observation","detail":"make_root_private's POSIX branch is root.mkdir(mode=0o700, exist_ok=True), the call it replaces; each root is a fresh name under a new contained temporary directory, so exist_ok never applies."}}]
---

# BPR-1263: Windows: staged_review_restart's roots carry CPython's owner-only ACL, and the staged journal refuses them

`staged_review_restart` failed in the [BPR-0905 Windows full suite](../../verification/BPR-0905-full-suite-2026-10-01.md),
and failed the same way when run alone, in 0.13 s:

```
RuntimeError: --kill-0-prepared exited before checkpoint (1):
FAILED: production staged journal opens
```

## Cause

`tests/scripts/staged_review_restart_tests.py` made each course's root with
`Path.mkdir(mode=0o700)`. On Windows, CPython writes that as its own owner-only
descriptor: explicit SYSTEM, Administrators and OWNER RIGHTS entries. The
stores' owner-private rule refuses that shape, so the staged journal would not
open in the first course. [[BPR-1233]] found and fixed the same cause in the
submission restart course.

## Fix

The same idiom: both root sites now call `make_root_private` from
`tests/end_to_end/bpr_narrow_owner_private.py` ([[BPR-0834]]). On POSIX that
is `mkdir(mode=0o700, exist_ok=True)`, the same as before. On Windows it
creates the directory and narrows it to the stores' own descriptor.

## Acceptance

- `staged_review_restart` passes on Windows, every kill point included.
- POSIX behavior is unchanged.

## Windows evidence (bpr-main-win, 2026-10-01)

Windows 11 Home 10.0.26200, Python 3.14, `build/windows-dev` Debug, at
`e86dafa19` plus this change. The row stays `WINDOWS_UNCONTAINED`, as
registered; its pin is [[BPR-1237]]'s.

```powershell
ctest --preset windows-dev -j1 -V -R "^staged_review_restart$"
ctest --preset windows-dev -j1 --output-on-failure -R "^(staged_review_restart|staged_review_session)$"
```

- `staged_review_restart` passes in 6.24 s. The verbose run prints all 24
  markers: 15 `staged-kill:<step>:<boundary>:pass` (5 steps × 3 boundaries) and
  9 `maintenance-kill:<step>:<boundary>:pass` (3 × 3).
- `staged_review_session` passes in 3.06 s.

POSIX is unchanged by construction: `make_root_private` takes the
`mkdir(mode=0o700)` path there.
