---
schema: "implement-rfc.card.v1"
id: "BPR-1113"
title: "Linux activation: fall through an over-long XDG_RUNTIME_DIR, and pin the symlink-cleanup and other-owner cases"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux"]
metadata: {"found_by":"independent rfc-reviewer-critical review of BPR-0906 (claude-fable-5-1, read-only, macOS from source), findings 1, 3, 4 and 6, with 2, 5 and 7 as documentation","reviewed_candidate":"03117e63","origin":"owner-authorized review, attended session cbd48b91000b00f1e72e1ad62c7c384c, decision 96421f40-a5d6-4cbf-90b4-e855e8650f8b, 2026-09-24","related_cards":["BPR-0906","BPR-0881","BPR-0895"],"reservation":"docs/issues/_reservations/BPR-1111--BPR-1115--c042082d78b1.json"}
acceptance: [{"criterion":"AC1 A private XDG_RUNTIME_DIR too long for the endpoint is passed over, and the endpoint resolves to the next candidate. sqlite_profile_activation pins it, and it fails on the unchanged product","outcome":"pass","revision":"d03e2d61","evidence":{"kind":"command","detail":"Linux (bpr-main-linux, WSL2, GCC 15.2, Qt 6.11.2): sqlite_profile_activation prints runtime-directory:xdg-72-bytes=followed:xdg-73-bytes=passed-over; the 72-byte directory's 107-byte endpoint binds, the 73-byte one resolves where it does with no XDG_RUNTIME_DIR. On the unchanged product (93601efc) the row fails at 'a private XDG_RUNTIME_DIR too long for the endpoint is passed over, not refused'; an off-by-one trial fails at 'yields to the next candidate'.","path":"docs/issues/60-done/BPR-1113-linux-activation-fall-through-an-over-long-xdg-runtime-dir-and-pin-the-symlink-cleanup-and-other-owner-cases.md#verification-on-linux"}},{"criterion":"AC2 The runtime row plants escape symlinks. Their targets are unchanged after the launcher removes the row's directory. A trial that removes the directory by following links fails the row","outcome":"pass","revision":"d03e2d61","evidence":{"kind":"command","detail":"test_network_isolation_runtime_directory: the runtime child plants escape-file and escape-directory links into the stand-in user runtime directory; after both nested launcher runs their targets are byte-identical. Trials: a canonical-following removal fails with ESCAPE_REACHED:.../escape-file, a directory-following removal with ESCAPE_REACHED:escape-directory.","path":"docs/issues/60-done/BPR-1113-linux-activation-fall-through-an-over-long-xdg-runtime-dir-and-pin-the-symlink-cleanup-and-other-owner-cases.md#verification-on-linux"}},{"criterion":"AC3 A directory owned by another user is passed over as XDG_RUNTIME_DIR, and a row pins it. A trial that drops the owner check fails it","outcome":"pass","revision":"d03e2d61","evidence":{"kind":"command","detail":"sqlite_profile_activation prints runtime-directory:xdg-other-owner=passed-over:/root (root-owned, mode 0700, only stat'ed); the endpoint resolves as with no XDG_RUNTIME_DIR. A host with neither /root nor /var/lib/private so owned prints a skip line. Trial without the st_uid check fails at 'a private directory another user owns is passed over as XDG_RUNTIME_DIR'.","path":"docs/issues/60-done/BPR-1113-linux-activation-fall-through-an-over-long-xdg-runtime-dir-and-pin-the-symlink-cleanup-and-other-owner-cases.md#verification-on-linux"}},{"criterion":"AC4 The launcher leaves no /tmp/bpr-run-* behind on the paths the review names, or the card records why a window stays open","outcome":"pass","revision":"d03e2d61","evidence":{"kind":"command","detail":"The runtime row preloads a test-only fault shim into nested launcher runs: signals 2, 15 and 1 raised right after mkdtemp, a refused SIGHUP handler, and a failed wait. At 93601efc all five left the directory; at the candidate all five print WINDOW_CLOSED (signals reach the child, exits 254/241/255; handlers exit 125 before any directory; wait exit 125 with its directory removed), and an ignored SIGHUP reaches the child ignored. SIGKILL/CTest timeout, unforwarded signals and a launcher crash stay open; the card records why.","path":"docs/issues/60-done/BPR-1113-linux-activation-fall-through-an-over-long-xdg-runtime-dir-and-pin-the-symlink-cleanup-and-other-owner-cases.md#what-stays-open-and-why-ac4"}},{"criterion":"AC5 Findings 2, 5 and 7 are stated in live documentation","outcome":"pass","revision":"d03e2d61","evidence":{"kind":"freeform","detail":"Finding 2 and 7: header comment on profile_activation_endpoint (profile_activation.hpp) and the runtime_directory() doc comment; finding 2 and 5: docs/development/build-and-test.md, automated-test network boundary. verify_documentation.py: DOCUMENTATION_OK.","path":"docs/issues/60-done/BPR-1113-linux-activation-fall-through-an-over-long-xdg-runtime-dir-and-pin-the-symlink-cleanup-and-other-owner-cases.md#documentation-ac5"}},{"criterion":"AC6 No regression: every test_network_isolation_* row, sqlite_profile_activation, the profile activation and owner admission rows, application_attended_launch_contract, the three Linux Secret Service rows, msvc_secure_crt_contract, and verify_documentation.py. If a row's registration changes, also run test_network_isolation_manifest","outcome":"pass","revision":"d03e2d61","evidence":{"kind":"command","detail":"Final pass 22/22 (all 8 test_network_isolation_* rows including manifest after the registration change, sqlite_profile_activation, the three profile admission/activation rows, application_attended_launch_contract, msvc_secure_crt_contract, architecture, format, documentation and contained-temporary rows); the three native Secret Service rows 3/3 inside a private dbus-daemon with a test-only keyring; 8/8 composition rows rebuilt at the rebased d03e2d61; verify_documentation.py DOCUMENTATION_OK.","path":"docs/issues/60-done/BPR-1113-linux-activation-fall-through-an-over-long-xdg-runtime-dir-and-pin-the-symlink-cleanup-and-other-owner-cases.md#verification-on-linux"}}]
---

