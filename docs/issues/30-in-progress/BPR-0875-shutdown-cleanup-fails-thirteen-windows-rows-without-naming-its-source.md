---
schema: "implement-rfc.card.v1"
id: "BPR-0875"
title: "Windows: an unnamed shutdown cleanup failure fails thirteen rows that had already passed everything they measure"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-merged-head-runs","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0875: Windows: an unnamed shutdown cleanup failure fails thirteen rows that had already passed everything they measure

## Defect

Thirteen rows fail on the Windows station **only on cleanup**. Twelve report
`PUBLIC_PATH_FAILED` with `cleanup_failed=1`, and **eleven of those carry
`presentation=1`** — every single thing the course set out to measure passed,
and then shutdown failed:

`application_uiforetw_selected_diff_contract`,
`application_workbench_composition_contract`,
`application_review_round_trip_composition_contract`,
`application_overview_since_review_public_path`,
`application_review_behaviour_contract`,
`application_saved_server_public_path`,
`application_appearance_theme_customization`,
`application_epoch_persistence_github-com`,
`application_epoch_persistence_ghes-3_18`,
`application_epoch_maintenance_public_path`,
`application_appearance_gesture`,
`application_appearance_store_fallback`, plus
`application_native_notification_attended_contract`, which refuses on the same
fact after printing `DISPLAY_BOUNDARY_OK`, `QT_ACTIVE_WINDOW_OK`,
`TRAY_SIGNAL_OK`, `ACTIVATION_OK` and `EXACT_LANDING_SELECTION_OK` with a
correct payload.

[[BPR-0860]]'s fix is confirmed working on Windows inside this cluster:
`WORKBENCH_DRAWER_WIDTH_WIRE_OK` and every other workbench marker print, the
composition trace and host compose, `presentation=1`, and the row still fails
at `PUBLIC_PATH_STAGE:23` on `cleanup_failed=1` after 5711 ms of its 40 s
budget. So this one fact is masking a large amount of working behaviour.

**`ShutdownCleanupFailed` has three sources** in
`src/app/src/github_application.cpp` (~11762-11812): the durable layout write,
the layout erase, and `remove_all(McpSnapshotRoot)`. **None is named in the
output.** Thirteen rows therefore report the same undifferentiated fact, and
finding which of the three fired needs a bisect rather than a run.

This is the run's first full Windows sweep, so the cluster is newly observed.
Whether it is newly broken is unknown and is part of the work.

## Scope

- **Name the three sources first.** Give each its own term with the path and
  the error it saw. One run should then say which of the three fires, on all
  thirteen rows at once, and it is plausible they do not all share one source.
  Do this before attempting a fix: the cluster is large enough that guessing
  costs more than instrumenting.
- Then fix the cause. A Windows-specific `remove_all` failure on a directory
  still held open is the obvious first suspect, and it is the same family as
  [[BPR-0868]] (the contained temporary helper swallowing a removal error) and
  the `tmp_pack_*` READONLY|ARCHIVE hazard this run already recorded.
- Establish whether the cluster predates this run.
- Check whether a person quitting the real application on Windows hits the
  same failure. Thirteen automated rows failing at shutdown is worth knowing
  about, but a reviewer losing their layout on quit would matter far more, and
  the durable layout write is one of the three candidates.

## Acceptance

- A cleanup failure names which of the three sources failed, and with what
  error, shown in a real run.
- The thirteen rows pass on Windows, or each remaining failure is a named,
  understood cause with its own record.
- The card states whether a person quitting the application on Windows was
  ever affected.
- macOS and Linux are unaffected: all thirteen pass there today.

## The open question is answered: a person is affected too (r5 critical review)

This card asked whether a person quitting the real application on Windows hits
the same failure. **They do, and here is the line:**
`src/app/src/profile_desktop.cpp:851-853` sets `ExitCode = 1` when
`profile_shutdown_cleanup_failed()`, in the same drain the real desktop uses.

