---
schema: "implement-rfc.card.v1"
id: "BPR-0849"
title: "The owner admission public path fails on Linux at the orderly owner quit"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0847-drain new defect 1","found_at":"03dd9ffd0eeda0866c2648859697be7b564dddbc","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0849: The owner admission public path fails on Linux at the orderly owner quit

## Defect

`application_profile_owner_admission_public_path` fails on `bpr-linux` with
`FAILED: orderly owner quit` in 0.74 s at `03dd9ffd`. It passes in 1.34 s
with a private `0700` `BPR_TEST_SOCKET_ROOT`.

The cause is the one [[BPR-0847]] named. The course puts its data root
straight into shared `/tmp` when `BPR_TEST_SOCKET_ROOT` is unset
(`tests/end_to_end/profile_owner_admission_contract.py:23`). An owner creates
its agent workspace neighbourhood only under a parent this user owns and
nobody else can write, so workspace recovery refuses and the owner's quit
exits 1. The macOS runner sets `BPR_TEST_SOCKET_ROOT` to a private folder.
Windows passes `dir=None`, the user's own temporary folder.

Reported by `bpr-linux` in `task-20260917-lnx-0847-drain`, not carded there.

## Scope

- Give the course the owner-only parent BPR-0847 gave the activation course,
  on POSIX when `BPR_TEST_SOCKET_ROOT` is unset. Keep the Windows branch and
  the orderly quit assertion unchanged.
- Check the other courses that take `BPR_TEST_SOCKET_ROOT` with a shared
  fallback, and name any that need the same change.

## Acceptance

- The course passes on Linux, 3 runs in a row, and still passes on macOS.

## Current frontier (dsp-004, bpr-linux, task-20260917-lnx-0846-0849)

- **Outcome:** the fix is in the course, on `linux-lane`. The course passes
  on Linux with it; the 3 runs in a row at the pushed commit are under
  "Verification at the commit". The card stays In Progress until main runs
  the fixed course on macOS.
- **Changed path:** `tests/end_to_end/profile_owner_admission_contract.py`
  only. No product source and no pinned digest changed.
- **Logs:** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-0846-0849/`.
  None is committed.
- **Still open:** the macOS run under "For macOS".

## Fix (dsp-004, bpr-linux)

The driver gets the parent [[BPR-0847]] gave
`profile_activation_contract.py`. A new `private_fixture_parent()` yields:

- `BPR_TEST_SOCKET_ROOT` when it is set, exactly as before (macOS's runner,
  and any host that sets it);
- `None` when it is unset and `os.name` is not `posix`, exactly as before,
  so Windows still uses the user's own temporary folder;
- otherwise, on POSIX, its own
  `tempfile.TemporaryDirectory(prefix="bpr-admission-course-", dir="/tmp")`.

The course root, `own-XXXXXXXX`, is made inside that parent. On POSIX only,
the driver then checks the root's parent with the product's own test (a
directory owned by this user, with no group or other write bit), so a shared
parent fails by name before any owner starts. Cleanup removes the course
root, then the parent with profile 1's neighbourhood beside the root,
whether the course passes or fails.

**Unchanged.** `quit_owner`'s `code == 0 and "OWNER_RELEASED"` check and
every other assertion, the C++ course, and the course's registration.

**Windows.** The course is registered outside `if(UNIX)`
(`tests/end_to_end/CMakeLists.txt`), so Windows runs this driver. On
Windows, `os.name` is `nt`. The only new code reached there is
`os.environ.get`, the `os.name` test and `yield None`: `os.geteuid`,
`lstat` and the `stat` bit tests sit under `if os.name == "posix":`.
`contextlib` and `stat` are portable modules. As a simulation (not a
Windows run), `main()` ran in-process on Linux with `os.name` patched to
`nt`, `os.geteuid` deleted and a fake application that speaks the course's
markers. The fixed driver and the unchanged driver both printed
`PROFILE_OWNER_ADMISSION_OK`, and both made exactly one
`TemporaryDirectory(prefix="own-", dir=None)`. A mutant with the POSIX
guard removed failed with `AttributeError` on `os.geteuid`, so the
simulation would have caught one (`windows-branch-simulation.log`).

**Socket paths.** Profile 1's endpoint is
`/tmp/bpr-admission-course-XXXXXXXX/own-XXXXXXXX/activate.sock`, 61 bytes,
and profile 2's is `…/profiles/profile-2/activate.sock`, 80 bytes, against
the adapter's limit of 108.

## Verification before commit (Linux, bpr-linux, WSL2)

The build is the stock `dev` preset at `e6b7193e`; `ninja -n` reported no
work, and only this Python driver changed after it. The course runs
offscreen by registration (`QT_QPA_PLATFORM=offscreen`,
`QT_STYLE_OVERRIDE=Windows`). The Secret Service `login` collection read
`(<false>,)` before each group. Each run was a serial
`ctest --test-dir build/dev -j1 -V -R '^<name>$'`.

| Run | Result | Time |
|---|---|---|
| The unchanged driver at `513fd30960053146d9233d70b3d581a1cb8e2447` (`before-…-run1.log`) | Fail, `FAILED: orderly owner quit` at `quit_owner(owner)`, profile 1, after profile 2's owner quit cleanly | 0.75 s |
| The fixed driver (`candidate-…-run1.log`) | Pass, `PROFILE_OWNER_ADMISSION_OK` | 1.34 s |
| The fixed driver with `BPR_TEST_SOCKET_ROOT=/tmp` (`candidate-negative-shared-parent-…-run1.log`) | Fail by name: `/tmp is not owner-only, so an owner's workspace recovery would refuse` | 0.03 s |

