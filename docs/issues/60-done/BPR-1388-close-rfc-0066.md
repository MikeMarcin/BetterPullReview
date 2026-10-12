---
schema: "implement-rfc.card.v1"
id: "BPR-1388"
title: "Verify and close RFC-BPR-0066"
status: "Done"
rfcs: ["RFC-BPR-0066"]
depends_on: ["BPR-0776","BPR-0783","BPR-0990"]
closeout_for: ["RFC-BPR-0066"]
priority: "P1"
metadata: {"mandate_revision":"home-authored-r1","rfc_base_sha":"b87b7e4ae8f62696c3af88abe3c943eaef65c6f5","reservation":"docs/issues/_reservations/BPR-1388--BPR-1388--5e9c0a9a9497.json","milestone_plan":{"id":"home-authored-v1","path":"docs/rfcs/plans/home-authored-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"a34080e0fbce7d7e8d9fdade436aa5826e21e4a5","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0066.md","detail":"AC01 direct evidence in the criterion ledger; full requirements preserved, candidate content verified."}},{"criterion":"AC02","outcome":"pass","revision":"a34080e0fbce7d7e8d9fdade436aa5826e21e4a5","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0066.md","detail":"AC02 direct evidence in the criterion ledger; full requirements preserved, candidate content verified."}},{"criterion":"AC03","outcome":"pass","revision":"a34080e0fbce7d7e8d9fdade436aa5826e21e4a5","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0066.md","detail":"AC03 direct evidence in the criterion ledger; full requirements preserved, candidate content verified."}},{"criterion":"AC04","outcome":"pass","revision":"a34080e0fbce7d7e8d9fdade436aa5826e21e4a5","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0066.md","detail":"AC04 direct evidence in the criterion ledger; full requirements preserved, candidate content verified."}},{"criterion":"AC05","outcome":"pass","revision":"a34080e0fbce7d7e8d9fdade436aa5826e21e4a5","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0066.md","detail":"AC05 direct evidence in the criterion ledger; full requirements preserved, candidate content verified."}}]
---

# Verify and close RFC-BPR-0066

## Outcome and scope

Implement RFC-BPR-0066 AC01–AC05, preserving the owner's settled waiting placement. The core supplies a row-scoped Home section alongside unchanged attention; the host carries accepted rows and persists independent per-window collapse; Qt presents existing panels and row state. Adopt the named live prose amendments without altering historical Done cards, plan exits or unrelated human/native obligations.

## Acceptance

- **AC01:** Author-role and authored-discovery rows with unread activity are absent from Needs me; another account's membership is independent.
- **AC02:** Actionable, waiting, quiet and unknown authored rows occupy one correct section and retain activity; notifications and repository attention remain unchanged.
- **AC03:** Rendered grouping, headers, filtered counts, collapse and keyboard navigation remain accurate, including authored-only and empty queues.
- **AC04:** Upgrade and two-window restart preserve view choices; My pull requests starts expanded and persists separately from Waiting on others; named group collapse stays session-only.
- **AC05:** Composed accepted refresh preserves exact selection and normal opening destination, adding no provider work; existing recent/onboarding/signed-out courses pass.

RFC base: `b87b7e4ae8f62696c3af88abe3c943eaef65c6f5`.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** With an authored PR and another person's PR both carrying unread activity, only the latter appears or counts in Needs me. Author-role and authored-discovery inputs produce the same separation, scoped to each row's account identity. | [Criterion ledger](../../verification/RFC-BPR-0066.md) and its focused public/core/Qt/storage checks | Pass | `a34080e0fbce7d7e8d9fdade436aa5826e21e4a5` |
| **AC02:** Changing an authored row between actionable, waiting, quiet and unknown activity produces the placement defined under Section membership, with one active row and no loss or fabrication of its activity explanation. In particular, unread activity moves it from Waiting on others to My pull requests while preserving its unread count. Its Home placement does not change notification eligibility or repository-drawer attention. | [Criterion ledger](../../verification/RFC-BPR-0066.md) and its focused public/core/Qt/storage checks | Pass | `a34080e0fbce7d7e8d9fdade436aa5826e21e4a5` |
| **AC03:** The rendered Home preserves the separation, accurate filtered counts and readable activity through every Group value, both State headers settings, collapse and keyboard navigation. An authored-only queue remains visible; empty sections add no empty panels. | [Criterion ledger](../../verification/RFC-BPR-0066.md) and its focused public/core/Qt/storage checks | Pass | `a34080e0fbce7d7e8d9fdade436aa5826e21e4a5` |
| **AC04:** After upgrading and restarting two windows with different saved Home views, My pull requests initially appears expanded and subsequently retains each window's own collapse choice. Unaffected view choices and the documented lifetime of named group collapse remain intact. | [Criterion ledger](../../verification/RFC-BPR-0066.md) and its focused public/core/Qt/storage checks | Pass | `a34080e0fbce7d7e8d9fdade436aa5826e21e4a5` |
| **AC05:** Through the composed application's accepted refresh and ordinary row actions, the same exact PR remains selected and opens in the intended window after its activity changes; section arrangement adds no provider work. Existing Recently viewed, onboarding and signed-out courses remain valid for their unchanged behavior. | [Criterion ledger](../../verification/RFC-BPR-0066.md) and its focused public/core/Qt/storage checks | Pass | `a34080e0fbce7d7e8d9fdade436aa5826e21e4a5` |

## Execution record

The full initial milestone is implemented at `a34080e0fbce7d7e8d9fdade436aa5826e21e4a5` under mandate `home-authored-r1`, preserving Q1 and every AC01–AC05 obligation. The [criterion ledger](../../verification/RFC-BPR-0066.md) records commands, rendered evidence, observed failures and corrections, native limits and documentation impact. Used frontend-design from the installed plugin before Qt edits. No review server or worktree belongs to this task; all task commands are finished at handoff. Existing review protection conflict queries return empty. Unrelated configuration changes and untracked drafts are preserved. Next action: coordinator consumes the completed milestone checkpoint; no further milestone or required decision remains.
