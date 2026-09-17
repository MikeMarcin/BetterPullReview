---
schema: "implement-rfc.card.v1"
id: "BPR-0873"
title: "Windows: the documentation rows need out-of-repository skill content and a git child, neither reachable from the container"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-merged-head-runs","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0873: Windows: the documentation rows need out-of-repository skill content and a git child, neither reachable from the container

## Defect

`documentation_structure` and `documentation_issue_graph_contract` fail on the
Windows station with `ModuleNotFoundError: No module named 'board_model'`.
Both pass uncontained (`DOCUMENTATION_OK`), so this is about the container, not
the documentation.

The station measured the cause and then measured the *next* one, which is what
makes this card worth acting on rather than granting a permission and moving
on:

1. `scripts/verify_documentation.py:38` inserts
   `.agents/skills/implement-rfc/scripts` onto `sys.path`. On that station this
   is a **symlink out of the repository** into
   `C:\Users\mikem\.mask\store\v1\<digest>\payload\implement-rfc`, and
   `.agents/` is untracked and git-ignored. The store grants the AppContainer
   no entry, so a contained row is denied it and `listdir` fails with
   WinError 5.
2. Granting the container read on that one payload made the import succeed —
   and the row then hit a **second** contained obstacle: `project_config`
   raises "not a Git consumer repository", because `git rev-parse` inside the
   container exits 128 with "fatal: Unable to read current working directory:
   Permission denied".

The grant was reverted after the measurement.

So these rows currently need out-of-repository skill content **and** a git
child process, and neither is reachable from inside the container. A
permission grant alone would not have fixed them, which is exactly the kind of
thing a single-obstacle diagnosis would have missed.

## Scope

Decide what these rows are for, then pick the matching registration:

- If they are documentation-structure checks over the repository's own files,
  the container buys little and `WINDOWS_UNCONTAINED` is the honest
  registration, with the reason recorded.
- If they should stay contained, then the skill content they import has to
  come from somewhere the container can read — a vendored copy inside the
  repository, or a check that does not import out-of-tree code — and the git
  dependency has to go or be satisfied differently.

Either way, record that the gate depends on content **outside the repository
through an untracked symlink**. That is a portability fact about the
documentation gate that nothing else states, and it affects any machine whose
skill store is arranged differently, not only a contained Windows row.

## Acceptance

- Both rows pass on Windows, with their registration and its reason recorded
  here.
- The documentation gate's dependency on out-of-repository content is written
  down, or removed.
- macOS and Linux keep passing both rows.
