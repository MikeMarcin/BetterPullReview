---
schema: "implement-rfc.card.v1"
id: "BPR-0646"
title: "Workbench chrome: strip, epoch line, modalities and rails"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# Workbench chrome: strip, epoch line, modalities and rails

First slice of the owner-locked [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)
direction (mockup states 1 to 3). Replaces the 220-pixel review header scroll
area in `workbench_window.cpp` and the toolbar it carries.

## Scope

- The 30-pixel strip as the full-surface asset draws it (full ink, bordered
  buttons, two drawer toggles at the left, modality control alone in the
  centre) with the listed controls; the existing next/previous
  change and file actions move into it with their positions; jump to next new
  comment reads the ledger's unread projection.
- The epoch line over the existing epoch selectors: nodes per upload, labels
  beneath, bracket handles, blue span, amber "my last review" tick with a
  tooltip; drag, click, ⌘-click, right-click menu, hover detail, and keyboard
  stepping of each handle; the named comparisons in a dropdown.
- Modality presets Overview, Read, Review, Discuss with ⌘1 to ⌘4; drawers that
  collapse to 18-pixel rails with a vertical label and an unread count; no
  automatic collapse; a per-window persisted modality.
- The signed-out state shows the device sign-in (code, open browser, cancel)
  in the header without scrolling; the appearance settings panel moves into the
  Settings window drafted in `settings_window.{hpp,cpp}` under `Cmd+,` until the
  RFC-BPR-0018 design replaces it.
- The appearance roles, the diff colour themes and the DiffLinkMap painting are
  not changed by this or any RFC-BPR-0028 slice; the owner tuned them already.
- Core decides, host performs, Qt presents; no new authority in Qt. Owned
  paths: `src/adapters/qt` workbench window, session and device panels, epoch
  line and settings window, `src/app` composition wiring, their focused tests
  and the render evidence they pin.

## Acceptance

- RFC-BPR-0028 AC-01, AC-02, AC-03 and AC-07 at 1280x852 and 1920x1080.
- `qt_workbench_window_scale_{100,125,200}`, `qt_composed_github_scale_100`,
  `application_composition_contract` and the public-path courses that pin
  header render evidence pass; the boundary gate passes.
- Attended capture on the Mac candidate showing the strip, epoch line, a
  collapsed and an open rail, and the device sign-in reachable while signed
  out.

## Notes

- Slice (a), settings window and signed-out sign-in. `settings_window.{hpp,cpp}`
  mounts a modeless Settings window with the seven RFC-BPR-0018 pages under
  `Cmd+,` (View menu, `QAction::PreferencesRole`); the appearance panel is
  adopted onto its Appearance page and leaves `workbenchReviewHeader`. The
  queue landing moves into the files drawer. The device sign-in panel is laid
  first in the header column and is shown only while there is no accepted
  session (`no_profile`, `establishing_session`, `session_failed`,
  `session_cancelled`), so it fits inside the unscrolled 220-pixel header; the
  focus order walks it before the session chrome, matching reading order.
  `qt_composed_github_scale_100` now pins that placement, the fit, the
  hide-on-session and the show-on-establishing transitions;
  `qt_settings_window` pins the pages, the adoption and the shortcut. The
  220-pixel `HeaderScroll` and the toolbar stay until the strip slice replaces
  them, so the `qt_workbench_window_scale_*` link-map evidence and the app
  composition digests are unchanged.
- Slice (a) verification, 2026-09-06: `qt_composed_github_scale_100`,
  `qt_settings_window`, `qt_workbench_window_scale_{100,125,200}`,
  `qt_appearance_*`, `qt_device_authorization_*`, `qt_github_session_*`,
  `application_composition_contract`, the appearance, queue, ledger,
  round-trip and workbench composition contracts, the device authorization
  course, the github.com public path and `architecture_boundaries` pass;
  strict tidy over the touched translation units and `clang-format --Werror`
  are clean; the signed `app-release` bundle builds. The attended capture of
  the new build could not be taken in that session because the Mac's screen
  was locked and the displays asleep; it stays owed to this card's acceptance.
- Slice (b), the strip. `workbench_strip.{hpp,cpp}` replaces the icon toolbar
  with RFC-BPR-0028's 30-pixel row: a `QToolBar` subclass that paints the bar
  and its rule, a `QProxyStyle` its presenters adopt (22-pixel bordered
  buttons, soft accent fill when checked, solid accent for the modality
  segments, disabled ink at 140 alpha rather than the platform's grey, an
  inset focus ring), painted line glyphs in the palette's ink, a divider, and
  the `modality_control`. Row order, left to right: files, identity, rule,
  previous and next change with "change 3 of 11", previous and next file with
  "file 3 of 40", spacer, the modality control, spacer, review details,
  private progress, refresh, comments, rule, account, settings. The change
  position is derived from the comparison surface's held result and
  selection exactly as the host derives the cursor it observes; the file
  position is the core's navigation cursor. Overview, Read, Review and
  Discuss (⌘1 to ⌘4) dispatch two exact `set_workbench_drawer_intent`s
  (Overview: files open, comments closed; Read and Discuss: files collapsed,
  comments open; Review: both open) and only when the core offers the drawer
  commands; the segment stays where it was chosen through a manual toggle.
  The account button reaches the device sign-in while signed out and the
  accounts settings page once signed in; refresh is the session chrome's own
  action. The window's width floor rises from 480 to 820 so no strip control
  ever folds into an overflow menu; the identity yields below 1180, the
  positions below 960, and the modality segments drop the shortcut beside
  their names below 1000. Not in this slice: the "next new comment" group,
  because no unread projection reaches the Qt layer yet (the ledger's
  `UnreadConversations` is never set by the host and the queue's unseen
  count collapses to a bucket); it lands with the unread projection under
  BPR-0648. Rails, the 18-pixel collapsed drawers, Discuss's widening and
  per-window persistence of the modality are the next slice. The
  `workbench_window_tests` widget-count sanity bound moved from a hundredfold
  to a fiftyfold ratio (609 widgets against 60,000 lines); the equalities
  that make the claim are unchanged. The UIforETW capture probe pins the new
  nineteen-entry row; both application digests repinned.
