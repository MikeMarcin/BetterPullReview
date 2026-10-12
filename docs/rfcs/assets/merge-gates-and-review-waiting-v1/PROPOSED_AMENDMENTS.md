# Proposed amendments to the adopted provider, Overview and Home contracts

This proposal accompanies the three draft RFCs; it edits no live requirement,
adopted plan, historical Done card or acceptance record. Adopt and reconcile
it only during authorized implement-rfc execution. Source identities below
were read on 2026-10-11 and refer to document bytes, not an execution baseline.

## Changes

| Live source and affected contract | Before | Proposed change and preserved obligations |
|---|---|---|
| [RFC-BPR-0002, Provider design](../../RFC-BPR-0002-github-host-auth-capability-layer.md#provider-design) | The original text limits GraphQL to review-thread and viewed-state features. | Permit narrowly compiled read queries for check requiredness/evaluated results, outstanding requests including code-owner origin and actor identity, PR review/merge/auto-merge observations and target-branch protection needed by the new RFCs. Preserve host/version capability shaping, read admission, bounded pagination, account leases and existing later GraphQL amendments. No general query executor or mutation is authorized. |
| [RFC-BPR-0010, buckets and item explanations](../../RFC-BPR-0010-review-queue-observers-notifications.md#queue-buckets) and [queue architecture](../../../architecture/review-queue.md#attention-buckets) | Authored waiting depends on read requested-reviewer counts; the old prose ties authored recognition to discovery/role facts. | Extend waiting to the accepted unmet-review requirement and preserve observations across lanes. Establish authorship for already enrolled PRs from accepted author/viewer identities. Keep existing attention precedence, enrollment intent, local-only observation and notification privacy. Existing unnumbered acceptance remains historical; new evidence belongs to the new Home RFC. |
| [RFC-BPR-0028, Overview and AC-04](../../RFC-BPR-0028-review-workbench-direction.md) | Participants/sign-off and an aggregate checks item supplement the existing Overview. | Add inspectable check results, outstanding users/teams and a visible merge-requirements explanation. Preserve the files table, progress, modality controls, summary splitter and review entry action. |
| [RFC-BPR-0032, Home and A6–A7](../../RFC-BPR-0032-calm-home-and-accounts-surfaces.md#home), as amended by RFC0066 | Home presents existing attention/section facts with native panels, groups, filters and accessible reasons. | Supply waiting actors and concurrent blockers to the same row anatomy; distinguish teams, people and approvals in summaries. Known blockers are not Quiet-items. Preserve section order, grouping, stored view choices and existing independent native/human obligations. |
| [RFC-BPR-0066, section membership and AC02/AC05](../../RFC-BPR-0066-home-authored-pull-requests.md#section-membership) | Authored rows with the waiting bucket stay in Waiting on others; other authored rows occupy My pull requests. Its section rearrangement adds no provider work. | Preserve that owner-set placement and unread precedence. New observations under this pack correct the input facts and row explanations. The prior “no provider work” claim remains true of the historical section rearrangement; it does not forbid this separately scoped bounded read extension. |
| [Workbench specification](../../../ux/workbench-specification.md) | Documents current Overview and Home presentation. | Adopt the new native disclosures and information hierarchy from the draft RFCs without redesigning the surrounding workbench. |

## Exact source identities

| Source | SHA-256 at inspection |
|---|---|
| RFC-BPR-0002 linked above | `e31bfd2de48c654cd8999bed8bf83916034b2d330825ef44688618ad9093b615` |
| RFC-BPR-0010 linked above | `a53993ef23f89a7d183f146fa5a276fbe3701107d5e575eb149895f5e13338dd` |
| RFC-BPR-0028 linked above | `1927dae8fc4f6d637941a21598fb393739f66033c95fd191c90593edf41597db` |
| RFC-BPR-0032 linked above | `5d219a6d10e7708f03e62068f39fa7492aad51208a225dc72fd5d07a9525c5cc` |
| RFC-BPR-0066 linked above | `c28e8a035f70c11e28ca58e1c75691d0c6a679b2eb6a71771e3580dd56490e17` |
| [Adopted home-authored-v1 plan](../../plans/home-authored-v1/milestone-plan.json) | `a672c3bc3e2cdd93200337dc3fcc665f14ffecdf0d219b428acdd2a52ac9aafb` |
| Queue architecture linked above | `0420363f2d9d8ab2178a617a673d6df23df6539e0c4f68e54996450da2465135` |
| Workbench specification linked above | `408766b9c687e115d7c43da34c57810890c0f76d73109fff1ee2fb6c0991aeeb` |

## Reconciliation and dependencies

The new [plan](milestone-plan.json) adds successor work. It does not amend or
replay the completed `home-authored-v1` plan or the old Overview/calm-Home
milestones. Source drift must be reconciled against the meanings above before
adoption; changing a new plan ID is not a workaround for an adopted-plan
conflict. RFC0066 is an explicit dependency of the new Home scope and was
Auto-Verified when inspected.

Historical cards [BPR-0647](../../../issues/60-done/BPR-0647-pull-request-overview-page.md),
[BPR-0988](../../../issues/60-done/BPR-0988-read-authored-and-directly-requested-pull-requests-of-a-saved-repository.md),
[BPR-0989](../../../issues/60-done/BPR-0989-one-personal-discovery-scheduler-per-profile-with-honest-queue-membership.md),
[BPR-1379](../../../issues/60-done/BPR-1379-populate-home-pull-request-titles-before-opening.md)
and [BPR-1388](../../../issues/60-done/BPR-1388-close-rfc-0066.md) remain Done.
Their tests and evidence are grounding, not acceptance of this new behavior.
No existing attended candidate is selected or superseded by this draft.

The separate [Home draft-chip work](../../RFC-BPR-0067-home-draft-pull-requests.md),
originating in [the earlier draft](../home-draft-pull-requests/RFC-DRAFT-0006-home-draft-pull-requests.md),
was adopted during this authoring turn and its BPR-1389 is In Progress. It
touches some of the same metadata paths. Reconcile concurrent fields without
removing either scope's facts or making that unrelated feature a mandatory
dependency. Existing unrelated release, platform, human and security
obligations remain unchanged.

The next authorized implementer imports provisional IDs, allocates live IDs
through the repository workflow, reconciles the above prose deltas, and binds
new evidence to the new RFCs. No live edits, source baseline capture, card
promotion or implementation approval occurred during this authoring turn.
