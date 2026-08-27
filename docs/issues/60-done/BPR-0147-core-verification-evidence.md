---
id: BPR-0147
title: "Core verification evidence: what it may and may not conclude"
status: Done
rfc: RFC-BPR-0009
depends_on: [BPR-0146]
---

# Core verification evidence: what it may and may not conclude

See the RFC-BPR-0009 verification plan for this slice's place in the sequence.

The rule this RFC exists to protect: **a nearby code change is evidence, not a
verdict.** Evidence reaches `needs verification` and never `verified`. Only an
explicit reviewer intent verifies, and this slice must not open a path around
that however convenient it would be.

## Required direct evidence

- The behaviour this slice owns is exercised directly, not inferred from a
  neighbouring slice passing.
- No path through this layer reaches `verified` without the explicit reviewer
  intent.
- Every refusal carries its own typed reason.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## Outcome

`observe` records evidence against a defect. The **only** state it can produce
is `needs_verification`. There is no argument, no accumulation, and no special
kind that reaches `verified`: a reviewer verifies, or nobody does.

### Every kind, from every state, tested

The test drives all three evidence kinds from all three states that can receive
them and asserts each reaches `needs_verification` and none reaches `verified`.
That is nine cases rather than one illustrative example, because the failure
this guards against is a special case being added later for one kind.

### The case most likely to be wired wrongly

`thread_resolved_externally` is the sharpest. Someone resolving the thread in
GitHub is shared state changing, and it is exactly what a verification looks
like from outside — which is precisely why it must not verify. The reviewer who
owns this defect has said nothing.

Breaking this so an external resolution verifies is the most plausible
"helpful" change anyone would make here, and it fails immediately.

### Evidence never disturbs a decision

Evidence against a `verified`, `withdrawn`, or `accepted_as_is` defect changes
nothing — and that is not an error. Things happen near settled defects.
Reopening is a separate explicit action, and letting a late reply undo a
reviewer's judgement would be evidence overruling the person it exists to
inform.

Further evidence on a defect already awaiting the reviewer changes nothing
either. More evidence does not ask harder.

### The intended path still works

A reviewer may verify what evidence flagged — that is what evidence is *for*.
The verification is theirs, not the evidence's, and the test shows the sequence
completing.

### Non-vacuity

Two behaviours were broken separately, each failing its own assertion, then
restored to green:

- An external resolution verifying — "evidence asks the reviewer to look".
- Evidence reopening a verified defect — "evidence leaves a decided defect
  exactly as it was".

### A tidy finding worth keeping

`clang-tidy` rejected the first version for two consecutive identical switch
branches. It was right: `needs_verification` and the three decided states were
written apart for their comments but did the same thing.

They are now one branch with both reasons in the comment. The distinction is
real — a defect already awaiting the reviewer is already asking, and a decided
defect has had a decision taken — but it lives in the reasoning rather than in
the code, and pretending otherwise was duplication.
