---
id: BPR-0347
title: Contain the Windows test suite in an AppContainer
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0339]
---

# Contain the Windows test suite in an AppContainer

[[BPR-0235]]'s first remaining criterion is to replace Windows' pre-launch
refusal with a real whole-suite boundary. Until now `WIN32` selected
`FailClosedAutomatedTestIsolation.cmake`, so every configured test refused
before it started: [[BPR-0336]] recorded 84 of 84 failing identically with
`Automated test launch refused: the Windows default-deny network boundary is
unavailable`, and zero test binaries executed.

Windows offers an unprivileged process no equivalent of a network namespace or
of `sandbox-exec`. The boundary that does exist is the AppContainer: a process
entered with **zero** capabilities gets neither `internetClient` nor
`privateNetworkClientServer`, and the Windows Filtering Platform then refuses
every external connect. The container token is inherited by every descendant
unconditionally, which is what makes a whole process tree safe rather than just
the one process CTest launched.

## Shape

`tests/network_isolation/windows_network_sandbox.cpp` is the launcher, and it
replaces the refusal script as `BPR_AUTOMATED_TEST_ISOLATION_PREFIX`. The
backend identifier moves from `windows-unavailable` to `windows-appcontainer`,
which `scripts/verify_test_network_isolation.py` and the counterexample probes
now both understand. `cmake/FailClosedAutomatedTestIsolation.cmake` is deleted
with the refusal it existed to print.

Three design points, each answering a way the boundary could have failed open
or failed incoherently:

- The launcher refuses unless `MPSSVC` is running, because the Filtering
  Platform that enforces the restriction is hosted there and a stopped service
  can fail the boundary open.
- The caller's command line is forwarded verbatim with only the launcher's own
  token removed, rather than rebuilt from `argv`, which would re-quote and
  could change what the test receives.
- `CreateProcessW` reports success even when the container cannot read the
  image it was handed, so the filesystem grants are established at configure
  time, before any test is registered. `icacls /grant:r` replaces this SID's
  own entry only, keeping repeated configures idempotent.

## The elevated step is not required, and was measured rather than assumed

This card originally refused the configure until the owner ran
`CheckNetIsolation.exe LoopbackExempt -a -n=BprAutomatedTest` from an elevated
prompt. That refusal has been removed, because the premise was wrong.

What an AppContainer actually restricts is reaching a loopback listener owned
by something else. Two processes inside the *same* container reach each other
over loopback with no exemption at all. Every fixture in this suite is created
by the test that uses it, and CTest launches the whole test through the
launcher, so listener and client are always in this one container together.

Measured directly, with the machine carrying no exemption at any point:

| case | result |
|---|---|
| listener and client in the same container, separate processes | connect returns `0`, payload received |
| listener outside the container, client inside | connect fails, `10035` |

So the old gate demanded an elevated command from the owner in exchange for
authority the suite never exercises, and refused the entire build until it was
granted. `test_network_isolation_loopback` proves the channel positively at
test time, which is where the claim belongs.

## Verified target-native on Windows

The boundary itself is proven, not asserted. All measurements are from this
host, an online machine, with the launcher built from the checked-in source.

- **The denial is real and is the boundary's.** `connect_ex(("1.1.1.1", 443))`
  returns `10013` (`WSAEACCES`) inside the container and `0` outside it. The
  outside value is what makes the inside value evidence: the host has working
  internet, so the refusal is not an offline machine.
- **The process is genuinely contained.** `TokenIsAppContainer` reads `1`
  inside and `0` outside. This is the Windows counterpart of the Linux
  namespace check [[BPR-0235]] added, and it defeats the same ambiguity: an
  errno alone cannot separate a working boundary from an absent one.
- **Descendants inherit it.** A Python child spawned inside the container
  reports `10013` for the same connect.
- `test_network_isolation_external`, `test_network_isolation_child`, and
  `test_network_isolation_loopback` all **pass** under `ctest --preset
  core-only`, as does `test_network_isolation_manifest`.
- `cmake --preset core-only` configures and generates, and
  `cmake --build --preset core-only` completes with zero failed edges.

**The suite went from 0 of 84 executing to 60 of 84 passing.** That is the
first time any BetterPullReview test has run on Windows.

## Still required

This card stays in `30-in-progress/`. 24 tests still fail, in three groups.

