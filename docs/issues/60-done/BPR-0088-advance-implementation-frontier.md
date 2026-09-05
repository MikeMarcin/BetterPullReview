---
schema: "implement-rfc.card.v1"
id: "BPR-0088"
title: "Advance the implementation frontier past RFC-BPR-0004"
status: "Done"
rfcs: ["RFC-BPR-0004"]
depends_on: ["BPR-0082"]
---

# Advance the implementation frontier past RFC-BPR-0004

RFC-BPR-0002, RFC-BPR-0003, and RFC-BPR-0004 are implemented and verified on
available targets but cannot close, because closure is strictly ordered and
RFC-BPR-0002 retains real Windows x64 evidence in BPR-0036. Every later RFC
inherits that block.

The blocker to starting RFC-BPR-0005 was policy rather than capability. The
canonical index already permitted "later-RFC work on available targets", and
`CONTRIBUTING.md` already deferred to the index and allowed available-target
implementation to advance past an open real-Windows verification card. Two
places nonetheless forbade it outright: the issue board's "beginning
RFC-BPR-0005 early" clause, and BPR-0083's "Do not begin RFC-BPR-0005 while
this closeout card remains open".

Record the product owner's direction to advance the implementation frontier
past RFC-BPR-0004 through the remainder of the pack on available targets, so
that macOS-runnable work is not held hostage to hardware that is temporarily
unavailable.

## Non-goals

This card does not change any RFC status, waive any Windows claim, permit an
out-of-order `Auto-Verified` transition, or make GitHub Actions an evidence
surface. Closure order is untouched: no RFC becomes `Auto-Verified` before
every listed dependency is `Auto-Verified`.

It also does not weaken evidence standards. Work that cannot be completed on
an available target is recorded on a live issue card naming exactly what is
missing and who must supply it, in one of three deferred classes:
target-native Windows x64 execution, owner-attended checks needing a person at
the keyboard, and external authority such as a designated safe GitHub mutation
target or a signing identity.

## Required direct evidence

- The canonical RFC index, the issue board, and BPR-0083 agree that the
  implementation frontier has advanced and that closure order is unchanged.
- `CONTRIBUTING.md` needs no edit, because its ordering rule already defers to
  the canonical index and already permits available-target implementation to
  advance past an open real-Windows verification card.
- The documentation gate, structural documentation contract, and issue-graph
  contract pass.

## Direct verification

- `docs/rfcs/README.md` now records the advanced implementation frontier for
  RFC-BPR-0005 through RFC-BPR-0014 on available targets, names the three
  deferred classes, and restates that closure remains strictly ordered.
- `docs/issues/README.md` no longer carries the "beginning RFC-BPR-0005 early"
  prohibition and states the same deferral discipline.
- `docs/issues/backlog/BPR-0083-close-rfc-0004.md` records that the frontier
  advanced while its own closure still requires RFC-BPR-0003 and therefore
  real Windows x64 evidence.
- `CONTRIBUTING.md` was read and deliberately left unchanged: its rule already
  defers to the canonical index and already allows available-target
  implementation to advance past an open real-Windows verification card, so an
  edit would have been redundant rather than clarifying.
- `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`; `documentation_structure` and
  `documentation_issue_graph_contract` pass 2/2.

## Result

The implementation frontier now advances past RFC-BPR-0004 on available
targets, with closure order, evidence standards, and every Windows claim
untouched. BPR-0088 is complete.
