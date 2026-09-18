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

## Fixed in the harness: one owned level under each course's root (bpr-win)

The base is `9ea17894022687512e15a015ac0dcc0bdedbae64`. The Windows
parent-trust rule is unchanged, as the integrator decided: it refuses a parent
another principal can write, and `build/windows-dev/tests/end_to_end` really is
such a parent. The courses get a parent that satisfies it instead.

### A real installation was never affected, measured both directions

| Parent of the data root | icacls | The row |
| --- | --- | --- |
| `C:\Users\mikem\AppData\Local\BetterPullReview` (real install) | `mikem:(F)`, SYSTEM, Administrators, and `CodexSandboxUsers:(RX)` — no `Authenticated Users`, and the one foreign principal has **no write right** | `WORKBENCH_COMPOSITION_CONTRACT_OK`, cmake exit 0, zero `CLEANUP_SOURCE` lines |
| `build\windows-dev\tests\end_to_end` (the courses) | `Authenticated Users:(I)(M)`, `Users:(I)(RX)` | fails with `agent-workspace-recovery:…:repair-required` |

Same row, same application, one variable. `CodexSandboxUsers` is a local
artefact of this host and would not exist on a clean Windows box; it does not
matter either way, because `workspace_parent_trusted` skips an allow entry that
grants no write right. So this is a test-environment defect, not a shipped one.

### The mechanism, and a deviation from the approved plan

The plan was a tests-owned tool wrapping `create_storage_root_owner_private`.
While implementing it I found the harness **already has** the shared mechanism:
`tests/end_to_end/bpr_narrow_owner_private.cmake`, which these very courses
already call for their `state` and `state/v1` directories, and which writes the
descriptor with `icacls /inheritance:r /grant:r`.

So no new tool and no new target: one function is added beside the existing one.

```cmake
function(bpr_private_data_root root_variable)
    set(parent "${${root_variable}}")
    file(MAKE_DIRECTORY "${parent}")
    bpr_narrow_owner_private("${parent}")
    set(${root_variable} "${parent}/data" PARENT_SCOPE)
endfunction()
```

The directory each course was given becomes the private parent, and the data
root moves one level inside it. Every course calls it in one line before its
first use of the root. **This adds no second descriptor author** — it reuses the
harness's own, which is the property the integrator asked for; it is not
`create_storage_root_owner_private`, which is the part that differs from the
approved plan and is flagged for that reason. If the two should be one author,
that is a follow-up and a small one: only this function would change.
[[BPR-0825]] item 3's answer would move it, not rewrite it.

### The inheritance hazard, checked deliberately

The integrator named the way this plan could silently fail: a created directory
that carries its own owner-private entries **plus** an inherited
`Authenticated Users:(M)`. `icacls` on the directory the mechanism creates:

```text
C:\code\git\bpr-windows\build\windows-dev\tests\end_to_end\workbench-composition-contract
    JAEGAW\mikem:(OI)(CI)(F)
```

One entry, and **no `(I)` entry at all**: inheritance is disabled, so nothing
foreign survives from the build tree. The hazard does not apply to this
mechanism.

### Result: thirteen rows, eleven green, two failing on other things

One run of all thirteen at this head. **Zero `CLEANUP_SOURCE` lines and zero
`PROFILE_QUIT_CLEANUP_SOURCE` lines remain**, so an ordinary quit no longer
exits 1 and [[BPR-0808]]'s repair presentation no longer appears.

