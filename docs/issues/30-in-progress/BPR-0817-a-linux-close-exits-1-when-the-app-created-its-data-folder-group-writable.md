---
schema: "implement-rfc.card.v1"
id: "BPR-0817"
title: "A Linux close exits 1 when the app created its data folder group-writable"
status: "In Progress"
rfcs: ["RFC-BPR-0033"]
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux during BPR-0799","found_at":"17f495fea40f6985c0d2f6b0430c7a71062f8085","run":"run-20260916-stations","dispatch":"dsp-001-r2","origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred follow-up; not an RFC0013 closeout dependency"}
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

## Owner-delegated amendment — 2026-09-23

**Delegation.** On 2026-09-23 the product owner delegated this card's two
parked questions to its implementer. The Mac integrator session relayed the
delegation verbatim: "for BPR-0817 let the implementor decide". The two
decisions below were made by the implementer (station `bpr-main-linux`) under
that delegation. They are not the owner's words, and they are not
observations. The observations stay in the sections that record them.
[RFC-BPR-0033](../../rfcs/RFC-BPR-0033-profile-chooser-and-the-last-window.md#owner-delegated-amendment--2026-09-23)
records the same two decisions.

### Q1: exit code. Decision: an orderly quit exits 0

A refused agent workspace recovery does not turn an otherwise orderly quit
into exit 1. This covers the pre-existing 0775 parent and every other refusal
that recovery returns: repair required, capacity or unavailable. Exit 1 is
kept for failures of the quit's own work:

- state that could not be flushed or delivered;
- the host's shutdown cleanup;
- removal of the workspace parent that this run created;
- a recovery whose result never reached the decision lane.

Reasons:

1. **The refusal is decided at startup. It is not a failure of the quit.**
   The refusal is correct, and it repeats on every launch until someone
   repairs the folder. Exit 1 would therefore mark every close of that install
   as failed while saying nothing about the close. It would also hide the
   failures the code exists for: a flush that lost state would look the same
   as a folder mode.
2. **Nothing is lost or left behind.** RFC-BPR-0033 B1 makes the Windows and
   Linux close "the existing drain and Quit path", and [[BPR-0798]] and
   [[BPR-0799]] observe that path exiting 0. With this refusal no workspace is
   created, no entry is removed, and the profile's state is saved as on any
   quit.
3. **The exit code was never a signal the person could see.** This card's
   AT-SPI walk found the exit code was the only signal at Home. A person who
   starts a desktop application from a launcher is never shown its exit code,
   so they had no signal at all. Making the refusal visible is Q2's job.
4. **The security rule is the refusal itself, and it does not change.** No
   agent workspace is created or reclaimed under a folder that another account
   can write. No folder the application did not create is repaired. The exit
   code enforces neither rule.
5. **No product decision made the refusal exit 1.** `0427dc76` reused the
   cleanup flag for the recovery result, and nothing records a decision to do
   so. [[BPR-0875]]'s note at that line already warns that the fold makes a
   reader look "for a shutdown fault that never happened".

### Q2: visibility. Decision: shown at Home, in the window's status bar

Every window of the profile states the refusal in its status bar, the way
RFC-BPR-0033 B3 reports a failed profile operation:

- on Home and on every other surface;
- for as long as the profile runs;
- in a window opened later as well.

The agent panel keeps its "Not started" refusal. No chooser, dialog or other
window is presented, and nothing is repaired automatically.

Reasons:

1. **The agent boundary already requires it.** It says "Missing, ambiguous or
   substituted ownership causes visible repair/refusal"
   ([agent boundary](../../architecture/ai-agent-boundary.md)). A refusal
   shown only in a panel that is not showing at Home is not visible to the
   person.
2. **Only the person can repair it.** The private-store policy refuses a
   group- or world-writable parent so that another local account cannot plant,
   swap or race an entry where agent workspaces are created and later removed.
   Those workspaces hold exact checkouts of what may be private code. For the
   same reason the application never changes a folder it did not create. If the
   person is not told, the folder is never repaired, and disposable agent
   workspaces stay unavailable without a word.
3. **B3's rule fits this case exactly.** A failure that happens while a window
   exists is reported in that window, and no other window is presented for it.
   Home is what a window shows at startup. Its status bar is already where Home
   reports what is wrong: the retained layout sentence ([[BPR-0742]]) and the
   profile refusal ([[BPR-0777]]).
4. **With Q1 at exit 0, the status bar is the only signal.** It therefore
   has to be where the person looks.

### Effect on acceptance

Criterion 2 is decided by this amendment, as follows. With a pre-existing 0775
parent, the refusal holds and nothing is repaired. The window's status bar at
Home states the refusal. Closing the last window exits 0. Criteria 1 and 3 are
unchanged.

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

## Owner-delegated decisions implemented (bpr-main-linux, 2026-09-23)

The full record is in
[the evidence file](../../verification/BPR-0817-linux-owner-private-data-root.md#owner-delegated-decisions-implemented-bpr-main-linux-2026-09-23).

- **Exit code (Q1).** Startup recovery now records a refusal as a refusal:
  `AgentWorkspaceRecoveryRefused` in the status record, plus one
  `AGENT_WORKSPACE_RECOVERY_REFUSED:<registry>:<reason>` line on standard
  error. It no longer sets the cleanup flag that a real quit turns into exit 1.
  Two failures still fold into that flag, both the quit's own: removal of the
  workspace parent this run created, and a recovery that could not be posted.
- **Visibility (Q2).** Every window of the profile says "Disposable agent
  workspaces are unavailable: their folder needs repair or has reached its
  limit." in its status bar, on Home and on every other surface. A window
  opened later says it too. The agent panel keeps its "Not started" refusal.
- **Unchanged.** The neighbourhood's refusal rule is unchanged, and so is
  `1c6eaf21`'s owner-private creation. Nothing repairs a folder the
  application did not create.
- **Evidence.** `application_profile_management_public_path` now drives a
  quit-policy desktop over a pre-existing 0775 parent under the stock `/tmp`.
  It requires three things:
  - the status bar says the refusal at Home, in both windows;
  - `exit_code() == 0` after the last window closes;
  - the parent is still 0775 and no neighbourhood exists.

  `application_agent_crash_recovery`'s `startup-repair` now also requires the
  status-bar sentence and no shutdown cleanup failure. Two negative controls
  (the old fold put back, and the status-bar call removed) fail those
  assertions. The 41-test integration selection passes 41/41.
- **Native Linux course.** Pending. It needs a visible window on the shared
  WSLg desktop, and the coordinator must clear that run first.

### Pinned-code review for the reseal

The review covers `application_review_round_trip_composition_contract`,
recorded there as the `BPR-0817 reseal` note.

- **Scope.** The pins matched at `0b57fe2e`. This was checked by printing the
  digests of a `git archive` export of that commit: full `895f514f…`,
  manifest `eb4cae53…`. Since then, the only change inside the pinned scope is
  this card's.
- **What changed.** In `github_application.cpp` and `github_application.hpp`:
  - the recovery completion records a refusal as a refusal instead of setting
    `AgentWorkspaceCleanupFailed`;
  - the shutdown fold drops the recovery refusal's name;
  - `present_agent_workspace_repair` also sets the window's status-bar
    sentence;
  - the status record gains `AgentWorkspaceRecoveryRefused`;
  - `<iostream>` is included for the one standard-error line.
- **Paths not touched.** No credential, vault, provider, transport, GitHub
  request or mutation path changed. The refusal rule and every filesystem
  effect are unchanged.
- **New pins.** Full application
  `3310863dc52f182c47b8bac10706fd91436230714d6b183135fb32e8b1c6eebe`.
  Production manifest
  `e7fa2809312c1aa38f9b5c6196b67fe83d7251ab2e460f2beb3c5bd210b5a016`. The
  mutation branch is unchanged at
  `a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.
- **Result.** The contract failed on the full-file digest before the reseal
  and passes after it. `application_composition_contract` passes all seven
  subtests.

## Native course on WSLg, 2026-09-23: the observer failed, so criterion 2 stays open

The run window was 18:11:42Z–18:12:15Z UTC, under bpr-main-win's desktop hold.
The courses ran `run-courses.sh` against the tests-disabled `app-release`
image of `14a18b45` (SHA-256 `08f6d04d…cc73`), under umask 0002, through the
station lease and the live-state guard. The guard left all eight live files
unchanged in both runs.

| Course | Result | Exit | Folder modes after | Status bar read through AT-SPI |
|---|---|---|---|---|
| Pre-existing 0775 parent | **Failed in the observer**, guard rc 4 | **-15**: the course stopped the application after its failure, before any close | parent 0775 (unchanged), data root 0700, no neighbourhood | `""`. The course waited 30 s for the refusal sentence and never read it |
| No parent (control) | Passed | **0** after caption close, 116 ms | parent 0700, data root 0700, neighbourhood 0700 | `""` |

- **The product did what it should in the 0775 run.** Standard error carries
  `AGENT_WORKSPACE_RECOVERY_REFUSED:…/data/BetterPullReview/BetterPullReview/agent-workspace-ownership-v1:repair-required`.
  The parent stayed 0775, and no neighbourhood was created.
- **The course could not read the status bar.** The label's text never reaches
  AT-SPI. Qt's labels do not implement the AT-SPI Text interface: bpr-linux's
  2026-09-17 tree dump records `UnknownInterface` for "Workbench status". The
  label's explicit accessible name also replaces its text as the name. The
  status bar was therefore never read in either run, and the control's "no
  refusal" check proves nothing.
- **Not observed.** The status bar at Home was not observed natively, and
  neither was the caption-close exit code over a 0775 parent. Criterion 2's
  automated evidence stands (`application_profile_management_public_path`
  asserts both), but the real-application observation is still missing.
- **Next.** The course needs two fixes:
  - read the status line from Qt's polite announcement, which carries the
    whole line as an AT-SPI announcement event, or from another readable
    surface;
  - always perform the caption close and record the exit code, even when
    the observation fails.

  After that, rerun the 0775 course under a new desktop hold. The course
  shows one window for about 30 s.

### Course fixed for the rerun (2026-09-23)

The course now reads the status line from the workbench frame's AT-SPI
announcements. It always sends the caption close and records the exit code,
and its control requires at least one announced line.

On a private Xvfb display, with the same `14a18b45` image:

- **0775 parent.** The course passes. The announced line ends with the
  refusal sentence, and the caption close exits 0.
- **Control.** The course passes. The line is "Not signed in. Public pull
  requests open read-only.", and the close exits 0.
- **Negative controls.** Both fail as intended.

This validates the course. It is not the WSLg observation. What remains is
the 0775 rerun on WSLg under a new desktop hold.

## Native WSLg rerun: criterion 2 passes (2026-09-23)

The 0775 course ran once, at 18:23:27Z–18:23:29Z UTC, under bpr-main-win's
desktop hold. It used the course at `7964d868` on the tests-disabled image of
`14a18b45` (`08f6d04d…`), on WSLg `:0` with `Weston WM`, under umask 0002.

- **Status line at Home.** The frame announced "Not signed in. Public pull
  requests open read-only. Disposable agent workspaces are unavailable: their
  folder needs repair or has reached its limit."
- **Exit.** The caption close exited **0**, 66 ms after the close.
- **Folders.** The parent stayed **0775**. The data root was created 0700.
  No neighbourhood was created.
- **Standard error.** It names the `repair-required` refusal.
- **Guard.** The guard reported `passed`, with the live files unchanged.

The full record is in
[the evidence file](../../verification/BPR-0817-linux-owner-private-data-root.md#native-wslg-rerun-of-the-0775-course-pass).

**Carried to the RFC-BPR-0033 closeout.** This card extended two courses: the
0775 section of `application_profile_management_public_path`, and the
status-bar sentence in `application_agent_crash_recovery`'s `startup-repair`.
Neither extension has run on macOS or Windows yet.
