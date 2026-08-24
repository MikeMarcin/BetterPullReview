---
id: BPR-0235
title: Enforce default-deny network isolation for live mutation tests
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0221, BPR-0347, BPR-0350]
---

# Enforce default-deny network isolation for live mutation tests

The current isolation guard inventories selected port implementations,
symbols, and factory calls. It does not cover generic network calls, Python
tests, subprocesses, or production application binaries launched by tests.
That is sufficient to describe today's tree, which has no live mutation
provider, but not to discharge the standing claim after one lands.

## Required final state

- Automated tests run with network denied by default at the process boundary.
- Exactly one explicitly named live-test lane may opt in, and only after
  proving the designated repository and pull-request identity before launch.
- CTest executables, Python tests, helper subprocesses, and application-binary
  launches are all covered; a generic HTTPS or `gh` path cannot bypass the
  port-symbol scan.
- Positive counterexamples prove that direct HTTPS, a script subprocess, and a
  production-application launch fail outside the designated lane.
- BPR-0142 cannot introduce or use the live provider until this gate passes.

The existing structural scan remains useful defense in depth. It is no longer
described as complete isolation by itself.

Implementation proceeds first on the target-native macOS boundary and the
cross-platform registration contract. This card must remain open until the
same default-deny property is proved on Linux and Windows and the live lane is
proved target-native against its immutable designated pull-request identity.

## Implemented and directly verified on macOS

- All configured tests are registered through one wrapper and carry the
  `bpr_network_isolated` label. A source contract permits raw `add_test` only
  inside that wrapper; the configured-manifest contract verifies the exact
  prefix on C++, Python, CMake, helper, and application commands. Isolated
  Python startup plus required Python/dynamic-loader environment scrubs prevent
  a test property from executing code before the sandbox; the manifest rejects
  any attempted override or missing scrub.
- The macOS runner derives a private profile for each test. It grants only
  runner-selected free IPv4/IPv6 ports and one fresh mode-0700 Unix-socket
  directory; undeclared tests receive no TCP port. Direct HTTPS-port access and
  the same access from a Python child receive `EPERM` while exact fixture
  channels work. A negative control keeps unrelated loopback and Unix listeners
  open outside the profile, proves both connections are denied, and proves
  neither listener receives a connection.
- The shipped application course invokes the real native HTTPS adapter and
  requires a typed network failure, proving a production binary launched by
  CTest inherits the boundary rather than merely selecting a no-network fixture.
- The live-mutation binary scan no longer skips on Windows when `nm` is absent;
  it selects `dumpbin`, `llvm-readobj`, or `objdump`, and no inspector is a
  failure.

Focused target-native evidence:

```sh
ctest --preset dev --output-on-failure \
  -R '^(test_network_isolation_(structure_contract|manifest|external|loopback|unrelated|child|application)|live_mutation_isolation|live_mutation_isolation_contract)$'
```

All nine focused boundary, structural, and application-counterexample tests
pass on macOS arm64. Representative C++, Python, CMake-script, native-loopback,
and public application tests also pass under the new prefix.

## Portable designated-live lane implemented

- The default configured manifest has zero `bpr_live_mutation` exceptions. An
  explicit default-off option requiring an absolute approval path registers
  exactly `live_review_mutation_designated_target`, with the sole
  `bpr_live_mutation` label and the fixed native-runner/approval command. Source
  and configured verifiers reject a second
  exception or an arbitrary name, prefix, label, target, or command.
- The test-only native parent has exactly one network authority: an
  unauthenticated bounded GET to the hard-coded public GitHub REST endpoint for
  `MikeMarcin/BPR-Test#1`. It disables proxies and redirects, sends no
  `Authorization` header, accepts no environment-selected target or URL, and
  matches the complete designated tuple including base SHA and head branch.
- A bounded versioned approval artifact and its immediate parent directory must
  be owned by the invoking POSIX user, non-symlink, and private. It carries the
  exact tuple, combined architecture-locked canary surface SHA-256, and a
  64-lowercase-hex session nonce. The
  parent retains both descriptors, descriptor-relatively moves the checked
  entry into a private claim, compares and unlinks that exact inode, proves the
  retained file has zero links, and holds the authority through child
  termination. Concurrent, repeated, substituted, missing, or failed
  consumption produces no launch, including deterministic substitution between
  identity check and namespace removal.
- The child receives only the versioned nonce capability on standard input,
  with no capability in arguments or environment. The caller cannot select a
  child or executable path. The exact canary and child source/header surface is
  independently hashed, combined into the approval identity, and compiled into
  the runner. After parent-side environment clearing, a private pipe, exact
  single-thread proof, two equal complete descriptor inventories, and a single
  fork create the child. Its branch iterates only the preallocated inventory,
  closes inherited descriptors, and uses fixed byte checks plus
  async-signal-safe descriptor calls and exit. A hermetic regression retains an
  inherited descriptor at or above 256, lowers `RLIMIT_NOFILE` to 64, and proves
  the child still closes it before restoring the parent limit.
  The parent owns bounded wait, timeout kill, and reap. Hermetic fake-port,
  fork-protocol, and native-race tests cover
  every individual tuple mismatch, approval failures, remote failures,
  malformed and oversized input, concurrent/repeated use, child-start failure,
  and the exact positive `inspect -> remote -> consume -> child` course.

Direct portable evidence on 2026-08-10 built the gate, native parent, and
canary; passed `designated_live_mutation_gate`, the isolation verifier unit
suite, and both default and explicit-opt-in manifest checks. The opt-in
manifest was inspected only. The live lane itself made no network call and was
not executed.