# BPR-1113: Linux activation: fall through an over-long XDG_RUNTIME_DIR, and pin the symlink-cleanup and other-owner cases

## Source

[[BPR-0906]]'s independent review, 2026-09-24. The owner asked for it in
attended session `cbd48b91` (decision `96421f40`). The reviewer was an
`rfc-reviewer-critical` (`claude-fable-5-1`), working read-only on macOS
from source. The candidate was `03117e63`. The review found no blocking
issue. It confirmed that the launcher only ever deletes its own `mkdtemp`
result, and that the product takes an `XDG_RUNTIME_DIR` only when the
directory is owned by `geteuid()` with mode `0700`. This card takes findings
1, 3, 4 and 6 as work, and findings 2, 5 and 7 as documentation. The review
is recorded on BPR-0906 under "Independent review (2026-09-24)".

Line references are the reviewer's, and the files are unchanged from
`03117e63` to the revision where this card was written. The product file is
`src/adapters/sqlite/src/profile_activation.cpp`. The launcher is
`tests/network_isolation/linux_network_sandbox.cpp`.

## Work

**1. An over-long private `XDG_RUNTIME_DIR` now fails instead of falling
through.** The product takes a private `XDG_RUNTIME_DIR` first
(`profile_activation.cpp:185-187`) and then checks the endpoint's length
(`:222`). The endpoint name adds 35 bytes, so on Linux a private directory of
73 bytes or more fails with `endpoint-path-too-long`. Before BPR-0906, such
a directory was never a candidate, and `/run/user/<uid>` or the next
candidate was used. The reviewer's fix is to skip the XDG candidate when its
size plus 35 is at least `sizeof(sun_path)`.

