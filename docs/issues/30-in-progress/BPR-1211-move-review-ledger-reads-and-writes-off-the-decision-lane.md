---
schema: "implement-rfc.card.v1"
id: "BPR-1211"
title: "Move review-ledger reads and writes off the decision lane"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_card":"BPR-0972","disposition":"architectural responsiveness gap identified during structural scaling inventory; no latency or whole-architecture acceptance supplied"}
---

# Move review-ledger reads and writes off the decision lane

BPR-0972 measures data visited by routine public gestures and records the existing
ledger's synchronous I/O. A bounded one-row read or write is not proof that the
serialized decision lane remains responsive during SQLite contention or slow
storage. This card retains that separate architectural gap. It does not waive the
application's effect/decision-lane boundary, infer Windows/Linux native evidence,
or replace BPR-0967's optimized hardware latency and memory budgets.

## Observed ownership and calls

`src/app/src/github_application.cpp` composes a window-owned
`review_ledger_application_host` and store in `compose_review_ledger`. Its
`dispatch_receipt` calls `submit` synchronously; `observe_selected_material` calls
`observe_material`; `refresh_shared_ledger` calls `refresh_receipt` after sibling
invalidation. Conversation Mark read calls `mark_conversation_read`. Accepted
Overview and private-context publication call `load_receipt`,
`private_observation` and `observe_conversations`. `retire_review_ledger` destroys
this host and store at authority replacement. Existing exact authority and
file/material guards must survive any scheduling change.

`src/application/src/review_ledger_application_host.cpp` currently calls the store
under its mutex: `observe_material` loads one receipt and may commit a core-decided
carry; `submit` commits one receipt; `refresh_receipt` loads one receipt;
`mark_conversation_read` writes one cursor. `load_receipt` and conversation receipt
reads have retained caches. `private_observation` directly loads the bounded
private-state observation; private note/defect/head/session commands also perform
synchronous store operations. Moving a whole locked method to a worker while the
GUI waits for that mutex would leave the responsiveness gap intact.

The BPR-0972 safe composed course has 515 files, 1,000 foreign-authored threads and
20,000 source lines. Selecting one file loads one receipt; Seen and Mark file read
each issue one persistence request and no receipt read; conversation Mark read
writes one conversation receipt and visits one thread/post. Exact per-gesture
private/conversation load counters and allocation limits are recorded in the
[BPR-0972 evidence](../../verification/bpr0972-active-changed-scaling.md).
These bounded counts say nothing about storage waiting time.

## Acceptance

- AC1: Move admitted ledger I/O off the decision lane with bounded outstanding work. Keep deterministic transitions and immutable typed completion on the serialized lane, and prevent GUI reads from waiting on a mutex held across I/O. Inventory startup, receipt, conversation and private-state calls so a cache hit cannot hide an unconverted miss or invalidation path.
- AC2: Preserve durable-before-publication and FIFO semantics for rapid gestures, exact file/material and profile/account/PR authority, and the lifetime of stores through retirement. A closed/replaced window or stale completion cannot publish into its successor. Sibling invalidation remains typed and changed-data bounded.
- AC3: Hold a fake store operation while independently advancing the decision lane; prove responsiveness, bounded admission, cancellation/late completion refusal, failure rollback, shutdown/drain and same-root recovery. Exercise the real composed receipt/conversation entry points and cold-cache/sibling paths, with direct negative controls for an inline effect or omitted authority check. Report native/platform limits explicitly.

## Continuation

Use safe local fixtures. The reserved allocation receipt is
`docs/issues/_reservations/BPR-1211--BPR-1211--eab9cb4396e7.json`.
No live account, private target, remote mutation, release or push is authorized.
