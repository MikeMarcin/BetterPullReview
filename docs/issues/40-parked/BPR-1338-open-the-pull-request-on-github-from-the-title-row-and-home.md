---
schema: "implement-rfc.card.v1"
id: "BPR-1338"
title: "Open the pull request on GitHub from a button beside its title, and from Home's rows"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","found_by":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1338--BPR-1338--5020946253b6.json","related_cards":["BPR-1129","BPR-1251","BPR-1255","BPR-1324","BPR-0776","BPR-0280"]}
parked_on: "human-review"
---

# BPR-1338: Open the pull request on GitHub from a button beside its title, and from Home's rows

## Owner notes

Owner, chat, 2026-10-08, verbatim: "I think an open in browser button next to
the pr title would be nice".

Scope extension, the same day, verbatim: "Open in GitHub from the home page on
a pr would also be useful".

## What exists

Nothing opens the pull request itself on GitHub. Links inside a description or
a conversation open through the window's link admission
(`core::admit_markdown_link`, `github_window_read_host::open_markdown_link`),
and the device sign-in opens its verification page. There is no menu item, no
title-row control and no row gesture for the pull request's own page.

## Scope

- **Title row.** A small icon button directly after the pull request's name,
  tooltip "Open on GitHub (⇧⌘O)" (Ctrl+Shift+O off macOS; the key is free in
  the shortcut set). The same window action is on the Pull Request menu.
- **Home.** Home's pull-request rows (every state panel and group) carry "Open
  on GitHub" in their context menu and as a small icon on the hovered row and
  on the current row while its list has focus. The window action and its key
  act on the focused row when Home shows.
- **Address.** The address is never taken from remote or typed text. The
  application host builds `https://{host}/{owner}/{repository}/pull/{number}`
  from the accepted profile authority's hostname and the accepted read model,
  through the same admission the window's links use (safe authority, safe
  owner and repository names, a number). A Home row is admitted only when its
  host and account are the window's accepted session's. The system browser
  opener still refuses anything that is not an absolute HTTP(S) address with a
  host and no userinfo. Opening reaches no network of its own. Public preview
  works the same way.
- **Width.** The button joins the title row's yield model (BPR-1251, BPR-1255,
  BPR-1324). It is counted in the name's room, so the title elides around it
  and yields once, then the owner, then the modality's shortcuts. No control
  folds into an overflow menu.

## Acceptance

- `BPR-1338:exists`: the card records that no "Open on GitHub" existed.
- `BPR-1338:admission`: core and host tests show the address is the canonical
  pull-request page built from the accepted authority, refused for an unsafe
  host, owner or repository, stale for an old window publication, and
  unavailable without a browser; a Home row on another host or account is
  refused. Each fails against the code before the change.
- `BPR-1338:title-row`: the button sits right after the name, carries the
  tooltip and accessible name, triggers one admitted open, and is absent with
  no pull request open. `qt_workbench_title_row_scale_{100,125,200}` and
  `qt_workbench_window_scale_{100,125,200}` pass on macOS.
- `BPR-1338:home`: a queue row's context menu and hover icon open that row's
  pull request; the key opens the focused row. The Home rows pass.
- `BPR-1338:owner-look`: the owner accepts the open-in-browser button
  and Home's open in one short attended session on a frozen build, and each
  opens the pull request in the owner's browser (v001 rejected; now
  `open-in-browser-v002` and `home-open-v002`).
- Windows and Linux runs of the title-row rows are owed, not gating this card's
  macOS close; BPR-1324 is changing the Windows title row at the same time.

## Implementation — 2026-10-08 (macOS)

The `frontend-design` skill was loaded before any of the visual work.

**What existed.** Nothing opened the pull request's own page: no menu item, no
title-row control, no ⌘-click on the name and no Home row gesture. Body links
went through `core::admit_markdown_link`.

**Design.**