**3. The AC2 row does not catch a cleanup that follows symlinks.** The
runtime child (`tests/scripts/network_isolation_counterexamples.py:214-230`)
leaves a socket and a directory, but no symlink. A removal that followed
links would still pass the row. The reviewer suggests adding escape
symlinks: links in the child's runtime directory that point outside it.

**4. The other-owner claim is not pinned.** The Linux case in
`tests/sqlite_contract/profile_activation_tests.cpp:350-396` covers a
private, a widened and a relative `XDG_RUNTIME_DIR`. It has no case for a
directory owned by another user. The reviewer suggests
`XDG_RUNTIME_DIR=/root` or `/var/lib/private`.

**6. The launcher has small `/tmp` leak windows.** They are at
`linux_network_sandbox.cpp:328-331` and `:335`. A signal that arrives between
`mkdtemp` (`:284`) and the handlers installed at `:328` also leaves the
directory behind.

## Documentation

**2. Two launches with different private `XDG_RUNTIME_DIR`s no longer meet.**
Each resolves its own endpoint, so the second launch degrades to the lease's
`owner_busy` path. That is the symptom [[BPR-0881]] fixed. The reviewer
judges it intended, but BPR-0906 does not say so.

**5. The bus pin can send the Secret Service rows to the real bus.** On a
host with no `DBUS_SESSION_BUS_ADDRESS` but a live `$XDG_RUNTIME_DIR/bus`,
`pin_session_bus` (`linux_network_sandbox.cpp:138-151`) sets the address. The
Secret Service rows then run against the real bus
(`tests/github_contract/linux_secret_service_native_tests.cpp:1288-1291`).
BPR-0906's "Not observed on this station" says the Secret Service rows were
not run with the address unset. It does not say that they would then reach
the real bus.

**7. "Without following links" is inaccurate.** It holds only for the
`bpr-<uid>` child, which uses `lstat` (`profile_activation.cpp:197`). The
configured `XDG_RUNTIME_DIR` and the top-level candidates are checked with
`stat` (`:185`, `:193`). BPR-0906's scope repeats the phrase from BPR-0881's
guarantee.

## Scope

- Skip the XDG candidate when the endpoint would not fit, so the next
  candidate is used as it was before BPR-0906.
- Add escape symlinks to the runtime child, both a file link and a directory
  link that point outside the runtime directory. Assert that their targets
  survive the launcher's removal.
- Add an other-owner case to the Linux profile activation contract. When the
  host has no such directory, the case must say that it was skipped, never
  pass silently.
- Close or bound the launcher's leak windows. Two ways are to block the
  forwarded signals from `mkdtemp` until the handlers are installed, and to
  remove the directory on each early `fail(...)` path. If closing a window
  is not worth it, record why.
- Say what 2, 5 and 7 mean in live documentation. Two places describe this
  today: `docs/development/build-and-test.md` covers the launcher's runtime
  directory, and the header comment in
  `src/adapters/sqlite/include/bpr/adapters/sqlite/profile_activation.hpp`
  covers the runtime directory the product resolves. The Done cards keep
  their text, and this card's closeout points at the correction.

## Acceptance

- AC1 A private `XDG_RUNTIME_DIR` too long for the endpoint is passed over,
  and the endpoint resolves to the next candidate. `sqlite_profile_activation`
  pins it, and it fails on the unchanged product.
- AC2 The runtime row plants escape symlinks. Their targets are unchanged
  after the launcher removes the row's directory. A trial that removes the
  directory by following links fails the row.
- AC3 A directory owned by another user is passed over as `XDG_RUNTIME_DIR`,
  and a row pins it. A trial that drops the owner check fails it.
- AC4 The launcher leaves no `/tmp/bpr-run-*` behind on the paths the review
  names, or the card records why a window stays open.
- AC5 Findings 2, 5 and 7 are stated in live documentation.
- AC6 No regression: every `test_network_isolation_*` row,
  `sqlite_profile_activation`, the profile activation and owner admission rows,
  `application_attended_launch_contract`, the three Linux Secret Service rows,
  `msvc_secure_crt_contract`, and `verify_documentation.py`. If a row's
  registration changes, also run `test_network_isolation_manifest`.

