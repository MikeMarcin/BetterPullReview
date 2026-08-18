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
