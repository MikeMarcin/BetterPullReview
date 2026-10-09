---
schema: "implement-rfc.card.v1"
id: "BPR-1342"
title: "Open a review, or open it in a new window, from Home and its row menu"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","found_by":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1342--BPR-1342--e04e479ee80b.json","related_cards":["BPR-1338","BPR-1017","BPR-0987","BPR-0776","BPR-0736"],"related_rfcs":["RFC-BPR-0024","RFC-BPR-0026","RFC-BPR-0033"]}
acceptance: [{"criterion":"BPR-1342:exists","outcome":"pass","revision":"05d3ab05d","evidence":{"kind":"observation","detail":"The card's 'What exists (inventory at 7e0c3e1a5)' records how Home opened a pull request before: queue click/Return in this window on Overview (BPR-0987 return), Recently viewed Open and Shift+Return Resume, row menus with only Open on GitHub (and Resume), New Window opening Home with no pull request carried."}},{"criterion":"BPR-1342:menu","outcome":"pass","revision":"05d3ab05d","evidence":{"kind":"command","detail":"qt_workbench_open_review_from_home_scale_{100,125,200}: a queue row's and a Recently viewed row's HomeRowMenu show Open Review (default, bold, Return), Open Review in New Window (Cmd+Return), Resume (Recently viewed only, Shift+Return), then Open on GitHub (Shift+Cmd+O), each naming its row; renders of both menus inspected. Failing-before control: menu items removed fails 'the menu is Open Review (the default), Open Review in New Window, Resume, then Open on GitHub'."}},{"criterion":"BPR-1342:keys","outcome":"pass","revision":"05d3ab05d","evidence":{"kind":"command","detail":"qt_workbench_open_review_from_home_scale_*: Return and a plain click open here; Cmd+Return, Cmd-click and middle-click ask for a new window and keep the row selected on queue and Recently viewed rows; Shift+Return still resumes. Controls: queue Cmd+Return ignored, Recently viewed Cmd+Return ignored, pointer handling off each fail at their named assertion."}},{"criterion":"BPR-1342:composition","outcome":"pass","revision":"05d3ab05d","evidence":{"kind":"command","detail":"application_personal_discovery_public_path through the real profile desktop: Open Review in New Window from the Following row's menu and Cmd+Return on its Recently viewed row each admit one more window of the same profile and runtime owner, which reads PR 17 and lands on Overview while the first window keeps its view; core_markdown_navigation refuses another host, another account, no account, unsafe names and an account's reading without its session; application_profile_management_host carries the review only on the fresh window's start. Controls: host drops the review, desktop never opens it, composition refuses every request, core without account or authority check each fail."}},{"criterion":"BPR-1342:same-pr","outcome":"pass","revision":"05d3ab05d","evidence":{"kind":"command","detail":"application_personal_discovery_public_path: with PR 17 open in the first window, Open Review in New Window opens a second view of it in a new window and the first window keeps its view, per RFC-BPR-0024; Open Review here keeps BPR-0987's return without reading again."}},{"criterion":"BPR-1342:failing-before","outcome":"pass","revision":"05d3ab05d","evidence":{"kind":"command","detail":"Nine mutation controls listed in the card's Evidence each fail at the named assertion and were restored byte for byte; application_review_round_trip_composition_contract resealed and passing at the rebased head, with architecture_boundaries, core_dependency_boundary, coding-format-check and git diff --check. Uncovered: restored-window attachment (recorded)."}},{"criterion":"BPR-1342:owner-look","outcome":"pass","revision":"05d3ab05d","evidence":{"kind":"observation","detail":"Owner accepted BPR-1342-open-review-from-home-v001 ('open review from Home', the one question set, including the second-view decision) with no note: decision f6d8c390, event 4, handed back at 5, session 3af3750636d854bea2950b78926d045a, on the frozen build of f880a6238 (executable SHA-256 942546768d801ed2...), the same change landed as 6374333dc and 05d3ab05d."}}]
---

# BPR-1342: Open a review, or open it in a new window, from Home and its row menu

## Owner notes

Owner, chat, 2026-10-08, verbatim: "I should be able to open a review and open
a review in a new window from the home, including from right click on a pr".

## What exists (inventory at `7e0c3e1a5`)

- **Queue rows.** A click, or Return/Enter on the current row
  (`home_list::eventFilter`, [[BPR-0736]]), opens the row's pull request in
  *this* window on Overview (`workbench_window::open_pull_request_item`). It
  replaces whatever pull request the window had; a row naming the pull request
  the window already shows returns to it without reading again ([[BPR-0987]]).
  Return with any modifier opens the same way. Middle-click and ⌘-click had no
  gesture of their own.
