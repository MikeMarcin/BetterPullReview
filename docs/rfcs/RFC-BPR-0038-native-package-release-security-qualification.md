---
id: "RFC-BPR-0038"
title: "Native Package and Release Security Qualification"
status: "Proposed"
area: "BPR"
milestone: "Release qualification"
depends_on: ["RFC-BPR-0014"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# RFC-BPR-0038: Native Package and Release Security Qualification

## Owner amendment 2026-10-28 — release candidate build cohorts

[[BPR-1799]] adds a product-specific sequential build number to publish-release
preparation. One durable source/configuration cohort uses the same number on
all platforms; unchanged retries reuse it and changed inputs require another
identity. Ordinary local builds allocate nothing. The
[design](../development/release-build-identity.md) and
[milestone plan](plans/release-candidate-build-numbering/milestone-plan.json)
bind native metadata, About/diagnostics, source/legal manifests and asset names.
Existing qualification/security obligations remain. Published `v0.4.0` source
and assets are immutable; selecting a corrected semantic version remains an
owner decision before publication.

## Summary

Complete the native, trust, security, privacy and distribution qualification
left open by RFC-BPR-0014. Owner amendment r4 explicitly separates the implemented
release packaging/maintenance foundation from permission to distribute a release.
This successor is outside the original fourteen-RFC pack. It does not reopen
RFC-0014 or imply that any failed or missing observation passed.

## Provenance and scope

The owner directed RFC-0014 to land now, with remaining work filed separately
and a later RFC if needed. The
[r4 mandate](plans/rfc-bpr-0014-release-readiness/mandate-owner-closeout-r4.md)
transfers all nine former release criteria below **verbatim**, including the r3
bounded owner-authenticated AF_UNIX exception. The original immutable source,
RFC-0014 base `d0a8a6dda005af12b3818234f4878a9f48a44146`, M1–M11
handoffs, failed scans and exact human decisions remain historical evidence.
This RFC's baseline is the clean split-entry revision
`4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`.

BPR-0958–0962 are the standalone follow-ups assigned to this successor. Their
paths, native criteria, blocked states and prior evidence remain intact.
BPR-0973 owns final qualification closeout. The shared
[adopted plan](plans/rfc-bpr-0014-release-readiness/milestone-plan.json)
keeps M8/M9 definitions and exits unchanged and adds a distinct successor
closeout; accepting RFC-0014 M12 does not accept those milestones or this RFC.

## Supported qualification matrix

- Windows 11 x64 on JaegAW, native supported MSVC, clean non-elevated
  desktop/account.
- macOS 13+ universal x86_64/arm64, executed on native Intel and Apple Silicon.
- Selected next-release Linux: Ubuntu 26.04 LTS x86_64 under WSL2/WSLg on
  JaegAW, testing the DEB and per-user archive, with a glibc 2.43 ceiling and
  actual C++ runtime prerequisites recorded.

These are the tested environments. Other environments, such as Windows 10,
other Linux distributions or a native Linux desktop, are unproven but not
disallowed: packages do not refuse them, and no document claims them as tested
or supported.

## Owner amendment 2026-10-16 — tested Windows and Linux environments

The owner selected JaegAW Windows 11 and its Ubuntu 26.04 WSL2/WSLg environment
as the Windows and Linux qualification environments. Windows 10 and a separate
native Ubuntu desktop left the required matrix
([record](../issues/_checkpoints/RFC-BPR-0038/owner-matrix-amendment-20261016.json)).
The amendment stayed on an unmerged branch until
[BPR-1743](../issues/60-done/BPR-1743-confirm-or-withdraw-the-october-16-qualification-matrix-amendment.md)
asked whether it stands. The owner confirmed it on 2026-10-23 UTC: "Windows 11
and ubuntu wsl2 are tested. If it works on other things is unproven, but not
disallowed" ([confirmation](../issues/_checkpoints/RFC-BPR-0038/owner-matrix-confirmation-20261023.json)).

Leaving the matrix is not a refusal. Untested environments stay installable,
and no install check, platform gate or package floor changes. Support claims
name only the tested environments and say plainly that others are untested.

The Windows 10/11 and clean Ubuntu desktop wording in earlier records and
shipped release notes remains history. It supplies no evidence. Windows 11
still needs a real non-elevated ordinary-user desktop, Credential Manager,
installation, authentication, rendered/accessibility and removal observations;
elevated SSH and administrative MSI extraction do not satisfy them. Ubuntu
still needs an actual WSL2/WSLg ordinary-user install, launch, authentication,
rendered course, Secret Service vault behavior, purge and removal against the
exact DEB and per-user archive; a build or WSL shell result alone does not
satisfy them. The amendment accepts no native, vault, crash-privacy,
vulnerability, trust, security-owner or distribution criterion. The RFC-0014
plan records it as [dated amendment r5](plans/rfc-bpr-0014-release-readiness/amendments/r5/amendment.json)
and leaves its M8/M9 text unchanged.

## Owner amendment 2026-10-11 — next-release Linux scope

The owner replaced Ubuntu 24.04 with Ubuntu 26.04 and removed RHEL from the
selected release set: "I dont care about redhat and i dont care about 24.04,
you can update all the scripts/configs you need to." The later instruction asks
for reusable readiness for a future release, probably from the Mac host to the
signing PC's remote stations, with no actual release now. Version remains
unchanged. [BPR-1400](../issues/60-done/BPR-1400-prepare-ubuntu-2604-release-route.md)
owns this preparation under `release-next-20261011-ubuntu2604-r2`, separately
from BPR-0961 qualification. Optional RPM mechanics remain outside the selected
set. Historical M8/M9 plans, baseline choices, scans and review packets retain
their original meaning; this amendment supplies no native or security evidence.
Required trust, signing, source/SBOM/digests, clean Ubuntu desktop/Secret Service,
crash/privacy and full selected-target acceptance remain pending.

## Owner amendment 2026-10-11 — reusable signing credential

The owner then requested: "I dont want to have to enter this GPG passphrase
every time i ask to publish a release, we need to store that in a keychain or
something." [BPR-1402](../issues/60-done/BPR-1402-reuse-release-signing-vault-and-native-verifier.md)
implements bounded producer preparation under
`release-next-20261011-vault-verifier-r3`: one exact Windows Credential Manager
credential unlocks the approved encrypted release key only in its validated
WSL GPG home. The maintained Linux package path unlocks automatically and
disables pinentry; missing/mismatched credentials stop it. Human enrollment is
a separate coordinator-owned wizard action. The same slice repairs native
Authenticode module loading when a PowerShell 7 parent pollutes `PSModulePath`.

The dedicated Azure profile signer and one disposable signed Windows probe,
and the approved Linux key's local sign/public-only verification/tamper smoke,
have separate producer evidence in the ledger. They qualify no final package,
remote station credential access, clean product vault/desktop, licensing,
security or publication. No version change, additional Azure signing, access
change or release follows from this amendment.

The JaegAW Windows 11 and WSL/Linux stations supplied actual native build,
installation, PE-inspection and inspector-probe evidence. Preserve it, but do not
call elevated SSH a clean ordinary-user desktop or Ubuntu 26.04 WSL an
Ubuntu 24.04/RHEL 10 qualification. Universal slices or Rosetta are not Intel
native execution. Required environments must be supplied, not silently substituted.

## Engineering and release policy

Reuse the project `publish-release` skill, `tools/release.sh`,
`tools/publish-release.sh` and existing native CPack drivers. Do not create a
parallel release path. Qt is owner-selected 6.11.2 in the current candidates;
changed dependency inventories require new exact-candidate evidence.

Corresponding source is a **separate version-bound release asset**, not embedded
in every installed app. Installed notices, source offers, exact URL/digest,
build/relink/replacement instructions, equivalent no-extra-charge recipient
access and retention remain required. No review of one platform transfers to
another or to changed bytes. Publication, signing/notarization authority and
security-owner applicability decisions must actually be supplied.

Apply the [release policy](../security/release-policy.md),
[package guide](../development/release-packages.md) and
[corporate security packet](../security/corporate-security-overview.md).
Keep one shared alignment controller/`DiffLinkMap`, core/host/Qt boundaries,
tests-disabled composition and absence of accidental test seams. Use clean
disposable roots, fixtures or approved safe targets. No GitHub Actions evidence,
credential disclosure, real-PR mutation or automatic diagnostic upload.

## Acceptance criteria

- Clean Windows, macOS Intel/Apple Silicon, and Linux test environments install, launch, authenticate, open a fixture PR, and uninstall successfully.
- Package signatures/notarization verify independently.
- Runtime dependency scan shows no WebEngine, browser engine, Node, local network server, arbitrary plugin loader, or undeclared network endpoint. The existing bounded owner-authenticated profile-activation AF_UNIX IPC is the sole declared listener exception; verify its owner/peer/frame/lifetime boundary and absence of network-service capability.
- SBOM, license notices, source-offer obligations, and checksums match the delivered artifacts.
- Hardened build settings, static analysis, sanitizers, fuzzers, secret scans, and vulnerability scans pass release policy.
- Credential vaults, file permissions, cache locations, retention, purge, and uninstall behavior match documentation.
- Diagnostic and crash artifacts pass canary redaction and contain no source/comment bodies by default.
- Corporate security overview clearly states network destinations, data stored, subprocesses, credentials, AI boundary, logging, and removal.
- Delivered target/dependency graphs and runtime inspection prove the functional core remains Qt/provider-free and all effects are reachable only through the packaged host/composition path.

## Accountable follow-ups

| Work | Remaining obligation |
|---|---|
| [BPR-0958](../issues/50-blocked/BPR-0958-qualify-macos-arm64-package.md) | Clean Apple Silicon account/authentication/Keychain, complete exact-package rendered course, full purge and documented residuals. Three earlier scoped human accepts remain candidate-specific. |
| [BPR-0959](../issues/50-blocked/BPR-0959-qualify-macos-intel-package.md) | Complete native Intel installation, authentication, rendering, vault, maintenance, purge and removal. |
| [BPR-0960](../issues/50-blocked/BPR-0960-qualify-windows-x64-package.md) | JaegAW Windows 11 ordinary-user desktop, auth, high-DPI/accessibility, Credential Manager and purge/removal; completed PE mechanics do not satisfy these. Windows 10 is untested and not refused. |
| [BPR-0961](../issues/50-blocked/BPR-0961-qualify-linux-x64-packages.md) | Ubuntu 26.04 under WSL2/WSLg on JaegAW: exact signed DEB/per-user archive, fixture/auth/Secret Service/purge/removal and rendered WSLg course. A WSL shell build alone does not supply it; other distributions and native desktops are untested and not refused. |
| [BPR-0962](../issues/50-blocked/BPR-0962-verify-release-trust-and-security-boundaries.md) | Independent final trust, Windows/Linux licensing, source access/retention, unresolved vulnerabilities, external hardening/PE CFG, active-operation network/vault and foreign-user IPC, full sanitizer/fuzz coverage, resident-material crash/privacy and broader test failures. |
| [BPR-0973](../issues/10-backlog/BPR-0973-close-rfc-0038.md) | Reconcile every original criterion against exact final artifacts, all follow-up decisions and explicit closeout dependencies. |

## Current evidence and stop conditions

The [successor ledger](../verification/RFC-BPR-0038.md) anchors the retained
exact candidates, latest raw failures, unanswered review and prerequisite map.
No BPR-0958–0962 criterion is newly accepted by this transfer. Raw Grype gates
remain exit 2 (32 macOS / 30 Windows); no security-owner disposition is invented.
The complete development run's 374/415 and serial Qt 75/84 results remain failed
broad suites, even where narrower regressions pass. Applicability proposals,
mechanical package audits, documentation and RFC-0014 closure cannot authorize
distribution. RFC-BPR-0035 updater live-private-release and Intel follow-ups
remain separately owned and unmodified.

## Owner amendment 2026-10-20 — release qualification runtime

The owner requested systemic implementation toward thirty minutes, including
grounded test overlap/consolidation and conditional qualification design, based
on the retained Astra Max report. The bounded [portfolio design](../development/release-qualification-portfolio.md)
and [three-milestone plan](plans/release-qualification-portfolio/milestone-plan.json)
own BPR-1673–1675. They preserve this RFC’s existing criteria and open native,
security, material and release authorization limits; their completion does not
close this RFC. The historical release-readiness plan and M8/M9 remain unchanged.