| Row | Result |
| --- | --- |
| `application_native_notification_attended_contract` | Pass, 13.18 s |
| `application_workbench_composition_contract` | Pass, 14.32 s |
| `application_overview_since_review_contract` | Pass, 47.46 s |
| `application_review_behaviour_contract` | Pass, 47.32 s |
| `application_saved_server_contract` | Pass, 45.75 s |
| `application_appearance_theme_customization_contract` | Pass, 25.49 s |
| `application_epoch_persistence_github-com` | Pass, 22.55 s |
| `application_epoch_persistence_ghes-3_18` | Pass, 21.58 s |
| `application_epoch_maintenance_public_path` | Pass, 27.55 s |
| `application_appearance_gesture_contract` | Pass, 35.15 s |
| `application_appearance_store_fallback_contract` | Pass, 21.02 s |
| `application_uiforetw_selected_diff_contract` | **Cleanup fixed** (`cleanup_failed=0`), still fails on its own [[BPR-0859]] Windows problem: `UIFORETW_SELECTED_DIFF_EVIDENCE_FAILED`, `UIFORETW_CAPTURE_FIRST_TARGET_FAILED`, `presentation=0`. It failed on those two markers before this change as well |
| `application_review_round_trip_composition_contract` | Fails only on the whole-file digest pin, as expected and **not resealed** |

### The introducing commit

`c94e02b1` (2026-09-17, [[BPR-0622]]) added `Q_OS_WIN` to the guard around the
workspace recovery post in `start_services`, and `dd88b9c9` put the ownership
journal on Windows the same day. Before those, Windows never called recovery.
This station's 2026-08-27 matrix run has
`application_workbench_composition_contract` passing in 6.90 s, which is the
before-evidence. So this is a regression from this run rather than a
long-standing fault.

## Where this card stands at the end of dsp-006, and the two steps left

Merged to main at `c1723a3f` (the naming) and `81844fef` (the fix). Against the
card's own four acceptance lines:

1. **"A cleanup failure names which of the three sources failed"** — met, and
   then some. The instrumentation named a **fourth** source the card did not
   list, and that is the one that fires. All thirteen rows report
   `agent-workspace-recovery:<data-root>\agent-workspace-ownership-v1:repair-required`,
   48 times across their phases. None of the three named sources ever fired.
2. **"The thirteen rows pass on Windows, or each remaining failure is a named,
   understood cause"** — eleven pass with zero cleanup terms and zero quit
   terms. `application_uiforetw_selected_diff_contract` reaches
   `cleanup_failed=0` and now fails on [[BPR-0859]]'s Windows problem instead,
   on the same two markers it failed on before the change, which is a named
   cause with its own card. **Not yet confirmed:**
   `application_review_round_trip_composition_contract`, which failed on the
   lane only because the lane carried the naming without main's reseal. It
   should pass once the station fast-forwards to `81844fef`; that is the single
   outstanding measurement on this card.
3. **"The card states whether a person quitting on Windows was ever affected"** —
   met, in both directions and by experiment rather than by reading. In the
   **test environment** yes: exit 1 from an ordinary quit, plus [[BPR-0808]]'s
   repair presentation in every window, from a refusal that was really about
   where a harness put a directory. For a **real installation**, no:
   `%LOCALAPPDATA%` grants no Authenticated Users, its one foreign principal has
   no write right, and the same row rooted there passes with zero cleanup-source
   lines while rooted in the build tree it fails. **The reviewer's window layout
   was never at risk** — the durable layout write never refused in any run.
4. **"macOS and Linux are unaffected"** — macOS is verified at the merged tree:
   twelve of the thirteen affected rows pass, and the thirteenth
   (`application_review_behaviour_contract`) passes too once an unrelated
   in-flight [[BPR-0890]] change is taken out of the tree, 93.86 s
   (`build/r2-logs/dsp6-merge2-isolate.log`). This matters because
   `bpr_private_data_root` moves the data root down one level on **every**
   platform. **Linux has not been re-run** and is the other outstanding
   measurement.

So: two measurements left, both cheap — the Windows round-trip row at
`81844fef`, and the twelve affected courses on Linux. Neither is expected to
surprise anyone, and neither has been done.

**Do not let the origin story get lost.** [[BPR-0622]]'s `c94e02b1`, dated
2026-09-17, introduced this *during this run* by adding `Q_OS_WIN` to the guard
around the recovery post in `start_services`; `dd88b9c9` put the ownership
journal on Windows the same day. Before that Windows never called recovery at
all. BPR-0622 carries a pointer back to here.

