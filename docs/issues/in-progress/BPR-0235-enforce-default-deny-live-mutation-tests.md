---
id: BPR-0235
title: Enforce default-deny network isolation for live mutation tests
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0221]
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
bound to an immutable designated pull-request identity.

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

## Still required before terminal status

- Run the checked-in user/network-namespace launcher and its counterexamples on
  target-native Linux.
- Replace Windows' current pre-launch refusal with a validated whole-suite
  boundary and run the same descendant/application matrix target-native.
- Create and owner-designate one `MikeMarcin/BPR-Test` pull request; the
  designated repository itself is already bound to database ID `1328277631`
  and node ID `R_kgDOTyvofw`, but the read-only 2026-08-10 inventory found no
  pull request. Register exactly one live lane only after its runner verifies
  the repository and PR identities before child creation; every mismatch must
  remain a no-launch result.
