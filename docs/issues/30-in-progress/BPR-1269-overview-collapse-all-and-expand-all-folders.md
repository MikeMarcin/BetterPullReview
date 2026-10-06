---
schema: "implement-rfc.card.v1"
id: "BPR-1269"
title: "Overview: collapse all and expand all folders"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"Owner note on attended item BPR-1213-collapse-v002, session cbd48b91000b00f1e72e1ad62c7c384c, event 863, decision 4cc89d72-64df-4345-8bd0-a3cee4b5266b, 2026-10-03","reservation":"docs/issues/_reservations/BPR-1269--BPR-1269--742bebe75676.json","related_rfcs":["RFC-BPR-0037"],"related_cards":["BPR-1213","BPR-0934"]}
---

# BPR-1269: Overview: collapse all and expand all folders

## Origin

The owner accepted [[BPR-1213]]'s per-folder collapse on the nested-folders
pull request and added, verbatim:

> Yes this works, although a collapse all/expand all would be nice.

Source: session `cbd48b91000b00f1e72e1ad62c7c384c`, candidate
`BPR-1213-collapse-v002` (fingerprint `e573cace…`, frozen build of
`0f6e7b292`), event 863, decision `4cc89d72-64df-4345-8bd0-a3cee4b5266b`,
saved 2026-10-03T05:16:19Z. The Accept stands on BPR-1213; this card carries
only the request.

## Current state

The Overview groups files under flat folder rows, one per folder that holds
files, named by its whole path (`docs/ux/workbench-specification.md`,
"Overview files"). Each folder row hides and shows only its own files, by
click or Left/Right. There is no way to collapse or expand every folder at
once, so a reviewer who wants a list of folder rows alone collapses each one.

## Intended work

Offer collapse all and expand all for the Overview's folder rows. Where the
gesture lives (a control near the file list, a menu action, a keyboard
shortcut, or more than one of these) is a design choice for the implementing
round. Keep it local to the page, as per-folder disclosure is: Qt owns the
expansion state, and no provider, Git, storage or publication effect follows.

## Acceptance

- AC1: One gesture collapses every Overview folder row, and one expands every
  row. Each folder keeps the files it had, in order. No comparison opens and no
  file is activated.
- AC2: The gestures are reachable by keyboard and named to accessibility. If
  the current file is hidden, focus goes to its folder, as for a single
  collapse. A same-authority refresh keeps the result: folders collapsed by
  collapse all stay collapsed, and a newly appearing folder starts expanded as
  it does now. Authority replacement clears the state.
- AC3: A focused Qt course and the composed `nested-folders-v1` public path
  cover collapse all, expand all, a per-folder toggle afterwards, and a refresh.
  `docs/ux/workbench-specification.md` "Overview files" describes the gestures.
- AC4: The owner judges the gestures on the nested-folders pull request against
  the exact candidate, as one Accept/Reject question.

## Linux implementation — 2026-10-06 (`bpr-main-linux`)

**Gestures** (`pull_request_overview_page`): two auto-raised glyph buttons sit
to the right of the Files heading, "Collapse all folders" (two chevrons closing
upward) and "Expand all folders" (two chevrons opening downward). They follow
the owner's preference for minimal text, with the names in tooltips and
accessible names. They take the keyboard (`Qt::StrongFocus`) and are shown
only while the list has a folder row and its heading is shown. They paint on
demand through [[BPR-1296]]'s `painted_glyph_icon`, in the palette's text ink,
refreshed with the appearance.

- Collapse all collapses each folder row in turn, so the existing `collapsed`
  handler moves a hidden current file to its folder, as a single collapse does.
- Expand all expands each folder row.
- Both stay on the page, like per-folder disclosure: they make no dispatch and
  no provider, Git, storage or publication effect.
- Same-authority refreshes keep the result, because the model reconciles rows,
  and a new folder arrives expanded. Authority replacement clears it.
- `docs/ux/workbench-specification.md` "Overview files" describes the
  gestures.

**Tests:**

- `test_collapse_all_and_expand_all_folders` in `qt_pull_request_overview_page`
  covers the buttons' names, tooltips, focus and glyph, and collapse all with
  each folder's files and order kept. It checks that a hidden current file
  leaves its folder current with no dispatch, that a refresh keeps the
  collapse while a new folder starts expanded, and that a per-folder Right
  works afterwards. It also checks expand all, the buttons gone without
  folders, and a replaced authority starting expanded.
- The composed `application_nested_overview_folders_public_path` covers the
  nested-folders pull request:
  - collapse all closes all 12 folder rows and hides all 17 files;
  - a click on `src` then shows only `src/main.cpp`;
  - expand all shows every file again, in order;
  - none of this opens a file or leaves Overview.

  It saves `nested-overview-folders-all-collapsed.png`.

**Control:** with `collapse_all_folders` doing nothing, both fail: "collapse
all closes every folder and keeps its files in order" and "collapse all closes
all twelve folder rows".

**Evidence** (`build/dev`, GCC 15.2, offscreen): every `qt_` row, the Overview
and nested-folders courses, setup destination, the round-trip composition
contract and `architecture_boundaries` pass 133/133. `coding-format-check` and
`DOCUMENTATION_OK` pass.

**Open:** AC4, the owner's Accept or Reject on the nested-folders pull
request against the exact candidate, run from the Mac. Suggested question:
*"Beside Files, two chevron buttons collapse and expand every folder at once
(names in their tooltips). Accept?"*
