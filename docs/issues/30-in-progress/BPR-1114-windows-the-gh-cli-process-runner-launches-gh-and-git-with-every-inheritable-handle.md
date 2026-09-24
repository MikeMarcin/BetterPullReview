---
schema: "implement-rfc.card.v1"
id: "BPR-1114"
title: "Windows: the gh CLI process runner launches gh and git with every inheritable handle"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"independent rfc-reviewer-critical review of BPR-1104 (claude-fable-5-1, read-only, macOS from source), finding 1","reviewed_candidate":"51784082","origin":"owner-authorized review, attended session cbd48b91000b00f1e72e1ad62c7c384c, decision ae582fb8-d480-456a-8ef5-79011030e131, 2026-09-24","related_cards":["BPR-1104","BPR-1115","BPR-1040"],"reservation":"docs/issues/_reservations/BPR-1111--BPR-1115--c042082d78b1.json"}
---

# BPR-1114: Windows: the gh CLI process runner launches gh and git with every inheritable handle

## Source

[[BPR-1104]]'s independent review, 2026-09-24. The owner asked for it in
attended session `cbd48b91` (decision `ae582fb8`). The reviewer was an
`rfc-reviewer-critical` (`claude-fable-5-1`), working read-only on macOS
from source. The candidate was `51784082`. The review found no blocking
issue. It found BPR-1104's handle list correct: it fails closed with respect
to handles, and the retry is idempotent and bounded. This card takes finding
1. The review is recorded on BPR-1104 under "Independent review
(2026-09-24)".

## Defect

BPR-1104 found that a Git child held another operation's pack spool. The
child had inherited the handle, because unguarded Windows Git launches used
Qt's default of inheriting every inheritable handle, and MSVC C runtime
streams are inheritable. BPR-1104 fixed that in the Git adapter's
`installed_git_process.cpp`, with `standard_handles_only`.

The GitHub adapter has a second launcher that was not changed.
`src/adapters/github/src/github_cli_process_runner.cpp:584-611` launches `gh`
and `git` on Windows with Qt's inherit-all default. It installs no
`setCreateProcessArgumentsModifier`. On Unix the same launch sets
`CloseFileDescriptors` (`:606-610`). The file is unchanged from `51784082` to
the revision where this card was written.

So `gh version`, `git --version` and `gh api` reads can still inherit, and
hold, a spool handle that another thread has open at that moment. That
reproduces BPR-1104's second cause. It is pre-existing. The reviewer's fix is
to apply the same handle list.

**Why P2.** It is the same fault that BPR-1104 measured at 3 of 80 seeds for
the Git adapter, before the list was added there. How often it happens
through this runner has not been measured.

## Scope

- Give every Windows launch in the gh CLI process runner a
  `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` of its standard handles, or no
  inheritance at all, as BPR-1104 did for unguarded Git.
- Share the mechanism if the architecture allows it, rather than copying it.
  Both files are adapters, and neither may depend on the other's provider
  types. If the two copies must stay separate, say why and keep them
  identical in behaviour.
- A launch that cannot build its list must not run the child with
  inheritance. Use the refusal shape that [[BPR-1115]] settles, and do not
  start then kill the child.

## Acceptance

- AC1 Every Windows launch through the gh CLI process runner inherits only
  its standard handles. The code shows it, and AC2 pins it.
- AC2 A committed Windows row holds an inheritable handle on a file, such as
  an `std::ifstream`. It starts a long-lived child through the runner, and
  while the child runs it asserts that an exclusive `CreateFileW` on that
  file succeeds. Removing the list fails the row (negative control).
- AC3 The Unix launch is unchanged, or the change is run on macOS and Linux.
- AC4 No regression on Windows: the gh CLI runner's contract and transport
  rows, `architecture_boundaries`, `architecture_boundary_contract` and
  `msvc_secure_crt_contract`.