- *Title row.* A borderless icon button right after the name: a page whose
  corner an arrow leaves (`strip_glyph::open_on_github`), in the strip's ink.
  It has no frame, like the modality's segments, so it reads as part of the
  name and not as another control. Tooltip "Open on GitHub (⇧⌘O)", from the
  key's native text. It is the window action `openOnGithubAction`
  (Ctrl+Shift+O, ⇧⌘O on macOS; free in the shortcut set, now asserted
  unique). The action is also first on the Pull Request menu. The button shows
  only while an accepted GitHub pull request's name does: never for a local
  cycle, never on Home, never before acceptance. Nothing in the row reads as
  disabled (RFC-BPR-0028).
- *Yield model.* The button goes with the name. It is counted in the name's
  room (`strip_identity_budget` measures the toolbar with it), so the title
  elides around it and yields once, then the owner, then the modality's
  shortcuts. It stays beside the number down to the floor. No control folds
  into an overflow menu.
- *Home.* Each queue row (every state panel, band and group) shows the same
  icon on hover and while it is current in a focused list. It sits in a side
  column the row keeps for it (`home_row_role::icon_action`), so no text runs
  under it and nothing reflows on hover. A right click makes the row current
  and opens a menu with the window's action. A Recently viewed row's menu
  holds the action too, beside Resume. The key acts on the row in the list the
  keyboard was last in.
- *Admission.* A gesture names a pull request, never an address:
  `application::pull_request_page_intent` is the window publication
  (`core::markdown_context`), a queue row's `core::pull_request_ref` or a
  Recently viewed row's place. The core admits the page's parts
  (`admit_pull_request_page`, `admit_queue_pull_request_page`,
  `admit_recent_pull_request_page`) with the body links' safe-authority and
  safe-name rules. A queue row must be on the accepted session's host and
  account; a Recently viewed row on that host, and an account's reading only
  under that account's exact authority. The hostname is always the accepted
  profile's. The application host builds `https://{host}/{owner}/{repo}/pull/{n}`
  (`application::pull_request_page_address`), the address the provider
  contract requires a pull request's `html_url` to equal. The read model
  stores no `html_url`, and remote text never becomes the address. The system
  browser opener still refuses anything but absolute HTTP(S) with a host and
  no userinfo. The core holds no scheme literal, so `core_dependency_boundary`
  and `architecture_boundaries` pass.
- *Public preview.* The window path needs only the read host's accepted
  authority and publication, which a public-preview session has. A public
  Recently viewed reading opens on the session's host.

**Found on the way: the composition dropped the window publication.**
`publish_review_conversations` and `refresh_review_conversations` in
`github_application.cpp` replace the read host's pull-request projection with
`review_comment_placement::project`, which carries no `MarkdownContext`. In the
real composition every window therefore published no context. Open on GitHub
stayed disabled; the end-to-end course showed `enabled=0` before the fix. Body
links should have hit the same "unavailable" answer. The composition now keeps
the host's context across the re-projection.

**Observation, not changed.** In `application_personal_discovery_public_path`,
leaving Home with `choose_home(false)` left the name hidden at the window's
width until the next resize. Leaving by a modality after a resize shows it.
The button goes with the name, so it hid too. This is recorded only; it may
predate this card.

## Evidence (macOS, `build/dev` Debug, offscreen, `-j1`)

- `core_markdown_navigation`: page, queue and recent admission. Unsafe hosts,
  owners and repositories, other host or account, malformed "owner/name",
  numbers out of range, other-account reading and an absent browser are all
  refused.
- `application_github_window_read_host`
  (`test_pull_request_page_admission`): opened with exactly
  `https://github.com/owner/repo/pull/42` while the title and body hold
  address-like text. Stale for another window, for an old publication and
  after replacement. Unavailable without a browser; a browser failure is
  explicit.
- `qt_composed_markdown_{1_0,1_25,2_0}`: the composed title-row button opens
  `https://ghe.example.invalid/owner/repo/pull/42` through the real read host.
  The old gesture is stale after a new publication.