- **17 — an owner-private temporary directory cannot be used inside an
  AppContainer.** `tempfile.mkdtemp` calls `os.mkdir(path, 0o700)`, and modern
  CPython translates that mode into a *protected* Windows DACL: `SYSTEM`,
  `Administrators`, `OWNER RIGHTS`, no inherited entries. A lowbox token's
  access check additionally requires the package SID or `ALL APPLICATION
  PACKAGES`, and that DACL names neither, so the contained process cannot use
  the directory it just created. Verified by contrast: plain `os.mkdir` in the
  same parent inherits `(I)(OI)(CI)(M)` for the container SID and works, and
  the directory's owner is the user rather than the package SID, so the
  `OWNER RIGHTS` entry cannot satisfy the restricted pass. Four C++ SQLite
  tests fail the same way through their own `temporary root becomes
  owner-private` check. This needs a decision, not just a patch: on POSIX the
  `0700` mode is a real protection against other local users, while inside an
  AppContainer the container is already the stronger privacy boundary. It
  should carry its own card.
- **1 — the binary symbol inspector reads no symbols.** `live_mutation_
  isolation` reports that the instrument found no `review_mutation_coordinator`
  in a binary that must contain it, and says so rather than passing quietly.
  [[BPR-0235]] made this path select `dumpbin`, `llvm-readobj`, or `objdump` on
  Windows; the selection or its parsing is wrong here. It fails closed, which
  is correct, but it proves nothing until repaired.
- **6 — not yet diagnosed individually:** `architecture_boundaries`,
  `conditional_branch_identifiers`, `platform_configuration_contract`,
  `application_icon_assets_contract`, `github_contract_fixture_security`,
  `sqlite_review_ledger_storage`.

Beyond the failures, `BPR_BUILD_QT=ON` has never been run under this boundary;
only the Qt-free `core-only` graph has. The Windows designated live lane
remains unimplemented and fail-closed, unchanged by this card.

## Note for anyone reproducing the launcher by hand

Invoke it with the program and its arguments as separate argv entries
(`sandbox.exe "C:\...\python.exe" script.py`). Passing the whole command as one
pre-quoted string makes `CreateProcessW` fail with `win32=5`, which looks like
a boundary defect and is not one. A bare `python` also fails, with `win32=2`,
because the launcher resolves the image itself and does not see the
`WindowsApps` execution-alias stub the shell would.

## The three groups were wrong — 2026-08-18

This card grouped its 24 remaining failures into three buckets and named the
first "one defect with one cause". That was a grouping by *symptom*. Removing
the symptom showed six independent causes underneath, and the counts were off in
both directions.

[[BPR-0359]] and [[BPR-0350]] have landed. The suite is at **68 of 84 passing**,
up from 60. Eight tests moved: `core_boundary_contract`,
`documentation_issue_graph_contract`, `github_recorded_fixture_contract`,
`attended_review_verdict_contract`, `project_source_dump_contract`,
`live_mutation_isolation_contract`, `architecture_boundaries`, and
`application_icon_assets_contract`.

The corrected taxonomy of the 16 that remain, each measured on this host:

| cause | tests | owner |
|---|---|---|
| the container cannot read any path ancestor, so `resolve(strict=True)` and the product's retained ancestor chain both fail | `sqlite_epoch_catalog_storage_contract`, `sqlite_epoch_catalog_behavior_contract`, `sqlite_review_mutation_recovery_storage`, `sqlite_review_ledger_storage`, `rfc0016_device_capture_security`, `rfc0016_device_live_runner_security` | [[BPR-0362]] |
| no symbol inspector reads an MSVC PE | `live_mutation_isolation`, `composed_host_verifier_contract` | [[BPR-0361]] |
| the Windows preset contract names Visual Studio 17 2022; the preset declares 18 2026 | `platform_configuration_contract` | [[BPR-0363]] |
| `git init` returns 128 inside the container | `operator_response_validation_contract` | undiagnosed |
| the isolation verifier's own contract rejects the Windows runner: `unsafe-fixture-root`, `profile-template-contains-network-allow` | `test_network_isolation_structure_contract` | undiagnosed |
| not yet diagnosed individually | `architecture_boundary_contract`, `conditional_branch_identifiers`, `dif_assets_contract`, `dependency_modernization_contract`, `github_contract_fixture_security` | undiagnosed |

Two corrections to what this card asserted:

- The "17 tests" in the first group were not one defect. Four of the six
  "undiagnosed" tests were the same temporary-directory cause, one was the DACL
  cause, and one — `architecture_boundaries` — was a byte-pinned digest against a
  CRLF checkout, which is [[BPR-0359]] and has nothing to do with containment.
