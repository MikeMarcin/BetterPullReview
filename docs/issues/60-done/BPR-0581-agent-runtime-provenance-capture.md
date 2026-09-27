---
schema: "implement-rfc.card.v1"
id: "BPR-0581"
title: "Capture requested and protocol-reported runtime provenance for each agent run"
status: "Done"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0579"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
acceptance: [{"criterion":"Bounded optional requested settings without profile-name inference","outcome":"pass","revision":"8e20e9e850c6f185e1107280a175b3fd8f9cec29","evidence":{"kind":"observation","detail":"Existing invocation host forwards settings; launch separates requested and explicit defaults. Unsupported requested/default settings on JSONL, Claude and terminal refuse before process calls. Commands, capability matrix and limitations: docs/verification/RFC-BPR-0023.md M5 and docs/architecture/ai-agent-boundary.md."}},{"criterion":"Per-invocation identity and immutable dispatch snapshot contract","outcome":"pass","revision":"8e20e9e850c6f185e1107280a175b3fd8f9cec29","evidence":{"kind":"observation","detail":"Validated AdmittedRun snapshots session/profile/display/protocol/binding/entry point/source set; two runs on one session remain distinct through request/stream/result. Missing or changed selected full bodies refuse. Per adopted M5 exit, M6 owns production store-before-dispatch wiring and complete assistant retention. Commands, capability matrix and limitations: docs/verification/RFC-BPR-0023.md M5 and docs/architecture/ai-agent-boundary.md."}},{"criterion":"Only genuinely protocol-reported runtime facts","outcome":"pass","revision":"8e20e9e850c6f185e1107280a175b3fd8f9cec29","evidence":{"kind":"observation","detail":"Documented field-level App Server and Claude reports and JSONL remote thread are captured with exact bases; absent version/session/run facts remain absent. Decoder and real child courses pass. Commands, capability matrix and limitations: docs/verification/RFC-BPR-0023.md M5 and docs/architecture/ai-agent-boundary.md."}},{"criterion":"External and unsupported runtime facts stay unknown","outcome":"pass","revision":"8e20e9e850c6f185e1107280a175b3fd8f9cec29","evidence":{"kind":"observation","detail":"External prose yields no report; JSONL model/effort/version fields remain unknown; absent fields stay absent and no content-self-report is fabricated. Commands, capability matrix and limitations: docs/verification/RFC-BPR-0023.md M5 and docs/architecture/ai-agent-boundary.md."}},{"criterion":"Requested and reported disagreement remains distinct","outcome":"pass","revision":"8e20e9e850c6f185e1107280a175b3fd8f9cec29","evidence":{"kind":"observation","detail":"Protocol loop and child fixtures retain distinct facts with typed conflicts; SQLite decoded-metadata transaction and restart preserve exact facts and three conflicts. Commands, capability matrix and limitations: docs/verification/RFC-BPR-0023.md M5 and docs/architecture/ai-agent-boundary.md."}},{"criterion":"No inference or credential capture","outcome":"pass","revision":"8e20e9e850c6f185e1107280a175b3fd8f9cec29","evidence":{"kind":"observation","detail":"Exact allowlisted child env/argv/stdin canary passes; metadata selects only named fields and owned SQLite files contain no ignored credential-like envelope canaries. Commands, capability matrix and limitations: docs/verification/RFC-BPR-0023.md M5 and docs/architecture/ai-agent-boundary.md."}},{"criterion":"Full partial conflicting malformed oversized stale cancelled and unknown evidence","outcome":"pass","revision":"8e20e9e850c6f185e1107280a175b3fd8f9cec29","evidence":{"kind":"observation","detail":"Focused core/storage 6/6, real child/finding 2/2 and architecture gate pass on final code. 2 MiB raw/8 MiB wire limits, escaping, late-ID refusal, zero-write/start negative controls, foreign-run staleness and partial cancellation facts are covered. Commands, capability matrix and limitations: docs/verification/RFC-BPR-0023.md M5 and docs/architecture/ai-agent-boundary.md."}}]
---

# Capture requested and protocol-reported runtime provenance for each agent run

## Objective

Extend the existing invocation/protocol/result path with honest bounded run metadata while preserving agent credential and process boundaries.

## Required final state

- Invocation requests can carry bounded optional requested model/effort settings without turning profile names into runtime claims.
- Each dispatch receives one run identity and snapshots profile/protocol/binding/entry point/private source IDs.
- Protocol adapters expose model/effort/tool version/remote IDs only when genuinely reported; App Server remote session/thread/turn data is surfaced as typed metadata.
- External-terminal and unsupported protocols cleanly report unknown.
- Requested and reported disagreement survives as two facts and a typed conflict.
- No prose/executable/profile-name scraping or provider credential capture exists.
- Protocol and process fixtures cover full, partial, conflicting, malformed, oversized, stale, cancelled, and unknown metadata.

## Non-goals

- Calling provider APIs directly
- Guaranteeing every agent protocol reports model or effort
- Storing raw environment values

## Completion evidence

- Per-protocol capability matrix
- Decoder/result fixture tests
- Child argv/env/IPC canary
- Secret scan of stored/evidence data

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
