---
id: "RFC-BPR-0088"
title: "Upgrade the pinned Qt runtime to 6.12.0"
status: "Auto-Verified"
area: "BPR"
depends_on: []
metadata: {"rfc_base_sha":"1a7d74f003b656469f5c6822c655a314fb58f663","mandate_revision":"qt-6-12-r2","milestone_plan":{"id":"qt-6-12-v1","path":"docs/rfcs/plans/qt-6-12-v1/milestone-plan.json"}}
---

# Upgrade the pinned Qt runtime to 6.12.0

Owner mandate `qt-6-12-r2` authorizes authoring and implementation on the task
branch. Immutable RFC_BASE_SHA: `1a7d74f003b656469f5c6822c655a314fb58f663`.
[Plan](plans/qt-6-12-v1/milestone-plan.json); [verification](../verification/RFC-BPR-0088.md).

## Problem and grounded current behavior

The owner requested authoring and implementation of an upgrade to Qt 6.12.
`cmake/BprQtProvider.cmake` currently acquires Qt 6.11.2; the distribution
source inventory pins QtBase, QtSvg and QtImageFormats 6.11.2 separately.
Linux packaging also binds the exact official Qt ICU73.2 companions before
building the same-version hardened replacement trio. An isolated version
change would break that package/source contract.

Qt announced [6.12.0 on 2026-09-30](https://www.qt.io/blog/qt-6.12-released).
The official source archives and SDK metadata are available and have been
inspected. Source pins must come from downloaded bytes and their published
checksums, rather than inferred URLs or a release announcement alone.

## Proposed behavior and boundaries

Use official Qt 6.12.0 for the existing pinned provider on macOS, Windows
and Linux, preserving explicit caller-installed Qt discovery, offline/cache
behavior and exact distribution version checks. Keep the existing module
set: Core, Network, Gui, Widgets, the Svg library and the WebP image plugin;
no Qt Quick, WebEngine or additional product feature is introduced.

Update the corresponding-source inventory, Linux companion binding and
active build/package documentation consistently. Preserve origin, byte,
module-closure, source-delivery and hardening refusal gates. Linux retains
ICU73.2 because the new official SDK supplies the same three ABI files; its
new archive identity and each raw file hash must be verified. Existing ICU
source and recipient build obligations remain intact.

Exercise the public provider/configure path and an available-host app build
and offscreen runtime path, plus focused acquisition/package/source/security
contracts, the full macOS developer test suite and documentation checks. The
owner added full-suite validation and subsequent three-platform v0.3.1 release
work on 2026-10-19; release preparation, native production and publication are
the coordinator's subsequent milestone, outside this upgrade assignment. Record actual candidate versions and
platform limits. This dependency upgrade does not certify native
Windows/Linux execution, universal package installation, new vulnerability
applicability or release approval. Existing release qualification obligations
and truthful historical Qt 6.11.x evidence remain unchanged. No release,
push, GitHub Actions, live GitHub course or owner desktop interaction is part
of this milestone. No product behavior or visual design change is intended.

## Owner compatibility decision

On 2026-10-19 the owner explicitly chose “Raise minimum to macOS 14.4” for
future Qt6.12 builds. Both official framework architectures record
`LC_BUILD_VERSION minos14.4`, matching [Qt's supported-platform policy](https://doc.qt.io/qt-6.12/supported-platforms.html).
The `macos-release` preset and active build/package policy use14.4. Current
v0.3.0 download requirements remain13+, as published. Source-building Qt to
retain macOS13 support is outside this mandate.

## Acceptance criteria

- **AC01:** Default provider configuration selects exactly Qt 6.12.0 and the existing required modules; focused positive and refusal controls preserve exact-version, offline and caller-root behavior.
- **AC02:** Qt source archives and the official Linux ICU archive match their recorded SHA-256 values, the three ICU files match their raw hashes, and focused package/source/security checks preserve the existing closure and source/hardening gates.
- **AC03:** An available-host application build and the full macOS developer suite, including representative composed offscreen runtime checks, use Qt 6.12.0 successfully; unavailable native/package evidence and actual failures remain explicit.
- **AC04:** Active dependency documentation and tests agree with the selected version; historical evidence remains truthful, and documentation checks pass with exact commands and candidate identities recorded.

## Verification and execution record

Completed on the task branch under mandate `qt-6-12-r2`. The
[verification ledger](../verification/RFC-BPR-0088.md) records actual source/ICU
hashes, positive/refusal provider courses, the full developer build, composed
runtime paths and the complete macOS suite: 810 passed, one configured Debug
skip, no failures across all 811 rows. The optimized memory course, native
Windows/Linux/Intel, trust, vulnerability and release obligations remain
explicit for the subsequent release milestone. The separate Threads scrolling
repair is a release blocker. Main integration and v0.3.1 publication belong to
the coordinator; this status accepts only the bounded dependency upgrade.