Carded out of this work: [[BPR-0899]] (the layout port's `write_failed()` reads
true while a write is in flight) and [[BPR-0900]] (the harness and the product
both write the owner-private descriptor and nothing pins that they agree —
which thirteen rows now depend on).

## The second outstanding measurement is done: Linux is unaffected (bpr-linux, dsp-007)

Base `271bb452`. `bpr_private_data_root(BPR_ROOT)` moves the data root down one
level on **every** platform, so this was a real question on Linux and not a
formality. **Thirteen rows from twelve course scripts, 13/13 Passed, `ctest`
exit 0, 76.17 s** (`build/r2-logs/dsp7-0875-twelve-courses.log`).

The twelve were identified from the tree rather than from prose — `grep -l
bpr_private_data_root tests/end_to_end/*.cmake` returns thirteen files, of which
`bpr_narrow_owner_private.cmake` is the definition and the other twelve are the
callers — and every row name below was resolved against `ctest -N`, not against
a card:

| # | Course script | Row (`ctest -N`) | Result |
| --- | --- | --- | --- |
| 1 | `appearance_gesture_contract.cmake` | `application_appearance_gesture_contract` | Passed, 6.22 s |
| 2 | `appearance_store_fallback_contract.cmake` | `application_appearance_store_fallback_contract` | Passed, 3.34 s |
| 3 | `application_appearance_theme_customization_contract.cmake` | `application_appearance_theme_customization_contract` | Passed, 5.13 s |
| 4 | `application_native_notification_attended_contract.cmake` | `application_native_notification_attended_contract` | Passed, 3.76 s |
| 5 | `application_overview_since_review_contract.cmake` | `application_overview_since_review_contract` | Passed, 8.16 s |
| 6 | `application_review_behaviour_contract.cmake` | `application_review_behaviour_contract` | Passed, 8.23 s |
| 7 | `application_review_round_trip_composition_contract.cmake` | `application_review_round_trip_composition_contract` | Passed, 1.64 s |
| 8 | `application_saved_server_contract.cmake` | `application_saved_server_contract` | Passed, 8.27 s |
| 9 | `application_uiforetw_selected_diff_contract.cmake` | `application_uiforetw_selected_diff_contract` | Passed, 15.06 s |
| 10 | `application_workbench_composition_contract.cmake` | `application_workbench_composition_contract` | Passed, 2.86 s |
| 11 | `epoch_maintenance_public_path_contract.cmake` | `application_epoch_maintenance_public_path` | Passed, 5.54 s |
| 12 | `epoch_persistence_contract.cmake` | `application_epoch_persistence_github-com` | Passed, 3.90 s |
| 12 | `epoch_persistence_contract.cmake` | `application_epoch_persistence_ghes-3_18` | Passed, 4.05 s |

Twelve scripts, thirteen rows: `epoch_persistence_contract.cmake` is registered
twice by a `foreach` over the GitHub.com and GHES fixtures, which is where the
card's "thirteen rows" and this dispatch's "twelve courses" meet.

Row 7 is worth calling out: `application_review_round_trip_composition_contract`
passes here, so [[BPR-0881]]'s whole-file digest reseal — the one Windows was
still failing on before it fast-forwarded — is correct at this tree.

So the card's fourth acceptance line now reads: macOS verified at the merged
tree, **Linux verified here**, and neither is affected by moving the data root
down a level.

### The [[BPR-0901]] margin holds on this host, and it is not close

Rows 5 and 6 are the two that carry `bpr_fixture_run_budget_ms` (40000) with
`RUNS 4` rather than `bpr_epoch_phase_budget_ms` (120000), and that macOS failed
at `elapsed_ms=40035` and `40052`. **Neither failed here**, and no
`RUN_BUDGET_EXPIRED` was emitted: the whole row took 8.16 s and 8.23 s
respectively, for all four of its runs together, against a 40 s budget for each
one. That is roughly a fivefold margin rather than a marginal pass, so this host
says nothing about whether macOS's failures are a path-length or 0700-parent
effect — it says the two rows are nowhere near the budget on Linux. Not
investigated and not changed here; the registration is untouched.
