---
id: "RFC-BPR-0008"
title: "GitHub Review Synchronization and Round-Trip Interactions"
status: "Auto-Verified"
area: "BPR"
milestone: "M3 Collaborative Round Trip"
depends_on: ["RFC-BPR-0002","RFC-BPR-0003","RFC-BPR-0007"]
pack_version: 2
metadata: {"milestone_plan":{"id":"rfc-bpr-0008-owner-closeout","path":"docs/rfcs/plans/rfc-bpr-0008-owner-closeout/milestone-plan.json"}}
---

# RFC-BPR-0008: GitHub Review Synchronization and Round-Trip Interactions

## Summary

Implement ordinary GitHub comment and whole-review publication, bounded
synchronization, explicit capability degradation and exact acting identity.
The owner-amended closeout below separates implemented behavior from deferred
live mixed-client, capability and private-target qualification.


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

Comment and draft reveal requests use RFC-BPR-0004 semantic source-line,
boundary, mapping, or change-group identities and navigate through its shared
comparison controller. Mapping uncertainty remains visible; the UI cannot
attach a comment to an arbitrary neighboring line or independently position
the two top panes.

## Owner amendment r4 — 2026-09-21

The owner directed completion of RFC-BPR-0014 with unfinished external/native
work retained as durable follow-ups. The coordinator explicitly assigned the
same amendment to this blocking canonical predecessor; the
[mandate and bounded plan](plans/rfc-bpr-0008-owner-closeout/mandate-r4.md)
record that scope interpretation. This changes acceptance, not observations.

RFC-0008 closes its implemented core/host/Qt/publication scope using the existing
designated live comment/reply evidence and focused local checks. Three existing
cards remain parked as standalone follow-ups, with their original IDs, paths,
partial decisions and permission boundaries intact:

- [[BPR-0143]] retains live external edit/delete/resolve/reopen, supported
  resolve/viewed capability round trips, local-conflict/server-truth and
  uninstall/database-loss qualification. Its four-action approval is not an
  execution result or authority for the additional state-loss course.
- [[BPR-0144]] retains real Comment, Approve and Request Changes attribution
  on a separately authorized suitable disposable PR/account.
- [[BPR-0641]] retains its unfinished private-PR smoke acceptance. The ordinary
  inline-publication implementation and prior passing local records remain valid.
  (Follow-on, 2026-09-25: the owner accepted that smoke in attended session
  `06ad5734`, and BPR-0641 is Done. The file comparison that did not render on
  the private pull request is [[BPR-1199]].)

These are explicitly outside this RFC's terminal gate; none is marked passed
or Done. Live Enterprise qualification remains skipped under its separate prior
owner direction. The original pack issue plan and dated evidence remain
unchanged. Product, credential, automated-test and remote-mutation boundaries
are not relaxed. An Auto-Verified status under this amendment is not complete
live interoperability, native-package or release qualification.

## Acceptance criteria

- A BetterPullReview comment is readable and replyable in the GitHub web UI with no tool-specific marker.
- Fixture synchronization accepts external creation, edits, deletion, resolution and reopening without duplication or local overwrite; the recorded live web reply returns after refresh.
- Unsupported resolve/unresolve and viewed/unviewed capabilities degrade explicitly in the composed application; supported live capability round trips remain with BPR-0143.
- Comment, Approve and Request Changes bind the selected account and ordinary GitHub review semantics through local wire, provider and public-composition evidence; real-account attribution remains with BPR-0144.
- Mutation retries are idempotent or guarded against duplication.
- No automated test can mutate a non-designated live repository/PR.
- Fixture server-truth reconciliation and durable indeterminate-write recovery pass; live mixed-client, uninstall and database-loss acceptance remains with BPR-0143.
- Head movement between draft and publish cannot silently submit against the wrong revision.
- Mutation legality and stale-head policy are core-tested without HTTP/Qt; host safe-target/idempotency tests and Qt draft/action tests prove no provider bypass.
- Ordinary inline publication retains exact per-gesture authority and its passing local implementation evidence; unfinished private-PR smoke acceptance remains with BPR-0641.

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
