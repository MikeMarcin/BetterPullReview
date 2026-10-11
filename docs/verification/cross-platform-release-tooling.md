# Cross-platform release tooling evidence

Observed 2026-10-10 for the changes on `codex/cross-platform-release`.
This records tooling behavior, not release approval or RFC-BPR-0038 closeout.

## Delivered behavior

The existing native package driver optionally signs Windows producer images
after its build and before CPack inventories them, checks staged byte identity
and native signatures, then signs the MSI before hashing final products. Linux
optionally signs RPM before hashing and verifies detached signatures for final
packages, runtime archives, SPDX, corresponding source and metadata. Full
approved OpenPGP fingerprints are required; verification uses a disposable RPM
database containing only that approved public key.

Signed output includes a `bpr.native-release.v1` manifest. The existing guarded
publisher accepts repeated `--manifest` arguments and optional `--macos` for
the standard macOS set. Offline preflight checks producer/version, all product
hashes, complete runtime archive/SPDX consistency, mandatory first-party images,
exact source references/sidecar and signing-receipt consistency. It refuses
conflicting asset names and preserves private-repository guards and atomic
main/tag push. Linux producer names and installed source URLs identify the
Ubuntu 24.04 or RHEL 10 baseline.

## Verification

| Surface | Observed result |
|---|---|
| macOS Python contracts | Native release, corresponding-source, package runtime and package audit tests pass. Publisher tests use fixture Git/GitHub executables; no remote mutation. Bash 3.2 exercised. |
| JaegAW native Windows, Python 3.14 | Native release and existing source/runtime contracts pass with declared Bash/POSIX skips. Disposable source snapshot; active station checkout untouched. |
| Windows network containment | The sandbox reached the new native release test, then CPython `build.resolve(strict=True)` failed with WinError 5 in its AppContainer scratch root. The uncontained registration is pinned to the measured `dos-volume-name` refusal. |
| Ubuntu 24.04 x86_64 container on JaegAW WSL | Native release, source and runtime contracts pass; actual baseline CLI returns `linux-ubuntu-24.04-x64`. |
| Red Hat UBI 10.0 x86_64 container on JaegAW WSL | Native release, source and runtime contracts pass; actual baseline CLI returns `linux-rhel-10-x64`. |
| Native Windows trust verifier | Valid timestamped Microsoft SDK SignTool executable accepted with publisher `Microsoft Corporation`; the same executable refused for `Michael Marcin`. This does not verify a BPR/Azure signature. |
| Real OpenPGP/RPM commands, Ubuntu container | Generated a disposable fixture key in a new temporary GNUPGHOME, signed and verified detached data and a fixture RPM; altered detached data refused. No owner release key used and no host trust database changed. RPM's missing default gpg2 path was corrected by explicitly selecting the installed GPG executable. |
| Critical review | `rfc-reviewer-critical` reproduced missing-EXE/null-receipt and unsafe-ZIP-directory bypasses. Both fixed and retested after regenerating ordinary hashes. Focused recheck found neither defect remained. |
| Documentation/skill checks | Documentation gate and its contract tests, skill validator, network-isolation verifier tests and whitespace checks pass. |

The Ubuntu image was
`ubuntu:24.04@sha256:534baea6a22c03a63003dbc8dbe78fe34bc0d7e595d9a9dc9834884ff530eb55`.
The Red Hat image was
`registry.access.redhat.com/ubi10/ubi:10.0@sha256:27251e29658cf10b31914ab9f2630d3f5b0d17d2996ee2dd7d6e2c091a84ab1b`.
Containers establish these command environments, not native desktop acceptance.

## Remaining release gates

No new BPR executable or installer was built, signed, installed or published in
this tooling verification. Producer contracts use deliberately synthetic
packages; real cryptographic smoke uses disposable fixture data/RPM. A selected
release still needs its committed version, normal native test/build gate, actual
CPack production, final installer extraction/audit, approved signing identities
and clean-account install/upgrade/removal qualification.

Azure client authentication and the effective Certificate Profile Signer role
remain prerequisites for the owner's Windows publisher. Linux needs the owner's
approved OpenPGP key and secure signing setup. Public UBI repositories inspected
here did not supply `rpm-sign`; RHEL signing requires a provisioned producer.
Source access/retention, legal disposition, vault, crash/privacy and vulnerability
gaps retain their existing release-readiness scope. Offline receipts cannot
independently establish native trust or installer/archive payload equivalence.

Integrated into `main` on 2026-10-10 from `codex/cross-platform-release` at
`591c98873`. The owner explicitly authorized overriding the local station
stop-proof requirement after the existing proxy could not inspect the old
Desktop endpoint's background commands. The transfer records that waiver,
the observed idle chat and free supervisor lease, and the unverified command
state; it does not claim runtime shutdown. Local ownership is generation 2,
and the old endpoint remains fenced. The obsolete confirmation wizard was
paused and its host stopped without recording a shutdown confirmation.
No active Windows/Linux station checkout was modified or stopped.
