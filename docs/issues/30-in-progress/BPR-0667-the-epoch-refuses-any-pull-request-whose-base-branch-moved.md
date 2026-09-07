---
schema: "implement-rfc.card.v1"
id: "BPR-0667"
title: "The epoch refuses any pull request whose base branch has moved"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P0"
---

# The epoch refuses any pull request whose base branch has moved

The owner opened `microsoft/terminal#20607` through Public preview in the
signed bundle on 2026-09-07 at 17:47. The pull request loaded and the review
history did not. The diagnostics log:

```
epoch_stage=smart_acquire    outcome=succeeded
epoch_stage=protocol_acquire outcome=failed failure_domain=protocol failure=oid_mismatch
epoch_stage=revision_prepare outcome=failed failure_domain=epoch    failure=unavailable
```

`read_git_protocol_v2_ls_refs_response` asks the remote for the pull request's
base ref and for `refs/pull/<n>/head`, and refuses with `oid_mismatch` unless
the advertised oid of each equals the oid the pull request recorded. The head
holds: `refs/pull/20607/head` is `472eb71d08b9331ae6f43e5d0e7e6c2fc44fe6f9`,
which is the head the API reports, and it never moves. The base does not: the
pull request recorded base `c7572cde0c69733e4511787dc963eb336f17adbf`, and
`refs/heads/main` on that repository now points at
`093e49e29a9f806ff83025c49be5d0c970673b00`.

A base branch moves every time anything else merges. This is not a merged-pull-request
case or an edge: on any repository with more than one contributor, the review
history is unavailable for nearly every pull request, within minutes of it
being opened. It is why the workbench the owner reached was empty.

## Scope

- The advertisement says what the remote has now; it is not a promise about
  what the pull request recorded. Stop requiring the base ref's advertised oid
  to equal the recorded base.
- Keep the guarantee the refusal exists for, at the place that can give it:
  the fetch names the exact base and head oids it wants, and the objects
  delivered are verified to be those oids. A server that substitutes an object
  is still refused.
- Decide what the base ref advertisement is still needed for, if anything, and
  say so where the code reads.
- A composed check against a recorded pull request whose base branch has moved
  since, so the case cannot silently return, and a negative that still refuses
  a substituted object.
- Owned paths: `src/application/src/git_protocol_v2.cpp` and its header, the
  epoch host's acquire, their tests.

## Acceptance

- A pull request whose base branch has advanced since it was opened, and one
  that has been merged and had its head branch deleted, both reach an exact
  comparison; the log records `protocol_acquire` succeeding.
- A response that advertises or delivers an object other than the wanted oids
  is still refused, and a test proves it.