So the Windows cluster is not test-only. A reviewer quitting the application
gets exit 1 from the same three unnamed sources at
`src/app/src/github_application.cpp:11762-11812`, and one of those three is
the durable layout write — which is the one whose failure a person would
actually notice, because it is their window layout.

That raises this card's stakes: it is not thirteen red rows, it is thirteen
red rows plus a user-visible failure sharing one unnamed cause. Naming the
three sources is still the right first step, and now it is worth doing for the
product's sake rather than only the suite's.

Related: [[BPR-0890]] records that a crash *after* the marker passes any row
with a `PASS_REGULAR_EXPRESSION`, so shutdown faults are exactly the class the
suite is currently weakest at seeing. The two should be read together.

## Named, and it is none of the three (bpr-win, task-20260917-win-0875-name-cleanup)

The base is `026d610ff236229cb08114bbb6a6a800de77c52b`. Instrumented first, as
the card asked, and the answer changes the card: **no shutdown cleanup fails at
all.**

### What the thirteen rows now say

Every one of them, in one run, prints the same term:

```text
PUBLIC_PATH_CLEANUP_SOURCE:agent-workspace-recovery:<data-root>\agent-workspace-ownership-v1:repair-required
```

and the desktop's own quit path prints the same thing:

```text
PROFILE_QUIT_CLEANUP_SOURCE:agent-workspace-recovery:<data-root>\agent-workspace-ownership-v1:repair-required
```

So the flag is not raised by the durable layout write, the layout erase, or
`remove_all(McpSnapshotRoot)`. It is raised at **startup**, by
`recover_installed_agent_workspaces`, and only reported at the quit, which is
why it looked like a shutdown fault. `AgentWorkspaceCleanupFailed` is folded
into the profile's `CleanupFailed`, and `status()` reports that as
`cleanup_failed=1` while the profile is closing.

### Why recovery refuses, measured to the exact clause

A reverted trial split the boolean chains and printed the first false:

1. `recover_installed_agent_workspaces` → `prepare_workspace_registry` returns
   false → `repair_required`.
2. Inside it, the first false is `prepare_workspace_neighbourhood`.
3. Inside that, the first false is `workspace_parent_trusted(parent)`, where
   `parent` is the neighbourhood's parent — the parent of the row's data root.

`workspace_parent_trusted` (`installed_git_process.cpp` ~3348) is **asymmetric
by construction**:

- **Windows:** the parent must be canonical, `private_directory`, and carry a
  DACL with no foreign write — owner, SYSTEM, Administrators and OWNER RIGHTS
  only.
- **POSIX:** the parent must be canonical, a directory, owned by this euid, and
  not group- or other-writable.

The rows put their data root in the build tree, so the parent is
`build/windows-dev/tests/end_to_end`, and `icacls` says:

```text
S-1-15-2-…-2136715157:(I)(OI)(CI)(M)      the AppContainer
BUILTIN\Administrators:(I)(OI)(CI)(F)
NT AUTHORITY\SYSTEM:(I)(OI)(CI)(F)
BUILTIN\Users:(I)(OI)(CI)(RX)
NT AUTHORITY\Authenticated Users:(I)(M)
```

`Authenticated Users:(M)` is foreign write, so Windows refuses. The same
directory on Linux and macOS is `0755` and owned by the user, which the POSIX
clause accepts — which is why the rows are green there and red here.

This is a plain filesystem fact, not containment: nine of the thirteen rows
already run uncontained, and the refusal is identical in both.

### Step 5: the cluster does not predate this run

