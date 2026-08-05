---
id: BPR-0169
title: Confirm observer enrollment leaves GitHub's reviewer state untouched
status: Backlog
rfc: RFC-BPR-0010
depends_on: [BPR-0142]
---

# Confirm observer enrollment leaves GitHub's reviewer state untouched

That BetterPullReview's correlator *cannot* issue a request is structural and
provable here: it holds no provider, so there is no path. That GitHub's own
reviewer and subscription state is also unchanged is a different claim, and it
needs a server to answer.

## What is missing

The designated repository and write-scoped token from BPR-0142, plus a pull
request in it the user is neither author nor reviewer of.

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