- Slice (b) observation, 2026-09-06, signed `app-release` bundle at 1280x852
  on the dark palette, signed out: the strip is one 30-pixel row under the
  title bar with the files toggle checked at the left, the change and file
  pairs as bordered chevron buttons, the four segments "Overview ⌘1", "Read
  ⌘2", "Review ⌘3" (filled) and "Discuss ⌘4" alone at the centre, and
  review details, private progress with its menu arrow, refresh, comments
  (checked), a rule, account and settings at the right; the identity and
  positions are absent because no pull request is open. The device sign-in
  panel is the top of the header beneath it. Captured with the computer-use
  window screenshot; no image retained.
- Slice (b2), the strip as the title row (owner direction, 2026-09-06: "This
  title bar is wasting a ton of space, even on windows title bars tend to be
  ultra compact now", with Fork, Visual Studio Code on both platforms,
  Windows Terminal, Ghostty and the Claude app as references, Windows Visual
  Studio Code the closest). RFC-BPR-0028's chrome paragraph and AC-01 now say
  the strip is the row the window controls sit in. On macOS the window takes
  `Qt::ExpandedClientAreaHint` and `Qt::NoTitleBarBackgroundHint`, declines
  the safe-area margin, and hides the title text once shown through
  `native_window_chrome.cpp` (the Objective-C runtime from C++, guarded to the
  Cocoa platform so the offscreen tests are untouched; the title stays set for
  the Dock and Mission Control). The strip's first item is a platform inset,
  78 pixels on macOS and zero elsewhere, so the traffic lights sit in the
  strip and Files follows them; the strip tracks a drag that starts on its
  bare surface (spacers, inset, dividers, labels; never a control) and moves
  the window under the hold, and double-clicking that surface zooms and
  restores. Qt's system move was tried first and did nothing for a real
  drag on this Mac, and it cannot be proved offscreen; the tracked drag is
  pinned by `test_strip_is_the_title_row`. The Windows form (compact menu
  at the left, caption buttons at the right, snap layouts through
  `WM_NCHITTEST`) is platform work of its own under BPR-0653. Observed in the
  signed bundle at 1280x820: no title bar, the traffic lights in the strip's
  left inset, Files at 86 pixels, the segments centred, settings at the
  right edge; the window is 32 pixels shorter in chrome than before. A real
  60 by 80 point drag on the spacer moved the window by exactly that, a
  double-click on the same surface zoomed it to the screen and a second
  double-click restored 1280x820 at its previous position.
- Slice (c), the rails. `workbench_drawer_rail` is an 18-pixel button the
  drawer panel shows in its collapsed state instead of leaving the surface:
  a rule on the edge the drawer reopens from, the drawer's name vertically
  ("Files", "Comments") with a summary beside it, a focus ring, and a click
  that the window turns into the drawer's own toggle action, so the core
  still decides. The panel hides everything in its column but the rail and
  fixes its width to the rail's; expanded restores the host's minimum and
  the persisted width. The centre sizing gives a collapsed drawer the rail's
  width. The files rail states the selected file's place among the
  actionable files from the core's cursor; the comments rail states the
  review thread count from the pull request projection; the unread count the
  design names waits for its projection. A restored drawer layout sets the
  modality segment (files open and comments collapsed reads Overview, files
  collapsed and comments open reads Read unless Discuss is showing, both
  open reads Review) until a modality is chosen in the window, after which
  a manual toggle leaves the segment alone. Pins that said a collapsed
  drawer is hidden now say it is its rail: the collapse tests, the restart
  test, the composed agent-surface test, and the UIforETW focused-layout
  probes.
- Slice (e), no review header (AC-01). The 220-pixel scroll area and its
  host widget are gone: the central widget is the splitter, so the chrome
  above the comparison is the strip and the epoch line only. The device
  sign-in, the session chrome (in its compact layout), the queue landing,
  the pull request summary (bounded to 200 pixels) and its description
  (hidden until a pull request is open) live at the top of the files drawer,
  in that order, above the files list, until the repositories drawer
  (BPR-0651) and the accounts page (BPR-0652) take them. The files drawer
  presents expanded whenever the core offers no drawer command, so sign-in
  and the locator can never be shut behind a rail the reviewer could not
  reopen; the core's drawer state is untouched and returns with the
  command. Review details now toggles the epoch line and the selected-change
  details only, and says so. The composed device sign-in test, its render
  crop, the review-details test, the settings window test and the UIforETW
  probes moved with the panels; the probe's "review_details_rect_in_window"
  now measures the epoch line.
  The attended look at this slice in the signed bundle is owed: the screen
  was locked again when it was built (25 of 25 focused selectors and the
  contracts pass).
