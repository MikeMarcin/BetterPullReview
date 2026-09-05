---
id: BPR-0169
title: Confirm observer enrollment leaves GitHub's reviewer state untouched
status: Parked
rfc: RFC-BPR-0010
depends_on: [BPR-0142]
parked_on: awaiting-live-github
---

# Confirm observer enrollment leaves GitHub's reviewer state untouched

That BetterPullReview's correlator *cannot* issue a request is structural and
provable here: it holds no provider, so there is no path. That GitHub's own
reviewer and subscription state is also unchanged is a different claim, and it
needs a server to answer.

## What is missing

A disposable pull request that MikeMarcin is neither author nor reviewer of,
plus read access to its reviewer, requested-reviewer, subscription and label
state through an independent client. BPR-0142 is Done and remembered sign-in
is retained. MikeMarcin authored BPR-Test#1, so that PR cannot prove this
criterion. Observer enrollment itself is local and requires no write token.

## Required direct evidence

- Reviewer list, requested reviewers, subscription state, and labels recorded
  before enrollment.
- The pull request enrolled locally by a repository, author, and path rule in
  turn.
- The same four read back from GitHub and compared, field by field, not merely
  eyeballed.
- The comparison run through a second client, so it is GitHub's answer rather
  than this tool's projection of it.

## What must not be done instead

Reading BetterPullReview's own projection back and calling it GitHub's state.
The projection is exactly what would be unchanged if the tool had made a
request it forgot to record.

## Parked checkpoint — 2026-09-05

The implementation dependency is Done. A suitable PR and independent read
surface remain missing; this card is parked awaiting live GitHub evidence.
A disposable PR requested for BPR-0144 may also be suitable, but only if the
actor's nonauthor/nonreviewer state is independently established first.

Read the four server fields before enrollment, apply and remove the local
repository/author/path rules one at a time, and read the same fields after each
course. A missing or unavailable subscription fact is a limitation, not proof
that it stayed unchanged. Preserve sign-in. No GitHub write is required or
authorized by this observer-only check; no Actions, push or paid CI may run.
BPR-0168 retains its accepted native notification evidence and still waits on
this card; no redundant notification approval is requested.
