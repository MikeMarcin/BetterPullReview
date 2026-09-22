---
schema: "implement-rfc.card.v1"
id: "BPR-0817"
title: "A Linux close exits 1 when the app created its data folder group-writable"
status: "Parked"
rfcs: ["RFC-BPR-0033"]
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux during BPR-0799","found_at":"17f495fea40f6985c0d2f6b0430c7a71062f8085","run":"run-20260916-stations","dispatch":"dsp-001-r2","origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred follow-up; not an RFC0013 closeout dependency"}
parked_on: "human-review"
acceptance: [{"criterion":"On the Linux station under umask 0002, a fresh data root is created owner-only, and closing the last window exits 0 with the neighbourhood created","outcome":"pass","revision":"1c6eaf21","evidence":{"kind":"observation","detail":"Native WSLg course on the tests-disabled image sha256 9b9ef069…f4a with umask 0002 and a fresh XDG root: the data root's parent and the root are 0700, and the neighbourhood is created 0700. Caption close, caption close, File > Quit and caption close all exit 0, with no chooser seen. Umask 0022 also exits 0 twice. On the pre-fix image the same course exits 1 and leaves the parent 0775. application_profile_startup_public_path asserts the same for a fresh nested install under umask 0002. Logs: task-20260917-lnx-build/native/course-candidate2-*.json and focused-0817.ctest.log."}},{"criterion":"With a pre-existing 775 parent, the refusal is visible to the person, and the exit code follows the recorded decision","outcome":"pending","revision":"1c6eaf21","evidence":{"kind":"freeform","detail":"Owner decision needed; RFC-BPR-0013, -0033 and -0024 do not decide it. With a pre-existing 0775 parent the refusal holds and nothing is repaired (agent_workspace_ownership, native), and the close still exits 1 because exit semantics were not changed. The refusal appears only in the agent panel, which an AT-SPI walk shows is not showing at Home, and the diagnostics log records no failure. Q1: should a refused agent workspace recovery still turn an otherwise orderly quit into exit 1, or should the quit exit 0 with the refusal reported only in the application? Q2: is the agent panel message enough, or must the refusal also be visible at Home, for example in the window status bar, as RFC-BPR-0033 reports failed profile operations?"}},{"criterion":"application_profile_management_public_path passes under the stock /tmp on Linux, and still passes on macOS","outcome":"pass","revision":"1c6eaf21 (Linux); 99712ec3 and 143509c3 (macOS)","evidence":{"kind":"command","detail":"Reconciled the existing macOS evidence already recorded in this card body (99712ec3, 3.59s) with the unchanged-source 2026-09-20 closeout rerun: application_profile_management_public_path passes 3.77s in build/rfc0013-resume-20260919/r24-closeout-focused.log. Linux stock-/tmp course remains passing at 1c6eaf21. Only the existing-writable-parent visibility/exit-policy decision remains pending; no owner answer is inferred."}}]
---

# BPR-0817: A Linux close exits 1 when the app created its data folder group-writable

## Owner-deferred follow-up — 2026-09-21

