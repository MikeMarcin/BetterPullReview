---
id: "RFC-BPR-0059"
title: "Clean PR Publication and Local Cycle Continuity"
status: "Auto-Verified"
area: "BPR"
milestone: "Review Cycles"
depends_on: ["RFC-BPR-0053", "RFC-BPR-0054", "RFC-BPR-0056", "RFC-BPR-0057", "RFC-BPR-0002", "RFC-BPR-0008", "RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041", "RFC-BPR-0042"]
pack_version: "review-cycles-v1"
metadata: {"pack": "bpr-review-cycles-v1", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# RFC-BPR-0059: Clean PR Publication and Local Cycle Continuity

## Outcome and decisions

Allow a finished cycle to become a real PR through explicit clean publication. The native v1 output is one squashed commit on a new branch, not destructive manipulation of the developer's branch or a rewrite of the cycle's local evidence. Inspection baseline R0, final local Rn and outgoing Git parent/target are separate identities.

Construct a complete outgoing candidate over the verified base tree, incorporating only selected content. Show preexisting work, chosen cycle improvements, omitted/unavailable scope and any preparation transformations. Module-only capture must never become implicit deletion of the rest of a repository. Changed LFS/filter/signature requirements without a qualified native path remain a visible capability limitation.

Add a narrow GitHub Git-data REST object/tree/commit/new-ref publisher and PR-creation capability over existing admitted HTTPS authority; the credential-free installed Git adapter remains local plumbing. The exact plan, permissions, target and attribution are previewed and separately authorized. Do not bypass native/enterprise policy with arbitrary git/gh commands.

Journal the non-atomic sequence and recover read-first after uncertainty. Verify branch/PR identity and actual head/base; no duplicate creation, blind retry, force push or pretend rollback. Preserve private cycle history, associate the actual PR and continue with GitHub as shared authority. Optional Review Tours reuse ordinary GitHub comments; local feedback history is not automatically published.

## Normative contracts

The detailed contracts [PUBLICATION.md](assets/bpr-review-cycles-v1/specs/PUBLICATION.md) are normative parts of this proposal. The common [domain](assets/bpr-review-cycles-v1/specs/DOMAIN.md), [decisions](assets/bpr-review-cycles-v1/supporting/DECISIONS.md) and [security boundary](assets/bpr-review-cycles-v1/specs/SECURITY-AND-PACKAGING.md) apply. If a live predecessor differs, record a specific compatibility decision; do not quietly implement a weaker alternate workflow.

## Acceptance criteria

- **RC59-01:** Distinguish R0, Rn and publication parent/target and visibly handle preexisting dirty work, selected improvements and base drift.
- **RC59-02:** Candidate/squash preparation uses isolated objects/index and never resets, stashes, cleans, commits or rewrites the user checkout/branch.
- **RC59-03:** Outgoing complete trees preserve untouched base entries; partial scopes, deletions, selected dependencies and empty candidates are handled explicitly.
- **RC59-04:** Bytes/modes/object format/filter/LFS/signing/protected-path requirements are preserved or capability-blocked with truthful external preparation.
- **RC59-05:** Native object/new-branch/PR creation is a separately admitted finite GitHub capability with exact host/account permissions and no implicit credential-helper push.
- **RC59-06:** Exact candidate/destination/title/body/attribution preview plus current authorization is required; edits or changed authority invalidate the plan.
- **RC59-07:** Object/ref/PR uncertainty, cancellation and concurrent drift recover without duplicate PRs, blind retries, force updates or unearned rollback claims.
- **RC59-08:** Squashing outgoing code and publishing retain every pinned local revision, round, response and verification until explicit history removal.
- **RC59-09:** Link the actual PR and reuse existing PR/tour/publication semantics without uploading private history or fabricating GitHub review events.
- **RC59-10:** Native supported-host publication has designated live evidence; external publication/linking stays usable but is not misreported as native qualification.

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
