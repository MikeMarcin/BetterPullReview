# Current-State Source Audit for RFC-BPR-0022

## Baseline

- Source dump SHA-256: `0c73fc6d9b0c19e89ef43d728ef5d7a3bdc86ddf8cc6e2828ae7abf6a0d257d2`
- Generated: `2026-09-04T11:47:19-07:00`
- Audit purpose: identify existing authorities and the minimum changes needed
  for private review threads, provenance, composition, and publication lineage.

This is supporting material, not a substitute for a live-tree audit on import.
Line numbers identify the supplied baseline and may move.

## Existing authorities and gaps

| Surface | Baseline fact | Consequence for RFC-BPR-0022 |
|---|---|---|
| `src/application/include/bpr/application/review_ledger_port.hpp:75-81` | `local_note_record` has ID, optional file/comparison attachment, and text | Preserve note semantics through migration, but replace the flat anonymous record as private-conversation authority |
| `review_ledger_port.hpp:99-105` | `private_ledger_observation` aggregates all notes and defects for a review | Add paged private-thread ports; do not load transcripts through this aggregate |
| `src/adapters/sqlite/src/review_ledger_database.hpp:26` | Schema version is `5` | Allocate the next live version after import; expected `6` only if the tree has not advanced |
| `src/adapters/sqlite/src/review_ledger_database.cpp` | Canonical schema has `local_note`, agent-session sequence/session, no transcript/provenance/composition tables | Add one normalized authority and atomically migrate legacy notes; avoid permanent dual write |
| `src/core/include/bpr/core/review_round_trip.hpp:60-72` | Draft stores ID, inline semantic anchor, body, state | Add optional durable composition correlation and durable ID allocation; preserve the existing state machine and inline publication semantics |
| `src/app/src/github_application.cpp:5978-5983` | Draft IDs are allocated from process-local `NextReviewDraft` | Replace or bridge with durable allocation so accepted compositions survive restart and lineage is stable |
| `src/core/include/bpr/core/agent_profile.hpp:93-106` | Profile identifies executable/protocol/workspace, not actual run model/effort | Do not treat profile name as runtime proof; introduce requested/configured/reported facts on each run |
| `src/core/include/bpr/core/agent_session.hpp:69-76` | Session records ID, profile, exact revision binding | Keep it as revision-pinned conversation context; add a distinct run ID for each invocation/turn |
| `src/application/include/bpr/application/agent_launch.hpp:346-348` | Decoder internally tracks remote session/thread/turn IDs | Surface them as bounded optional protocol-reported run metadata where available |
| `agent_launch.hpp:381-398` | Run result exposes outcome/events/stderr/exit/protocol failure/findings, no runtime provenance | Extend typed result/completion data; do not rely on raw logs or prose scraping |
| `src/application/include/bpr/application/agent_chat_session_host.hpp:267-296` | Side-chat host owns current session policy and delegates to the one invocation host | Persist transcript around this host; do not add a second launcher or let the transcript decide session policy |
| `src/core/include/bpr/core/agent_finding.hpp:98-117` | Finding retains provider, anchor, analyzed OIDs, claim/evidence/consequence/suggested comment | Snapshot exact validated findings and identify by run + ordinal + digest before using them in lineage |
| `src/application/include/bpr/application/agent_finding_host.hpp` | Current host retains a latest delivery and promotes a finding to one local artifact | Replace lossy promotion with durable source reference and composition lineage while retaining explicit user actions |
| `src/application/include/bpr/application/review_mutation_port.hpp:204-210` | Mutation receipt returns `ServerCommentId` | Record publication lineage after successful mutation and reconciliation without modifying public text |
| `src/adapters/qt/include/bpr/qt/side_chat_panel.hpp` | Side-chat panel presents controls/metadata but no durable transcript model | Add an incremental private-thread transcript and provenance presentation, coordinated with shared conversation navigation |
| `src/adapters/qt` workbench composition | GitHub conversations, draft editor/workshop, agent launcher, side chat, and finding queue share the conversation drawer | Redesign navigation coherently; do not merely stack another unbounded panel |

## Existing behavior to preserve

1. GitHub remains the shared authority; deleting the local database changes no
   GitHub state.
2. Every AI action is explicit.
3. Agent authentication and GitHub access remain independently configured; BPR
   passes no provider or GitHub credential.
4. Private notes/drafts are included in agent context only after explicit
   selection and dispatch-time identity validation.
5. Sessions remain pinned to exact host/repository/PR/base/head/epoch identity
   and become stale rather than retargeting.
6. Finding anchors use RFC-BPR-0004 semantic identities and never measured Qt
   coordinates.
7. Publishing uses RFC-BPR-0008 core proposal -> host coordinator -> GitHub
   adapter -> typed result/reconciliation.
8. Public comments are ordinary GitHub comments under the authenticated user's
   identity and contain no BetterPullReview-only marker.
9. One serialized host completion lane, typed request/session/revision/generation
   identity, cancellation, and stale-result rejection remain in force.
10. Qt remains a presentation adapter and applies bounded incremental
    projections.

## High-risk joins

### Legacy note migration

The old row does not prove who wrote it. Migration must preserve exact bytes and
attachment while assigning `legacy_unknown`. Labeling it `You` would invent
history; labeling it AI would be worse.

### Session versus run identity

The side chat can continue a session for more than one turn. Finding identity or
provenance keyed only to session collides as soon as a second run returns
ordinal zero. A separate durable `agent_run_id` is required.

### Session retention

The current session catalog is bounded and pruned. A private transcript may
outlive those rows. Run provenance must snapshot the exact binding/profile facts
it needs and must not be deleted merely because session catalog retention
expires.

### Draft persistence and publication correlation

The baseline draft is process-local, while private threads are durable. A
composition accepted immediately before restart must not return as source
history with no draft or ambiguous publication state. Durable draft identity
and composition correlation are part of the implementation boundary.

### Remote success followed by local failure

A GitHub mutation may succeed before publication-lineage persistence fails. The
recovery design must use the existing idempotency and server reconciliation
facts to attach the one real server comment; it must never submit a replacement
comment merely to obtain a local record.

### Honest runtime provenance

Some protocols expose model/effort/version facts and others do not. The UI must
show basis-qualified requested/reported values and `unknown`, not one unlabeled
string. A protocol adapter may expose an opaque remote identifier but may not
present it as authentication or proof of a model.

## Live-audit checklist

Before implementation, verify:

- current RFC index/status and whether RFC-BPR-0022 collides;
- current review-ledger schema and all supported migration origins;
- whether RFC-BPR-0008 has made drafts durable or generalized their targets;
- whether new agent metadata already exists in profiles, requests, protocol
  events, or run results;
- whether side chat or findings became durable after this baseline;
- current drawer/workbench information architecture;
- current resource limits and active-set performance expectations;
- current issue board `NEXT_ID`, reservations, and frontier exceptions;
- current documentation gate and platform evidence policy.