`c94e02b1` (2026-09-17, [[BPR-0622]], "supervise guarded Git and recover agent
workspaces on Windows") added `Q_OS_WIN` to the guard around the recovery post
in `start_services`, and `dd88b9c9` the same day put the ownership journal on
Windows. Before that, Windows never called recovery, and this station's
2026-08-27 matrix run has
`application_workbench_composition_contract` passing in 6.90 s. So the cluster
was introduced by this run's own Windows enablement, and no bisect was needed
to say so.

### Step 3: the person, and what they lose

A real quit hits the same source: the desktop's `ExitCode = 1` path
(`profile_desktop.cpp` ~852) prints
`PROFILE_QUIT_CLEANUP_SOURCE:agent-workspace-recovery:…:repair-required`.

- **The reviewer's window layout is not lost.** The durable layout write never
  refused in any of these runs; the term would have said
  `durable-layout-write:…` if it had.
- What the person does get is exit 1 from an ordinary quit, and — because
  recovery refuses while the profile is not closing — [[BPR-0808]]'s repair
  presentation in **every** window, from a refusal that is about where the
  test harness put a directory.

### What this change adds (naming only, no behaviour)

- `github_application::shutdown_cleanup_detail()` names the first refusal as
  `<source>:<path-or-window>:<error>`. It is an accessor, not a field on
  `github_application_status`: adding a `std::string` to that record made the
  seeding processes die with `0xC0000409`, because the record crosses a seam by
  value and its layout is load-bearing. That is measured, not guessed — the
  crash appeared and went away with the field.
- Terms for all five raising sites: `durable-layout-write` (with the window,
  the attempted/completed store counts, and `unavailable`, `cancelled` or
  `in-flight-at-drain`), `layout-erase` (with the window and
  `unavailable`/`cancelled`/`threw`/`post-refused`/`delivery-refused`),
  `mcp-snapshot-remove` (with the path, `error.value()` and `error.message()`),
  `agent-workspace-recovery` (with the registry and the ownership failure), and
  `agent-workspace-parent-remove` (with the path).
- `profile_shutdown_cleanup_detail()` for the desktop's exit code, so a real
  quit names its cause on the way out.
- One latent trap recorded while reading: `profile_window_layout_port::store`
  sets `WriteFailed` **before** performing the write and clears it from the
  result, so `write_failed()` also reads true while a write is in flight. The
  new term distinguishes those two (`in-flight-at-drain` versus a named
  failure); nothing here changes the behaviour.
- `cleanup_failed=1`, every existing marker and their order are unchanged. The
  new term is printed after `PUBLIC_PATH_VERDICT`, and
  `NATIVE_NOTIFICATION_CLEANUP_SOURCE` after the existing refusal line.

### Step 4: not fixed here, because the fix is a decision

The cause is clear but not contained, and both candidate fixes are somebody
else's call:

1. **Test-side.** Give those rows a data root whose parent is owner-private,
   as [[BPR-0856]]'s POSIX arrangement does. That is thirteen harness files,
   and CMake cannot write a DACL, so each row would need the application or a
   helper to create the private parent.
2. **Product-side.** Relax the Windows clause of `workspace_parent_trusted` to
   the POSIX one — owner-owned, no foreign write — so an inherited
   `Authenticated Users` entry on a build directory is tolerated. That is
   workspace ownership authority and weakens a deliberate Windows rule.

A real installation is not affected either way: the shipped data root sits
under `%LOCALAPPDATA%`, whose parent does not grant Authenticated Users.

**The question for the owner of that rule: should the Windows parent-trust
clause match the POSIX one, or should the thirteen rows be given an
owner-private parent?**

### Verification

`windows-dev` Debug, full build, warning-clean. One run of all thirteen rows
names the source in all thirteen (48 occurrences across their phases; each row
runs the application more than once).

`application_review_round_trip_composition_contract` fails on its whole-file
digest pin, as the envelope warned, and its cleanup answer is read from its
output rather than its verdict. **The pins are not resealed**; printed with
`-DBPR_PRINT_PINNED_DIGESTS=ON` at this commit:

- `full_application`: `158d5acdc0cd4b046d3f75ae60c689b9a7957a9184915396bbfdf53f9d1bfb9c`
- `production_app_manifest`: `1808976917d0c22d32b4c71fd5fc12c0aab08035c252aa45b5214c2de1ce0588`
- `production_mutation_branch`: `a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a` (unchanged)

The repository's `format_check.py` (clang-format 22.1.8) passes on `src/app`.
