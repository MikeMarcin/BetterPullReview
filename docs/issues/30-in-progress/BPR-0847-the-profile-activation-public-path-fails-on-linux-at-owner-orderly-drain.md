---
schema: "implement-rfc.card.v1"
id: "BPR-0847"
title: "The profile activation public path fails on Linux at the owner's orderly drain"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-epoch-and-startup new defect 2","found_at":"98262aee0ac0897985e97784ebfc74a7c6225a3a","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0847: The profile activation public path fails on Linux at the owner's orderly drain

## Defect

`application_profile_activation_public_path` fails on `bpr-linux` at
"owner orderly drain". It failed twice, including with `src/app/main.cpp`
at `98262aee`, before [[BPR-0843]]'s change.

Reported by `bpr-linux` in `task-20260917-lnx-epoch-and-startup`, not carded
there.

## Scope

- Capture the failing step's state on Linux, and whether macOS passes the
  same course at the same head.
- Fix the cause, whether it is in the course or in the product's owner
  shutdown.

## Acceptance

- The course passes on Linux, 3 runs in a row, and still passes on macOS. The
  cause is named here.

## Current frontier (dsp-004, bpr-linux, task-20260917-lnx-0847-drain)

- **Outcome:** the cause is named below, and it is in the course. The fix
  is committed on `linux-lane`. The card stays In Progress until main runs
  the fixed course on macOS.
- **Changed path:** `tests/end_to_end/profile_activation_contract.py` only.
  No product source and no pinned digest changed.
- **Logs:** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-0847-drain/`.
  None is committed.
- **Still open:** the macOS run under "For macOS". The sibling course's
  defect under "New defect" is reported, not carded.

## Cause

The cause is in the course, not in the product's owner shutdown.

1. An owner's startup recovers journaled agent workspaces in a
   neighbourhood beside its data root, `<data root>-agent-workspaces-v1`
   (`src/app/src/github_application.cpp`, the `Q_OS_UNIX || Q_OS_WIN`
   branch).
2. The Git adapter creates that neighbourhood only under a parent that this
   user owns and nobody else can write (`workspace_parent_trusted` in
   `src/adapters/git/src/installed_git_process.cpp`). Otherwise recovery
   returns `repair_required`.
3. A refused recovery sets `AgentWorkspaceCleanupFailed`. The profile's
   shutdown folds it into `CleanupFailed`, and `profile_desktop` sets exit
   code 1 when the runtime retires. The owner's
   `expect(desktop->exit_code() == 0, "owner orderly drain")` then fails.
4. The course made its root in `BPR_TEST_SOCKET_ROOT`, or in `/tmp` when
   that is unset. Profile 1's data root is the course root itself, so its
   neighbourhood's parent was `/tmp`: shared, mode 1777, owned by root, which
   the sandbox's user namespace shows as uid 65534. Profile 2's data root is
   nested inside the private course root. That is why profile 2's owner
   drained cleanly and profile 1's failed.

**Why only Linux.** The macOS runner, `scripts/run_network_isolated_test.py`,
sets `BPR_TEST_SOCKET_ROOT` to its own 0700 `fixture` directory, so the
parent there is private. The Linux network sandbox,
`tests/network_isolation/linux_network_sandbox.cpp`, sets none, so the
course fell back to `/tmp`. The product's rule is the same on both.

**The refusal is correct.** Under a parent other users can write, another
user could swap the neighbourhood out. A real data root sits under a
directory its user owns. The rule (`77651b107c155e00bf5fd420b10a47685495b795`)
and the recovery's effect on the exit code
(`0427dc761c1691e3afa277b383dd738321859237`) both predate the course
(`a654669c7475d5b0254e071bbbdd25b3e4dfc20b`).

**How it was found.** Temporary stderr tracing at every source of the exit
code was added, run and reverted; none of it is committed
(`instrumentation-{1,2,3}.diff`, `instr-*.log`). It showed:

- window layouts, the MCP snapshot, host cleanup and review restoration all
  cleaned up;
- the only flag set at `poll_shutdown` was the agent workspace one;
- on Unix, shutdown never removes a workspace parent, so the flag came from
  startup recovery;
- recovery refused with `parent=/tmp uid=65534 euid=1000 mode=1777` and
  `error=repair_required`.

As a control, the same binaries passed with `BPR_TEST_SOCKET_ROOT` set to a
private 0700 directory (`instr-3-private-root.log`).

## Fix (dsp-004, bpr-linux)

When `BPR_TEST_SOCKET_ROOT` is unset, the driver makes its own private
parent, `tempfile.TemporaryDirectory(prefix="bpr-activation-course-",
dir="/tmp")`, and puts the course root inside it. `/tmp` keeps the socket
path short: 72 bytes for profile 2's endpoint, against the adapter's limit of
108. The driver then checks the root's parent with the product's own test (a
directory owned by this user, with no group or other write bit). A parent
that fails is named before any owner starts. Cleanup removes the parent and
the neighbourhood beside the root, whether the course passes or fails.

**Why the course still asserts an orderly drain.** Neither C++ check
changed. `exit_code() == 0` after Quit still needs every cleanup to succeed,
for each owner that checks it: the first, the other profile's, the recovered
and the survivor. On Linux that now includes a workspace recovery that
actually runs; before the fix it was refused outright. The fix gives Linux
the layout macOS always had. A real drain failure still exits 1.

**macOS and Windows.** On macOS the runner sets `BPR_TEST_SOCKET_ROOT`, so
the new branch is not taken. Only the parent check is new there, and the
product already reads the same parent inside the sandbox. The course is
registered under `if(UNIX)`, so Windows never runs it. No portable source
changed.

## Verification before commit (Linux, bpr-linux, WSL2)

Every row runs offscreen by registration (`QT_QPA_PLATFORM=offscreen`,
`QT_STYLE_OVERRIDE=Windows`). The build is the stock `dev` preset, rebuilt
after the tracing was reverted; neither binary contains the trace text. The
Secret Service `login` collection read `(<false>,)` before each course group.

| Run | Result | Time |
|---|---|---|
| Course at `03dd9ffd0eeda0866c2648859697be7b564dddbc`, before any change (`repro-1.log`) | Fail, `FAILED: owner orderly drain` | 1.26 s |
| The unchanged driver against the clean rebuild (`ab-head-script.log`) | Fail, the same step | 1.19 s |
| The fixed driver (`candidate-2-pass.log`) | Pass, `PROFILE_ACTIVATION_OK` | 2.70 s |
| The fixed driver with `BPR_TEST_SOCKET_ROOT=/tmp` (`candidate-2-negative-shared-parent.log`) | Fail by name: `/tmp is not owner-only, so an owner's workspace recovery would refuse` | 0.16 s |

