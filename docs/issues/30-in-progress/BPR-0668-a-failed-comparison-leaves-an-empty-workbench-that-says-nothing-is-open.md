---
schema: "implement-rfc.card.v1"
id: "BPR-0668"
title: "A failed comparison leaves an empty workbench that says nothing is open"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---


# A failed comparison leaves an empty workbench that says nothing is open

With [BPR-0667](../40-parked/BPR-0667-the-epoch-refuses-any-pull-request-whose-base-branch-moved.md)'s
acquire failure, the owner was left looking at a workbench with two empty code
panes, an empty file tree, "Select a file comparison" in the centre, and this
status line:

> No pull request is open in the workbench. The workbench could not be updated.
> Review history could not be updated. The exact comparison is unavailable.

A pull request *was* open: the strip named it, the drawer showed its title,
author, branches and merged state, and the conversations drawer listed seven
comments. The first sentence is false, and the remaining three name three
failures without naming one thing the reviewer can do. Nothing offers a retry.

This is the failure a reviewer meets first, and it must not read as "the
product is broken and you are stuck".

## Scope

- The status line says what is true: which part failed, and what still works.
  Never "no pull request is open" while one is open.
- A comparison that could not be prepared offers the retry the refusal already
  supports, in the surface the reviewer is looking at.
- The files list comes from the pull request's own file set, so a failed
  comparison leaves the files visible and the panes explained rather than both
  empty.
- Owned paths: `src/adapters/qt` workbench window status and empty states,
  the projections that feed them, their tests.

## Acceptance

- With the comparison unavailable, the workbench names the open pull request,
  lists its files, explains the one thing that failed and offers the retry.
- The false "no pull request is open" line cannot be produced while a pull
  request is open; a test pins it.
