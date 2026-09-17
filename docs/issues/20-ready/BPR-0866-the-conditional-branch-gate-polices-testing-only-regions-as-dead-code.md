---
schema: "implement-rfc.card.v1"
id: "BPR-0866"
title: "The conditional-branch gate polices BPR_BUILD_TESTING regions as code no host compiles"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-sweep-fixes, diagnosing BPR-0854 item 3","found_at":"e84eaba2","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0866: The conditional-branch gate polices BPR_BUILD_TESTING regions as code no host compiles

## Defect

`scripts/verify_conditional_branches.py` exists to catch identifiers left
behind by a rename inside a preprocessor branch **the host does not compile** —
code a compiler will never check. It decides what the host compiles from
`host_macros()`, which returns platform macros only. `BPR_BUILD_TESTING` is not
among them, so every `#if defined(BPR_BUILD_TESTING)` region is treated as dead
code the gate must police.

Those regions are not dead. The `dev` preset compiles the very translation
units that hold them with `-DBPR_BUILD_TESTING=1`, so the compiler already
checks every identifier in them. The gate is doing redundant work on live code,
and doing it with a text scanner's precision.

That is where all eleven of [[BPR-0854]] item 3's findings came from. Every one
sat under `#if defined(BPR_BUILD_TESTING)` in a compiled translation unit. Two
pattern gaps turned that misclassification into concrete false positives —
lambda init-captures, and a call whose name folds onto a same-spelling local —
and both gaps are now fixed. But the misclassification behind them is
untouched: the next construct the scanner cannot parse will produce the next
batch of false positives in exactly these regions, and the next investigation
will start from scratch.

## Scope

Two candidate directions, from the station that found it:

1. **Teach `host_macros()` the build's own definitions.** Most faithful to the
   gate's premise, and hardest: the script has no view of the build's compile
   flags, so it would need them passed in, or would have to infer them, and an
   inference that is wrong in the quiet direction reintroduces the blind spot
   the gate exists for.
2. **Scan `BPR_BUILD_TESTING` regions as live.** Simpler and matches reality
   for every build this project actually runs: the tests-enabled build compiles
   them, and a tests-disabled release build compiles the *other* side of those
   branches, which the gate would still police. The risk is narrow — a
   rename inside a testing-only region in a tree only ever built without
   testing would go unreported by this gate.

Pick one with evidence rather than by preference, and say on this card which
regions the gate then still polices, so the next reader knows what a green
`CONDITIONAL_BRANCH_IDENTIFIERS_OK` does and does not assert.

While there: record the gate's remit plainly somewhere a reader of its output
will find it. It reports a name whose **folded spelling collides with a
declared name**; a typo colliding with nothing is invisible to it. During
BPR-0854 item 3's acceptance, renaming a call to a freshly invented spelling
was correctly not reported, patched or unpatched. That is by design, and it is
easy to misread `OK:695` as "no misspellings exist".

## Acceptance

- The gate no longer reports findings in regions the build compiles, or the
  card records why it must keep doing so.
- The two rules from BPR-0854 item 3 still hold: an init-capture declares its
  name, and a call to a function the file defines is not a rename. Their five
  tests in `tests/scripts/conditional_branch_identifier_tests.py` still pass.
- `conditional_branch_identifiers` passes, and a planted rename in a region the
  gate still polices is shown to be caught.
- The card states what a green gate asserts, including the collision-only
  remit.
