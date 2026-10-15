---
schema: "implement-rfc.card.v1"
id: "BPR-1539"
title: "File and change arrows do nothing after opening another pull request from Home"
status: "In Progress"
rfcs: ["RFC-BPR-0076"]
depends_on: []
priority: "P1"
metadata: {"source":"Owner bug report, 2026-10-15","owner_note":"I got into another situation where the prev/next file buttons and prev/next change buttons do nothing.","owner_repro":"Had just finished a review, went to Home, pasted a new pull request URL to open it, possibly twice before the first open finished. Afterwards Previous/Next file and Previous/Next change did nothing; clicking files in the tree still switched files; reopening or selecting files did not help; only relaunching recovered.","related":["BPR-1517","BPR-1532","BPR-1538"]}
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
