---
id: "RFC-BPR-0053"
title: "Review Cycles: Domain, Local Authority and Durable History"
status: "Auto-Verified"
area: "BPR"
milestone: "Review Cycles"
depends_on: ["RFC-BPR-0001", "RFC-BPR-0003", "RFC-BPR-0007"]
pack_version: "review-cycles-v1"
metadata: {"pack": "bpr-review-cycles-v1", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# RFC-BPR-0053: Review Cycles: Domain, Local Authority and Durable History

**Status (2026-10-04): Auto-Verified.** [BPR-1186](../issues/60-done/BPR-1186-close-rfc-0053.md)
judged all ten criteria whole against the cards' recorded evidence, with runs on
macOS arm64 and the Linux and Windows stations; the
[execution ledger](../verification/RFC-BPR-0053-0061.md#bpr-1186-rfc-bpr-0053-closed-2026-10-04)
holds each row and its limits. RFC-BPR-0001, 0003 and 0007 are Auto-Verified. No
owner verdict was needed. Windows compiles out the crash courses, as in every
SQLite contract; no window or command removes a single cycle; profile purge
of a profile that published is refused until BPR-1283.

## Outcome and decisions

Introduce Review Cycles as a first-class local review subject, independent of a PR and of an agent session. A cycle owns exact captured source revisions, versioned feedback, frozen rounds and reviewer judgments. A linked PR is a provider association, not the required root of every record.

Reuse the bounded material/diff interfaces and shared application architecture. Existing PR IDs, authentication and epoch scope stay unchanged. Local authoring must not depend on a fabricated account or on loading authenticated PR caches after their authority has expired. Introduce explicit local profile/workspace scope and typed source subjects at the seam; do not broadly make all old authority fields optional.

The host owns paged durable history and content pins. The core owns finite transitions, strong identities and stale-result decisions. SQLite/content-store recovery must install a complete immutable revision or leave none visible. Freeze, disclosure, implementation authorization, recipient response, reviewer verification and publication are separate state transitions even when the UI combines convenient preparation actions.

This RFC explicitly expands the forward product contract beyond PR-only review. It does not rewrite settled RFC evidence or turn BPR into a hosted review service. Source data not deliberately published remains local; a later GitHub-only recovery cannot reconstruct private local cycles. Keep schema migration, archive/removal and old-profile compatibility documented and tested.

## Normative contracts

The detailed contracts [DOMAIN.md](assets/bpr-review-cycles-v1/specs/DOMAIN.md) are normative parts of this proposal. The common [domain](assets/bpr-review-cycles-v1/specs/DOMAIN.md), [decisions](assets/bpr-review-cycles-v1/supporting/DECISIONS.md) and [security boundary](assets/bpr-review-cycles-v1/specs/SECURITY-AND-PACKAGING.md) apply. If a live predecessor differs, record a specific compatibility decision; do not quietly implement a weaker alternate workflow.

## Acceptance criteria

- **RC53-01:** Create and reopen a local cycle with no PR, remote, Git commit, account or agent, including an empty initial diff.
- **RC53-02:** Persist distinct cycle, revision, item, round, response, verification and publication identities without fake provider IDs or counter reuse.
- **RC53-03:** Use bounded provider-independent material/application ports while preserving the existing PR authority and epoch contracts.
- **RC53-04:** Crash during source/catalog/draft/freeze persistence leaves complete recoverable records or explicit unavailable state, never a partial ready round.
- **RC53-05:** Expected revisions and operation IDs reject stale/conflicting writes and deduplicate identical retries across interfaces.
- **RC53-06:** Core cycle services initialize and operate without agent libraries, provider configuration or executable discovery.
- **RC53-07:** Existing PR profiles, caches, receipts and drafts survive additive migration without fabricated local-cycle ownership or altered historical evidence.
- **RC53-08:** Pinned source/round/verification evidence survives publication and quota pressure; explicit removal previews irrecoverable local history loss.
- **RC53-09:** Scope versions and partial coverage distinguish unknown source from deletion and do not imply project-wide review.
- **RC53-10:** Current product/domain/security documentation records the narrow new local/publication scope and unchanged shared-GitHub authority.

## Implementation and evidence

Use the associated authored cards and imported milestone grouping. New names/types/paths
are proposals until reconciled with the live tree. Preserve functional-core/host/Qt
ownership and existing test isolation. Implement focused production-path unit/host/
native checks; use unchanged predecessor evidence where inputs have not changed.
Source inspection, reference fixtures and this archive's validator are not runtime
implementation, native/platform behavior, live provider acceptance or user approval.

Record command/gesture, result, producing revision, environment and limitations for
each acceptance criterion. Required live or attended observations stay open until
actually performed on authorized targets. Do not start models, alter shared skills,
write live repositories, or bypass corporate policy merely because a test card exists.
Keep historical RFC status/evidence unchanged; update current architecture forward.

See the [integration map](assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md), [acceptance matrix](assets/bpr-review-cycles-v1/verification/ACCEPTANCE-MATRIX.md) and [product verification contract](assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md).