- `qt_workbench_open_on_github_scale_{100,125,200}` (new rows): button
  placement, tooltip, icon, accessible name, menu, click and key. A refusal is
  said in the status bar. The button is absent with no pull request, before
  acceptance, in a local cycle and on Home. On Home: the hover icon inside the
  row at its right, its click, the right-click menu and the key on the
  current row; a Recently viewed row; the last list wins.
- `qt_workbench_title_row_scale_{100,125,200}` and
  `qt_workbench_window_scale_{100,125,200}`: 6/6. The width sweep now
  publishes an accepted pull request, so the button is in the row at every
  width down to the floor. The title still yields once, then the owner, and
  1920 and 1280 still show "Fix the". The title-row order lists
  `workbenchOpenOnGithubAction` after the name.
- `qt_home_list_scale_*`, `qt_composed_github_scale_*`,
  `application_setup_destination_public_path` and
  `application_profile_window_review_restoration_public_path` pass.
- `application_personal_discovery_public_path`, through the real composition
  and the recording browser: the title-row button opens
  `https://github.com/octo-org/review-repository/pull/17`, a Needs me row
  `/pull/4003` and the Recently viewed row `/pull/17`.
- `core_dependency_boundary`, `architecture_boundaries`,
  `coding-format-check` and `git diff --check` pass.
- Failing-before controls (mutations, restored byte for byte): with the button
  left out of the row, `qt_workbench_title_row_scale_100` fails at the
  title-row order and the open-on-GitHub row at "the button sits right after
  the name". Without the publication check, the host test fails at "another
  window cannot borrow this page". Without the account check, the core test
  fails at "another account's row is refused". Without the row icon, the
  open-on-GitHub row fails at "the row's Open on GitHub icon waits…". Without
  the composition fix, the end-to-end course fails at "the open pull request
  offers Open on GitHub".
- Pre-existing, not this card: `qt_workbench_home_discovery_scale_*` fails at
  "the empty Needs me lane is not shown while nothing is saved". A
  `bpr-1336-zoom` binary built without this change fails the same way.

## Pending

- `BPR-1338:owner-look`: one short attended session on a frozen build, with
  `open-in-browser-v002` and `home-open-v002` (v001 rejected; see below).
- Windows and Linux runs of the title-row and window rows. On Windows the
  button takes about 26 px more of the name's room. BPR-1324 is changing the
  Windows title row (shortcuts drop below 1300 px), so "the whole name fits at
  1280" and the Windows floor need a run after both land.

## Owner look — prepared 2026-10-08

- Frozen build of `aebf45eb4` (source export and Debug tests-on build inside
  `/Users/jaeger/.mask/attended-review/bpr1338-open-on-github-v001`;
  `better_pull_review` sha256 prefix `98f52cc292a9b0cb`). It is started with
  `python3 /Users/jaeger/.mask/attended-review/bpr1338-open-on-github-v001/launch.py`.
  The launcher checks the build and fixture tree and reports an exit within
  seconds. An offscreen smoke run reached `ATTENDED_PULL_REQUEST_OPENED`.
