---
id: BPR-0262
title: Authenticate supervisor launch and retain capsule identity
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0261]
---

# Authenticate supervisor launch and retain capsule identity

The BPR-0256 critical review proved that any tests-disabled shipped app accepts
the attended approval flag directly. The app cannot distinguish the verified
capsule supervisor from another parent, while the approval subject and runtime
bootstrap bind neither supervisor nor inventory. The same review identified a
check/use gap for nested capsule entries after path-based verification.

The read-only-image and kernel-audited Mach launch topology was approved for
implementation and hermetic verification only through the exact operator
statement recorded in
[designated-live-macos-containment-v001](../../verification/reviews/designated-live-macos-containment-v001/operator-packet.md).
BPR-0262 remains Backlog only because BPR-0261 must land first. The accepted
subject grants no live GitHub, credential, publication, package,
signing/notarization, or RFC-closeout authority.

## Required final state

- The shipped app refuses attended authority unless it can authenticate that
  the exact bound supervisor launched it through the verified capsule path.
- The attended approval subject binds the supervisor and capsule inventory
  identities that the operator approves; a direct GUI/app launch cannot forge
  or bypass that provenance through argv, environment, settings, or a generic
  inherited pipe.
- Supervisor provenance is established before `QApplication`/GUI creation and
  before the in-app public observation or grant bootstrap.
- The supervisor retains and revalidates the capsule identities needed to close
  the verification-to-launch window, including nested executable dependencies
  and resources; substitution at an injected post-verification barrier refuses
  launch.
- The opt-in CTest manifest still names only the supervisor. Default graphs
  remain zero-exception and tests-enabled applications remain fail-closed.

## Non-goals

- Do not grant authority from parent PID alone, a forgeable nonce, argv,
  environment, or persistent Qt/application state.
- Do not weaken signature, inventory, public-target observation, approval, or
  credential separation checks.
- Do not execute the designated live lane.

## Verification

- A production-shaped hermetic launch course proves the exact app plus exact
  approval refuses when invoked directly or by an alternate launcher and
  succeeds only through the bound supervisor protocol.
- A deterministic barrier mutates/replaces a nested capsule entry after static
  verification and requires refusal before GUI resume.
- Architecture, capsule, manifest, bootstrap, production-symbol/source,
  format/tidy, documentation, and diff gates pass.