No `bpr-admission-course-*` or `own-*` directory was left in `/tmp`.

Gates: `git diff --check` is clean, and
`python3 scripts/verify_documentation.py --root .` prints
`DOCUMENTATION_OK`. No C++ file changed, and the repository has no Python
format or lint gate.

## Other courses with a shared fallback

None needs the change. These are the other readers of
`BPR_TEST_SOCKET_ROOT` in `tests/` and `scripts/`.

- **`tests/sqlite_contract/profile_activation_tests.cpp` ~39**
  (`sqlite_profile_activation`). It uses `mkdtemp` under
  `BPR_TEST_SOCKET_ROOT` or `/tmp`, so its root is private (0700). It links
  only the SQLite adapter, so no agent workspace neighbourhood is involved.
  On POSIX, `validate_private_storage_root` checks the root itself, not its
  parent. It passed on Linux at `513fd309` in 4.13 s.
- **`tests/scripts/network_isolation_counterexamples.py` ~252**
  (`test_network_isolation_loopback`). This is pure Python: it binds a
  socket in a private `contained_temporary_directory` and runs no product
  code. It passed in 0.03 s.
- **Courses that put a data root straight into the default temporary
  folder without reading `BPR_TEST_SOCKET_ROOT`.**
  `application_profile_window_restart_public_path` and
  `application_profile_window_layout_public_path` both passed on Linux at
  `513fd309`, in 5.77 s and 23.86 s. They are outside this card's scope,
  and they were run only as a check.

## Independent review

An `rfc-reviewer` reviewed the diff (`candidate.diff`, sha256
`69ca4805e8e53c5427f5664b9e68ba193ffdcc54294adc23ce034700e93dbac4`) and
found no blocking finding. It confirmed that:

- the set and unset branches keep their old parent on Windows, and nothing
  POSIX-only is reachable there;
- no owner check is weakened;
- cleanup and context-manager order are correct on both paths;
- the macOS branch passes the new parent check, as it does for the
  activation course;
- the socket paths are within the limit;
- the simulation is sound for what it claims;
- neither surveyed site needs the change.

Two non-blocking notes:

- The docstring said "the same parent". The two courses share the
  construction, not the directory, so it now says "the same kind of
  parent".
- A set but empty `BPR_TEST_SOCKET_ROOT` resolves against the working
  directory, as before, but on POSIX it now meets the parent check first.
  That matches the activation course. No runner sets an empty value, so
  this is kept.

The same reviewer then confirmed the final diff (`candidate-2.diff`, sha256
`307bcc168e994de44beb04a6642ff04d837f137b84afaac5d1b3685954b73881`): only
the docstring's wording and wrap changed.

## For macOS

- **Still needed:** run `application_profile_owner_admission_public_path`
  on macOS with the fixed driver. It takes the `BPR_TEST_SOCKET_ROOT`
  branch, so only the new parent check can differ there.