- New short session `7f79445692d6c7d5ab35bbdeb22df34f`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1338-open-v001/state`,
  host detached. Two Accept/Reject candidates, each ready at event cursor 6:
  `BPR-1338-open-in-browser-v001` and `BPR-1338-home-open-v001`.
- Next: on hand-back, read `status --after 6`. Accept on both closes
  `BPR-1338:owner-look`; a Reject's note is the next round's change.

## Follow-up — 2026-10-08: two rows e8e1567c4 broke on main

BPR-1341's verification found two rows red on main after e8e1567c4.

- `application_uiforetw_selected_diff_contract` (`title_row_exact_order`).
  The composition's UIforETW course still expected 17 title-row actions. It
  now expects `workbenchOpenOnGithubAction` right after the name, 18 in all,
  as `qt_workbench_title_row_scale_*` does. The Windows caption buttons move
  to index 18. Because `github_application.cpp` changed, the round-trip pin
  in `application_review_round_trip_composition_contract.cmake` is resealed
  over BPR-1334's reseal (full digest `567618d4…`, manifest `4030cfee…`).
  The change is test evidence only and the mutation pin is unchanged.
- `application_profile_empty_desktop_native_menu_bar`. The window's Pull
  Request menu gained Open on GitHub, but the bar a Mac keeps with no window
  open did not, so closing the last window removed it. `desktop_menu_bar` now
  offers it first, on ⇧⌘O, followed by a separator, and disabled as BPR-1304
  did for View's items. The menu's accessible description gives the reason:
  "Open the pull request on GitHub, or keep it or its repository in my queue,
  unavailable while no window is open". The item's status tip gives the same
  reason.
- The tests now read the window's own menu instead of restating it.
  `qt_profile_management_window` compares the desktop bar's Pull Request with
  a real window's, command by command, including separators, checkability
  and keys. A window's Comment names its own unavailability in its title,
  and that state is dropped before comparing. The native course compares the
  resident bar's Pull Request items with the windowed bar's, read before the
  close, and requires every one of them to be disabled. Before this change,
  only its whole-bar description comparison caught the missing item.

Evidence (macOS, `build/dev` Debug, offscreen, targets only, on `5ad22d37e`):
`qt_profile_management_window`, `application_uiforetw_selected_diff_contract`
and `application_review_round_trip_composition_contract` pass. As a
failing-before control, `qt_profile_management_window` was run against main's
`desktop_menu_bar.cpp` and failed at "Pull Request carries the window menu's
own commands". `test_profile_empty_desktop_native_menu_bar` compiles. Its run
needs the owner's Aqua session (cocoa), so it was not run here and is owed on
the owner's native run. `clang-format` and `git diff --check` pass on the
changed files.

Found while rebasing onto BPR-1334 (`5ad22d37e`): that card added Line
Details to the window's View but not to the windowless bar. As a result,
`qt_profile_management_window` failed at "with the window menu's own
commands, in the window menu's own order". The native course would fail
for the same reason. In a separate commit, the desktop bar now offers Line
Details after One pane, checkable and disabled, and the row's View indices
shift by one. The change is noted on BPR-1334's card.
`qt_profile_management_window` and
`application_profile_management_public_path` pass.

## Owner verdicts — session `7f79445692d6c7d5ab35bbdeb22df34f`, handed back at 9

Both candidates on the frozen `aebf45eb4` build were rejected, verbatim:

- `BPR-1338-open-in-browser-v001` (event 8): "The button is there and good but
  doesn't seem to actually open my browser."
- `BPR-1338-home-open-v001` (event 7): "The button is there and good but it
  doesn't seem to actually open my browser."

`BPR-1338:owner-look` stays open. The button and the Home row are accepted
as they look. What failed is the open.

## Why no browser opened — 2026-10-08

The attended launcher started the build with `--github-fixture github-com`.
In `github_profile_runtime`, `InsideAutomatedTestBoundary` is
`inside_automated_test_boundary() || aFixture.has_value()` (BPR-0797). So
every fixture run, an attended look included, composed
`recording_device_authorization_browser`. That browser stores the address,
answers "opened" and starts nothing. The window's Open on GitHub and body
links used the same `ComposedBrowser`. `QDesktopServices::openUrl` was never
reached. Link admission had accepted the address, and the launcher's
environment was clean: it removed `BPR_AUTOMATED_TEST_BOUNDARY` itself. The
v001 copy said the browser would open a GitHub 404. That claim was never
observed: the earlier evidence read the recorder, never the platform launch.
A second problem would have shown next: the synthetic
`octo-org/review-repository#17` really is a 404 on github.com.

The real product was not affected. Without a fixture and outside the marker,
`ComposedBrowser` is `system_browser_opener`, and the same admitted address
reaches `QDesktopServices::openUrl`.

**Fix.**

- The window's own opens (the pull-request page from the title row, Home
  rows and Recently viewed, plus body links) now go through
  `github_profile_runtime::WindowBrowser`.