- **Recently viewed rows.** Activation or Return opens on Overview in this
  window; Shift+Return resumes the saved reading
  (`workbench_window::dispatch_recent_review`).
- **Row menus** ([[BPR-1338]]). A queue row's right click shows one item, Open
  on GitHub. A Recently viewed row's menu (`Qt::ActionsContextMenu`) shows
  Resume and Open on GitHub. Neither offers opening the review, and no menu
  item shows a key.
- **New windows.** Profiles › New Window (and ⌘N on the empty desktop's bar)
  dispatches `application::new_bpr_window_intent` to the profile desktop, which
  asks the profile management host for a fresh window of the same profile
  under its one runtime owner (RFC-BPR-0024). The new window opens Home
  ([[BPR-1017]]: already signed in, with the queue). Nothing carries a pull
  request into it; there are no tabs (RFC-BPR-0026's tabs were superseded by
  the Home direction).
- **The same pull request in two windows.** RFC-BPR-0024 allows it: "Two
  windows in one profile may show the same PR at different epochs or
  positions", and "an explicit New Window request always creates another
  window".

## Scope

- Home's row menu, for queue rows and Recently viewed rows, holds **Open
  Review** (the default, in this window), **Open Review in New Window**, Resume
  (Recently viewed only), then Open on GitHub. Each item shows its key.
- Keys on a current row: Return opens; ⌘Return (Ctrl+Return off macOS) opens in
  a new window; ⇧Return still resumes a Recently viewed row. ⌘-click and
  middle-click on a row open it in a new window.
- The new window is the existing New Window path in the same profile: the same
  intent, the same management host reservation and the one runtime owner. The
  request carries one admitted destination; the new window opens Home and then
  opens that pull request through its Home bar's own path, on Overview.
- **Same pull request already open elsewhere: a second view.** Open Review in
  New Window always opens another window, even when this or another window
  shows that pull request, because RFC-BPR-0024 says an explicit New Window
  always creates one and two windows may show the same pull request at
  different positions. Open Review in this window keeps [[BPR-0987]]: a window
  already showing it returns to it without reading again.

## Acceptance

- `BPR-1342:exists`: the card records how Home opened a pull request before.
- `BPR-1342:menu`: a queue row's and a Recently viewed row's menu offer Open
  Review (default) and Open Review in New Window with their keys, before Open
  on GitHub; each names the row it was opened on.
- `BPR-1342:keys`: Return opens, ⌘Return opens in a new window, ⌘-click and
  middle-click open in a new window, on queue and Recently viewed rows.
- `BPR-1342:composition`: through the real profile desktop, a new window of the
  same profile opens the row's pull request; no second profile owner, the first
  window keeps its view; a row on another host or account is refused.
- `BPR-1342:same-pr`: with the pull request open in the first window, Open
  Review in New Window opens a second view of it and leaves the first as it was.
- Each test fails against the code before the change. The round-trip
  composition pins are resealed.
- `BPR-1342:owner-look`: the owner accepts "open review from Home" in one short
  attended session on a frozen build.

## Implementation — 2026-10-08 (macOS)

The `frontend-design` skill was loaded before the menu work.

- **The menu.** `HomeRowMenu` is Home's one row menu for both lists. A queue
  row's right click and, now, a Recently viewed row's (`Qt::CustomContextMenu`
  instead of `Qt::ActionsContextMenu`) make the row current and open it:
  Open Review (`homeOpenReviewAction`, the default, drawn bold, ↩), Open Review
  in New Window (`homeOpenReviewInNewWindowAction`, ⌘↩), Resume (⇧↩, shown
  only on a Recently viewed row), a separator, Open on GitHub (⇧⌘O). The keys
  show in the context menu on macOS too (`setShortcutVisibleInContextMenu`).
  The menu's shortcuts are `Qt::WidgetShortcut` on the menu alone, so they
  never act from the window; the lists read the keys themselves. Resume is
  hidden only while the menu is open on a queue row, because a hidden action
  is a disabled one and Recently viewed's hover button follows it.
- **Keys and pointer.** `home_list` reads ⌘Return on a queue row and ⌘-click /
  middle-click on queue and Recently viewed rows (`read_new_window_click`: the
  press makes the row current, a release on the same row opens it, the whole
  gesture is taken so a click does not also open here and ⌘ does not toggle
  the selection away). The window reads ⌘Return on Recently viewed beside its
  Return and ⇧Return.
- **Admission.** A gesture names a row: `application::review_window_intent`
  is a queue row's `core::pull_request_ref` or a Recently viewed row's place.
  The window's composition admits it into a `core::window_review_destination`
  (`core::admit_queue_review_window`: the accepted account's row on the
  accepted host, safe names; `core::admit_recent_review_window`: the rule
  Recently viewed's Open follows, safe names). Open Review in New Window is
  enabled exactly when Profiles › New Window is.
- **The window.** `github_profile_runtime::ReviewWindowRequest`, set by the
  profile desktop, dispatches the profile's ordinary
  `new_bpr_window_intent`, now carrying `Review`. The management host attaches
  it to that one fresh window's start command (never a restored one). The
  desktop keeps it on the window entry and, after the window opened Home,
  calls `github_application::open_window_review`, which re-admits the parts,
  requires the window's own session to match an account's destination, and
  submits the canonical address to that window's Home bar path
  (`setup_destination_host::submit`), so it lands on Overview as a pasted link
  does. One profile, one runtime owner; nothing new reaches GitHub.
- **Same pull request.** Always a second view in a new window (RFC-BPR-0024);
  Open Review here keeps BPR-0987's return without reading again.
- Specification: Home's "Opening a row" and the Profiles paragraph. Threat
  model: Open Review in New Window takes no text.

## Evidence (macOS, `build/dev` Debug, offscreen, serial)

At the rebased candidate (`f880a6238` over `9a8208fba`):

- `core_markdown_navigation`: queue and recent admission into a new window's
  destination; another host, another account, no account, unsafe names, an
  account's reading without its session are refused.
- `application_profile_management_host`
  (`test_new_window_carries_its_review_to_the_fresh_window_only`): the review
  rides only the fresh window's start; a plain New Window and Open carry none.
- `qt_workbench_open_review_from_home_scale_{100,125,200}` (new rows): the
  menu's order, default, texts and keys on a queue row and a Recently viewed
  row; each item names its row; Return and a plain click open here; ⌘Return,
  ⌘-click and middle-click ask for a new window and keep the row selected;
  ⇧Return still resumes; a refusal and an unavailable New Window are said and
  ask for nothing. Renders of both menus were inspected (↩, ⌘↩, ⇧↩, ⇧⌘O).
- `application_personal_discovery_public_path`, through the real profile
  desktop: with PR 17 open in the first window, Open Review in New Window from
  the Following row's menu and ⌘Return on its Recently viewed row each admit
  one more window of the same profile and owner, which reads PR 17 itself and
  lands on Overview, while the first window keeps its view; no mutation.
- `qt_workbench_open_on_github_scale_{100,125,200}`,
  `qt_composed_github_scale_{100,125,200}`, `qt_home_list_scale_*`,
  `application_profile_multi_window_public_path`,
  `application_profile_management_public_path`,
  `application_setup_destination_public_path`,
  `application_profile_window_review_restoration_public_path`,
  `application_review_round_trip_composition_contract` (resealed),
  `application_composition_contract`, `architecture_boundaries`,
  `core_dependency_boundary`, `coding-format-check` and `git diff --check` pass.
- The composed and restoration courses caught a real defect in the first
  candidate (hiding Resume disabled it); fixed before commit `f880a6238`.
- An uncommitted smoke under the attended launch's fixture options
  (`github-com`, `RepeatReadSessions`) opened PR 17 in a new window twice in a
  row, so the owner-look build can do what it asks.

**Failing-before controls** (each a mutation of the candidate, restored byte
for byte, failing at the named assertion):

- menu items removed: "the menu is Open Review (the default), Open Review in
  New Window, Resume, then Open on GitHub";
- queue ⌘Return ignored: "⌘Return opens the current row in a new window";
- Recently viewed ⌘Return ignored: "⌘Return opens a Recently viewed row in a
  new window";
- pointer handling off: "⌘-click opens the row in a new window, not here";
- management host drops the review: "the fresh window's start carries the
  review, in the same profile";
- desktop never opens the carried review: "the new window opens the row's pull
  request on its own";
- composition refuses every request: "Open Review in New Window from a queue
  row's menu admits one more window";
- core without the account check: "a queue row needs an accepted account";
- core without the authority check: "another account's reading does not open
  a new window".

Uncovered: a mutation that also attaches the review to restored windows still
passes (the host test has no retained-window case).

## Pending

- Windows and Linux runs of the new Qt rows and the discovery course. They
  remain owed and do not block; a failure there opens a new card against this
  change.

## Owner look — prepared 2026-10-08

- Frozen build of `f880a6238` (source export and Debug tests-on build inside
  `/Users/jaeger/.mask/attended-review/bpr1342-open-review-v001`;
  `better_pull_review` sha256 prefix `942546768d801ed2`). It is started with
  `python3 /Users/jaeger/.mask/attended-review/bpr1342-open-review-v001/launch.py`.
  The launcher checks the build and fixture tree and reports an exit within
  seconds. An offscreen smoke run reached `ATTENDED_PULL_REQUEST_OPENED`.
- New short session `3af3750636d854bea2950b78926d045a`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1342-open-v001/state`,
  host detached. One Accept/Reject candidate, ready at event cursor 3:
  `BPR-1342-open-review-from-home-v001` ("open review from Home"). Its text
  states the same-pull-request decision (a second view) and invites a Reject
  note if the owner wants focus instead.
- Integration: rebased onto `be0c3315f` (BPR-1327, which resealed the pins
  over BPR-1340); the pins were resealed again over it. At the rebased head
  `core_markdown_navigation`, `application_profile_management_host`,
  `qt_workbench_open_review_from_home_scale_{100,125,200}`,
  `qt_workbench_open_on_github_scale_100`, `qt_composed_github_scale_100`,
  `application_personal_discovery_public_path`,
  `application_profile_multi_window_public_path`,
  `application_review_round_trip_composition_contract`,
  `architecture_boundaries` and `core_dependency_boundary` pass. The frozen
  build is `f880a6238`, the same BPR-1342 change on `9a8208fba`.
- Next: on hand-back, read `status --after 3`. Accept closes
  `BPR-1342:owner-look`; a Reject's note is the next round's change.

## Owner decision, session 3af37506 (2026-10-08)

Session `3af3750636d854bea2950b78926d045a`, read from cursor 3 to event 5
(handed back).

| Candidate | Verdict | Note |
|---|---|---|
| `BPR-1342-open-review-from-home-v001` | Accept (`f6d8c390`, 00:36:55Z) | (none) |

The verdict was given on the frozen build of `f880a6238` (executable SHA-256
`942546768d801ed2…`), the same BPR-1342 change that landed on main as
`6374333dc` and `05d3ab05d`. It satisfies `BPR-1342:owner-look`, asked as one
question: "open review from Home", including the second view of a pull request
already open. The other criteria stand on their recorded evidence at
`05d3ab05d` (Evidence, above).
The card is Done.

The session's candidate was released (`resume-development --all`, event 6).
The host was already stopped. The frozen root
`/Users/jaeger/.mask/attended-review/bpr1342-open-review-v001` was deleted.

## The Review Cycles shortcut walk (bpr-main-linux, 2026-10-09)

`application_review_cycle_desktop_public_path` failed on main at "no window
shortcut claims Return or Shift+Return". This card's row menu gives
`homeRecentResumeAction` a `Qt::WidgetShortcut` ⇧Return on Home's list
(`workbench_window.cpp`). The test walked every `QAction` of the window
regardless of shortcut context, but a key bound to one widget is not a window
shortcut. Test-only fix in `tests/end_to_end/review_cycle_desktop_public_path.cpp`:
- **The walk:** it still refuses any window- or application-scoped Return or
  Shift+Return. `Qt::WidgetShortcut` and `Qt::WidgetWithChildrenShortcut`
  actions are set aside.
- **Set-aside actions (two found, `homeRecentResumeAction` among them):** each
  must be bound to no text field (line edit, text edit, plain text edit), and
  not to the code view or a widget holding it.
- **Keystrokes:** Return and Shift+Return typed in a text field, and Return
  in the reading pane, must fire none of them.

Controls, each reverted:
- Binding the action to the code view fails "a widget-scoped Return key
  belongs to no text field and does not hold the code".
- Making it window-scoped did not make the keystroke check fail, because a
  line edit claims Return itself. The structural check is the guard against
  that regression; the keystroke check is kept as a smoke test.

Linux passes.

## Windows: the course's identity check and the title row's elision (bpr-main-win, 2026-10-09)

`application_personal_discovery_public_path` failed on Windows ("the first
window has the pull request open behind Home") at every revision tried from
`772200c95` on.
- **Cause:** at the course's window width, the title row's elision
  ([[BPR-1324]]) shows `octo-org/review-repository  #17` without the title,
  because Windows' fonts leave it less room. The course required the label's
  text to start `…#17 ` with the title after it.
- **Fix (test-only):** the first window's check and the new window's wait now
  read the label's tooltip, which always names the whole pull request. The row
  passes 3 of 3 on Windows (Debug).
