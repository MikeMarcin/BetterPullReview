---
schema: "implement-rfc.card.v1"
id: "BPR-2156"
title: "Local draft polish: the Changes tab, folder icons in Files and Overview, the Edit view's underrail"
status: "In Progress"
rfcs: ["RFC-BPR-0113", "RFC-BPR-0114"]
depends_on: []
priority: "P3"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2156--BPR-2156--821c526b22b1.json","origin":"owner notes on accepted looks in attended session 767b92339e278ea303b3836f21426fd3 (handed back 2026-11-13T06:41:17Z)"}
---

# BPR-2156: Local draft polish

The owner accepted the local draft in the pull request window, with these
notes, verbatim:

- `local-changes-commit-push` (accept): "I think \"Local Changes\" should just
  be \"Changes\" The folder icons in changes probably make it look better here
  than they do in the files tab and probably also in overview, maybe we should
  add them there too.. although they probably need to be styled to match the
  OS folders since blue is defintely a mac thing."
- `edit-view` (accept): "Something weird with the underrail here but
  otherwise seems good." Its screenshot (in the session) shows the blue rule
  under the document tab strip starting partway along, not under a tab.

## Scope

- Rename the author-state tab "Local Changes" to "Changes" (labels,
  accessible names, tooltips and courses that read them).
- Folder icons in the Files drawer's tree and in Overview's files, as the
  Changes tab has them, drawn per platform (macOS's blue folder on macOS; the
  platform's own folder look on Windows and Linux).
- The Edit view's tab strip underrail: the rule sits under the active tab, or
  is not drawn, in the Edit view.
- The same `edit-view` note went on: "Btw save should use the standard save
  icon." Added here (2026-11-14, coordinator's instruction to do it this
  round): an ID could not be allocated from this branch, since the allocator
  reserves only on `main`'s integrator.

## Acceptance

- The tab reads "Changes" everywhere it is named; the local draft courses pass.
- Files and Overview show folder icons styled per platform; renders at 100 %
  and 200 % on macOS, and the platform icon source on the others.
- The Edit view shows no stray underrail in a render of the look's fixture.
- Edit's Save wears the platform's save icon (Reload its refresh icon).

## Progress (2026-11-14, branch `claude/parallel-backlog-2156-2172-2173`)

- "Changes" in the tab's label, accessible name and the writing-hooks text;
  `qt_local_draft_window` and `application_local_draft_author_desktop_public_path` pass.
- The style's folder icon in the Files tree (in a file mark's slot) and on
  Overview's folder rows; `qt_files_tree` checks both, and the folder rows
  pass at 125 % and 200 %.
- The underrail was the review bar's progress-track rail, drawn at the hidden
  track's stale geometry: `workbench_strip::group_rail` now draws no rail for a
  group with a hidden end. Reproduced first in `qt_workbench_window_scale_100`
  (rail 875..1081 under the hidden track); all three scales pass.
- Save's icon: `QIcon::fromTheme(DocumentSave)`, the style's save icon as the
  fallback; checked in `qt_local_draft_window`.
- Pending: the owner's look on a frozen build.

## Owner look (session `9afc3306d0cf788e0c2eaaec43d5b882`, frozen `200ba998fa15`, handed back 2026-11-14)

- `BPR-2156-changes-tab`: **Accept** (no note).
- `BPR-2156-folder-icons`: **Accept** (no note).
- `BPR-2156-edit-underrail`: **Accept** (no note).
- `BPR-2156-save-icon`: **Reject**. The owner, verbatim: "Drop the word Save just have the icon.\nAlso that icon does not look like the normal save icon, isn't it just QIcon::ThemeIcon::DocumentSave"

Open: Save as the icon alone.

## Save icon, second round (2026-11-14)

Save is now the icon alone, named and tipped "Save", from
`QIcon::fromTheme(QIcon::ThemeIcon::DocumentSave)` with the style's save icon
where the platform has none (checked in `qt_local_draft_window`). On macOS,
Qt draws `DocumentSave` as the SF Symbol `square.and.arrow.down`, which is
the icon of the first look too: the next look shows it rendered and asks
again.
