---
schema: "implement-rfc.card.v1"
id: "BPR-1381"
title: "Home keeps its layout still: the way back beside the bar, its status at the foot"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","found_by":"Owner chat, 2026-10-10","reservation":"docs/issues/_reservations/BPR-1381--BPR-1381--5ea80a741be3.json","related_cards":["BPR-1357","BPR-0775","BPR-1200","BPR-1365"],"related_rfcs":["RFC-BPR-0032"]}
acceptance: [{"criterion": "BPR-1381:way-back", "outcome": "pass", "revision": "63f71c54a", "evidence": {"kind": "command", "path": "docs/issues/60-done/BPR-1381-home-keeps-its-layout-still.md", "detail": "qt_composed_github_scale_100 and _200: over an open review the way back is right of the bar frame with its centre inside the frame's height, the title label hidden, and the button's accessible description begins 'Reviewing '."}}, {"criterion": "BPR-1381:still", "outcome": "pass", "revision": "63f71c54a", "evidence": {"kind": "command", "path": "docs/issues/60-done/BPR-1381-home-keeps-its-layout-still.md", "detail": "application_home_open_feedback_public_path, 3 s fixture delay, light and dark: the bar frame's and the list's top are unchanged while a row opens, while the band says the bar's opening and a notice, and with Home over the open review."}}, {"criterion": "BPR-1381:bar-activity", "outcome": "pass", "revision": "63f71c54a", "evidence": {"kind": "command", "path": "docs/issues/60-done/BPR-1381-home-keeps-its-layout-still.md", "detail": "Same course: homeOpenActivity is visible in the bar frame while the band says the bar's opening and hidden after Cancel."}}, {"criterion": "BPR-1381:echo", "outcome": "pass", "revision": "63f71c54a", "evidence": {"kind": "command", "path": "docs/issues/60-done/BPR-1381-home-keeps-its-layout-still.md", "detail": "Same course: while the band says the status bar's message the window reports home_status_echoes_bar and the status bar draws nothing; a problem in the band does not."}}, {"criterion": "BPR-1381:plate", "outcome": "pass", "revision": "63f71c54a", "evidence": {"kind": "artifact", "path": "docs/issues/60-done/BPR-1381-home-keeps-its-layout-still.md", "detail": "Offscreen renders inspected, light, dark, high-contrast light and dark at 1100 px: bar waiting, notice, problem, reviewing. The neutral band is a visible plate at Home's foot; the status bar beneath is quiet while it speaks."}}, {"criterion": "BPR-1381:regression", "outcome": "pass", "revision": "63f71c54a", "evidence": {"kind": "command", "path": "docs/issues/60-done/BPR-1381-home-keeps-its-layout-still.md", "detail": "qt_composed_loading_scale_100, qt_composed_home_sign_in, qt_composed_home_refusal_scale_100, qt_composed_inline_drawer_scale_100, qt_surface_capture_course_scale_100 and _200, application_setup_destination_public_path pass; clang-format 22 clean; clang-tidy on the three changed sources reports only the earlier compact_below naming finding from BPR-1324."}}]
---

# BPR-1381: Home keeps its layout still: the way back beside the bar, its status at the foot

## Owner notes

Owner, chat, 2026-10-10, verbatim: "this top section on the home page that
says Reviewing <PR> and Back to the review button causes all the layout of the
page to shift and that section goes away when you open a new pr which shifts
the screen under your cursor and is extremely disorienting.

I also feel like the home screen needs at least a background plate behind this
log/status message thats beneath the open input box. And I'm not sure that is
the best place for log/status/operation update text to begin with."

The owner asked for the work to be done directly.

## What exists (inventory at `157a03f6e`)

- **The way back.** Over an open review `workbench_landing` is in
  `landing_state::back_to_review`: a full-width shaded row (AlternateBase,
  closed by a Mid hairline) that `place_home_landing` inserts at index 0 of
  Home's column, above the bar. It is shown while `HasGithubPullRequest ||
  HasCachedOverview`; opening another pull request clears that, so the row
  leaves, and the bar, the band and the list all move up by its height while
  the pointer is still on the list.
- **The status band** ([[BPR-1357]]). `home_status_line` sits directly under
  the bar and is shown only while one of its labels is. In notice and busy
  tones it fills with Base inside a Mid rule, which is how the bar itself is
  drawn, so it reads as a second empty field rather than a plate. Shown or
  hidden, it pushes the list down or pulls it up under the pointer.

## Scope

1. The way back stands at the bar's right, on the bar's line: Back to the
   review and ⌘0, with the review's name and position in the button's
   description and tooltip. Showing or leaving it changes the bar's width
   only.
2. Home's status band is docked at Home's foot, under the list, so showing it
   moves nothing above it. Its neutral tones are a plate of the window's ink
   over its ground, not Base in a Mid rule. Problems keep the failure tint.
3. What the bar is opening is said in the bar, where its shortcut stood: the
   rows' turning mark and "Opening…". The band at the foot carries the full
   sentence and Cancel or Retry.
4. Courses assert that the bar and the list do not move when a row opens,
   when the band speaks, or when Home shows over an open review.

## Acceptance

- `BPR-1381:way-back`: over an open review the way back is at the bar's
  right, on the bar's line, with no title row and the review named in its
  description (`qt_composed_github`).
- `BPR-1381:still`: through the composed application with a slow fixture, the
  bar and the list keep their positions while a row opens, while the band says
  the bar's opening or a notice, and with Home over the open review
  (`application_home_open_feedback_public_path`).
- `BPR-1381:bar-activity`: the bar shows its own opening mark while the band
  says the bar's opening, and hides it when the opening ends.
- `BPR-1381:echo`: while the band says the status bar's message, the status
  bar under it does not draw it a second time.
- `BPR-1381:plate`: offscreen renders in light, dark and both high-contrast
  modes show the band on a visible plate at Home's foot.
- `BPR-1381:regression`: the Home, loading, sign-in, refusal, drawer, capture
  and setup-destination courses still pass.

The owner asked for this to be done directly, so no attended look is parked on
it; the owner judges the arrangement in use.

## Evidence

Implemented at `63f71c54a`. `application_home_open_feedback_public_path`
passes with the new assertions. The ten Home-related CTest rows listed under
`regression` pass on macOS in the dev preset. Offscreen renders were inspected
and not committed.

Noticed, not changed: in high-contrast dark the Open bar's own rule is
nearly invisible against the ground. That was the case before this card.
