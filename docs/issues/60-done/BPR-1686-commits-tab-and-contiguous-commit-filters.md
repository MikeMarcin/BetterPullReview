---
schema: "implement-rfc.card.v1"
id: "BPR-1686"
title: "Commits tab and contiguous commit filters drive the review"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1685"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"milestone":"RFC-BPR-0089 M1","reservation":"docs/issues/_reservations/BPR-1685--BPR-1688--a54310676f44.json","frontend_design":"frontend-design:frontend-design loaded before the Commits tab, the chip and the filtered heading were drawn","related_cards":["BPR-1685","BPR-1687","BPR-1688"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"07920aeb4473341ec22c435eb76896ce08f3e7b5","evidence":{"kind":"command","command":"python3 tests/end_to_end/rfc0089_commit_filter_live_course.py --build build/dev --work DIR --prefix rfc0089-m1-live3 --cleanup","detail":"Live, MikeMarcin/BPR-Test #111 (closed, branches deleted; also #110 on the pre-rebase candidate): C1 source (4 files), C2 regenerating 321 files, C3 mixed (8 files); the shipped composition offscreen, signed in through gh (sparse review, chain trees through gh). The Commits tab listed C1 to C3 with headline, author and the file count of git diff-tree C^ C. Show only selected on each commit gave exactly that commit's paths (4, 321, 8 of 325) and every diff checked matched git diff path by path: endpoints, both blobs, removed and added lines (34 diffs). Clear all filters restored the 325 files each time. Fixture course application_commit_filter_public_path covers the same through recorded GitHub, and qt_commit_filter_view and application_commit_filter_model the truncated, base-merge, suspended and refused states and their reasons. macOS."}},{"criterion":"AC2","outcome":"pass","revision":"07920aeb4473341ec22c435eb76896ce08f3e7b5","evidence":{"kind":"command","command":"ctest --preset dev -R '^application_commit_filter_public_path$' and the live course","detail":"Hide selected on C2: every file C2 did not touch is Whole PR's own key (no filter endpoints), so its material and receipt are Whole PR's; C3's hand edits on generated files show C2..C3; src/version.txt, which hidden C2 changed between shown C1 and C3, reads nothing and says why (M3 splits it). The chip reads 2 of 3 commits shown in Overview and the Files drawer. Show only {C1, C3}, then Hide C3, then Show selected C3 yield the stated sets. The live run before the surface fix kept the previous diff on screen for the split file and failed; the fixed build passed. macOS."}},{"criterion":"AC3","outcome":"pass","revision":"07920aeb4473341ec22c435eb76896ce08f3e7b5","evidence":{"kind":"observation","detail":"GitHub's Viewed state: two files marked Viewed before the live course; all 325 files' viewerViewedState equal before and after every filter gesture. The fixture course holds no review mutation terminal after the gestures. macOS."}},{"criterion":"AC4","outcome":"pass","revision":"07920aeb4473341ec22c435eb76896ce08f3e7b5","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_commit_filter_model|qt_commit_filter_view_scale_(100|200))$'","detail":"After a force push that removes a hidden commit, the filter stops applying to it and the chip names it (short ID and headline); a hidden commit still present keeps applying. Rematch offers are M3. macOS."}}]
---

# BPR-1686: Commits tab and contiguous commit filters drive the review

RFC-BPR-0089 M1. The `frontend-design:frontend-design` skill was loaded
before these surfaces were drawn.

## Change

- **Commits tab.** Overview's Status drawer has a Commits tab beside Status.
  Each row shows the headline (wrapping), the author, the short ID in the code
  font and the file count once the chain's trees are read, with tags for
  Hidden, Not in this comparison, Merge and Merge from base; side commits sit
  under their merge. Extended selection and a context menu offer Show only
  selected, Hide selected, Show selected, Clear all filters, Copy commit ID
  and Open on GitHub (which opens only a listed commit). The model reconciles
  rows by commit ID without a reset, and the tab reads the trees the first
  time it shows.
- **What a filter drives.** The filtered file set replaces the comparison's in
  the Overview tree, the Files drawer, Next and Previous file, the actionable
  order and the counts ("4 of 325 files, filtered by commits"). A chip in
  Overview and in the Files drawer names the filter ("1 of 3 commits shown"),
  offers Clear all filters and opens the Commits tab from Review.
- **Each file's diff.** A path every commit that changed it is shown for keeps
  the comparison's own diff and receipt. Otherwise the diff is exact, from the
  parent of the first shown commit that changed it to the last, read through
  the epoch host, which authorizes only the endpoints its touch job resolved.
  A file a hidden commit changed between two shown ones keeps its row, reads
  nothing and says why; M3 splits it.
- **States.** Truncated at 250, a merge from the base, a suspended filter (a
  reversed or empty comparison, or one that is not a first-parent range), a
  refused gesture and a gone commit are shown with the core's reasons.
- **Comments stay head-only.** A comment from a filtered diff is refused
  unless its right side is head and it is not a new base-side root; thread
  marks are suspended on filtered diffs.

Shared files touched: `workbench_window`, `pull_request_overview_page`,
`overview_status_drawer`, `github_application.cpp`, `epoch_application_host`,
`main.cpp` and the tests' CMake lists.

## Acceptance

- **AC1.** RFC AC01 on the owner's shape, live and on the fixture.
- **AC2.** RFC AC02 for contiguous filters; a split file is shown as reading
  nothing (M3 splits it).
- **AC3.** RFC AC05: GitHub's Viewed state is unchanged by any filter action.
- **AC4.** RFC AC06, M1's part: the chip names a commit a force push removed.

## Not in this card

Find in files on the filtered set, and restoring the filter with the window:
see [[BPR-1688]].

## Verification

See the acceptance evidence and the
[M1 checkpoint](../_checkpoints/RFC-BPR-0089-M1/handoff.md).
