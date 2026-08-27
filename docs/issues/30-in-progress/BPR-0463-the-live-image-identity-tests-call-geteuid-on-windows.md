---
id: BPR-0463
title: The live-image identity tests call os.geteuid on Windows
status: In Progress
rfc: RFC-BPR-0008
depends_on: []
---

# The live-image identity tests call os.geteuid on Windows

`test_network_isolation_structure_contract` fails its first Windows run
since the [[BPR-0448]]/[[BPR-0449]] live-image work landed: three
`LiveImageIdentityTests` cases error with
`AttributeError: module 'os' has no attribute 'geteuid'` and a fourth
fails downstream of the same platform difference. The identity contract
is written in POSIX owner terms (`st_uid` versus the effective uid), the
designated live lane exists only on macOS, and the tests were only ever
run where `os.geteuid` exists.

## Required final state

- `LiveImageIdentityTests` skips where `os.geteuid` is absent, with a
  self-describing reason, following the suite's existing
  `ProtectedStageZeroLoaderTests` platform-skip idiom.
- The verifier's `inspect_live_image` refuses typed — not with an
  `AttributeError` — on a platform that cannot express the POSIX owner
  check, keeping the live-image path fail-closed everywhere.
- The structure-contract suite passes on the Windows host.
