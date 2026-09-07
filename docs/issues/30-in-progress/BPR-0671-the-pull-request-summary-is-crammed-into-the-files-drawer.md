---
schema: "implement-rfc.card.v1"
id: "BPR-0671"
title: "The pull request summary is crammed into the files drawer"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
---


# The pull request summary is crammed into the files drawer

The owner, 2026-09-07: "It looks like the entire set of information for this PR
is being crammed into a tiny box in files browser."

It is. `workbenchDrawerTop` is a 420-pixel-high scroll area inserted into the
files drawer's column, holding the repository, number, title, author, branches
and merged state, above the file tree.
[BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
put it there on purpose and said so: it stays "until the repositories drawer
(BPR-0651) and the overview (BPR-0647) take them". Both of those are Done and
neither took it, so the placeholder shipped.

The strip already names the repository, number and title, so the box also
repeats what is directly above it.

## Scope

- The summary moves to the overview, which is the page RFC-BPR-0028 gives it,
  and the strip keeps the one-line identity it already draws.
- The files drawer holds files.
- Owned paths: `src/adapters/qt` workbench window drawer composition and the
  overview page, their tests and the digests they pin.

## Acceptance

- The files drawer's top box is gone; the identity is in the strip and the
  detail is on the overview.
- Nothing that was in the box is lost.