- `WindowBrowser` is `ComposedBrowser`, except in a tests-enabled graph whose
  fixture options set `AttendedWindowBrowser`. There it is the real
  `SystemBrowser`.
- `main` sets that option only for `--attended-launch` outside the automated
  test boundary. A CTest row that passes `--attended-launch` keeps the
  recorder.
- Device sign-in keeps the recorder in every fixture run, as BPR-0797 asks.
- A tests-only `--attended-open-on-github` (requires
  `--attended-open-pull-request`) presses the window's action once after the
  pull request opens. It prints `ATTENDED_OPEN_ON_GITHUB_TRIGGERED`, so one
  native check needs no hand on the desktop.
- The attended look moves to the `uiforetw-74` fixture. Its pull request,
  `google/UIforETW#74`, is real and public, so the browser shows a real page,
  https://github.com/google/UIforETW/pull/74, answered 200 when checked. The
  look still reads nothing from GitHub.

**Evidence (macOS, `build/dev` Debug, targets only, on `d8738b93c`).**

- `application_attended_recent_replay` runs `attended_open_on_github_course`
  on the launcher's composition: `uiforetw-74`, the fixture account, and
  `AttendedWindowBrowser`. The platform launch is substituted. The title
  row's button, a Following row and a Recently viewed row each launch exactly
  `https://github.com/google/UIforETW/pull/74`, once per gesture, and the
  recorder stays untouched. Device sign-in reaches no platform launch.
  Without the option, the same gestures reach the recorder and never the
  platform.
- Failing-before control: the composition change was reverted to
  `WindowBrowser = deviceBrowser` and the course was run again. It failed at
  "an attended look hands exactly the pull request's page to the system
  browser: the title row's button" with `launches=0 recorded=1`, which is the
  owner's v001 result. The file was then restored byte for byte.
- These also pass: `core_dependency_boundary`, `architecture_boundaries`,
  `application_device_authorization_browser_{handoff,absent,address}`,
  `application_composition_capabilities_{inside_boundary,product_run}`,
  `application_attended_launch_contract`, `composed_application_hosts` and
  `application_personal_discovery_public_path`.
  `application_review_round_trip_composition_contract` passes after the
  reseal (full `cb188cd0…`, manifest `a5471bbf…`, mutation unchanged).
  `clang-format`, `git diff --check` and the documentation gate pass too.
- Native, once: the frozen v002 build ran on Aqua outside the boundary with
  `--github-fixture uiforetw-74 --attended-launch
  --attended-open-pull-request --attended-open-on-github`. It printed
  `ATTENDED_OPEN_ON_GITHUB_TRIGGERED:status=Opened the pull request on GitHub
  in your browser.` The default browser is Chrome. Its live session file
  (`Sessions/Session_…`, started 19:34) was written at 20:07:29, right after
  the trigger, and now contains `https://github.com/google/UIforETW/pull/74`.
  Chrome's history had last recorded that address about 25 days earlier. No
  screen was captured and the desktop was not driven. The tab is left open
  in the owner's Chrome.

## Owner look v002 — prepared 2026-10-08

- Session `f566ce97a272a070a50df9a65399649c` released v001 (`7f794456…`,
  `resume-development --all` at 10). Its frozen root was deleted.
- The frozen build of `d8738b93c` (source export and Debug tests-on build) is
  inside `/Users/jaeger/.mask/attended-review/bpr1338-open-on-github-v002`.
  It is started with
  `python3 /Users/jaeger/.mask/attended-review/bpr1338-open-on-github-v002/launch.py`,
  which checks the build and the `uiforetw-74` fixture and reports an exit
  within seconds.
- New short session `f566ce97a272a070a50df9a65399649c`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1338-open-v002/state`,
  host detached. Two Accept/Reject candidates, both ready at cursor 6:
  `BPR-1338-open-in-browser-v002` and `BPR-1338-home-open-v002`.
- Next: on hand-back, read `status --after 6`. Accept on both closes
  `BPR-1338:owner-look`.