Focused unattended commands:

```sh
cmake --build build/dev --target \
  test_better_pull_review_designated_live_mutation_gate_tests \
  tool_better_pull_review_designated_live_mutation_runner \
  test_better_pull_review_designated_live_mutation_canary \
  test_better_pull_review_designated_live_mutation_runner_digest_mismatch
ctest --test-dir build/dev --output-on-failure \
  -R '^(designated_live_mutation_(gate|runner_digest_mismatch)|test_network_isolation_(structure_contract|manifest)|live_mutation_isolation(_contract)?|architecture_boundaries|architecture_boundary_contract)$'
python3 tests/scripts/verify_test_network_isolation_tests.py
python3 scripts/verify_documentation.py --root .
```

The separate offline opt-in configure used an intentionally nonexistent
absolute approval path, built only the runner and canary so CTest could resolve
their exact commands, passed `test_network_isolation_manifest`, and inspected
`ctest --show-only=json-v1`. It found one exact live exception. The ordinary
`build/dev` JSON manifest found zero.

## Linux criterion discharged — 2026-08-17

The checked-in user/network-namespace launcher and its counterexamples ran on
target-native Ubuntu 26.04 x86_64. All eight isolation tests pass, including
`test_network_isolation_application`, so a production binary launched by CTest
does inherit the boundary on Linux as it does on macOS.

Both counterexamples failed on the first run and the boundary was not why.
Outside the sandbox `connect_ex("1.1.1.1", 443)` returns 0 with `lo` and
`eth0` present; inside it returns `ENETUNREACH` with exactly `lo`, no route,
and a different net-namespace inode. The probe accepted only `{EPERM,
EACCES}`, which is what `sandbox-exec` produces, so it rejected the correct
Linux answer. BPR-0346's sibling commit made each backend declare its own
denial shape rather than widening the set for everyone, and added a structural
check so an offline host cannot satisfy the Linux path by accident. macOS
semantics are unchanged.

The run also found that the launcher maps the invoking user to uid 0, so every
Linux test runs as namespace-root and root is exempt from the permission
checks the product relies on. That is not a network property and it is not
repaired here; BPR-0342 carries it.

## Still required before terminal status
- Replace Windows' current pre-launch refusal with a validated whole-suite
  boundary and run the same descendant/application matrix target-native.
- Run the default-off designated lane target-native only when the later live
  provider/write card supplies its separately authorized session authority and
  attended procedure. This slice deliberately ran no public GET or live lane;
  its compiled-in harmless canary did run only in the hermetic protocol
  contract. Native Windows approval ownership and child creation are explicit
  fail-closed results rather than completed Windows implementation/evidence.

Per the product owner's 2026-08-10 scheduling decision, target-native Linux and
Windows execution is deferred until the end of the remaining portable work.
This changes order only and does not weaken either terminal criterion.

## Lane — 2026-08-16

Moved to `50-blocked/` by BPR-0319. The remaining portable work is done, and all
three remaining criteria require target-native execution on Linux or Windows.

The third of them carries a second blocker the lane does not record: the
designated live lane also needs the separately authorized session authority
and attended procedure that the later live provider/write card supplies. A
Linux and a Windows machine unblock the first two criteria outright; the third
still waits on the product owner after that.

## Lane — 2026-08-18

Returned to `10-backlog/` by [[BPR-0358]]. A Windows host now exists, so the
lane rule that put this card in `50-blocked/` no longer admits it.

Neither remaining criterion is discharged by that. The Windows boundary
criterion is now a dependency rather than a platform blocker: [[BPR-0347]] built
the AppContainer boundary target-native on this host and [[BPR-0350]] owns the
temporary-root defect still keeping the suite 24 tests short, and this card
records that criterion satisfied only when the same descendant and application
matrix passes there. The designated-live criterion is unchanged and still waits
on the product owner's separately authorized session authority.

## The Windows criterion, rewritten on the host — 2026-08-22

The bullet above that asks to "replace Windows' current pre-launch refusal
with a validated whole-suite boundary" is discharged in part and narrowed in
part, and the narrowing is measured rather than conceded.

[[BPR-0347]] built the boundary: a zero-capability AppContainer entered by the
checked-in launcher, whose denial was measured against a working internet
connection and which every descendant inherits. What [[BPR-0362]] then
measured, on the Windows host, is that a lowbox token is refused three things
no test can do without and no directory grant can supply, because the objects
involved are kernel objects whose DACLs name no application package:
translating a volume device into a drive letter (so `GetFinalPathNameByHandleW`
in DOS form, `Path.resolve(strict=True)`, and Git's `getcwd` fail while the
NT-form name resolves completely), running a `.bat` file through `cmd.exe`,
and `ninja`'s named pipe.

So the Windows property this card can honestly claim is:

> Every automated test runs with network denied by default at the process
> boundary, **except the exact set registered `WINDOWS_UNCONTAINED`, which
> `scripts/verify_test_network_isolation.py` pins by name and measured cause
> and fails in both directions when the set and the manifest disagree.**

That set is nine tests at the time of writing: the four SQLite storage
contracts, `rfc0016_device_capture_security`,
`test_network_isolation_structure_contract`,
`operator_response_validation_contract`, `coding_standard_format_gate_contract`,
and `dependency_modernization_contract`. Each passes uncontained and fails
contained at the named call; none performs network I/O. On macOS and Linux the
keyword changes nothing and the whole suite stays contained.

What is *not* discharged: the descendant/application counterexample matrix
has not been replayed target-native on Windows, and the designated live lane
remains deliberately unimplemented there. Those stay on this card.
