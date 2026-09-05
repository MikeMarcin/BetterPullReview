---
schema: "implement-rfc.card.v1"
id: "BPR-0404"
title: "The frontier table says \"Nothing\" where it means \"a Windows session\""
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# The frontier table says "Nothing" where it means "a Windows session"

The frontier table's third column answers one question: what does this root
need before it can move? For the three active-lane roots it currently answers
"Nothing."

| Root | Lane | Needs |
|---|---|---|
| BPR-0347 | `30-in-progress/` | Nothing. The Windows suite runs here and its own remaining failures are the work |
| BPR-0358 | `30-in-progress/` | Nothing. It records that the Windows platform arrived and repairs the lanes that assumed it had not |
| BPR-0240 | `30-in-progress/` | Nothing. Its portable gates have run and target-native Windows is this host |

Every word of that is true and it still misleads, because "here" and "this
host" mean the project's Windows 11 x64 machine and the table does not say so.
Read from a macOS session — which is where this board is read most of the time —
those three rows say "available to start now", and they are not.

The board already knows better. Sixty lines below the table, the caveat
paragraph spells it out:

> One caveat the lane names cannot carry: "this host" is the project's, not any
> one session's. The Windows roots are executable on the Windows 11 x64 machine
> BPR-0358 recorded, and are not executable from a macOS session.

A caveat sixty lines below a table does not correct the table. The table is what
gets read first, and it is the artifact `scripts/verify_documentation.py` derives
and enforces — the verifier checks that the right cards are listed under the
right lanes, and cannot check whether the "Needs" cell tells the truth.

## Why this is not a lane change

Filing these under `50-blocked/` would be the wrong repair, and [[BPR-0358]]
already settled why. That lane admits a card only when its work "requires
target-native execution on a **different** operating system", measured against
the project's host set, not the current session's. The project has a Windows
host. Moving them back would make `50-blocked/` mean "was blocked once", which
is the reading [[BPR-0319]] built the lane to prevent, and it would also be a
false claim that the tooling deriving this table would then propagate.

The honest fix is smaller and is entirely in the third column: say what the root
needs. A queue position stated plainly is not a blocker.

## Required final state

- The three active-lane rows name the session they need rather than saying
  "Nothing".
- `40-parked/` and `50-blocked/` rows are unchanged; they already name what they
  need.
- No card changes lane, status, `blocked_by`, or `parked_on`.
- The caveat paragraph stays. It carries the reasoning; the table carries the
  answer.

## Non-goals

- Do not move any card to `50-blocked/`.
- Do not touch anything under `src/`; see the dev-binary hold in the board
  README.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
git diff --check
```

## Result — 2026-08-19

The three rows now name the session they need. [[BPR-0256]]'s row gained the one
fact a reader most needs from this table today — that it is the only root any
session can advance — because the table's job is to say where work can start.

| claim | surface | command | proves it | result | evidence |
|---|---|---|---|---|---|
| the table still matches the lanes | `scripts/verify_documentation.py` | `python3 scripts/verify_documentation.py --root .` | `verify_frontier_roots` fails on an omitted card, a wrong lane cell, a duplicate, or a count that disagrees | `DOCUMENTATION_OK` | this commit |
| the documentation contract suite still passes | `tests/scripts/verify_documentation_tests.py` | `python3 tests/scripts/verify_documentation_tests.py` | 94 cases including the frontier-derivation cases [[BPR-0399]] added | 94 tests, OK | this commit |
| no card moved | Git | `git diff --stat` over `docs/issues/` | only the README's third column and this card changed; no file left its lane | no lane change | this commit |
| no whitespace damage | Git | `git diff --check` | it fails on trailing whitespace and conflict markers | clean | this commit |

### The limit of this repair

The verifier derives which cards appear and under which lane. It cannot check
whether the "Needs" cell is true, and this card does not make it able to — the
cell is prose about what a root is waiting for, and there is no mechanical
source for that. So this correction is of the same kind as the ones it follows:
found by reading a claim against what the board actually knows, not by a gate.
That the same column drifted twice in three days ([[BPR-0399]], and now this) is
the argument for reading it that way again.
