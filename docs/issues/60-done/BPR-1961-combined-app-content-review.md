---
schema: "implement-rfc.card.v1"
id: "BPR-1961"
title: "Review and bind latest accepted application composition content"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind": "test-correctness", "found_by": "frozen43Mac qualification / latest-main source audit", "reservation": "docs/issues/_reservations/BPR-1959--BPR-1961--64e658d8c3db.json", "milestone_plan": {"id": "bpr-1959-1961-mac-release-contracts", "path": "docs/rfcs/plans/bpr-1959-1961-mac-release-contracts/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d84cde4d80164207d388ce41c63857a309eadff6","evidence":{"kind":"observation","detail":"Reviewed all12changed manifest sources/commit range9c..52e plus unpinned fixture OIDs.142manifest/197fullinventory unchanged; independent baseline913e8/latest/final4420recomputation matches maintainedprinter; other full-app/mutation pins exact.","artifact_root":"candidate43/implementer-contract-fixes"}},{"criterion":"AC2","outcome":"pass","revision":"d84cde4d80164207d388ce41c63857a309eadff6","evidence":{"kind":"observation","detail":"Actual prior content mismatch retained; source auditPASS, existing hostile async-mutation direct-bypass contractPASS1.84s; exact1962test-only two-file delta reviewed. Self-review is not independent/native release approval.","artifact_root":"candidate43/implementer-contract-fixes"}},{"criterion":"AC3","outcome":"pass","revision":"d84cde4d80164207d388ce41c63857a309eadff6","evidence":{"kind":"observation","detail":"Format22/focused tidy22, documentation and scoped board gatesPASS. Proven source intentionally integrated and ACTUALLY pushed authoritative main d84cde4d80164207d388ce41c63857a309eadff6. All protected producers unchanged, native/diagnostic commands ended/source copies restored. Final release/native trust and Windows new-wrapper measurement/pin remain coordinator/sibling obligations, not waived.","artifact_root":"candidate43/implementer-contract-fixes"}}]
---

# BPR-1961: Review and bind latest accepted application composition content

## Acceptance

- AC1. Independently recompute unchanged142-source inventory at last matching9c and combined latestmain, review every changed source/effect boundary and commit range, reseal only production manifest pin; preserve full-app/mutation/inventory pins.
- AC2. Retain actual prior digest failure; maintained printer agrees with independent recomputation and existing adversarial composition contract passes, without full native acceptance claims.
- AC3. Format/docs/board and intentional latest-main integration pass; retain raw43failures, exact proof scope and pending final native qualification.

## Mandate

Coordinator approved this exact test-contract scope. Frozen producer source/builds remain untouched; all source fixes land authoritative main.

## Recorded content review

Last matching source9c3936cb5ad659a7487a44b7d17ebddadf1207ca through52e5e27d2, including accepted canonical main bea6b6bad and BPR-1962. The original913e8b38770b835ddb1aa6a8e7fca85d363764901c4ce608a930113824e9ef26 pin fails against latestmain6ba34d0f…; the final reviewed binding is4420e34793a02ffd48f0901f44797e9fdf8e2348747ed11826f0d9778118505b.

The ordered142-member production manifest and197-member full source inventory are unchanged. Independent LF-normalized per-file SHA256 recomputation reproduces the old pin and agrees with the maintained printer for the final pin. github_application.cpp and its full-application pin are unchanged; the production mutation branch and all other pins are unchanged.

Reviewed source/effect boundaries:

- agent_composition.cpp/.hpp: native-history/session forwarding and quiescence reporting, existing invocation host and launcher ownership retained.
- window_agent_chat.cpp: accepted native conversation metadata, explicit revision continuation, history-loading refusal and tool-free writing dispatch; existing entry-point/profile/epoch/intent/grant checks remain.
- window_agent_setup.cpp: accepted native-history restoration/presentation, old history preserved, explicit New/history selection and save refusal; no provider mutation or credential access added.
- window_composition.hpp and window_construction.cpp: native-session store and owned host/projection callbacks, captured per-agent/per-scope binding and lifetime guards, existing process/workspace host ownership.
- window_tours.cpp: accepted selected-agent native sessions, scoped histories, explicit old-revision continuation, removed-stop refusal, current source/preparation context and captured proposal baseline; uses existing read-only launch and TourHost dispatch, never infers a GitHub publication grant.
- window_workbench.cpp: accepted Go to line appearance roles only.
- review_submission_workflow.cpp/.hpp: publication presentation extracted as a value accessor; write/planning/submission/qualification gates unchanged.
- fixture_threaded_reply_course.cpp and window_probe_states.hpp: BPR-1962 test-only accepted-refresh readiness, same single gesture/oracle/budget.
- The additional unpinned github_public_path_fixture.cpp change only updates source-find fixture OIDs for accepted Go to line native source text.

Complete covered commits:

- 52e5e27d2733c9b72a7b3b939bb484ff093c4170 BPR-1962 await accepted reaction refresh before checking reconciliation
- bf09db0a3c73836587a4d6a0ea6e447b18b79829 Preserve explicit tour continuation at the saved revision
- 2de6a3336a02b7aae5a62700d17b620eff98451d Add explicit transcript replay and preserve legacy history provenance
- a89b0fa88f5a8e4c0eae8495ab8a44da8f7377e8 BPR-1953: give tour chats current source and preparation context
- 3b582b3a29352204e8300bf0a63f5343173b279c Merge Go to line navigation and accepted compact panel
- 5ecd7583dd45e7a3cbf0780a3416fe89ae7cd09e Add native tour sessions and retained scoped history
- 26fdce665d7ff09f9ecd52e0df6765ce569b21ff Continue native Agent-tab conversations and retain resume ownership
- df4ccfc8587e203048eb3d74499f6305480e5720 Refine Go to line with a compact exclusive side picker

Prior accepted behavior and critical review are recorded on BPR-1951 through BPR-1957 and RFC-BPR-0096. This is additional scoped implementer self-review, not independent acceptance. External candidate43/implementer-contract-fixes contains composition-review-input.diff, composition-reseal-independent.json, composition-reseal-final.json, printer, negative/positive source-audit logs and existing adversarial control. The source audit and hostile direct-bypass control pass; neither constitutes full native release qualification.
