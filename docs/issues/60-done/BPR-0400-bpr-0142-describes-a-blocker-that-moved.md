---
id: BPR-0400
title: BPR-0142 describes a blocker that moved twice
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# BPR-0142 describes a blocker that moved twice

[[BPR-0142]] says of its own blocker:

> [[BPR-0235]] separately owns the CTest launcher boundary and remains in
> progress until Linux and Windows are proved target-native and the only live
> exception enforces the now-designated pull-request identity before child
> creation.

Two of those clauses have since stopped being true, and they moved in opposite
directions.

`BPR-0235` is in `10-backlog/`, not an active lane — [[BPR-0358]] moved it there
when the Windows host arrived and `50-blocked/` stopped applying. And its Linux
half is not outstanding: [[BPR-0156]] re-established the Ubuntu 26.04 x86_64
matrix and BPR-0358 records the Linux criterion as discharged on 2026-08-17. Only
the Windows half remains.

The sentence is a dependency description on a card that cannot start until that
dependency clears, so it is the first thing anyone picking up BPR-0142 reads
about why they cannot.

## How it survived two sweeps

Worth recording, because it is a property of this board's files rather than of
this one card. [[BPR-0399]] repaired eight lane claims in `docs/issues/README.md`
and deliberately did not machine-check prose. A later sweep for the same class of
error across all 70 open cards missed this line twice, both times because the
scan was line-oriented and these cards are hard-wrapped: `[[BPR-0235]]` sits on
one line and `remains in progress` on the next. Re-running it over
whitespace-normalized text found it immediately.

Across all 70 open cards that sweep then found exactly one real mismatch — this
one. The only other hit is `BPR-0083-close-rfc-0004.md`'s closeout row, which
reads "Verify BPR-0054 is in `60-done/`"; that is an instruction to check at
closeout rather than a claim, and the same row's live-blocker cell correctly says
BPR-0054 remains open. It is correct as written.

That result is why BPR-0399's non-goal stands: one stale sentence in seventy
cards does not justify pinning prose.

## Required final state

- BPR-0142's blocker sentence names BPR-0235's actual lane and does not present
  the discharged Linux criterion as outstanding.
- No other card text changes. This is a description repair, not a re-scoping of
  BPR-0142 or BPR-0235.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
ctest --test-dir build/core-only --output-on-failure \
  -R '^(documentation_structure|documentation_issue_graph_contract)$'
```

## Result — 2026-08-19

The sentence now names `10-backlog/` and states that only the Windows half
remains, citing [[BPR-0156]] and [[BPR-0358]] for the discharged Linux
criterion. Nothing else in BPR-0142 changed, and neither card's scope moved.

Re-running the normalized-text sweep afterwards returns no real mismatch across
the 70 open cards. Three hits survive and all three are the scan's own artefacts,
checked individually rather than waved off:

| hit | why it is not drift |
|---|---|
| `BPR-0083:214` "Verify BPR-0054 is in `60-done/`" | a closeout-checklist instruction to check at closeout, not a claim; the same row's live-blocker cell correctly says BPR-0054 remains open |
| `BPR-0083:79` "BPR-0054/RFC-BPR-0003 also remains a hard canonical closeout dependency, so **this card** stays in backlog" | "this card" is BPR-0083, which is in `10-backlog/`; the pattern mis-attributed the clause to the nearest preceding id |
| this card, twice | it quotes the defect text it exists to describe |

`DOCUMENTATION_OK`; `documentation_structure` and
`documentation_issue_graph_contract` pass 2/2; `git diff --check` clean.

A pattern that mis-attributes a clause to the nearest preceding id is not a
verifier, which is the other half of why [[BPR-0399]] declined to pin prose. The
scan is a one-off measurement, not a gate, and is deliberately left uncommitted
as tooling.
