---
schema: "implement-rfc.card.v1"
id: "BPR-0873"
title: "Windows: the documentation rows need out-of-repository skill content and a git child, neither reachable from the container"
status: "In Progress"
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

## Classified (dsp-006 scout): containment policy, plus a portability defect that is not about Windows

The containment rule and the main-side sequencing constraint are recorded once
on [[BPR-0872]]; this card is settled by it — both rows check the repository's
own files and reach no network, so they should be pinned `WINDOWS_UNCONTAINED`
with their measured cause.

Two obstacles, not one, and the pinned vocabulary already names both: the
out-of-repository import, and git's child exiting 128 (`dos-volume-name`, for
git's `getcwd`; `scripts/verify_test_network_isolation.py:46-50`).

**The separate, cross-platform fact worth its own record:**
`scripts/verify_documentation.py:37-38` does
`sys.path.insert(0, .../.agents/skills/implement-rfc/scripts)` and then
`import board_model`. On the Windows station `.agents` is an untracked symlink
into the MASK store, which is outside both granted read scopes
(`cmake/WindowsTestIsolationSetup.cmake:124-138`) — so the denial is
**structural**, not a missing ACL. That the repository's documentation gate
imports a module from outside the repository is true on every platform and is
independent of whatever containment decision this card takes.

## Landed, written down, and measured at `470920d5` (bpr-main-win, 2026-09-23)

**Registration.** Both rows are pinned `WINDOWS_UNCONTAINED` under a term made
for them, `repository-content-outside-checkout`. The keywords and the
dictionary entries landed together in `d3b5a3ef` (2026-09-17), after
`bpr-win` measured the rows uncontained (`4030bb6b`). The reason is recorded
above and in the dictionary's comment: the gate imports `board_model` through
an untracked symlink out of the checkout, and `project_config` needs a Git
child behind it. The same measurement found a real Windows defect in
`documentation_issue_graph_contract`'s fixture: `README.md` was written with no
encoding, so the ANSI code page turned an em dash into `0x97` and the UTF-8
reader refused it. Six calls in `tests/scripts/verify_documentation_tests.py`
now name their encoding (`4030bb6b`).

**Both rows pass on Windows.** Run `bpr-main-win-20260923`: MSVC 19.51 Debug,
`build/dev` at `470920d5`, uncontained as registered.
`documentation_structure` passes in 1.79 s and
`documentation_issue_graph_contract` in 4.46 s. `test_network_isolation_manifest`
passes at `4fd95fe5` (4.43 s) with both pins.

**Written down where a reader of the gate looks.** Until now the dependency
was stated only in the pin's comment. `docs/development/build-and-test.md`, in
its automated-test network boundary section, now says it:

- `scripts/verify_documentation.py` imports `board_model` and `project_config`
  from `.agents/skills/implement-rfc/scripts`, and `project_config` asks Git
  for the repository root.
- The repository does not track `.agents/`; it is the installed implement-rfc
  skill, and a symlink out of the checkout where skills come from the MASK
  store.
- So a checkout without that installation cannot run the gate, and neither can
  a sandbox that cannot read the store.

That is true on every platform, which is why the text does not frame it as a
Windows fact. The same edit replaces the section's stale "three things a lowbox
token cannot do" with the measured list the dictionary now carries.

**macOS and Linux.** The keyword acts only on the `windows-appcontainer`
backend, and the dictionary is checked only there. Neither row's registration
changed anywhere else. No macOS or Linux run was made from this station.