Owner amendment r4 transfers this remaining work out of RFC-BPR-0013 terminal
acceptance. This card keeps its ID, path, status, dependencies and all historical
evidence; only the RFC0013 association is removed. The RFC0033 association is retained. The
[amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its origin and continuation.

Pending: Linux pre-existing-parent visibility and orderly-exit policy; fresh-root and native acceptance already recorded remain intact.
No owner decision, native observation or repair is supplied by this transfer.

## Defect

bpr-linux observed this while proving [[BPR-0799]]
([evidence](../../verification/BPR-0799-linux-native-empty-desktop-quit.md),
"Defect: a group-writable data-root parent makes every Linux close exit 1").
Under umask `0002`, the application creates `$XDG_DATA_HOME/BetterPullReview`
itself, as mode `775`. Startup recovery then calls
`recover_installed_agent_workspaces` → `prepare_workspace_neighbourhood`
(`src/adapters/git/src/installed_git_process.cpp:3003`), which refuses a parent
writable by group or other and returns `repair_required`. The agent workspace
neighbourhood sits beside the data root (`src/app/src/github_application.cpp:4796`).
Shutdown counts that cleanup failure, so the process exits **1**
(`src/app/src/profile_desktop.cpp:853`). The window still closes with no
chooser, the diagnostics line still reads `outcome=shutdown failure=none`, and
nothing tells the person why.

| Course | Umask | Data-root parent | Neighbourhood created | Exit |
|---|---|---|---|---|
| `scratch-0799-release-umask002` | `0002` | 775, created by the application | no | 1 |
| `scratch-0799-release-umask002-parent755` (control) | `0002` | 755, created before launch | yes | 0 |

With `USERGROUPS_ENAB yes`, `pam_umask` gives an Ubuntu user-private-group
login session umask `0002`, so an ordinary desktop login plausibly hits this.
The station's own shell used `0022`, so this is inferred, not observed on a
desktop session. `application_profile_management_public_path` also fails
"orderly exit code" under a stock `/tmp` for the same reason, and passes with a
private `TMPDIR`.

## Scope and direction

- **Folders the application creates.** Create the data root, its parent
  folder and the workspace neighbourhood with explicit owner-only modes,
  independent of umask. The refusal then never fires on a fresh install. This
  part is an ordinary defect fix.
- **Pre-existing group-writable parents** (earlier installs, or a folder a
  person made). Keep refusing to place agent workspaces there. Surface that
  refusal visibly: [[BPR-0808]] restored the agent panel's repair message.
  Decide separately whether a refused *workspace cleanup* should turn an
  otherwise orderly quit into exit 1. That is a product decision about exit-code
  meaning. If the answer is unclear, record the exact question for the owner and
  do not change the exit semantics silently.
- Make the course independent of the host's `/tmp` mode, or make it assert the
  intended behaviour there, with no private-`TMPDIR` dependency hidden in the
  harness.

## Acceptance

- On the Linux station under umask `0002`, a fresh data root is created
  owner-only, and closing the last window exits 0 with the neighbourhood
  created.
- With a pre-existing `775` parent, the refusal is visible to the person, and
  the exit code follows the recorded decision.
- `application_profile_management_public_path` passes under the stock `/tmp`
  on Linux, and still passes on macOS.

## Linux station fix and evidence, 2026-09-17 (task-20260917-lnx-build, dispatch dsp-001-r2)

bpr-linux fixed the fresh-install half at `1c6eaf21`. The full record is
[the Linux evidence](../../verification/BPR-0817-linux-owner-private-data-root.md).

- **Fix.** Startup now creates the data root and every missing folder above it
  through `adapters::sqlite::create_storage_root_owner_private`.
  - On POSIX, each folder is created with `mkdirat` below its opened parent,
    opened `O_NOFOLLOW`, checked to belong to the effective user, and set to
    exactly 0700 with `fchmod`, whatever the umask.
  - No folder that already existed is changed. Windows is unchanged.
  - The neighbourhood still refuses a pre-existing group- or world-writable
    parent. The exit-code semantics are unchanged.
- **Criterion 1: pass (native).** On WSLg under umask 0002, with a fresh XDG
  root:
  - The data root's parent and the root are 0700, and the neighbourhood is
    created at 0700.
  - Caption close, caption close, File > Quit and caption close all exit 0.
    Umask 0022 also exits 0.
  - The same course on the pre-fix image exits 1 and leaves the parent 0775.
  - `application_profile_startup_public_path` asserts the same through the
    composition.
- **Criterion 3: Linux pass, macOS not run.**
  `application_profile_management_public_path` passes with root-owned 1777
  `/tmp` and `TMPDIR` unset. Its data root now sits inside its private scratch.
- **Criterion 2: pending the owner.**
  - With a pre-existing 0775 parent, the refusal holds and nothing is repaired.
  - The close still exits **1**.
  - The refusal appears only in the agent panel, which is **not showing at
    Home** (AT-SPI walk). The diagnostics log shows no failure.
  - RFC-BPR-0013, -0033 and -0024 do not decide either point. The two exact
    questions are at the end of the evidence file:
    1. Should a refused workspace recovery still turn an orderly quit into
       exit 1?
    2. Must the refusal also be visible at Home?
- **Tests added or changed.**
  - `sqlite_storage_root_contract`: umask 0002 creation and refusals.
  - `agent_workspace_ownership`: 0775 and 01777 parents are refused and
    nothing is created; a 0700 parent under umask 0002 creates 0700.
  - `application_profile_startup_public_path`: a fresh nested install under
    umask 0002.
  - `application_profile_management_public_path`: roots its data inside its
    private scratch.
  - Negative controls fail as expected.
- **Review.** `rfc-reviewer-critical` found no blocking finding. Its three
  non-blocking findings were adopted before commit:
  - No path-based `chmod` of a created root.
  - Identical validation for created and pre-existing roots.
  - Accurate header wording.

  A recheck confirmed all three resolved.
- **Portable files touched.**
  - Product: `src/adapters/sqlite/include/bpr/adapters/sqlite/storage_root.hpp`,
    `src/adapters/sqlite/src/catalog_database.cpp`,
    `src/app/src/github_application.cpp`.
  - Tests: `tests/sqlite_contract/storage_root_tests.cpp`,
    `tests/scripts/agent_workspace_ownership_tests.py`,
    `tests/end_to_end/profile_startup_public_path_tests.cpp`,
    `tests/end_to_end/profile_management_public_path_tests.cpp`.

## Round-trip digest reseal on main (dsp-002)

`1c6eaf21` replaced one call in `src/app/src/github_application.cpp`, and
that change moved the pinned whole-file and manifest digests of
`application_review_round_trip_composition_contract`. The review that covers
it is the `rfc-reviewer-critical` pass recorded in
[the Linux evidence](../../verification/BPR-0817-linux-owner-private-data-root.md),
which found no blocking issue. Following the reseal procedure in that
contract ([[BPR-0819]]), main resealed the range `99d5cbc2..fb84f41c`, which
holds only this change:

- whole file `fed42059189f4c5efae78c4191cf5298253b87425fbc278b9ec7c0038b47c91b`;
- manifest `b44257151eacac413c88212d988e21288f685b98423c979f779f59a0a6dd0108`;
- mutation branch unchanged.

## macOS half of criterion 3 (dsp-002)

`application_profile_management_public_path` passes on macOS at `99712ec3`
(3.59 s, `build/r2-logs/dsp2-int2-courses.log`), and so do
`application_profile_startup_public_path` and `agent_workspace_ownership`.
Criterion 3 is therefore met on both platforms. The card stays Parked on the
owner's answers to criterion 2's two questions.
