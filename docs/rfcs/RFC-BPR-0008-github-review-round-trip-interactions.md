---
id: RFC-BPR-0008
title: GitHub Review Synchronization and Round-Trip Interactions
status: Proposed
area: BPR
milestone: M3 Collaborative Round Trip
depends_on: [RFC-BPR-0002, RFC-BPR-0003, RFC-BPR-0007]
pack_version: 2
---

# RFC-BPR-0008: GitHub Review Synchronization and Round-Trip Interactions

## Summary

Read and write ordinary GitHub review comments, replies, reviews, file viewed state, and thread resolution while preserving mixed-client interoperability and user identity.


## Motivation

BetterPullReview becomes useful to a team only if its interactions round-trip through the ordinary pull request. The workbench must never create a parallel collaborative protocol.

## Goals

- Synchronize reviews, review threads, replies, issue comments, viewed-file state, and thread resolution.
- Create local drafts and publish ordinary GitHub comments/reviews under the user identity.
- Support Comment, Approve, and Request Changes review submission.
- Preserve mixed-client edits, replies, deletion, resolution, and reopening.
- Add stale-head and target-safety checks to every mutation.

## Non-goals

- Creating or merging PRs.
- Installing a bot, GitHub App, label scheme, status check, or hidden BetterPullReview metadata.
- Automatically resolving comments because code changed.

## Architectural allocation

- **Core:** owns draft/publish semantic state, review command legality, shared/private distinction, target/head revalidation policy, and acceptance/rejection of mutation results. It emits semantic mutation proposals rather than HTTP calls.
- **Host:** owns synchronization, mutation coordination, acting identity, REST/GraphQL execution, idempotency, retries, reconciliation, safe-target enforcement, and conversion of GitHub results into typed events. GitHub remains the authority after every mutation.
- **Qt adapter:** owns editors, pending-review presentation, target/head confirmation, progress, errors, and `QAction` binding. A widget never calls a mutation endpoint directly.
- External-client changes return through the same host synchronization path and core transition flow; no BetterPullReview-only backchannel is required.

## Synchronization

GitHub IDs are canonical. BetterPullReview maintains local projections and refresh cursors but re-reads server state after mutations. Unknown external changes update the local projection; local assumptions never overwrite them without an explicit user action.

## Draft and publish flow

Draft comments may remain local or be represented by GitHub's native pending-review facilities where supported. Publishing presents host, repository, PR, expected head, acting identity, and review disposition. A head mismatch requires explicit revalidation or remapping.

## Acceptance criteria

- A BetterPullReview comment is readable and replyable in the GitHub web UI with no tool-specific marker.
- A comment created or edited externally appears correctly after refresh.
- Resolve/unresolve and viewed/unviewed state use native GitHub capabilities when available and degrade explicitly when not.
- Reviews appear under the authenticated user's identity with ordinary GitHub semantics.
- Mutation retries are idempotent or guarded against duplication.
- No automated test can mutate a non-designated live repository/PR.
- Uninstall, mixed-client, database-loss, and server-truth acceptance tests pass.
- Head movement between draft and publish cannot silently submit against the wrong revision.
- Mutation legality and stale-head policy are core-tested without HTTP/Qt; host safe-target/idempotency tests and Qt draft/action tests prove no provider bypass.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Interaction is ordinary GitHub state | Independent GitHub client fixture | Read/reply/resolve without BetterPullReview present |
| External edits round-trip | Sync integration | Edit in second client and refresh workbench |
| Acting identity is correct | Safe test target or signed fixture | Compare review author/account to active profile |
| Wrong target is impossible | Mutation coordinator | Alter host/repo/PR/head between prepare and execute |
| Mutation path cannot bypass host | Mutation trace and Qt tests | Every publish/resolve action follows core proposal -> host coordinator -> result event |

## Documentation impact

Review/comment/thread mapping, drafts, publish semantics, identity, stale-head behavior, retries, capability fallback, safe test policy, and mixed-client contract are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
