---
schema: "implement-rfc.card.v1"
id: "BPR-1338"
title: "Open the pull request on GitHub from a button beside its title, and from Home's rows"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","found_by":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1338--BPR-1338--5020946253b6.json","related_cards":["BPR-1129","BPR-1251","BPR-1255","BPR-1324","BPR-0776","BPR-0280"]}
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
