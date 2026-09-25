---
schema: "implement-rfc.card.v1"
id: "BPR-1198"
title: "Reconcile epoch refresh with the active tab selected-file identity"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_at":"f6de40b633ea8b879d3643ad5a259169fd976975","plan":"docs/rfcs/plans/bpr1198-refresh-identity/milestone-plan.json","mandate":"user-unattended-r1","milestone_plan":{"id":"bpr1198-refresh-identity","path":"docs/rfcs/plans/bpr1198-refresh-identity/milestone-plan.json"}}
---

# BPR-1198: Reconcile epoch refresh with the active tab selected-file identity

## Defect and scope

The full Mac suite at `f6de40b6` expired in `application_review_behaviour_contract`, GHES seed, at stage 4 after 120076/120000 ms. Four file roots were present; the six review-wide roots were expected. This resembles terminal [[BPR-0818]], but [[BPR-1132]] now restores the active tab's exact file and comparison. Distinguish a legitimate selected view from a stale conversation anchor before correcting the product or course. Preserve 120000 ms.

## Acceptance

- Exact terminal observations distinguish host selection, active tab file/pair, displayed comparison and conversation anchor; name the cause.
- The public review-behaviour contract passes on macOS with fail-before/pass-after evidence; a focused view requires exact reconciled identities, and a genuinely unselected view still requires all six roots. Preserve BPR-0818 regression coverage and the 120000 ms budget.

## Authority

Standalone follow-up assigned by the unattended coordinator. Owner mandate `user-unattended-r1`; source baseline `f6de40b6`. New Windows/Linux runs are deferred by the owner. No native acceptance or RFC closeout is claimed.

## Cause and correction

This is a stale course expectation introduced by the document tabs in [[BPR-1132]], not a recurrence of BPR-0818's stale visual hold. RFC-BPR-0052's decided document-tab contract retains the active tab's file, comparison and reading position in Read/Review. The review-behaviour seed's natural Start review gesture creates that tab. After an explicit refresh accepts the new comparison, the tab host restores its file on that pair. The drawer then correctly contains the four file-thread roots; the review summary and issue-comment roots are still available when no file is selected.

The original course could pass only in the temporary empty gap before the tab load completed. An observation-only GHES seed passed in 30384 ms while tab #1 had `loading` state and `TabPending=1`, with no host selection, display or anchor. Waiting for that pending tab to finish, while retaining the original six-root condition, reproduced the failure at 120071/120000 ms and 10000 stage-4 ticks. The terminal had:

- tab #1 ready and no pending comparison or tab load;
- host, tab, published diff, displayed diff and conversation anchor all naming `review.cpp` identity `9465cd9b3db7ae71c07f8b011e6bbee61fd04e731349b261613cbebfb64ff3a6`;
- tab, accepted and displayed publication all on base `757e8f259231f0cb31b6774c14e6acdbf54b48f3` / second head `6458d96bf0ccddc1ef2fcf60e61c93a28e69d665`;
- the exact published comparison key displayed, no line anchor, and all four expected file roots and their children.

The corrected epoch-refresh stage waits for the tab to be ready, then requires the full typed comparison identities, selected/published/displayed file, displayed publication key and drawer anchor to agree before checking all expected focused identities. In Overview/Discuss, or with no active tab in Read/Review, it requires no selected/published/displayed file or anchor and all six review-wide roots. The host and window must agree on whether the modality presents tabs. The review-behaviour driver additionally requires a focused seed marker, so a transient unselected pass cannot satisfy its Start review course. The first corrected GHES seed passed in 30967 ms. Both the 120000 ms phase budget and the parent/child timeout registrations are unchanged.

## Source-pin review

Reviewed the complete pinned-scope delta from `f6de40b6` to this implementation commit; no intervening commit changed a pinned source. `github_application.cpp` adds one read-only, test-build-only accessor copying host/tab/publication identities. `main.cpp` adds test-course identity reconciliation and pre-shutdown diagnostics; ordinary product behavior and the mutation branch are unchanged. The fixture header is test-build-only. The CMake driver strengthens the seed expectation without changing bounds.

Resealed the full application digest from `5bc3de4d81fc806d9ad3a1b9dd0470200e5b00030daaddd0edc2345aea6cbbb6` to `fd60006f81dc815a9c43dd875dfc0865b9224cd8076bb651d4b93912393d1ab6`, and production source manifest from `394acf6599072cf46bd80e45a56d0ce1557bf0d15b56ab41892d5695c535fb47` to `392d25853d021fc300a86002297cfa5e676ab293877e819b7c1528929c25bd43`. Mutation-branch digest remains `a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.

Runtime-only evidence: `/Users/jaeger/.codex/tmp/bpr1198-my4d8usp/`, including the observation-only patch, `before.log`, `before-settled.log`, and `after.log`. Final verification and command state follow in the milestone handoff.

### Modality refinement after the first candidate

The shared plain seed rejected `8ebf9e6c` at 120060 ms because Overview correctly retained tab #1 on its older pair without presenting it: host selection, published/displayed file and anchor were all absent, and all six review-wide roots were present. The course must distinguish a retained tab from a presented tab. The follow-up observes the host's Read/Review modality, checks that the window agrees, and applies focused identity reconciliation only there. Overview/Discuss keep the original unselected contract. This refinement changes only the same test accessor and course guard; the full pinned scope `f6de40b6..8ebf9e6c` plus this follow-up has been reviewed, with no product policy or mutation-branch change.

Final reseal after the modality refinement: full application `4f5514d0792433b199f7181f82d5f8153387b07c6dbec9453f7389600194ad5f`, production source manifest `f0e1b80eaaf3d3307f851fd7b36c794f5576ccc64faa95b246163ebb867cb593`; mutation branch remains unchanged. The earlier hashes above describe the first candidate, whose Overview case was corrected before integration.
