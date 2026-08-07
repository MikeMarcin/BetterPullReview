---
id: BPR-0179
title: "Simplify RFC-BPR-0011 and prepare its closeout"
status: Done
rfc: RFC-BPR-0011
depends_on: [BPR-0171]
---

# Simplify RFC-BPR-0011 and prepare its closeout

The behaviour-preserving pass, then the ledger.

## What this slice did

### Simplification

One removal, and reasoning for two non-removals.

**Removed** the `sizeof`-sum credential guard from
`core_agent_context_supplement`, replaced by a structured binding over every
field. The sum had to be hand-computed with padding — wrong on the first
attempt — and it fails for the wrong reason on a layout change while passing
for a field swapped with one of equal size.

`core_agent_profile` keeps the sum form. Left alone deliberately: it is
committed, it was *proven* to fire on a `Token` field, and rewriting a working
guard to match a sibling's style is churn rather than simplification. The
reasoning is in the ledger so the next person choosing between the forms does
not rediscover it.

**Not removed:** `agent_run_outcome::failed_to_start`, unreached because the
canary always starts — but a wrongly configured executable is a real state, and
deleting it would force the launcher to report failing to start as a
cancellation, which is a different thing to a reviewer. And the
`external_terminal` protocol, unexercised end to end but mandatory: a corporate
harness with no machine-readable interface must still be launchable, or
reviewers are pushed onto a provider their host forbids.

### The ledger

Eight claim-matrix rows moved off `Unstarted`. Each names the card and the
`ctest` name that proves it, and the two that are weaker than the row
originally imagined say so:

- **Every invocation is explicit** is `Verified structurally` rather than by
  interaction. The panel holds no process and the architecture scan enforces
  that the Qt adapter contains no `QProcess`, so a refresh cannot reach one
  because there is none to reach — but the refresh-and-navigate interaction the
  row described is not run, because no composed surface exists to run it
  against.
- **Existing-checkout mode is explicit** notes that the risk wording is shown
  and that no attended review has judged whether it is sufficient.

A `Not delivered` section names what a reader would otherwise have to infer
from silence — chiefly that nothing composes the agent surface, so a reviewer
cannot invoke an agent in the product.

RFC-BPR-0011 stays `Proposed`. Closure order is unchanged: RFC-BPR-0002 still
needs Windows x64 evidence (BPR-0036) and every later RFC inherits that block.

## Required direct evidence

- Each removal is named, with why the delivered design made it unnecessary.
- No claim-matrix row reads Unstarted.
- Every Verified row names the card and the command that proves it; anything
  not delivered is named as not delivered.
- RFC-BPR-0011 stays `Proposed`; no closure claim is made.
