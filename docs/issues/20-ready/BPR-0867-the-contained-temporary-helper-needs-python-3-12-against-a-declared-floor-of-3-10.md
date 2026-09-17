---
schema: "implement-rfc.card.v1"
id: "BPR-0867"
title: "The contained temporary helper needs Python 3.12 while the tree's declared floor is 3.10"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-sweep-fixes, while moving thirteen tempfile sites onto the helper","found_at":"e84eaba2","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0867: The contained temporary helper needs Python 3.12 while the tree's declared floor is 3.10

## Defect

`scripts/contained_temporary_support.py` calls
`shutil.rmtree(..., onexc=...)`. The `onexc` parameter was added in **Python
3.12**; before that the callback parameter is `onerror`, and passing `onexc` is
a `TypeError`. The tree's declared interpreter floor is **3.10**
(`cmake/BprDependencies.cmake`).

So every course that uses this helper silently requires an interpreter two
minor versions newer than the project says it needs. There is no check that
catches the gap: central discovery accepts a 3.10 interpreter, and the failure
would appear only when a course tried to clean up, as a `TypeError` from inside
the helper rather than as "your Python is too old".

This is pre-existing and tree-wide — more than twenty call sites already depend
on it. [[BPR-0854]] item 1 adds nine more files to that dependency, which is
how it was noticed; item 1 did not create it and is not blocked by it. Every
machine in this run happens to have 3.12 or newer, so nothing is red today.
That is precisely why it wants a card rather than a discovery later on
whichever machine is oldest.

## Scope

Decide which of the two is wrong, and make the tree say so:

- **Raise the declared floor** to the version the helper actually needs, in
  `cmake/BprDependencies.cmake` and anywhere else the floor is stated, and let
  central discovery refuse an older interpreter with a message that names the
  reason. Cheapest, and honest, if 3.12 is acceptable on every machine that
  must build this project — including the Windows and Linux stations and any
  fresh contributor checkout.
- **Or make the helper work at 3.10**: select `onexc` or `onerror` by
  `sys.version_info`, and keep the floor. Slightly more code in one place, and
  it keeps the stated contract true.

Either way, add the check that would have caught it: a gate that the helper's
requirements do not exceed the declared floor, or a course that exercises the
helper's cleanup path on the floor version. A silent two-version drift is the
actual defect here; whichever way the version question is settled, the drift
should not be able to recur unobserved.

Related, and worth settling together: [[BPR-0868]] covers the same helper's
cleanup-error semantics.

## Acceptance

- The helper's real interpreter requirement and the tree's declared floor
  agree, and the card names which one moved and why.
- An interpreter below the declared floor is refused with a message naming the
  reason, or the helper is shown to work at the floor version.
- A check exists that fails if the helper's requirement drifts above the
  declared floor again.

## `bpr-linux`'s interpreters (dsp-005, bpr-linux/int-12)

Collected on station `bpr-linux` (JaegAW, WSL2, Ubuntu 26.04 LTS). As with
`clang-format` on [[BPR-0865]], this station has two interpreters and `PATH`
decides which one answers:

| Resolution | Version | Has `shutil.rmtree(onexc=)` (3.12+) |
|---|---|---|
| `python3` (PATH) → `/home/mmarcin/.local/bin/python3` | `Python 3.13.15` | Yes |
| `/usr/bin/python3` → `python3.14` | `Python 3.14.4` | Yes |

`python3` does **not** resolve to `/usr/bin/python3` here; it resolves to
`/home/mmarcin/.local/bin/python3`. Both satisfy the helper's real 3.12
requirement, so this station is green today and cannot exercise the gap — it
has no interpreter at or near the declared 3.10 floor in either location.

**Why the difference matters to the floor question.** The two resolutions
differ by a full minor version (3.13 against 3.14), which shows that the
interpreter a course actually runs on this station is a `PATH` accident rather
than a property of the machine. A floor stated in
`cmake/BprDependencies.cmake` is checked against whichever interpreter central
discovery happens to select, so "every machine in this run has 3.12 or newer"
is a statement about today's `PATH` on three machines, not a guarantee the
declared floor is honest. That is the silent drift this card names, observed
concretely: nothing on this station would report the discrepancy, because
nothing here is old enough to fail.

The cheap option — raising the declared floor — is unobstructed by this
station: both of its interpreters already exceed 3.12, so neither would be
refused.

**Not run here.** No interpreter was installed, no floor was changed and the
helper's cleanup path was not exercised at 3.10; this task is runs, not fixes.
`contained_temporary_usage_contract`, which covers the helper, passes at the
merged head (6 tests OK, 2.53 s) — on 3.13.15, so that pass says nothing about
3.10.