Linux only. The product change is under `__linux__`, and the launcher is
Linux-only.

## Fixed at `d03e2d61` — bpr-main-linux, 2026-09-24

Claimed at `93601efc`. The product, launcher, fault shim, tests and
documentation are one commit, `d03e2d61`. It was verified before its rebase
at `d0aa52ef`, which has the same content for every path it touches. The
rebase brought in BPR-0803 and BPR-1107 work in the Qt adapter and the app.
So the tree was rebuilt at `d03e2d61`, and the eight rows that compose the
app or the launcher were run again there. All eight passed.

### What changed

- **The product (finding 1).** `runtime_directory()` in
  `src/adapters/sqlite/src/profile_activation.cpp` takes the endpoint's name.
  A private `$XDG_RUNTIME_DIR` whose endpoint would not fit `sun_path` is
  passed over, like one that is not private. The next candidate then holds
  the endpoint, as it did before [[BPR-0906]]. A new helper,
  `endpoint_fits`, makes this check, and `resolve_endpoint` uses it for its
  own final check, so the two cannot disagree. The name and its separator
  add 35 bytes, so on Linux a directory of 72 bytes is taken and one of 73
  is passed over. macOS and Windows resolve exactly as before, because the
  XDG branch is Linux-only.
- **Escape links (finding 3).** The runtime row's child now plants
  `escape-file` and `escape-directory` in its runtime directory. They are
  links to a file and a directory in the stand-in user runtime directory.
  After the launcher removes the child's directory, the row checks that
  both targets are exactly as planted. The launcher still uses
  `std::filesystem::remove_all`, which removes a link and never what it
  names. The launcher's comment now says so.
- **Other owner (finding 4).** `sqlite_profile_activation` has a new case.
  It names the first of `/root` and `/var/lib/private` that another user
  owns with mode `0700`, the mode the product asks for, so only the owner
  check can refuse it. The case only `stat`s that directory. It requires
  the directory to be passed over, and the endpoint to resolve where it does
  with no `XDG_RUNTIME_DIR` at all. A host with neither directory prints
  `runtime-directory:xdg-other-owner=skipped:...` and does not pass
  silently.
- **Leak windows (finding 6).** The launcher now blocks SIGINT, SIGTERM and
  SIGHUP before `mkdtemp`, and installs their forwarding handlers while
  there is still nothing to remove. A signal that arrives before the child
  exists is held until the child does, and is then forwarded to it. The
  child restores the signal actions and mask the launcher inherited before
  its `execvp`, so an ignored SIGHUP, as under `nohup`, stays ignored. Every
  failure after the directory exists now removes it first. That covers
  `runtime-environment`, `fork`, `wait` and a failed unblock. A failed
  unblock also kills and reaps the child first.
- **The fault shim.** `tests/network_isolation/linux_launcher_fault_shim.cpp`
  is a test-only `MODULE` library, registered only in the Linux branch of
  `tests/CMakeLists.txt` and never installed. The runtime row preloads it
  into nested launcher runs. It stands in front of `mkdtemp`, `sigaction`
  and `waitpid` to raise a signal right after the directory is made, to
  refuse the SIGHUP handler, or to reap the child and report that the wait
  failed. It records the directory it saw made, then unsets its own
  variables and `LD_PRELOAD`, so the launcher's child runs without it. The
  launcher has no hook for it. The row passes the shim as a new argument,
  so its registration changed, and `test_network_isolation_manifest` passes
  with it. `msvc_secure_crt_contract` allowlists the shim, because Windows
  never builds it.
- **The course mirror.** `tests/end_to_end/profile_activation_contract.py`
  mirrors the new length check in its copy of the candidate order.

### What stays open, and why (AC4)

A launcher still leaves its directory in three cases:

- It is killed outright, as by a CTest timeout. No code runs in a process
  killed by SIGKILL.
