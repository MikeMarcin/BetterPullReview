---
schema: "implement-rfc.card.v1"
id: "BPR-1784"
title: "The tour wizard shows its three steps: Problem, Solution, Stops"
status: "Done"
rfcs: []
depends_on: ["BPR-1783"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1782--BPR-1784--53c6249f60af.json","kind":"feature","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0039","RFC-BPR-0085","RFC-BPR-0092"],"related_cards":["BPR-1776","BPR-1781"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "eae53b8d951f02442b59f1b252155992360e2a0d", "evidence": {"kind": "command", "detail": "core_tour_steps (tests/core/tour_steps_tests.cpp) pins decide_tour_steps: an empty or absent draft opens on Problem; written Problem/Solution are done and the next unwritten step opens; the open step is current whatever its progress; a step passed unwritten needs attention while one after the open step stays to do; Stops are done only when there is a stop and every one is Ready, need attention for a needs-attention stop or a website conflict; next_tour_step order and stable names. Passes; core_overview_tour still passes."}}, {"criterion": "AC2", "outcome": "pass", "revision": "eae53b8d951f02442b59f1b252155992360e2a0d", "evidence": {"kind": "command", "detail": "qt_review_tour_steps (tests/adapters_qt/review_tour_steps_tests.cpp) drives review_tour_editor with edits applied by core::apply: three numbered step headers; states follow the draft; a written Problem stays open until Next; Next folds it with Change and puts the keyboard in Solution's text; Change and a header reopen a step with focus in its text; Next from Solution opens Stops with Add stop focused; a new untitled stop takes focus in its title (BPR-1783); folded Stops say '1 of 2 stops ready'; all Ready marks Stops done with Preview and publish enabled; a Problem passed unwritten needs attention and still opens; the tour page's steps choose the section; another draft opens on its first unwritten step; the open Problem step fits a 280 pixel drawer; drawer principles hold. Passes."}}, {"criterion": "AC3", "outcome": "pass", "revision": "eae53b8d951f02442b59f1b252155992360e2a0d", "evidence": {"kind": "command", "detail": "All 37 tests matching tour, qt_drawer_parts, qt_drawer_surfaces_fit, qt_workbench_window_scale and qt_hide_drawers_native_menu_key (excluding unbuilt sqlite and composed-drawer targets) pass at this revision, including application_review_tour_composition_{public_path,scale_125,scale_200} (seed_route's title focus), application_review_tour_cold_import_public_path, all application_review_tour_publication_*, application_review_tour_writing{,_wrapper}, application_review_tour_cli_ordinary_{native,stop}, qt_review_tour_publication_working, qt_workbench_window_scale_{100,125,200} and qt_drawer_surfaces_fit_scale_{100,125,200}; application_review_tour_host and the core tour tests pass."}}, {"criterion": "AC4", "outcome": "pass", "revision": "eae53b8d951f02442b59f1b252155992360e2a0d", "evidence": {"kind": "artifact", "detail": "Design frames before code: docs/design/BPR-1784-tour-steps-v1/ (index.html, frames/*-light.png and *-dark.png for empty, problem, stops, ready, attention). Offscreen renders from qt_review_tour_steps, light and dark, saved outside the repository in the session scratchpad bpr-1784/: empty, empty-280, problem-done, stops-in-progress, ready-to-publish, needs-attention, page."}}, {"criterion": "AC5", "outcome": "pending", "revision": "eae53b8d951f02442b59f1b252155992360e2a0d", "evidence": {"kind": "observation", "detail": "Owner's look deferred (\"leave the looks for later\"); no attended-review session opened."}}]
---

# BPR-1784: The tour wizard shows its three steps

## Request

The owner, 2026-10-27, verbatim:

> the tour wizard should borrow some inspiration from the agent setup
> wizard... make it more clear that the steps are fill out problem, fill out
> solution, fill out stops.

## Grounded current behavior

