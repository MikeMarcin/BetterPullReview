---
schema: "implement-rfc.card.v1"
id: "BPR-1354"
title: "Every tooltip shows next to its control"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","origin":"BPR-1335 owner look, session b4b76c70c8e54f1933410b582189d0e8, BPR-1335-add-button-v002 Accept note","decision_id":"e7d697dd-056d-418f-bc6d-ffa10b11d687","reservation":"docs/issues/_reservations/BPR-1353--BPR-1354--7fb3f9709caa.json","related_cards":["BPR-1335","BPR-1328","BPR-1083"],"acceptance":["BPR-1354:inventory","BPR-1354:one-placement","BPR-1354:position","BPR-1354:control","BPR-1354:guard","BPR-1354:specification","BPR-1354:owner-look"]}
---

# BPR-1354: Every tooltip shows next to its control

## Owner note

On [[BPR-1335]]'s add-button v002 look (session
`b4b76c70c8e54f1933410b582189d0e8`, decision
`e7d697dd-056d-418f-bc6d-ffa10b11d687`, 2026-10-09T05:06:32Z) the owner
accepted the "+" with this note, verbatim:

> bug: Why is this damned tooltip far away again. This is a consistent problem.

The attached screenshot (kept in the session file, not committed) shows the
"+" on line 608 and its "Add comment (↩)" tooltip about three lines lower,
near line 611, and to the right of the button.

