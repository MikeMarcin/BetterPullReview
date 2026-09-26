---
schema: "implement-rfc.card.v1"
id: "BPR-0972"
title: "Verify application-wide active-set and changed-set scaling"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693","milestone_plan":{"id":"bpr0972-active-changed-scaling","path":"docs/rfcs/plans/bpr0972-active-changed-scaling/milestone-plan.json"}}
---

# Verify application-wide active-set and changed-set scaling

## Transferred obligation

Original RFC0013 architecture-instrumentation acceptance row 9 beyond recorded bounded paths.

Owner amendment r4 on 2026-09-21 transfers this work from RFC0013 terminal
acceptance. The [amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its provenance. This card is unfinished; no observation is supplied by
the transfer and it creates no RFC0014 release qualification.

Existing diff index/cache/history, host identity and Qt loading/selection evidence demonstrates bounded behavior at named paths. It does not prove blanket active-set/changed-set scaling over the entire application. BPR-0940 separately retains composed GitHub/window layout failures; this card must not treat green internal helpers as whole-application proof.

## Acceptance

- AC1: Map routine selection, receipt, conversation, queue, overview, epoch and source operations to actual public entry points and retained owners. Identify full scans, resets, synchronous I/O and untagged completions with concrete evidence.
- AC2: Add focused structural counters or direct negative controls only where current evidence can miss a named claim; verify bounded updates scale with active plus changed data, completion is serialized and stale/cancelled work cannot publish.
- AC3: Exercise representative work under concurrent background loading and after close/reopen; report exact source, data sizes, visited records, allocation/reset counts and platform limits, with BPR-0967 owning hardware latency/memory budgets.

## Continuation

Start with the inventory or diagnosis above on a separately assigned exact
candidate. Reuse relevant passing evidence with its actual input limits.
Use safe fixtures; no live account, private-target access, remote mutation or
release is authorized by this card. Required native or human evidence remains
pending until actually observed.

## Candidate source review and composition reseal — 2026-09-26

Implementer self-review binds `a2fded0cd482aaaaaee0399764b60145cd4938ac` through
`90e21d0c`, with product candidate `2cb4c2be`. The baseline LF-normalized full
application hash exactly matches the prior pin. `git log` across every production
app manifest source shows these pinned-scope changes only: `bcb982da`, `c55456e4`,
`0a8f9d79`, `9ed1ce7d`, `10b96b31`, `2cb4c2be`. The other changed app files are
fixture-only access/provider support outside that production manifest.

The full composition diff was reviewed. Retained receipt revisions and named
invalidations are conservative on unknown/overflowed changes; exact profile,
account, repository, PR and identity-generation checks remain before sibling
publication. The writer's own notification skips reads already committed by its
host. Selected-material refresh also retains generation/file/comparison-digest
checks. Cache state is dropped on provider acceptance or ledger retirement.
Conversation indexes preserve duplicate-identity fallback and full-observation
semantics. Queue deltas retain durable membership, typed provider completion and
full attachment/lag replay; notification authority checks remain. Same-head epoch
catalogue updates change only retained header facts; changed heads and unresolved
changed-since-review observations keep complete projection. Indexed restoration,
tab and link membership checks preserve the prior accepted-order membership test.
The read-only fixture counters do not drive a host. The existing consent helper's
`exact_current` to `exactCurrent` rename changes no check or branch.

No credential, vault, transport capability, trust, mutation consent or release
behavior changes. The production mutation-branch hash remains
`a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.
The reviewed full-source pin is
`d6f8e7eb70525e9845aa6a3952f03b3c2b826b7747d83ae0a28436d2b02252ae`;
the production manifest pin is
`c107944197417caf4487fad1e3432e0ffd5e343eede211cf6b0be088d5674fd5`.
Printing/resealing these hashes is not test evidence. Final integration remains
required before the card is finished.

The [operation inventory and measured evidence](../../verification/bpr0972-active-changed-scaling.md)
and [eight compiled negative controls](../../verification/bpr0972-negative-controls.json)
retain exact bounds and source hashes. “Active” includes the complete accepted
PR's compact navigation order: receipt deltas project changed file data but still
maintain dense positions. This is O(active order + changed data), not a claim of
O(changed data) alone. [BPR-1211](../10-backlog/BPR-1211-move-review-ledger-reads-and-writes-off-the-decision-lane.md)
retains the named synchronous ledger I/O gap; no whole-architecture or no-I/O
acceptance is implied. Windows/Linux native remain deferred; BPR-0967 owns
optimized hardware latency/memory budgets.


### Exact-material reload failure amendment

The follow-on self-review identified a concrete failure path: a sibling's failed
SQLite receipt reload was treated as absent comparison material, restoring a
withheld row while retaining the old receipt cache. A later equal receipt could
then leave that row visible. Selection itself already clears the receipt cache;
that pending-selection route was not the defect. The correction makes the
conservative restoration the `else` of exact-material admission, retaining the
last accepted receipt/order when the exact reload fails. Authority, selected-file,
generation, material and ownership guards are unchanged.

The public sibling course temporarily renames only its own disposable SQLite
receipt table after a public writer commit, before queued sibling delivery. The
old production binary fails `failed exact-material reload preserves accepted
receipt and withheld order` (74.74 seconds). It then restores the table and checks
that the next equal accepted receipt leaves the correct order in place. No
production fault-injection seam or account/store outside the fixture is used.
The reviewed six-line composition change updates the full-source pin to
`08bb28a3ed661125fbfd01222b530e3e463b833b818949e970470b6b67cd3b95`
and manifest pin to
`bb2924c4d4c05fb6e0b498ac15db1068c3d622fd76af5216ab1b6b7c7a23ff50`;
the mutation branch pin remains unchanged. The repaired public course passes in 80.51 seconds, including the later PR
replacement. The composition contract and strict application/test tidy pass. The
fixture restores the captured canonical schema and rows transactionally; a simple
rename-back was correctly refused on the later open because it rewrote SQL
quoting. Final integration remains required.