Gates: `git diff --check` is clean, and
`python3 scripts/verify_documentation.py --root .` prints
`DOCUMENTATION_OK`. No C++ file changed, so clang-format and clang-tidy
have nothing to check.

**Independent review.** An `rfc-reviewer` reviewed the diff and found no
blocking finding (`candidate.diff`, sha256
`57b5c112b0e2aadcbb572dc6c73593b3bd968008fdc6fb56d567ac319d5b588c`). It
confirmed each link of the cause against the source, and that the refusal
of a shared parent is intended. It confirmed that no owner's drain check is
weakened, and that the macOS branch is unchanged. It confirmed cleanup on
both paths, the socket length, and that nothing pins the driver. Two of its
notes were taken up: a distinct directory prefix, and the check written as
a named boolean. The same reviewer then confirmed the final diff
(`candidate-2.diff`, sha256
`302971c6d260049d505b54594fed8bec23ddb8d19f86f6bcfd6270ac71d20da5`, identical
to the commit). Its prediction that the sibling course fails the same way
held; see "New defect".

## For macOS

- **Main's evidence, not this station's:** macOS passed this course 3 of 3
  at `a1e276b7af1fff9ff04f00bc5c941cc5401e4adf` source (`build/r2-dev`,
  `build/r2-logs/dsp4-0847-mac-a1e276b7-run{1,2,3}.log` on the Mac). That
  was before this fix.
- **Still needed:** run `application_profile_activation_public_path` on
  macOS with the fixed driver. It takes the `BPR_TEST_SOCKET_ROOT` branch,
  so only the new parent check can differ there.

## New defect (reported, not carded)

`application_profile_owner_admission_public_path` fails on Linux at
`FAILED: orderly owner quit`, from the same cause:
`tests/end_to_end/profile_owner_admission_contract.py` also falls back to
`/tmp`.

- At `03dd9ffd` it fails in 0.74 s (`probe-owner-admission.log`).
- With a private 0700 `BPR_TEST_SOCKET_ROOT` it passes in 1.34 s
  (`probe-owner-admission-private-root.log`).

This card's dispatch covers one course, so it is not fixed here. The same
`private_fixture_parent` change applies.
