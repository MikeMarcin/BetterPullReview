---
schema: "implement-rfc.card.v1"
id: "BPR-1423"
title: "Composed PR Search drawer, result activation and invalidation"
status: "Done"
rfcs: ["RFC-BPR-0077"]
depends_on: ["BPR-1421","BPR-1422"]
metadata: {"milestone":"pr-search","source_id":"DRAFT-0019","rfc_base_sha":"aa3d71fe470627e1e78488eb51a9114a788f33bd","mandate_revision":"find-and-find-in-files-v1-r1","milestone_plan":{"id":"find-and-find-in-files-v1","path":"docs/rfcs/plans/find-and-find-in-files-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7d56dc1e528fcc24320bd6287a2274362d5bd5f2","evidence":{"kind":"observation","detail":"OriginalAC03-AC07 pass through native Search commands: whole both-side unopened/filtered/excluded/added/removed/rename corpus, actual historical Refresh/pinnedpair, exactspan/F4/sidefocus/siblingdrafts/pathfilters/dedup, races, qualifiedbinary/decode/mode/unavailableRetry/completeempty, supportednative/scaled/accessibledescription/key/editor/modality paths.","references":["docs/issues/_checkpoints/find-and-find-in-files-v1/m5-native-public.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m5-source-review.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m5-reveal-negative.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m5-integration.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m5-tidy.json"]}},{"criterion":"AC2","outcome":"pass","revision":"7d56dc1e528fcc24320bd6287a2274362d5bd5f2","evidence":{"kind":"observation","detail":"Exact pending reveal binds all PRauthority/catalogue/tab/pair/path/side/blob/query/span identities andmonotonicreadinggeneration. LaterA-to-B-to-Aordinarynavigation unequivocallyrejectsoldLeft1102; cancellation-removal mutantfails theoriginalassertion. OrdinaryOpenFile andselectedendpoint revalidate before sharedreveal.","references":["docs/issues/_checkpoints/find-and-find-in-files-v1/m5-native-public.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m5-source-review.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m5-reveal-negative.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m5-integration.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m5-tidy.json"]}},{"criterion":"AC3","outcome":"pass","revision":"7d56dc1e528fcc24320bd6287a2274362d5bd5f2","evidence":{"kind":"observation","detail":"Independentpane/PRquery/options runtime-only; paneFindretainsdecorations whilePRexactselectionworks. Realaccountsignout clearsquery/scope/records anddrainsterminals; clear/Close/Escape/reset/shutdown pathsquiescewithoutoldauthority resurrection.","references":["docs/issues/_checkpoints/find-and-find-in-files-v1/m5-native-public.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m5-source-review.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m5-reveal-negative.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m5-integration.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m5-tidy.json"]}}]
---

# BPR-1423: Composed PR Search drawer, result activation and invalidation

## Acceptance

- **AC1:** AC03–AC07 pass through real Search-tab commands and key paths: historical membership, exact result activation/F4, sibling preservation, filters/deduplication, stale completion/reveal, qualified coverage and accessible native layout.
- **AC2:** Selection/reveal binds PR authority, catalogue, tab, comparison, exact path/side/blob/span and query generation; later reading gestures cancel it.
- **AC3:** Pane and PR query state remain independent and runtime-only; authority changes clear sensitive text/results and all newly admitted effects reach their terminal.

## Continuation

Accepted under `find-and-find-in-files-v1-r1` on product candidate
`7d56dc1e528fcc24320bd6287a2274362d5bd5f2`. The
[acceptance](../_checkpoints/find-and-find-in-files-v1/m5-acceptance.json) and
[native public evidence](../_checkpoints/find-and-find-in-files-v1/m5-native-public.json)
prove all three complete card criteria. Final added/removed activation tests also
select/focus the sole available exact side and preserve the sibling tab.
The whole RFC and performance/integration reconciliation finish on BPR-1424;
no human criterion is outstanding.
The [import checkpoint](../_checkpoints/find-and-find-in-files-v1/import-r1.md) records
scope, preserved work, verification and the next action.


## Combined source review

The complete Find source delta over accepted main `31ab8129f9` and merge
`a4dc59cea` is reviewed in [m5-source-review.json](../_checkpoints/find-and-find-in-files-v1/m5-source-review.json).
It includes pane product `a7fb9af0a`, PR WIP `0220258f4` and final M5 corrections.
Both controller pairs enter the explicit production manifest and exact tree
inventory. Authority/query/endpoint/reading seals, host lifetime, epoch priority,
ordinary tab opening and bounded native projection retain existing ownership.
The production mutation-branch digest is byte-identical; all source and language
negative controls remain required. The inherited agent-disabled workshop fixture
now uses the existing compile guard and reports unavailable rather than failing
linkage. Main CLI/navigation/C++ source and unrelated records are preserved.
