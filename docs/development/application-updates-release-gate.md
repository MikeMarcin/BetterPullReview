# Application updates: release operator gate

This page is for maintainers preparing a macOS release that carries the
self-updater. The user guide is [Application updates and recovery](application-updates.md).
The [verification ledger](../verification/RFC-BPR-0035.md) records the
qualified Apple Silicon packages, the Intel follow-up and the outstanding live
private-service evidence; implementation does not mean final release acceptance.

## Build and sign

Build from an identified committed source with tests disabled using the documented
[local release workflow](../../packaging/README.md). The universal ZIP must contain
exactly `BetterPullReview.app`, including `Contents/MacOS/bpr_update_helper`, both
CPU slices, the expected bundle identifier/team and the deployed runtime/notices.
Sign all nested code, use hardened runtime and secure timestamps, notarize and
staple the package, and repeat native signature/trust and relocation checks. A
checksum is integrity metadata, not a publisher trust root. Signing rotation
needs a separately reviewed trust migration.

## Exercise before distribution

Before distributing an updater-bearing release, retain source/archive/executable
digests and exercise a participating older signed package through explicit
Download and Install, healthy receipt, retained state, shutdown timeout,
missing-receipt recovery and interrupted-rename recovery. Record native Apple
Silicon and Intel execution separately; universal headers or Rosetta do not prove
Intel hardware acceptance. Use disposable roots and synthetic review state. A
live private-release/vault check requires its own explicitly authorized account
and target, including the observed asset redirect route and revocation. Fixtures
cannot establish those live observations.

## Baselines

Releases predating the signed helper and startup-receipt protocol require a
manual bootstrap update. The historical 0.1.3 package is staging/trust evidence,
not a participating baseline. The disposable 0.0.350–0.0.352 qualification
versions are unpublished and numerically below 0.1.3; do not distribute them as
an upgrade.

## Current status

The current recorded package course is Apple Silicon only. Owner amendment r3
defers Intel updater validation to [[BPR-0950]], outside M4 and RFC-BPR-0035
closeout; it does not report Intel success or amend RFC-BPR-0014's wider matrix.
Authorized live private-service/vault evidence remains on [[BPR-0911]], with
the explicit RFC-BPR-0014 dependency on [[BPR-0912]]. The backup-helper
recovery course has been exercised on Apple Silicon; the recovery steps in the
user guide are operator guidance, not an attended usability or Intel
qualification claim. RFC-BPR-0014 still owns the complete uninstall/purge
qualification.

Building, signing or notarizing a local artifact grants no tag, push, GitHub
release or installed-app replacement authority. Publication is a separate
authorized operation.
