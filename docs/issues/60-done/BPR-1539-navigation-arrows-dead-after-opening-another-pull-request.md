---
schema: "implement-rfc.card.v1"
id: "BPR-1539"
title: "File and change arrows do nothing after opening another pull request from Home"
status: "Done"
rfcs: ["RFC-BPR-0076"]
depends_on: []
priority: "P1"
metadata: {"source":"Owner bug report, 2026-10-15","owner_note":"I got into another situation where the prev/next file buttons and prev/next change buttons do nothing.","owner_repro":"Had just finished a review, went to Home, pasted a new pull request URL to open it, possibly twice before the first open finished. Afterwards Previous/Next file and Previous/Next change did nothing; clicking files in the tree still switched files; reopening or selecting files did not help; only relaunching recovered.","related":["BPR-1517","BPR-1532","BPR-1538"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"6ea8f221f9fa5b23a4f12937e8d353a108df5d04","evidence":{"kind":"observation","detail":"Composed reproduction on base 33a89d7d7 with a temporary trace: after opening #18 from Home over a #17 Review tab, tabs=1 survived, the active tab's comparison never matched the accepted one (cmpmatch=0), tab state went to unavailable and every Next file press was dropped (next_file_moved=0, empty status) with or without a repeated paste; same-PR double open navigated normally. Root cause: invalidate() clears ReadingDestination before acceptance, so observe_restoration_read skipped TabHost->reset().","refs":["docs/issues/30-in-progress/BPR-1539-navigation-arrows-dead-after-opening-another-pull-request.md"]}},{"criterion":"AC2","outcome":"pass","revision":"6ea8f221f9fa5b23a4f12937e8d353a108df5d04","evidence":{"kind":"command","detail":"application_review_navigation_entry (new run_pull_request_switch_course: #17 then #18 pasted twice; no carried-over tabs; Next file opens #18's file in a ready exact tab; file arrows show their bounds; change arrows step or state their bound) and application_document_tabs_restoration_public_path pass; application_profile_window_review_restoration_public_path passed (183 s) on the identical tree.","refs":["docs/issues/_checkpoints/BPR-1539/verification.json"]}},{"criterion":"AC3","outcome":"pass","revision":"6ea8f221f9fa5b23a4f12937e8d353a108df5d04","evidence":{"kind":"command","detail":"The existing unavailable-material section of the navigation course now also asserts the refusal status message for file and change presses; its surface explanation, focus and no-newer-pair checks are unchanged and pass. Bounds, entry matrix and cancellation courses pass unchanged.","refs":["docs/issues/_checkpoints/BPR-1539/verification.json"]}},{"criterion":"AC4","outcome":"pass","revision":"6ea8f221f9fa5b23a4f12937e8d353a108df5d04","evidence":{"kind":"observation","detail":"Negative control: with adopt_tab_destination restricted to the old ReadingDestination condition and the test unchanged except printing the tab count, the course failed at 'Next file opens the second pull request's first file in its own tab' with tabs=1. clang-format 22.1.8 check and clang-tidy 22 on the touched translation units pass; documentation gate prints DOCUMENTATION_OK.","refs":["docs/issues/_checkpoints/BPR-1539/verification.json"]}}]
---

# BPR-1539: File and change arrows do nothing after opening another pull request from Home

The owner's Previous/Next file and Previous/Next change arrows stopped doing
anything after they finished a review, went to Home and opened a different pull
request by pasting its URL (possibly twice). The files tree still chose files,
and only relaunching recovered. [RFC-BPR-0076](../../rfcs/RFC-BPR-0076-file-change-navigation-enters-review.md)
binds a navigation press to the active document tab's exact pair; the arrows
must either act or say why they did not, never silently do nothing.

## Acceptance

- AC1: Diagnose the dead-arrow state with a deterministic composed
  reproduction of the owner's sequence, naming the root cause and the state
  that survives reopening but not relaunching.
- AC2: After opening another pull request from Home (including a repeated
  paste), Previous/Next file and Previous/Next change act on that pull
  request's own file and comparison, or show their ordinary bound.
- AC3: A press the composition refuses because the active tab's exact view is
  unavailable shows a visible reason for the bound-message duration instead
  of silently doing nothing; existing bounds, unavailable-material handling
  and focus are unchanged.
- AC4: Regression course through real QActions on the composed application
  with a negative control showing the course fails on the old behaviour.

## Coordination

BPR-1532 (unmerged) extends change navigation in `workbench_window.cpp`'s
`perform_review_navigation`; this card stays out of that function and the
shortcut bindings. BPR-1538 owns the GHES "review status unknown" submission
path; the dead arrows do not depend on it (the reproduction has no review
submission). Never operate the owner's desktop.

## Diagnosis and result

Reproduced deterministically on the composed application with the synthetic
two-pull-request fixture: open #17, enter Review (one document tab), go Home,
open #18 (once or pasted twice). A repeated paste is not needed; the defect
follows any switch to a different pull request in the same window after a
Review tab exists. Reopening the same pull request, twice or not, was fine.

Opening from Home dispatches a fresh read, which invalidates the shown pull
request's epoch authority at once; the composition's `invalidate()` clears
`ReadingDestination`. When the new read is accepted, `observe_restoration_read`
saw no previous destination and skipped the `TabHost->reset()` that
[[BPR-1132]] requires for another pull request. The kept tab named #17's
comparison, which the core refuses for #18 (different authority scope), so
every tab load came back unavailable. A retained arrow press then reached the
unavailable branch of `continue_review_navigation` and was dropped with focus
only. The files tree selects through the workbench host, so it still switched
files; reopening kept the same destination, so only a relaunch recovered.

The tabs now record the destination they belong to (`TabDestination`) and
start again when the accepted read or the epoch handoff names another one. A
press refused because the active tab's exact view is unavailable shows "This
tab's file is unavailable. Choose another file or comparison." for the
three-second bound duration. The contract is recorded in the
[workbench specification](../../ux/workbench-specification.md). No core
change: the core's refusal of a foreign comparison is correct; the
composition failed to start the collection again. BPR-1538 (GHES review
status) is not involved; the reproduction submits no review.