It is the third time. [[BPR-1083]] v004 ("The tooltip feels strangely far
away.") added the anchored tooltip for label marks, and [[BPR-1328]] ("Whys
this tooltip so far from the button?") anchored the title row and review bar.
Each fixed the surface the owner happened to be looking at; everything else
still used Qt's placement.

## Cause

`QToolTip` places its window at the pointer plus the platform cursor's
height (`QTipLabel::placeTip`: offset `(2, cursor height)`). On macOS that
offset follows the system cursor size, so with a larger pointer the tip
lands several lines below and to the right of whatever it names. Only
widgets that opted in, one by one, used the anchored tooltip; the "+"
(`diffLineAddButton`, a child of the head pane) never did.

## Scope

Every tooltip the Qt adapter shows, by one placement next to the control's
own rectangle: directly below it, left edges aligned, four pixels apart, or
above it where below would leave the screen.

- Widgets with their own `toolTip()`, anywhere in the application.
- Item views' per-item tooltips (`Qt::ToolTipRole`), tab bars' per-tab
  tooltips, header sections and menu actions: anchored to the item, tab,
  section or action rectangle.
- Painted controls and hover regions with no widget (gutter cells, scroll-bar
  ticks, marks, nodes on the epoch line): anchored to the painted rectangle.
- Programmatic tooltips (`QToolTip::showText`), including the info glyph's
  keyboard popover.

Out of scope: the system tray icon's tooltip, which the operating system
draws.

## Acceptance

- `BPR-1354:inventory`: the card lists every tooltip route found and what
  each now does.
- `BPR-1354:one-placement`: all of them go through one anchored placement;
  no `QToolTip::showText` remains outside it.
- `BPR-1354:position`: a test shows the tooltip's window directly below (or,
  at the screen's bottom edge, directly above) its anchor, left edges
  aligned, for the "+" and for at least two other kinds of control.
- `BPR-1354:control`: the same test fails against the code before the change.
- `BPR-1354:guard`: a repository check refuses a new `QToolTip::showText`
  outside the anchored placement.
- `BPR-1354:specification`: the specification states the rule for every
  tooltip, not only the title row and review bar.
- `BPR-1354:owner-look`: the owner accepts "tooltips appear next to their
  control" on a frozen build.

## Inventory (`BPR-1354:inventory`)

Every route by which the Qt adapter showed a tooltip at the time of the
change (`grep` over `src/` for `setToolTip`, `ToolTipRole`, `setTabToolTip`,
`setToolTipsVisible`, `QToolTip::` and `QEvent::ToolTip`), and what it does
now.

| Route | Where | Before | Now |
|---|---|---|---|
| A widget's own `toolTip()` outside the title row and review bar: about 240 `setToolTip` calls, among them the diff's "+" (`diffLineAddButton`), the open card's buttons and pills, Review Cycles, the share and publish sheets, settings, Home, sign-in | everywhere | `QToolTip` at the pointer plus the cursor's height | Anchored to the widget, by the application-wide placement |
| A child with no tooltip of its own | everywhere | Qt hands the event to the parent, which showed it at the pointer | The parent's tooltip, anchored to the parent |
| Item views (`Qt::ToolTipRole`): Files tree, Files drawer, Home and Review queue, Repositories drawer, conversation finder, settings search results, private review lists, epoch history, combo-box popups (share sheet, private composition editor) | list, tree and table viewports | `QToolTip` at the pointer, bound to the item | Anchored to the item's row |
| Header sections (`headerData`, `Qt::ToolTipRole`): the Overview files' comments column | header viewport | `QToolTip` at the pointer | Anchored to the section |
| Tab bars (`setTabToolTip`): the conversation tray tabs, the document tab strip | tab bar | `QToolTip` at the pointer, bound to the tab | Anchored to the tab |
| Menu actions in menus that show tooltips (`setToolTipsVisible`): the comparison menu, the card menu, the tour editor's menu, the preflight menu | menu | `QToolTip` at the pointer | Anchored to the action's row |
| Updates line: a node, and the history statement elsewhere on the line (`epoch_line::event`) | painted | `QToolTip::showText` at the pointer | `anchor_tooltips` source: below the node and its label, or below the line |
| Info glyph's keyboard popover (`info_glyph::show_popover`) | focus | `QToolTip::showText` at the glyph's bottom-left plus the cursor offset | Below the glyph, held by focus (`anchored_tooltip_hold::focus`) |
| Diff gutter: a thread's tinted number | painted | Anchored ([[BPR-1335]]) | Unchanged |
| Diff scroll-bar ticks | painted | Anchored ([[BPR-1290]]) | Unchanged |
| Conversation cards' marks and rails; open card's marks, rails and avatars; mark preview entries | painted | Anchored ([[BPR-1083]]) | Unchanged |
| Overview chips (pointer), label settings rows, label completion, workshop candidate line, markdown links (pointer), Mark colours link, Overview buttons, label controls | opted in | Anchored | Unchanged |
| Title row and review bar (`anchor_descendant_tooltips`) | opted in | Anchored ([[BPR-1328]]) | The application-wide placement; the per-strip follower is removed |
| Overview chip reached with Tab, a markdown link reached with Tab, "Nothing new in this file." | keyboard | Anchored, but held by the pointer, so gone within 100 ms when the pointer was elsewhere | Anchored and held by focus |
| System tray icon | operating system | Drawn by the operating system | Out of scope |

## Change

- `install_anchored_tooltips()` puts one event filter on the application
  object; it runs by itself when the application object is made
  (`Q_COREAPP_STARTUP_FUNCTION` in `anchored_tooltip.cpp`), so no window,
  test or composition has to opt in, and `src/app` is unchanged. It answers
  every widget's `QEvent::ToolTip` through `standard_tooltip_target`: a
  menu's action, a tab, a header section, an item view's item, or the
  widget's own `toolTip()`. A point that names nothing is left to Qt, which
  hands it to the parent, and the parent comes back through the same filter.
- A widget that paints several controls names each with `anchor_tooltips`
  (now marked so the application filter leaves it to its source). The
  updates line moved from its own `event()` to a source.
- `show_anchored_tooltip` takes an `anchored_tooltip_hold`: `pointer` (as
  before) or `focus`, for tooltips a keyboard reviewer raised.
- A tooltip line longer than about eighty characters wraps instead of
  running across the window (Qt wrapped only past the screen's width); a
  1068-pixel info-glyph sentence is now 566 by 36.
- The resize seams' cursor hold (`resize_handle_cursor.cpp`) watches the
  application for its handle's events, because the tooltip event is now
  answered at the application and never reaches a filter on the handle.
- `architecture_boundaries` refuses `QToolTip::showText` anywhere under
  `src/adapters/qt` and `src/app` (`BPR-1354:guard`).
- The specification states the rule for every tooltip (workbench
  specification, primary actions; a pointer in the conventional comments
  document).

## Controls (failing before)

The new and changed tests, built against main's Qt adapter (`b9aff5200`):

- `qt_anchored_tooltips` (new): 28 failures, every one "the tooltip is the
  anchored one, not Qt's at the pointer" and its position checks, for the
  button, the child without a tooltip, the list item, the tab, the header
  section, the menu action and the button at the screen's bottom edge
  (`anchored=0 qt=1` in each line of its output).
- `qt_diff_workbench_scale_100`: "light: the "+"'s tooltip is the anchored
  one, not Qt's at the pointer" (`anchored=0 qt=1`).


## Checks on the candidate

macOS arm64 Debug (`build/dev`), offscreen, load average 100 to 360 (other
agents' builds). The frontend-design skill was loaded for this slice; the look
is Qt's own tooltip panel, so the work is placement and wrapping only.

- `qt_anchored_tooltips`: every kind directly below its control, left edges
  aligned and 4 px apart: button (53,69 80x25, tip at 53,98), child without
  a tooltip (its parent), list item, tab, header section, menu action; the
  button at the screen's bottom edge gets it directly above (tip bottom 4 px
  over the button). A 150-character tooltip wraps to 566 by 36.
- `qt_diff_workbench_scale_{100,125,200}`, light and dark: the "+"'s tooltip
  is the anchored one, at `anchored_tooltip_geometry` of the button's rect,
  directly below it (moved left only by the offscreen screen's edge).
- `qt_epoch_comparison_scale_{100,125,200}`: the history statement directly
  below the updates line, and a node's tooltip directly below the node and
  its label. `qt_workbench_window_scale_*`: the same statement in the window,
  and BPR-1328's strip test (every title-row and review-bar control) through
  the application-wide placement now that the strip's own follower is gone.
- `qt_appearance_settings_panel`: each info glyph focused from the keyboard
  shows its sentence directly below the glyph (above it for the last one, at
  the screen's edge) with no pointer near, and it goes on focus out.
- Integration pass on the tree rebased onto `bb9053520`, 26 of 26:
  `architecture_boundaries`, `qt_anchored_tooltips`,
  `qt_diff_workbench_scale_*`, `qt_workbench_window_scale_*`,
  `qt_epoch_comparison_scale_*`, `qt_appearance_settings_panel`,
  `qt_feedback_label_controls`, `qt_feedback_label_threads`,
  `qt_composed_github_scale_*`, `qt_github_conversation_panel`,
  `qt_workbench_conversation_reveal`, `qt_workbench_line_draft`,
  `qt_composed_inline_drawer_scale_*`,
  `application_setup_destination_public_path`,
  `application_change_arrows_across_files`,
  `application_review_round_trip_composition_contract`.
- Before the rebase, every `qt_*` row (140) and every `*public_path*` row
  (45) ran. The failures were all present without this change:
  `qt_pull_request_overview_page` and `qt_pull_request_overview_chips_scale_{125,200}`
  ("a refresh keeps the scroll position and the chip", recorded on main by
  [[BPR-1331]]), `qt_workbench_home_discovery_scale_*` (recorded by
  BPR-1338), `qt_appearance_profile` ("the reprojected position keeps its
  comparison and semantic anchor") and
  `application_review_cycle_desktop_public_path` ("no window shortcut claims
  Return or Shift+Return"). The last two fail the same way in another
  agent's fresh build of main (`bpr1356`, `7342fc4f8`), and
  `qt_appearance_profile` and `qt_pull_request_overview_page` failed here
  with the application-wide placement switched off.
- `clang-format` 22.1.8 is clean on the changed files; `clang-tidy` 22
  (`tidy.py --warnings-as-errors`) passes on the changed sources and the new
  test. Nothing under `src/app` changed, so the round-trip pins stand.
- The build folder briefly had two builds in it (another agent stopped the
  first by pattern); everything that ran was rebuilt afterwards, and the
  frozen build is made from the commit in its own root.
