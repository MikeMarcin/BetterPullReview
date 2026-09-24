# BPR-0906 Linux measurement: which owner-exit paths unlink the activation endpoint

**Later: the Linux fix.** This file measured the problem and chose no fix. The
fix came later, on `main`, and [[BPR-0906]] records it. On Linux the product
now binds in `$XDG_RUNTIME_DIR` when that names an owner-private directory.
The Linux test launcher gives each row its own such directory and removes it
afterwards. The exit paths below are unchanged: an owner ended by a signal
still leaves its socket, but now it leaves it in the row's directory.

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-007` (station
  dispatch `bpr-linux/int-16`), mandate r2. Station `bpr-linux`
  (`stn-3f7fcc92`, `wsl2`), checkout `/home/mmarcin/bpr-linux`, branch
  `linux-lane`, authority `lane-commit`. Base
  `1ec82785328b9b6574b102c095f52374d224a94e`.
- **Scope of this file.** [[BPR-0906]]'s first scope item only: *does the
  owner unlink its endpoint on an orderly exit, and which paths skip it?*
  This file is evidence. **No fix was chosen and none was implemented**, per
  the dispatch: the choice is entangled with [[BPR-0895]] and is decided once,
  on `main`, for both platforms. The card is not moved and no ID was minted.
- **State.** Complete. Every path below was produced on this host, not
  reasoned about. One previously unnamed leaking row was found and reproduced.
- **Commands.** None is active. Two read-only scouts ran and have returned.
- **Host hygiene.** `/run/user/1000` held no `bpr-activate-*.sock` before this
  work and holds none after it. Every socket this work created was removed by
  name, never by a glob sweep.

## Station, revision and build

Ubuntu 26.04 LTS on WSL2 (`Linux 6.18.33.2-microsoft-standard-WSL2 x86_64`),
2026-09-17, branch `linux-lane` at
`1ec82785328b9b6574b102c095f52374d224a94e`, clean tree. CMake 4.4.2, Ninja
1.13.2, GCC 15.2.0, Qt 6.10.3, Python 3.13.15 (`/home/mmarcin/.local/bin/python3`).
Stock `dev` preset, no flag overrides: `CC`, `CXX`, `CFLAGS`, `CXXFLAGS`,
`LDFLAGS`, `TMPDIR` and `BPR_TEST_SOCKET_ROOT` were all unset.
`XDG_RUNTIME_DIR=/run/user/1000` was set by the session but the product does
not read it ([[BPR-0894]] left that as deliberate future work); it probes
`/run/user/<uid>` directly and that is the directory every socket below
appeared in.

- **Configure.** `cmake --preset dev`: **exit 0**.
- **Build.** `cmake --build --preset dev`: **exit 0**, 258 steps.
- **Keyring.** The `login` collection `Locked` property read `(<false>,)`,
  **exit 0**, before the course groups.

Row names were resolved against `ctest --preset dev -N` (**exit 0**), not card
prose: `application_profile_startup_admission` (#124),
`sqlite_profile_activation` (#147),
`application_profile_activation_public_path` (#362),
`application_profile_owner_admission_public_path` (#363),
`application_attended_launch_contract` (#336),
`application_device_binding_restart` (#353),
`application_device_binding_launch_renewal` (#354),
`application_device_binding_relaunch` (#355).

## The headline: a survivor is reclaimed, not refused

**A stale endpoint left by a dead owner is reclaimed on the next start with
the same root.** Measured, not inferred.

`make_profile_activation_server` finds an existing entry at the derived name
and, before binding, proves it is dead:
`src/adapters/sqlite/src/profile_activation.cpp:413-427`. It requires the
entry to be a socket owned by this user at `0600`
(`private_socket`, `profile_activation.cpp:194-198`), then connects a probe to
it and demands `ECONNREFUSED` — nobody listening — and only then unlinks it
(`profile_activation.cpp:419-424`). A live owner answers that probe, so the
branch returns `unavailable` and **leaves the live socket alone**; that is the
guard the card asks for.

Produced on this host: an owner was started against a root, `SIGKILL`ed, its
socket confirmed present as `srw-------` inode 348, and a second owner started
against **the same root**. It reached `OWNER_READY`, the endpoint at the same
name was now inode **349**, and its own orderly quit removed it. Reclaimed, and
reclaimed by unlink-and-rebind rather than by reuse.

So the accumulation is **inert, not harmful**, and [[BPR-0906]] is the milder
half of the pair: [[BPR-0895]] can be decided close to alone. Two boundaries
qualify that, both measured:

- **Accumulation is unbounded across *different* roots, not across the same
  one.** The name is a digest of the lexically normalised root
  (`endpoint_name`, `profile_activation.cpp:149-162`), so reclaim only ever
  happens when a root is used again. The harness never reuses one: two
  consecutive runs of the *same passing row* left **two different names**
  (`bpr-activate-8eda46f659f9bcc5.sock`, then also
  `bpr-activate-402a84a6b2a8d5f6.sock`), because its data root is a fresh
  `mkdtemp` each time. Nothing will ever reclaim either, because nothing will
  ever name those roots again. The reclaim path makes a *profile's* endpoint
  self-healing; it does nothing for the debris.
- **A leftover that is not a socket is refused forever.** `private_socket`
  rejects a non-socket before the liveness probe runs, so the bind fails. A
  plain `-rw-------` file planted at a root's own digest name made that root's
  owner start with `profile-activation-endpoint-unavailable bind-refused`
  (`src/app/src/github_application.cpp:12770-12775`) and keep running without a
  rendezvous; the file was still there afterwards. The product's own comment at
  `github_application.cpp:12770-12772` states the consequence: such a profile
  "answers its own second launch with owner-busy forever". The product never
  creates that shape itself, so this is the boundary at which accumulation
  would become harmful, not a defect observed in it.

## Why an orderly exit is not the whole answer

The only unlink on an exit path is a **C++ destructor**,
`~native_server()` at `src/adapters/sqlite/src/profile_activation.cpp:263-280`,
and it is guarded three ways (`profile_activation.cpp:274`):

1. `detail::validates_profile_runtime_lease(Root, Lease)` — the same process
   must still hold a genuine lease on the **same root inode**
   (`src/adapters/sqlite/src/catalog_database.cpp:4378-4392`: the lease's held
   descriptor is `fstat`ed and compared `st_dev`/`st_ino` against an `lstat` of
   the root path, which also fails outright when the root is gone);
2. the endpoint must still exist;
3. it must be the **same** `st_dev`/`st_ino` the owner captured when it bound
   (`profile_activation.cpp:261`).

Guards 1 and 3 are deliberate and correct — they are what stops an owner
unlinking an endpoint it no longer owns — but they mean **an orderly exit can
still leave the socket**, which the paths below show. There is no signal
handler anywhere in `src/**` for `SIGTERM`, `SIGINT`, `SIGHUP`, `SIGSEGV` or
`SIGABRT` (the only `signal()` calls are `SIGPIPE` handling and a child's
re-raise in `src/adapters/agent/src/unix_agent_supervisor.cpp:59,87,117`), and
there is no exit-path sweeper: the only three unlinks of an endpoint in the
whole tree are `profile_activation.cpp:275` (this destructor) and
`:422`/`:434` (the *next* launch's reclaim).

## The paths

`survives` is whether `/run/user/1000/bpr-activate-<digest>.sock` was still
there after the owner was gone. Each row used its own fresh data root, so each
concerns exactly one endpoint. Exit statuses are reported as observed.

| Path | Survives | Code that unlinks, or fails to | How it was produced |
|---|---|---|---|
| Orderly quit (the product's own shutdown: `desktop->quit()`, drain to quiescence) | **No** | `~native_server()` runs and all three guards hold, `profile_activation.cpp:263-280`, unlink at `:275` | Owner to `OWNER_READY`, touch the quit file; **exit 0**, `OWNER_RELEASED`; socket inode 339 present before, absent after |
| `SIGTERM` (what a driver's `Popen.terminate()` sends) | **Yes** | No handler in `src/**`; default disposition, destructor never runs | `send_signal(SIGTERM)` to a live owner; **killed by signal 15**; inode 340 unchanged |
| `SIGINT` | **Yes** | As above | **killed by signal 2**; inode 341 unchanged |
| `SIGHUP` | **Yes** | As above | **killed by signal 1**; inode 342 unchanged |
| `SIGKILL` (what a driver's `Popen.kill()` sends) | **Yes** | Uncatchable by construction | **killed by signal 9**; inode 343 unchanged |
| Crash: `SIGABRT` (abort) | **Yes** | Default disposition, no destructor | **killed by signal 6**; inode 344 unchanged |
| Crash: `SIGSEGV` | **Yes** | Default disposition, no destructor | **killed by signal 11**; inode 345 unchanged |
| **Data root removed under a live owner, then an orderly quit** | **Yes** | `~native_server()` runs but guard 1 declines: `lstat(root)` fails, `catalog_database.cpp:4385-4388`, so `:274` is false and `:275` is skipped | `rm -rf` the root, then the quit signal (kept outside the root); **exit 0** and `OWNER_RELEASED` — a fully orderly exit that still leaves inode 346 |
| **Data root replaced under a live owner (same path, new inode), then an orderly quit** | **Yes** | Same guard, by the inode-identity clause `held.st_ino == named.st_ino`, `catalog_database.cpp:4387-4388` | `mv` the root aside, recreate `0700`, `cp -a` the contents back, then quit; **exit 0**, `OWNER_RELEASED`; root inode 5506626 → 5506674; inode 347 left behind |
| CTest `TIMEOUT` firing | **Yes** | Nothing product-side can run | Measured in a throwaway project outside this repo: a victim with handlers for `SIGTERM`/`SIGINT`/`SIGHUP`/`SIGQUIT` under `TIMEOUT 5`. CTest **exit 8**, `***Timeout`; **no handler fired**, and the victim's own child died with it — the whole process group is killed uncatchably. `TIMEOUT_SIGNAL_NAME` and `TIMEOUT_SIGNAL_GRACE_PERIOD` appear **nowhere** in this tree, so no row can opt into a catchable signal |
| Owner-side `std::exit` on a failed assertion | **Yes** (by construction) | `std::exit` runs static destructors but not `main`'s automatic ones, so the owner's `desktop` is never destroyed; `tests/end_to_end/profile_activation_public_path_tests.cpp:22-27`, reached from the 30 s bound at `:29-38` | Not produced directly. Named because it is the one leak that a *failing* row adds on top of the driver's own kill |

### A live owner's endpoint is not taken

Also produced, because the card asks for a row showing a cleanup cannot steal a
live name: with an owner live and listening, its root was replaced so a
*second* owner could take the new root's lease and attempt the **same** digest
name. The second owner's probe connected successfully, so it refused the bind,
logged `profile-activation-endpoint-unavailable bind-refused`, and started
without a rendezvous. The live owner's endpoint was **inode 350 before and
inode 350 after**, untouched, and the first owner was still running.

## What a green sweep actually leaves behind

The activation rows themselves are clean. `ctest --preset dev -R` over
`application_profile_startup_admission`, `sqlite_profile_activation`,
`application_profile_activation_public_path` and
`application_profile_owner_admission_public_path`: **exit 0**, 4/4 passed,
`PROFILE_ACTIVATION_OK` and `PROFILE_OWNER_ADMISSION_OK` in the log, and
**zero** endpoints left behind. The activation contract's own deliberate
`SIGKILL` at `tests/end_to_end/profile_activation_contract.py:222` is followed
by a restart on the same root at `:224` that reclaims the survivor, and an
orderly `quit_owner` at `:226` that removes it — so that row cleans up after
itself by construction.

The leak is elsewhere, and the code says why: **every owner-only start binds an
endpoint**, at `src/app/src/github_application.cpp:12765-12766`, not only the
rows with "activation" in the name. Any row that starts a real owner and then
signals it leaks one.

`application_attended_launch_contract` is such a row, and it was reproduced
four times:

- Run alone: **exit 0** (passed), `/run/user/1000` went from 0 to 1
  `bpr-activate-*.sock`.
- Run again: **exit 0** (passed), 1 → **2**, a *different* digest.
- `application_device_binding_restart`, `application_device_binding_launch_renewal`
  and `application_device_binding_relaunch` each ran alone: **exit 0**, count
  unchanged at 1. They do not leak.

The path is `tests/end_to_end/attended_launch_contract.py:122-155`:
`test_it_outlives_the_automated_budget` starts the real `better_pull_review`
with `--attended-launch`, and an attended launch is *designed* never to exit on
its own, so the test's `finally` must signal it — `process.terminate()` at
`:150` (SIGTERM), escalating to `process.kill()` at `:154`. SIGTERM has no
handler, so the destructor never runs. This row is not doing anything wrong;
it is the shape [[BPR-0906]] predicted, found by name.

That is a second, previously unnamed source, so the earlier sweep's two
survivors should not be assumed to be the activation rows' doing.

## What this constrains about a fix, without choosing one

Recorded so the joint [[BPR-0895]] decision has the Linux facts, not as a
recommendation:

- "Unlink on orderly exit" is **already implemented** and already works. It
  cannot be the whole fix here, because the two orderly-exit rows above still
  leave a socket by design of the lease guard, and because the leaking row
  exits by signal, not orderly.
- A SIGTERM handler would close the largest measured hole (the attended-launch
  row, and any driver `terminate()`), and cannot help a crash, a `SIGKILL` or a
  CTest timeout.
- A launch-time sweep of the runtime directory is the one option that reaches
  the debris, and it is the one the card warns about: the digest names a root,
  so deciding a name is dead means deciding a *root* is gone, and the directory
  is shared with every profile the user runs. The existing per-name liveness
  probe (`profile_activation.cpp:419-421`) is the safe primitive that already
  exists and is proven above not to disturb a live owner.
- Nothing here argues for touching or re-binding the endpoint to keep it alive,
  which is the shape [[BPR-0895]] may want; that would make this debris
  strictly worse.

## Exact probes and commands

Every command was run from `/home/mmarcin/bpr-linux` with
`PYTHONDONTWRITEBYTECODE=1` exported for each `python3` invocation.

```sh
# gates and rows
cmake --preset dev                                   # exit 0
cmake --build --preset dev                           # exit 0
ctest --preset dev -N                                # exit 0, row names
ctest --preset dev -R '^(sqlite_profile_activation|application_profile_activation_public_path|application_profile_owner_admission_public_path|application_profile_startup_admission)$' --output-on-failure   # exit 0
ctest --preset dev -R '^application_attended_launch_contract$'          # exit 0, leaks one
ctest --preset dev -R '^application_device_binding_restart$'            # exit 0, leaks none
ctest --preset dev -R '^application_device_binding_launch_renewal$'     # exit 0, leaks none
ctest --preset dev -R '^application_device_binding_relaunch$'           # exit 0, leaks none

# the runtime directory, before and after every group
ls -la /run/user/1000/bpr-activate-*.sock
```

The exit-path probe was a throwaway driver at `/tmp/bpr0906_probe.py`,
deliberately **not** added to this tree: it is a measurement, not a test, and
adding a row would have been the product/test change the dispatch forbids. It
is reproducible from its description alone, and the part that matters is small.
It derives the endpoint exactly as
`tests/end_to_end/profile_activation_contract.py:81-92` does — the digest of
`os.path.normpath(root)`, in the directory chosen by that file's
`runtime_directory()` — then for each path it:

1. makes a fresh `0700` root under a fresh `0700` parent in `/tmp`, and seeds
   the registry with `profile_activation_public_path seed <root> 1 <signal>`;
2. starts `profile_activation_public_path owner <root> 1 <signal>` with
   `QT_QPA_PLATFORM=offscreen` and `QT_STYLE_OVERRIDE=Windows` (the row's own
   `ENVIRONMENT`, `tests/end_to_end/CMakeLists.txt:1086-1091`) and waits for
   `OWNER_READY`;
3. records the endpoint's mode and inode;
4. applies exactly one termination;
5. records the process's exit status and the endpoint's mode and inode again.

The signal file is passed outside the data root for the two root-removal paths,
so the owner can still be told to quit after its root is gone.

**Host left as found.** The probe removed only names it had derived from roots
it created itself, never a glob of the directory; the ten it created and the
two the `ctest` runs created were removed individually by name. `ls -la
/run/user/1000/bpr-activate-*.sock` exits 2 (no match) now, as it did before
this work, and the rest of `/run/user/1000` is byte-for-byte the same listing.
