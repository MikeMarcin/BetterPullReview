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
