---
schema: "implement-rfc.card.v1"
id: "BPR-0667"
title: "Public preview cannot open a merged pull request"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---


# Public preview cannot open a merged pull request

The owner opened `microsoft/terminal#20607` through Public preview in the
signed bundle on 2026-09-07 at 17:47. The pull request loaded, and the review
history did not. The diagnostics log records the epoch reaching the acquire and
stopping there:

```
epoch_stage=smart_acquire    outcome=succeeded
epoch_stage=protocol_acquire outcome=failed failure_domain=protocol failure=oid_mismatch
epoch_stage=revision_prepare outcome=failed failure_domain=epoch    failure=unavailable
```

The smart acquire succeeding is [BPR-0663](../40-parked/BPR-0663-public-preview-refuses-the-epoch-smart-acquire.md)'s
fix working; this is the next stage. That pull request is merged and its head
branch `fix-soft-font-reset` no longer exists on the remote, so a fetch that
names the branch cannot return the head the pull request carries. A merged or
closed pull request is the ordinary case for a reviewer catching up, not an
edge, and today it produces a workbench with nothing in it.

## Scope

- Establish what `protocol_acquire` asked for and what it got before changing
  anything: the oid it named, the ref it named, and what the remote advertises
  for that pull request. `refs/pull/<n>/head` and `refs/pull/<n>/merge` remain
  after a branch is deleted; a branch name does not.
- Make the anonymous acquire name a ref the remote still advertises for the
  pull request, and keep the exact-oid check that `oid_mismatch` exists to
  make.
- A composed check over a recorded merged pull request whose head branch is
  gone, so the case cannot silently return.
- Owned paths: the epoch acquire in `src/adapters/git`, the anonymous
  factories in `src/app/src/github_application.cpp`, their tests.

## Acceptance

- A merged public pull request opened through Public preview shows its updates
  on the epoch line and an exact comparison, and the log records
  `protocol_acquire` succeeding.
- The exact-oid refusal still fires when the remote really does return a
  different object.
