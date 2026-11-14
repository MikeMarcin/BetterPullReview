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

## Acceptance

- The tab reads "Changes" everywhere it is named; the local draft courses pass.
- Files and Overview show folder icons styled per platform; renders at 100 %
  and 200 % on macOS, and the platform icon source on the others.
- The Edit view shows no stray underrail in a render of the look's fixture.