- The symbol-inspector failure is two defects, not one.
  `verify_live_mutation_isolation.py` selects `dumpbin`, which returns no symbol
  table for a linked image; `verify_composed_hosts.py` has no Windows candidate
  at all. [[BPR-0361]] carries both.

`architecture_boundary_contract` fails at the same
`VerifyArchitectureBoundaries.cmake:1152` message [[BPR-0359]] fixed, but against
fixture files the test itself writes, so it is probably the same class through a
different door — a test writing text without `newline=""`. Probably is not
measured, so it stays undiagnosed here.

`BPR_BUILD_QT=ON` still has never been run under this boundary, unchanged by any
of this work.

## Build note for this host

Every `cmake` and `ctest` invocation must run with the MSVC x64 environment
imported. Without it, configure fails inside
`cmake/WindowsTestIsolationSetup.cmake` with `fatal error C1034: windows.h: no
include path set` while building the AppContainer launcher, which reads like a
launcher defect and is not one.

The launcher also prints `Failed to find real location of <image>` on every
invocation before the test runs. It is not fatal and the test proceeds, but it
appears in the output of all 84 tests and nothing explains it. It is likely the
same ancestor-read denial [[BPR-0362]] measures, seen from inside the launcher.

## The SQLite storage path, opened — 2026-08-18

[[BPR-0368]] and [[BPR-0369]] between them made the Windows epoch catalog work
for the first time. `sqlite_epoch_catalog_behavior_contract` passes uncontained.

Two new items land on this card's undiagnosed list as a result, both reachable
only now:

- `sqlite_epoch_catalog_storage_contract` runs deep into the suite and stops at
  `live catalog retains no-delete-share handles for every ancestor and the
  catalog leaf`. Undiagnosed.
- The same test emits `SKIP: file-symlink rejection requires Windows symlink
  privilege; directory-junction rejection remains covered`. That is a
  deliberate, self-describing skip rather than a failure, but it means one
  [[BPR-0053]] adversary is not covered on an unelevated host and should be
  recorded there rather than discovered again.

Both epoch-catalog tests still time out **under the launcher** while passing or
progressing uncontained. That gap is the container's and belongs to
[[BPR-0362]].

## One undiagnosed row was stale, diagnosed from macOS — 2026-08-19

`test_network_isolation_structure_contract` is recorded here, and again in
[[BPR-0358]]'s "13 that remain", as failing because "the isolation verifier's own
contract rejects the Windows runner: `unsafe-fixture-root`,
`profile-template-contains-network-allow`". That attribution cannot be right at
HEAD, and the reason is a date.

Both strings exist in exactly one place, `scripts/run_network_isolated_test.py:64`
and `:79`, and they are reachable from the test suite only through
`write_profile`, which `tests/scripts/verify_test_network_isolation_tests.py:31`
imports directly and calls at `:614`, `:640`, and `:651`. Run against the
suite's current fixtures on macOS:

| case | fixture root | template | result |
|---|---|---|---|
| the `:651` shape | Windows-shaped `C:\Users\...` | pre-granting | `SystemExit 125` from **`profile-template-contains-network-allow`** — the template check runs first, so `seatbelt_string` is never reached |
| a clean template | Windows-shaped `C:\Users\...` | benign | `SystemExit 125` from **`unsafe-fixture-root`** |
| the `:614` shape as it stands today | `PurePosixPath("/private/tmp/bpr-fixture")` | benign | writes the profile, no failure |

The middle row is the only way to produce `unsafe-fixture-root`, and it needs a
Windows-shaped root reaching `seatbelt_string` with a benign template. That was
the old shape of `test_derived_profile_pins_exact_ports_and_fixture_root`, which
derived its fixture from the host temporary directory. [[BPR-0349]] replaced it
with a pure POSIX path at `3b8964e`, 2026-08-17 21:17, and left the reasoning in
a comment at `:601-608`.

This card's taxonomy was written at `f372f83`, 2026-08-18 10:37, and BPR-0358's
at `c55f2d7`, 11:05 — both **after** that fix. The row carried a symptom string
forward from a measurement taken on an older tree.

The `:651` call still passes a real host path, so it is Windows-shaped there, but
the first row above shows that is harmless: it exits 125 on the template, which
is the assertion that test makes, on every host.

**What this does and does not settle.** It settles the attribution: neither named
message can be produced by the suite as it now stands, so the recorded cause is
gone. It does not settle whether the test passes on Windows — that is unmeasured
from here, and if it still fails it fails for some other reason and needs a fresh
capture. The row should be treated as *unmeasured*, not as *diagnosed*.