- It gets a signal it does not forward, such as SIGQUIT. The launcher
  forwards the same three signals as the macOS runner, and CTest sends
  nothing else.
- It crashes.

Closing these would need a later process to sweep stale `/tmp/bpr-run-*`
directories. A sweep can remove a live row's directory, which is the hazard
BPR-0906 turned down for the user's runtime directory. What is left behind
holds only what its own test left, sits in `/tmp` and not in the user's
runtime directory, and is removed by name or when the host clears `/tmp`.
`docs/development/build-and-test.md` records the same.

### Documentation (AC5)

The Done cards keep their text. This section points at the corrections.

- **Finding 2.** The header comment on `profile_activation_endpoint` in
  `src/adapters/sqlite/include/bpr/adapters/sqlite/profile_activation.hpp`
  now says the following. Two launches whose `$XDG_RUNTIME_DIR`s name
  different private directories resolve different endpoints. The second
  therefore reports the lease's `owner_busy`. That is intended: ownership
  stays exclusive, and only the handoff is lost.
  `docs/development/build-and-test.md` says the same for tests.
- **Finding 5.** `docs/development/build-and-test.md` now says what the bus
  pin does to the Secret Service rows. On a host with no bus address but a
  live `$XDG_RUNTIME_DIR/bus`, the three Linux Secret Service rows reach the
  user's own session bus and Secret Service, and they write and delete
  their integration item there. That is also what happens wherever an
  address is set. The doc says how to keep them off it.
- **Finding 7.** The header comment and the doc comment on
  `runtime_directory()` now say which checks follow links. The variable and
  the top-level candidates are checked through a symbolic link. Only the
  `bpr-<uid>` child is checked without following one. This corrects
  BPR-0906's "without following links".

### Verification on Linux

WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2, `build/dev` (Debug). The full tree
was rebuilt with no warnings. Every run went through `ctest`, offscreen.
`DISPLAY`, `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` were unset, and
`HOME`, `XDG_CONFIG_HOME`/`DATA`/`CACHE`/`STATE`/`RUNTIME` were fresh. A
wrapper recorded the `bpr-` names each run added to `/run/user/1000`, by a
read-only listing, and any `/tmp/bpr-run-*` it left. Every run added none
and left none. The logs are station records under `build/logs/bpr-1113/`
and are not committed.

| Claim | Check | Result |
| --- | --- | --- |
| AC1 fails on the unchanged product | `sqlite_profile_activation`, the new cases built against the product at `93601efc` (`red-ac1-unchanged-product.log`) | Fails at `a private XDG_RUNTIME_DIR too long for the endpoint is passed over, not refused`. The 72-byte case passes and binds |
| AC1 holds | The same row at the candidate | `runtime-directory:xdg-72-bytes=followed:xdg-73-bytes=passed-over`. The 72-byte endpoint is 107 bytes and binds. The 73-byte one resolves exactly where it does with no `XDG_RUNTIME_DIR` |
| AC2 holds | `test_network_isolation_runtime_directory` | Pass: both escape targets are unchanged after both nested runs, and the earlier assertions still hold |
| AC3 holds | `sqlite_profile_activation` | `runtime-directory:xdg-other-owner=passed-over:/root`. `/root` is `root`-owned, mode `0700`, and was only `stat`ed |
| AC4 fails on the unchanged launcher | The runtime row with the launcher at `93601efc` (`red-ac4-head-launcher.log`) | All five windows fail: `signal:2`, `signal:15` and `signal:1` leave the directory and kill the launcher (-2, -15, -1). `handlers` and `wait` leave it with exit 125. The row removed each leftover by the name the shim recorded |
| AC4 holds | The runtime row at the candidate | `WINDOW_CLOSED` for all five. Each signal reaches the child, which dies by it (exit 254, 241, 255). `handlers` exits 125 before any directory exists. `wait` exits 125 with its directory removed. An ignored SIGHUP reaches the child still ignored |
| AC5 holds | `verify_documentation.py` | `DOCUMENTATION_OK` |
| AC6, no regression | Final pass, 22 rows (`integration-final.log`): all 8 `test_network_isolation_*` rows, including `manifest` after the registration change; `sqlite_profile_activation`; `application_profile_startup_admission`, `_activation_public_path` and `_owner_admission_public_path`; `application_attended_launch_contract`; `msvc_secure_crt_contract`; both architecture boundary rows; `coding_format_check`; both documentation rows; `linux_secret_service_contract`; both `contained_temporary_*` rows | 22/22 |
| AC6, Secret Service | `linux_secret_service_native_setup`, `_native` and `_native_cleanup`, run inside a private `dbus-daemon` on a path socket, with a test-only `gnome-keyring-daemon` unlocked from standard input under a fresh `HOME` (`secret-service-private.log`) | 3/3. Both daemons were stopped by exact PID afterwards. The user's own bus and keyring were never addressed |
| Rebased tree | `d03e2d61`, rebuilt: the runtime, application and manifest `test_network_isolation_*` rows, `sqlite_profile_activation`, the three profile admission and activation rows and the attended launch row (`post-rebase-d03e2d61.log`) | 8/8 |
| Stable | The two changed rows, 10 times each (`stability.log`) | 20/20 |
| Gates | `/usr/bin/clang-format` 22 on the five touched C++ files; scoped `tidy.py --warnings-as-errors` on the four translation units; `git diff --check` | Pass. The shim's three interposers carry `NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)`, because the C library declares them with its own reserved parameter names |

