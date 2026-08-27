---
id: BPR-0468
status: In Progress
title: "The native HTTPS verifier grew three scenarios and \"current\" stayed put"
rfc: RFC-BPR-0002
depends_on: []
---

# The native HTTPS verifier grew three scenarios and "current" stayed put

`tests/scripts/verify_native_https_transport.py` runs 35 scenarios in its
`default` suite. Eight places across the live documentation call it the
*current* 32-scenario verifier, including three cells of
`docs/verification/RFC-BPR-0002.md` — the ledger [[BPR-0035]] will close
RFC-BPR-0002 against.

The interesting part is that not one of those sentences was wrong when it was
written. All of them landed in `3c0dab4c` on 2026-07-30, and the tuple held
exactly 32 entries that day:

| Revision | Date | `SCENARIOS` |
|---|---|---|
| `f5390ad5` | 2026-07-30 | 15 |
| `14eb3dd3` | 2026-07-30 | 31 |
| `3c0dab4c` | 2026-07-30 | 32 |
| `0f6bc421` | 2026-08-10 | 32 |
| `bbc50e49` | 2026-08-12 | 35 |
| `HEAD` | 2026-08-27 | 35 |

`bbc50e49` is [[BPR-0263]], disambiguating indeterminate comment recovery. It
added `mutation-witness`, `mutation-witness-failure`, and
`mutation-proxy-no-fallback` to the tuple because it needed them, which is the
right thing to have done. Nothing asked it to revisit a number written in
seven other files thirteen days earlier.

## Why this is not a repair of the number

The obvious fix — change 32 to 35 — would replace eight true sentences with
eight false ones. BPR-0030 ran a 32-scenario verifier on 2026-07-30 and its
card says so. The RFC-BPR-0002 ledger rows record what that run executed. A
run record is not allowed to acquire scenarios it did not execute, and a done
card is not edited to say something that did not happen.

What is actually wrong is one word. "The complete **current** 32-scenario
verifier passed" was a statement about the present when it was written and is
a statement about the present when it is read, and those are now different
numbers. The figure is historical; the framing says it is not.

So the repair binds each figure to the run that produced it and states the
live count once, where a reader looking for it will be.

This also settles the direction, which matters for closeout: the count grew.
Every 32-scenario claim is satisfied by a 35-scenario verifier that is a
superset of it, so no RFC-BPR-0002 evidence is weaker than its ledger says.
This is an accuracy defect in the record, not a hole in the verification.

## Required final state

- No live document calls a past scenario count *current*. Each figure names
  the run that produced it, or the date it was true.
- The live count appears once, in `docs/verification/fixture-catalog.md`,
  beside the growth that produced it, naming the three scenarios [[BPR-0263]]
  added and the date.
- `docs/verification/RFC-BPR-0002.md`'s three cells state what BPR-0030's run
  executed without asserting it is the present configuration.
- [[BPR-0030]]'s card is not edited. It is a done card recording a run that
  happened, its figure was correct, and this card does not rewrite history to
  make a later sentence read better.

## Non-goals

- No change to `tests/scripts/verify_native_https_transport.py`, to any
  scenario, or to which suite runs where. The code is right; the prose aged.
- No new gate. The previous two cards in this session each earned one because
  the drift class was mechanically decidable from the lanes. This one is not:
  distinguishing "the verifier currently has N" from "the run executed N"
  is a reading, and a checker that could not tell them apart would demand the
  falsification this card exists to refuse.
- No re-run of the verifier. Nothing here changes what it does, and a green
  35-scenario run would not make a 2026-07-30 record more or less true.
