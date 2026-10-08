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
  (`open-in-browser-v001`) and Home's open (`home-open-v001`) in one short
  attended session on a frozen build.
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
  `open-in-browser-v001` and `home-open-v001`.
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
