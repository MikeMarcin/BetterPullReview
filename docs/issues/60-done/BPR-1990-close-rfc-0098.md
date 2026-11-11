---
schema: "implement-rfc.card.v1"
id: "BPR-1990"
title: "Verify and close RFC-BPR-0098"
status: "Done"
rfcs: ["RFC-BPR-0098"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0002-close-home-next-move.md","draft_title":"Close the Home next-move RFC"}
depends_on: ["BPR-1989"]
closeout_for: ["RFC-BPR-0098"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"58db13bb5992e74265d7057ec150c48860ce434f","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R core_home_next_move"}},{"criterion":"AC02","outcome":"pass","revision":"58db13bb5992e74265d7057ec150c48860ce434f","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'core_home_next_move|application_home_next_move_public_path'"}},{"criterion":"AC03","outcome":"pass","revision":"58db13bb5992e74265d7057ec150c48860ce434f","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R application_home_next_move_public_path"}},{"criterion":"AC04","outcome":"pass","revision":"58db13bb5992e74265d7057ec150c48860ce434f","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'core_home_next_move|application_review_queue_enrollment|qt_review_landing_model'"}},{"criterion":"AC05","outcome":"pass","revision":"58db13bb5992e74265d7057ec150c48860ce434f","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'application_home_next_move_public_path|github_paged_read'"}},{"criterion":"AC06","outcome":"pass","revision":"2f772b6f00b3be54d27e4b5462815332977e955b","evidence":{"kind":"observation","detail":"Owner accepted BPR-1989-home-moves, BPR-1989-word-place and BPR-1989-own-pull-requests in attended session bb8c24c5 with no notes"}}]
---

# BPR-1990: Verify and close RFC-BPR-0098

## Scope and acceptance

Judge every acceptance criterion of
[RFC-BPR-0098](../../rfcs/RFC-BPR-0098-home-says-whose-move-my-pull-request-is.md)
against actual evidence from the cards this one depends on, including its
live, installed-agent and owner-attended criteria, the reconciliation of the
live RFCs it amends, and the documentation gate. Record failures and unreached
requirements exactly.

Leave live IDs, the implementation baseline and acceptance records to
authorized execution. This proposed closeout carries no passing evidence and
changes no live RFC's status.

This is part of `accepted-scope` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** For each row of the decision table, a fixture pull request with exactly those facts yields that next move from the core, and a pull request matching several rows yields the first. | `core_home_next_move`: one fixture per table row; first matching row wins | Pass | `58db13bb5` |
| **AC02:** On Home, authored rows appear in My pull requests in the table's order, Waiting rows remain in Waiting on others, and no authored row appears in Needs me. | `core_home_next_move` (`test_placement_and_order`); `application_home_next_move_public_path` | Pass | `58db13bb5` |
| **AC03:** A Returned row's sentence names the reviewer who requested changes and the number of conversations awaiting my reply; a Ready row's sentence names its approvers. An approved, mergeable pull request never reads Quiet. | `application_home_next_move_public_path` sentences and tones; never Quiet when approved and mergeable | Pass | `58db13bb5` |
| **AC04:** With the conversation field unavailable, a changes-requested pull request is still Returned, no row states a conversation count, and a pull request with only unanswered conversations is not claimed Returned. | `core_home_next_move` (`test_absent_facts_claim_nothing`), the composed course, `application_review_queue_enrollment`, `qt_review_landing_model` | Pass | `58db13bb5` |
| **AC05:** The Home read for a profile with authored pull requests makes no more requests than before, and its added fields are bounded as stated. | Composed course: two searches and two batched queries as before; `github_paged_read` fragment bounds (50 threads) | Pass | `58db13bb5` |
| **AC06:** The owner accepts the rows on a frozen build against their own open pull requests. | Owner look, session `bb8c24c5…` (`23a80fed`, `cae206d9`, `0b371105`), frozen `2f772b6f0` and live `58db13bb5` | Pass: all three accepted | `2f772b6f0` |

## Closeout (2026-11-11)

Every criterion judged against the [ledger](../../verification/RFC-BPR-0098.md). The owner fix round (BPR-2048, BPR-2049, accepted) changed Home's refresh and Seen without changing a criterion. The address-review-feedback-v1 pack proposes amending the Returned rule (P09, unconfirmed); that is a later change, not part of this RFC's acceptance. Documentation gate run before this commit. RFC-BPR-0098 is Auto-Verified.