**Negative controls.** Each trial broke one part of the candidate, rebuilt it
and failed at its own assertion. The candidate was then restored, and its
diff was compared byte for byte with the saved one.

- **No owner check.** `owner_private_directory` without `st_uid`:
  `sqlite_profile_activation` fails at `a private directory another user
  owns is passed over as XDG_RUNTIME_DIR`.
- **Off by one.** `endpoint_fits` with `<=`: the row fails at `a private
  XDG_RUNTIME_DIR too long for the endpoint yields to the next candidate`.
- **A removal that follows links.** The launcher's removal recursed through
  `is_directory` and removed each entry's `canonical` path. The runtime row
  fails with `ESCAPE_REACHED:.../escape-file`. Without the `canonical`
  removal, so that the removal only recurses into linked directories, it
  fails with `ESCAPE_REACHED:escape-directory`.
- **The child reset to default actions.** A child that set `SIG_DFL`
  instead of the inherited actions fails the row with
  `SIGHUP_NOT_INHERITED`. This was the first candidate. Self-review caught
  it, and it is the reason the child restores what the launcher inherited.

The trials in the product test exit through `std::exit`, which skips the
scratch fixtures' destructors. Their `/tmp/bpr-act-*` and `/tmp/bpr-xdg-*`
leftovers were removed by exact name after each run. So were the 2 that the
red AC1 run left.

### Evidence owed elsewhere

None for acceptance. Everything that changed behavior is Linux-only: the XDG
branch is under `__linux__`, the launcher and shim do not build anywhere
else, and the course mirror checks `sys.platform`. The one shared change is
the signature of `runtime_directory()` and its `[[maybe_unused]]` parameter
in `profile_activation.cpp`. macOS compiles that, and this station did not.
The next macOS build compiles it, and `sqlite_profile_activation` there
covers the unchanged macOS resolution.

### Acceptance status

- **AC1** pass at `d03e2d61`: `sqlite_profile_activation`. It failed on the
  unchanged product.
- **AC2** pass: `test_network_isolation_runtime_directory`, with two
  link-following trials that fail it.
- **AC3** pass: `sqlite_profile_activation` against `/root`, with the
  no-owner trial failing it.
- **AC4** pass: the review's windows are closed and the row drives each of
  them. The three that stay open are recorded above.
- **AC5** pass: the header comment, the `runtime_directory()` doc comment
  and `build-and-test.md`.
- **AC6** pass: 22/22 and 3/3 at `d0aa52ef`, and 8/8 on the rebased
  `d03e2d61`.
