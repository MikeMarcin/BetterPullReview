---
schema: "implement-rfc.card.v1"
id: "BPR-0468"
title: "The native HTTPS verifier grew three scenarios and \"current\" stayed put"
status: "Done"
rfcs: ["RFC-BPR-0002"]
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

- No document that describes present state calls a past scenario count
  *current*. That is the board README, `docs/verification/fixture-catalog.md`,
  and `docs/verification/RFC-BPR-0002.md`: a reader consults them to learn
  what is true now, so a stale "current" in them is a wrong answer.
- The live count appears once, in `docs/verification/fixture-catalog.md`,
  beside the growth that produced it, naming the three scenarios [[BPR-0263]]
  added and the date, and pointing at `SCENARIOS` as the authority rather
  than at another sentence.
- [[BPR-0030]]'s card keeps its own wording, including the word *current* at
  its line 57. This is the one place the rule is deliberately not applied, and
  it is worth stating why rather than quietly exempting it. A done card is a
  dated record of what one session did; its "current" is scoped to the day it
  was written, the way a lab notebook's is. Editing it would make the record
  of the run disagree with the run. The cost is real — a reader who opens
  BPR-0030 alone still meets the stale word — and it is paid deliberately,
  because the alternative is a board whose closed cards get rewritten whenever
  a later change makes their prose read oddly.

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

## What changed — 2026-08-27

Eight sentences, no numbers falsified. Each 32 kept its value and gained the
run or the date that produced it: `extends the current verifier to 32` became
`extended the verifier to 32 scenarios on 2026-07-30`; `the complete current
32-scenario verifier passed` became `passed ... that day`; the ledger's
`BPR-0030's current macOS verifier passed all 32 scenarios` became `passed all
32 scenarios it carried on 2026-07-30 ... BPR-0263 has since taken the suite to
35, so that run is a subset of the current one`.

The live count is stated once, in the fixture catalog beside the growth, and
it points at `SCENARIOS` rather than at itself. That is deliberate: a second
sentence carrying the number is a second thing to keep in sync, which is the
defect this card is repairing.

## Verification

| Claim | Command | Result |
|---|---|---|
| The live default suite has 35 scenarios | `ast.literal_eval` of `SCENARIOS` in `tests/scripts/verify_native_https_transport.py`, and an independent count of quoted entries | 35 by both |
| The figure was 32 when written | `SCENARIOS` counted at every revision that touched the file | 15 → 31 → 32 on 2026-07-30, 32 until `0f6bc421`, 35 from `bbc50e49` on 2026-08-12 |
| The docs were written while it was 32 | `git log -S'32 scenarios'` and `-S'32-scenario'` over the four affected files | all originate at `3c0dab4c`, 2026-07-30 |
| Nothing mechanical asserts 32 | grep for a scenario count in the verifier, tests, and CMake | no assertion; the figure is prose only |
| No stale framing remains | `grep -rn "current 32\|current.*32 scenario" docs/` excluding BPR-0030's card | none |
| The gate passes | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |
| No whitespace damage | `git diff --check` | clean |

The verifier was not re-run. Nothing in this card changes what it does, and a
green 35-scenario run today would say nothing about whether a sentence written
on 2026-07-30 was true.
