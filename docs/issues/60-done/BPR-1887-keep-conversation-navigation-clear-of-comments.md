---
schema: "implement-rfc.card.v1"
id: "BPR-1887"
title: "Keep conversation navigation clear of line-aligned comments"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/BPR-1887--BPR-1887--9c514ed2528a.json","related_cards":["BPR-1781","BPR-1886"],"milestone_plan":{"id":"responsiveness-latency-v4","path":"docs/rfcs/plans/responsiveness-latency-v4/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"6b29d074081d6399f0edf4883de5bd3485a1364c","evidence":{"kind":"observation","detail":"The existing conversation link is laid out in the Comments toolbar, outside the comment viewport. Label, unread badge, StrongFocus and Space activation to Overview Comments are retained; availability follows the card page show/hide lifecycle."}},{"criterion":"AC2","outcome":"pass","revision":"6b29d074081d6399f0edf4883de5bd3485a1364c","evidence":{"kind":"observation","detail":"Focused composed GitHub, conversation-panel and card-view courses pass. The public refresh course preserves the comparison, earlier conversation publication, 18 reads, visible remaining-details status, and zero blank samples or renewed wash. Geometry fits before/after refresh and at a narrow window."}},{"criterion":"AC3","outcome":"pass","revision":"6b29d074081d6399f0edf4883de5bd3485a1364c","evidence":{"kind":"observation","detail":"The new geometry assertion fails on pre-fix product 8b33cba9f and passes after the fix. Final 100%/200% offscreen courses and inspected full/narrow renders show separation. All changed C++ files pass clang-format 22; card-view source and end-to-end test pass clang-tidy 22; documentation and whitespace gates pass. The whole-window lint retains only the two documented existing diagnostics."}},{"criterion":"AC4","outcome":"pass","revision":"6b29d074081d6399f0edf4883de5bd3485a1364c","evidence":{"kind":"observation","detail":"Card evidence records inherited architecture/manifest/lint failures without passing credit. One journal line is added, and this source revision is landed on main."}}]
---

# BPR-1887: Keep conversation navigation clear of line-aligned comments

The owner identified the pull-request conversation link overlapping a comment
in the responsiveness course's rendered application. The same overlap is in
the saved BPR-1885 render, before BPR-1886. The navigation link is a child of
the viewport with fixed top coordinates, where a line-aligned card can also
be painted. The existing tests checked the link's text/action but not its
separation from comments. A passing functional course did not make that
render acceptable.

## Acceptance

- AC1: Give the link layout space outside the composed comment viewport;
  preserve its label, unread count, keyboard access and Overview destination.
- AC2: Preserve code-line alignment, card order, refresh loading feedback and
  the current comparison; verify navigation and narrow composed geometry.
- AC3: Add a regression that fails against the overlapping version, inspect
  corrected application renders and run focused source/documentation checks.
- AC4: Record inherited gate failures, add one journal line and land on main.

## Evidence

The composed window now places the existing link in the Comments toolbar,
beside its existing filters and next-unread control. The toolbar owns its
geometry; viewport positioning remains only for standalone card views. When
the unread badge appears or disappears, the link invalidates its size hint,
without requesting another layout on every comparison frame. Text, count,
focus policy, callback and code-line placement are retained. Show/hide events
keep the toolbar-owned link tied to its card view's availability.

The frontend-design and code-simplifier skills were applied. The corrected
full-window offscreen render shows the navigation link above the comment's
author and body; the owner saw it and replied “that looks much better” on
2026-11-06. The existing palette, typography and controls remain in use.

- Against pre-fix product source `8b33cba9f`, the new composed refresh check
  fails at “conversation navigation has layout space outside the comment
  viewport.” The corrected version passes before and after refresh, checks
  that the label and unread badge fit, activates the link with Space, verifies
  Overview's Comments destination, verifies view visibility propagation, and
  checks geometry again at a 960-by-640 requested window.
- `qt_composed_github_scale_100`, `qt_github_conversation_panel`,
  `qt_conversation_cards_view` and `application_refresh_current_file_public_path`
  pass. Existing card tests retain line alignment, ordering, read counts,
  navigation dispatch, expansion and scroll behavior.
- The 330 ms-per-read composed render course passes all refresh assertions:
  fresh conversation at 1,666 ms, settled at 5,991 ms, 18 reads, zero blank
  comparison samples and no renewed wash. The remaining-details marker stays
  visible. These are synthetic regression observations, not a new latency claim.

The 100% and 200% offscreen refresh renders were inspected. These establish
layout separation; they are not a native system-appearance acceptance claim.
The final 200% course passes the visibility, narrow-window and keyboard checks
with all refresh assertions retained: fresh at 1,679 ms, settled at 6,003 ms,
18 reads, zero blank samples and no renewed wash. The corrected narrow render
keeps the link, unread badge, author and wrapped comment text separate.

All four changed C++ files pass clang-format 22.1.8. The conversation-card
source and end-to-end test pass clang-tidy 22.1.8 with warnings as errors;
the header is also compiled through those units. Documentation and
`git diff --check` pass. Unchanged documentation regressions and composition
inventory checks reuse the passing round-one evidence. The journal contains
one line for this correction; the closeout records its landed source revision.

The inherited BPR-1884 architecture refusal and earlier architecture-fixture,
network-manifest and unrelated whole-window lint failures remain documented in
[[BPR-1885]] and [[BPR-1883]]; this fix does not claim those gates pass.
Whole `workbench_window.cpp` clang-tidy was rerun and still reports only the
previously recorded `QueueFilterModel` null-object diagnostic and
`finish_choice` naming violation. Its diff contains only the four toolbar
placement lines; neither diagnostic is in that change.

Source, regression and journal landed on main at `6b29d074081d6399f0edf4883de5bd3485a1364c`.
