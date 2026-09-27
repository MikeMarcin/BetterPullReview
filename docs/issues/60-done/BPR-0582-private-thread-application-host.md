---
schema: "implement-rfc.card.v1"
id: "BPR-0582"
title: "Implement the private thread application host and durable side-chat/finding ingestion"
status: "Done"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0580","BPR-0581"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
acceptance: [{"criterion":"Reviewer prompts and run admission commit atomically before process dispatch; persistence failure launches nothing.","outcome":"pass","revision":"1aba50e3bf72a3d4523a58572cbe79d8e814a9cc","evidence":{"kind":"observation","detail":"Peer SQLite observes committed prompt/run at existing process entry; admission fault leaves neither and starts no process. Focused integration 10/10; final affected 4/4; real child 1/1; final host 1/1; commands and limits in docs/verification/RFC-BPR-0023.md M6. M8 UI and M9/M10 acceptance remain allocated."}},{"criterion":"Side chat dispatches through the existing invocation host and complete correlated responses become agent-authored turns.","outcome":"pass","revision":"1aba50e3bf72a3d4523a58572cbe79d8e814a9cc","evidence":{"kind":"observation","detail":"Existing invocation process port and actual child canary retain full assistant items once, exact CR/LF and Unicode, item boundaries and immutable run actor across reopen. Focused integration 10/10; final affected 4/4; real child 1/1; final host 1/1; commands and limits in docs/verification/RFC-BPR-0023.md M6. M8 UI and M9/M10 acceptance remain allocated."}},{"criterion":"Cancellation/failure/timeout/stale/duplicate/late completions produce typed outcomes and cannot create false complete turns.","outcome":"pass","revision":"1aba50e3bf72a3d4523a58572cbe79d8e814a9cc","evidence":{"kind":"observation","detail":"Explicit cancellation wins over late success; failed/cancelled/timed-out bodies remain incomplete; exact original success persists before stale UI. Foreign run, malformed metadata/body/finding terminalize protocol failure, duplicate terminal conflicts and storage failure/recovery never claim success or rerun. Focused integration 10/10; final affected 4/4; real child 1/1; final host 1/1; commands and limits in docs/verification/RFC-BPR-0023.md M6. M8 UI and M9/M10 acceptance remain allocated."}},{"criterion":"Validated findings are snapshotted under run + ordinal + digest and can be linked into threads without lossy anonymous promotion.","outcome":"pass","revision":"1aba50e3bf72a3d4523a58572cbe79d8e814a9cc","evidence":{"kind":"observation","detail":"Full retained finding is addressed by run/ordinal/digest; explicit reply preserves finding subject and dependency edge; selected finding dispatch and reopen pass. Focused integration 10/10; final affected 4/4; real child 1/1; final host 1/1; commands and limits in docs/verification/RFC-BPR-0023.md M6. M8 UI and M9/M10 acceptance remain allocated."}},{"criterion":"Private source selection carries only exact IDs; the host reloads/revalidates them at dispatch and refuses missing/edited/duplicate/cross-review/over-limit sources.","outcome":"pass","revision":"1aba50e3bf72a3d4523a58572cbe79d8e814a9cc","evidence":{"kind":"observation","detail":"Only exact store-reloaded IDs enter launch; deleted/edited/duplicate/cross-review/over-limit and old note input refuse. Final source claim blocks a peer edit until input acknowledgement and cancelled claims send no private bytes. Focused integration 10/10; final affected 4/4; real child 1/1; final host 1/1; commands and limits in docs/verification/RFC-BPR-0023.md M6. M8 UI and M9/M10 acceptance remain allocated."}},{"criterion":"Session catalog pruning leaves referenced run provenance readable.","outcome":"pass","revision":"1aba50e3bf72a3d4523a58572cbe79d8e814a9cc","evidence":{"kind":"observation","detail":"Two runs on continued session remain distinct and readable after real ledger catalog pruning beyond 128 rows and SQLite reopen. Focused integration 10/10; final affected 4/4; real child 1/1; final host 1/1; commands and limits in docs/verification/RFC-BPR-0023.md M6. M8 UI and M9/M10 acceptance remain allocated."}},{"criterion":"Bounded deltas update only changed thread/turn/provenance identities.","outcome":"pass","revision":"1aba50e3bf72a3d4523a58572cbe79d8e814a9cc","evidence":{"kind":"observation","detail":"Admission/terminal emit bounded changed identity deltas; finding-reply delta retains terminal run outcome. Full bodies stay behind paged store queries; recovery uses 16-row/256 KiB pages. Focused integration 10/10; final affected 4/4; real child 1/1; final host 1/1; commands and limits in docs/verification/RFC-BPR-0023.md M6. M8 UI and M9/M10 acceptance remain allocated."}}]
---

# Implement the private thread application host and durable side-chat/finding ingestion

## Objective

Coordinate deterministic private-thread decisions with durable storage and the existing agent invocation host.

## Required final state

- Reviewer prompts and run admission commit atomically before process dispatch; persistence failure launches nothing.
- Side chat dispatches through the existing invocation host and complete correlated responses become agent-authored turns.
- Cancellation/failure/timeout/stale/duplicate/late completions produce typed outcomes and cannot create false complete turns.
- Validated findings are snapshotted under run + ordinal + digest and can be linked into threads without lossy anonymous promotion.
- Private source selection carries only exact IDs; the host reloads/revalidates them at dispatch and refuses missing/edited/duplicate/cross-review/over-limit sources.
- Session catalog pruning leaves referenced run provenance readable.
- Bounded deltas update only changed thread/turn/provenance identities.

## Non-goals

- A second agent process port
- Implicit whole-thread context inclusion
- Automatic follow-up or rerun

## Completion evidence

- Fake store/process/manual executor tests
- Ordering trace proving commit-before-launch
- Reordering/cancellation/staleness matrix
- Private-context child-input canary
- Session-prune restart test

## Documentation impact

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04 with implementation deferred at that time. Owner mandate
r1 on 2026-09-26 now authorizes implementation, plan revision and milestone
integration. The [live audit](../../verification/RFC-BPR-0023-live-audit.md)
supersedes stale baseline assumptions; required acceptance is unchanged. Follow
the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
