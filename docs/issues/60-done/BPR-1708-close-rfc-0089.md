---
schema: "implement-rfc.card.v1"
id: "BPR-1708"
title: "Verify and close RFC-BPR-0089"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1685","BPR-1686","BPR-1687","BPR-1688","BPR-1689","BPR-1690","BPR-1691","BPR-1692","BPR-1693","BPR-1695","BPR-1696","BPR-1697","BPR-1703","BPR-1704","BPR-1705"]
closeout_for: ["RFC-BPR-0089"]
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1703--BPR-1708--2cc864d62b8c.json","checkpoints":["docs/issues/_checkpoints/RFC-BPR-0089-M1/handoff.md","docs/issues/_checkpoints/RFC-BPR-0089-M2/handoff.md","docs/issues/_checkpoints/RFC-BPR-0089-M3/handoff.md","docs/issues/_checkpoints/RFC-BPR-0089-M4/handoff.md"],"follow_ups":["BPR-1706","BPR-1707"]}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"observation","detail":"M1 evidence reused (BPR-1686 AC1: fixture and live, BPR-Test #111); the composed course passes again on the candidate.","refs":["docs/issues/60-done/BPR-1686-commits-tab-and-contiguous-commit-filters.md","docs/issues/_checkpoints/RFC-BPR-0089-M1/handoff.md"]}},{"criterion":"AC02","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"observation","detail":"M1 evidence for contiguous filters (BPR-1686 AC2) and M3's split files (BPR-1696) reused; the composed course passes again on the candidate.","refs":["docs/issues/60-done/BPR-1686-commits-tab-and-contiguous-commit-filters.md","docs/issues/60-done/BPR-1696-read-a-split-files-per-commit-diffs.md"]}},{"criterion":"AC03","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"observation","detail":"M3 evidence reused (BPR-1696: fixture and live, BPR-Test #114, with the synthesized-tree negative control).","refs":["docs/issues/60-done/BPR-1696-read-a-split-files-per-commit-diffs.md","docs/issues/_checkpoints/RFC-BPR-0089-M3/handoff.md"]}},{"criterion":"AC04","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"observation","detail":"M2 evidence reused (BPR-1691, BPR-1692, BPR-1693: fixture through the shipped composition, live BPR-Test #113 for the attribute half).","refs":["docs/issues/_checkpoints/RFC-BPR-0089-M2/handoff.md","docs/issues/60-done/BPR-1692-read-linguist-generated-at-head.md"]}},{"criterion":"AC05","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"observation","detail":"Every milestone's live course left GitHub's Viewed state unchanged (M1 325 files, M2, M3 325 files twice, M4 326 files on #118); filter, search and restore gestures hold no review mutation terminal in the composed courses.","refs":["docs/issues/_checkpoints/RFC-BPR-0089-M4/handoff.md"]}},{"criterion":"AC06","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"observation","detail":"M3 evidence reused (BPR-1697: fixture and live, BPR-Test #115); BPR-1688 adds a restored filter's gone commit, named as after a force push.","refs":["docs/issues/60-done/BPR-1697-offer-the-same-change-after-a-force-push.md","docs/issues/60-done/BPR-1688-find-in-files-and-window-restore-under-a-commit-filter.md"]}},{"criterion":"AC07","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"observation","detail":"M4: fixture through the shipped composition (head line 17 and C1 line 5 published and reconciled once through the real provider session; refusals before Publish; host negative controls refuse a commit outside the chain before any write) and live BPR-Test #118 (the composition's placements published through gh as decided, because a tests-enabled build holds no publication grant: commit_id head at the mapped line, commit_id C1 outdated after the warning).","refs":["docs/issues/60-done/BPR-1704-publish-a-placed-comment-from-a-filtered-diff.md","docs/issues/60-done/BPR-1705-say-where-a-filtered-comment-goes-and-mark-threads.md"]}},{"criterion":"AC08","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"command","command":"ctest --preset core-only -R '^core_(filtered_comment|commit_filter|generated_files|source_search_policy|window_review_restoration)$' && ctest --preset dev -R '^(architecture_boundaries|architecture_boundary_contract)$' && python3 scripts/verify_documentation.py --root . && python3 tests/scripts/verify_documentation_tests.py","detail":"Every core decision of the RFC (shown-set operations, filter legality, each path's diff, generated precedence, comment target, thread markers, rematch, the filtered search scope and the kept filters) runs under core-only without Git or Qt; the architecture boundaries pass; the documentation gate passes apart from the provisional IDs the coordinator allocates."}}]
---

# Close RFC-BPR-0089

## Scope and acceptance

Cover RFC-BPR-0089 AC01 to AC08 with the evidence of milestones M1 to M4 and
[[BPR-1688]]. Candidate `c44429b78`, on main `ab1698599`.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01** Commits tab, Show only C1, Clear all filters, truncated and base-merge reasons | Pass: M1, [[BPR-1686]] AC1, fixture and live (#111). |
| **AC02** Hide C2 keeps other files' material and read state; chip in Overview and Review; combined operations | Pass: M1 for contiguous filters, M3 [[BPR-1696]] for the split file. |
| **AC03** A file C2 changed between C1 and C3 shows both commits' own diffs, C2 named | Pass: M3 [[BPR-1696]], fixture and live (#114), negative control. |
| **AC04** Generated files, overrides, `-linguist-generated`, Hide files these commits changed | Pass: M2 [[BPR-1691]], [[BPR-1692]], [[BPR-1693]], fixture and live (#113). |
| **AC05** GitHub's Viewed state unchanged | Pass: every live course; no mutation terminal in the composed courses. |
| **AC06** Force push names the gone commit and offers the same change | Pass: M3 [[BPR-1697]], fixture and live (#115). |
| **AC07** Head line when unchanged at head; C1 after the warning; reconcile once; negative control | Pass: M4 [[BPR-1704]], [[BPR-1705]]; fixture reconciles through the provider; live #118 publishes the decided placements through `gh`. |
| **AC08** Core decisions under `core-only`; boundaries and documentation gates | Pass on the candidate. |

## Final closeout record

The RFC's design also offers RFC-BPR-0022's file comment with a permalink for
a left-side line that does not map to the merge base. No criterion names it;
M4 refuses such a line before Publish with the reason, and
[[BPR-1706]] tracks the fallback. [[BPR-1707]] records two
reread and refresh gestures the courses had to repeat. Windows and Linux are
unmeasured. No owner judgment was needed: no criterion is a look-and-judge
one, and nothing was parked.