The Tour tab (RFC-BPR-0085 D4) is a route: the brief (Problem, Solution) as
one compact card that opens in place, then the stops on an amber route line.
Nothing says there is an order or how far along the tour is. The agent setup
(RFC-BPR-0092, `agent_setup_view.cpp`; design frames
`docs/design/BPR-1747-agent-setup-v1/`) is a numbered list of steps: a
circle per step (number, green tick when done, amber when it needs
attention), the current step open with its controls, later steps quiet, done
steps folded to one line with Change.

## Outcome

- The tour reads as three numbered steps in the agent setup's visual
  language: **1 Problem**, **2 Solution**, **3 Stops**. Each shows its state
  (to do, current, done with a tick, needs attention), the current one open,
  done ones folded to one line with what was written and Change.
- Step 3 holds the existing stops route (numbered amber stops, readiness
  rings), so stop numbers and step numbers must not be confused (e.g. the step
  circle is neutral/green, stops stay amber).
- Moving on is explicit (Next, or ⌥↩ as the stops use) and never blocks
  editing an earlier step.
- Publish stays at the foot once all three are done enough to publish
  (existing readiness rules); [[BPR-1782]]'s feedback applies there.
- Works in the drawer and on the expanded tour page; [[BPR-1781]] may also
  show the tour on Overview, which reads the same step states.
- Load `frontend-design:frontend-design` first; design frames before code
  (the agent setup's frames are the model).

## Acceptance

- Qt tests: the three steps and their states follow the draft; done steps
  fold and Change reopens; focus lands in the current step's first field
  (Problem's text, Solution's text, a new stop's title per [[BPR-1783]]).
- Offscreen renders light and dark: empty tour, Problem done, both done with
  stops in progress, ready to publish.
- The owner accepts the look on a frozen build (attended review).

## Built

- **Core.** `bpr/core/tour_steps.hpp`: `tour_step` (problem, solution,
  stops) and `tour_step_state` (to do, current, done, needs attention), both
  closed enums whose counts are pinned by `static_assert`.
  `decide_tour_steps(draft, open)` is pure: Problem and Solution are done
  once they hold more than whitespace. Stops are done once there is at least
  one and every one is Ready. They need attention while any stop does or a
  website conflict is held. The open step is current. A step before it that
  is still unwritten needs attention; one after it is to do. With no open step
  given, the first unwritten step is chosen.
- **Qt** (`review_tour_editor`). The brief card is replaced by three step
  blocks in the agent setup's language (`tour_step_row`, `tour_steps_frame`).
  Each block has a numbered circle and a neutral rail: the accent while open,
  a green tick once done, a red ring with "!" when it needs attention, quiet
  while to do. Circles are never amber, so they are not mistaken for stops.
  Done steps fold to one line with what was written and Change; any header
  opens its step. Problem and Solution each have **Next**, bound to the same
  Alt+Return as a stop's Ready, next (shortcut registry entry
  `tour_ready_next`). Moving on puts the keyboard in the open step's first
  field: Problem's text, Solution's text, a new stop's title, or Add stop
  when there is none. Step 3's body is the existing amber route, and choosing
  a stop opens it. Another draft opens on its first unwritten step; after
  that only the author moves between steps. On the tour page the same steps
  choose the section, and the open step survives Expand and Back. The route
  now lays out its column when its height changes, because it sits several
  widgets deep. Publish, readiness rules and BPR-1782's waiting panel are
  unchanged. Widgets perform no effects. `frontend-design:frontend-design` was
  loaded first.
- **Docs.** RFC-BPR-0085 D4 is amended to describe the three steps.

## Not verified

- The owner's look on a real display, light and dark (deferred, pending).
- Overview's read-only tour summary (BPR-1781) does not show the step
  states yet. `decide_tour_steps` is available to it.
- The Alt+Return key on Next was not pressed through the native shortcut
  map in a test. The test checks the binding and presses the button.
- `sqlite_tour_owner_channel` and `sqlite_review_tour_storage` were not built
  or run because disk was low. Their code is unchanged. The composed
  inline-drawer scales (`qt_composed_inline_drawer_*`) and coding-tidy were
  not run.
- Windows and Linux were not run; macOS offscreen only.
