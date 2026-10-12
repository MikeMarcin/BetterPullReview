---
schema: "implement-rfc.card.v1"
id: "BPR-1400"
title: "Prepare the next Ubuntu 26.04 release route"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
platforms: ["win", "linux"]
milestone: "Next-release Ubuntu preparation"
metadata: {"milestone_plan":{"id":"release-next-ubuntu2604","path":"docs/rfcs/plans/release-next-ubuntu2604/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"6b06c981419e15215fa9ffcd97de7585c611ea13","evidence":{"kind":"command","detail":"Exact WSL source snapshot: real baseline CLI emits linux-ubuntu-26.04-x64; source name matches. Runtime 19/19 pass, including native production-install CMake configure DEB default, obsolete Ubuntu/wrong architecture and GLIBC2.44/optional RPM ceiling refusal.","path":".mask/sessions/release-next-20261011/ubuntu2604-wsl-checks.json"}},{"criterion":"AC2","outcome":"pass","revision":"6b06c981419e15215fa9ffcd97de7585c611ea13","evidence":{"kind":"command","detail":"Exact WSL snapshot native release12/12 and source assets11/11 pass. Real producer/verifier fixture paths cover wrong target/generator, stale revision, digest/source/runtime substitution, missing signatures and wrong signer. Bash publisher uses only local git/gh stubs.","path":".mask/sessions/release-next-20261011/ubuntu2604-wsl-checks.json"}},{"criterion":"AC3","outcome":"pass","revision":"6b06c981419e15215fa9ffcd97de7585c611ea13","evidence":{"kind":"observation","detail":"Owner r2 baseline/scope/future-readiness amendment is recorded separately in live RFC/card/ledger and operator/skill docs. Historical M8/M9 packs and failures untouched. Mac publisher/Windows and Ubuntu WSL producers hand over complete signed assets; private credentials stay on producer PC. Documentation gate passes including final evidence edits;67 documentation tests pass. Windows runtime17/19/native11/12/source6/11 checked cases pass with8 explicit native/POSIX/Bash skips; all42 pass on WSL. Version unchanged; wider release obligations explicitly pending."}}]
---

# Prepare the next Ubuntu 26.04 release route

## Mandate and boundary

The owner selected Ubuntu 26.04 x86_64 for the next release on 2026-10-11,
replacing Ubuntu 24.04 and removing RHEL from the selected release scope:
"I dont care about redhat and i dont care about 24.04, you can update all the
scripts/configs you need to." Linux is an optional convenience distribution.
Available WSL2 may produce ordinary Linux binaries; WSL/WSLg checks do not
establish clean Ubuntu desktop, installation or Secret Service qualification.
This is mandate revision `release-next-20261011-ubuntu2604-r2`, separate from
observations and historical acceptance. The inherited M8/M9 plan and evidence
remain unchanged; this issue has a separate bounded preparation plan.

The later instruction is: "No actual release yet, just make it work so the next
time I say do the release, probably from the mac host to this box's remote
stations, it works." Keep the version unchanged and prepare reusable producer
and Mac publisher guidance. No Linux key is chosen; the coordinator handles
Windows signing prerequisites and manual setup separately.

Prepare existing DEB and per-user archive mechanics, retaining source assets,
SBOM, final digests and approved signing/trust checks. Existing optional RPM
mechanics may remain, but RHEL is not a required/current release target.
No version selection, full release build, package installation, signing/key
enrollment, tag, push or publication is authorized by this slice. Do not close
RFC-BPR-0038, BPR-0961 or BPR-0962.

## Acceptance

- AC1: Admit Ubuntu 26.04 x86_64 through the public packaging baseline/target path; use distinct `linux-ubuntu-26.04-x64` binary and source names and enforce its glibc 2.43 ceiling. Refuse obsolete Ubuntu 24.04, wrong architectures and over-ceiling ELF requirements.
- AC2: Exercise native manifest and installed source binding contracts for the new target, including wrong target/generator, stale producer, digest/source substitution and signing-receipt rejection without weakening other platforms' security checks.
- AC3: Update current release/operator/skill documentation and record the owner's Linux scope amendment separately from actual evidence. Preserve historical scans and explicit clean-desktop/vault/crash/trust/publication gaps; run focused contracts and documentation gates, reporting Windows skips and any isolated WSL evidence truthfully.

## Verification and execution record

Owner: `/root/ubuntu2604_release_prep`, sole engineering leaf on `main`.
Entry revision: `960b03bc3`; source baseline before allocation:
`b67d720dd72323e126d9b1db0073fde2bfb257a4`. The reserved ID receipt is
`docs/issues/_reservations/BPR-1386--BPR-1386--cfa5ce12359a.json`.
The coordinator owns ignored wizard state and Azure/read-only prerequisites.
Durable continuation: `.mask/sessions/release-next-20261011/ubuntu2604-handoff.json`.

Preparation candidate `6b06c981419e15215fa9ffcd97de7585c611ea13`:

- The pre-change public WSL baseline path refused Ubuntu 26.04. The exact
  candidate's baseline CLI now emits `linux-ubuntu-26.04-x64` and its source
  name includes that target.
- Isolated WSL2 snapshot: package runtime 19/19, native release 12/12 and
  source assets 11/11 pass, no skips. The tiny CMake production-install
  configure proves DEB default and the exact baseline in generated inputs;
  no application build, installation or signing ran. The ABI fixture accepts
  GLIBC_2.43, rejects GLIBC_2.44, and preserves optional RPM's 2.39 rejection.
  Public producer/verifier/source contracts also refuse obsolete/mismatched
  targets, wrong architecture, stale producer, tampering and incomplete or
  wrong signing receipts. Bash publication tests use only local git/gh stubs.
- Windows: runtime 17/19 checked (two native/POSIX skips), native 11/12
  checked (one Bash skip), source 6/11 checked (five Bash skips). All checked
  cases pass. Documentation gate and all 67 documentation tests pass.
- Native observed environment: WSL2 Linux 6.18.33.2, x86_64, glibc 2.43,
  CMake 4.4.2, GCC 15.2. Initial bare `wsl --exec` could not find CMake;
  the configured `bash -lc` PATH resolves it and the unchanged candidate
  passes. Raw failure and successful records remain in the ignored session.
  The source snapshot and all test-owned temporary roots are removed.

Self-review: one shared Python target map binds source names and native
manifest admission; ELF checks use the selected baseline's ceiling. Ubuntu's
CPack default selects only DEB, preserving optional RPM signing/security
mechanics. No global security tests or C++/Qt product paths changed. Current
operator/skill guidance covers the Mac publisher and native signing-PC
producers; version remains 0.1.8. Prior protected M8/M9 source and reviews stay
unchanged. The follow-on guide clarification only records configured WSL PATH.
No human/native/release acceptance is claimed beyond this preparation scope.

## Mac/Windows reconciliation

Renumbered from Windows BPR-1386 to BPR-1400 during authorized integration on 2026-10-12 because the Mac had independently allocated BPR-1386. Historical acceptance SHAs and original reservation provenance are retained. The new integration reservation is `docs/issues/_reservations/BPR-1400--BPR-1402--f418763b96dd.json`.
