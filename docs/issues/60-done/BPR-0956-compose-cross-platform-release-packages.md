---
schema: "implement-rfc.card.v1"
id: "BPR-0956"
title: "Compose cross-platform release packages"
status: "Done"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0952","BPR-0955"]
milestone: "M6 Composition and public-path integration"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"da12f5568f8ab207d8a75451cda9b3de1a49df63","evidence":{"kind":"observation","detail":"Application-only Windows per-user x64 MSI, macOS13 universal, Ubuntu24.04/RHEL10 DEB/RPM and per-user Linux archive definitions implemented. Core-only build/tests2/2 and complete committed architecture archive pass; native unavailability explicitly stays with M8. See handoff-m6.json."}},{"criterion":"AC2","outcome":"pass","revision":"da12f5568f8ab207d8a75451cda9b3de1a49df63","evidence":{"kind":"artifact","detail":"Existing tools/release.sh --prepare-only produces clean tests-disabled universal local candidate, pinned source/notices/SPDX71packages144relationships43filehashes plus ZIP/SBOM checksums. Final artifact at build/releases/m6-da12f556; appSHA d31b371b1aad6caef369bdf60212b0a156b913ee7834d1f88b7424c8d8221852; ZIP SHA c0fe65a9978b9786f40953349806492afe7c3cce24e4de659901d92586c1306c; SPDX SHA 4fb15f0fd7ec256355fa39b6b75b096812512e4527a1e14a74eb6642360aa7fe. Existing publish script only source/syntax checked; no release mutation. Independent trust/licensing approval remains M9."}},{"criterion":"AC3","outcome":"pass","revision":"da12f5568f8ab207d8a75451cda9b3de1a49df63","evidence":{"kind":"observation","detail":"Native CUA: Use GitHub CLI, read-only local fixture consent, PR17, Start review at review.cpp. AX and screenshot show earlier int review_line_1..6=11..16 and later=21..26; Diff ready with1 semantic change, one selected replacement Diff correspondence map and one Shared comparison position scrollbar. Normal Quit exit0; qualification guard passed. No in-product fixture hook or alternative UI. Local available package AC3/exit2 complete. Final-source two-window producer/store/logger retirement, retained lease/vault, clear/purge, stale inventory and startup recovery direct public test also passes. Windows/Linux full native matrix remains M8 as bound exit allows."}}]
---

# Compose cross-platform release packages

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Complete the application installation/package graph for Windows per-user x64, macOS 13+ universal and the declared oldest-supported Linux x64 DEB/RPM baseline, retaining an independently buildable/testable Qt/provider-free core. Add install/export rules only where the delivered application packages need them; a separate core SDK or general CMake embedding support is not required.
- AC2: Reuse the macOS release path and product identity; produce exact-artifact checksums, SBOM, dependency provenance, runtime notices and source-offer obligations while collecting only declared required runtime libraries/plugins.
- AC3: Exercise each available tests-disabled package through the real composition to a fixture PR and exact selected-file diff with one alignment controller and DiffLinkMap; preserve restricted install paths and the qualified macOS-only updater exception.

## Verification and execution record

Implemented under mandate r3 at source candidate
`da12f5568f8ab207d8a75451cda9b3de1a49df63` (main implementation
`65dd7dd4add669606f67f39da05e6b420effec77`). All three M6 criteria now pass.
After the user unlocked the Mac, the final packaged native UI replay completed
without changing the source or package, under resume commit `02325a49`.
The [M6 ledger](../../verification/RFC-BPR-0014.md#milestone-6-composition-and-public-path)
and [durable handoff](../../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m6.json)
bind exact artifacts, commands, corrections, native limits and the resume action.

- AC1 implementation pass: application-only per-user Windows MSI, macOS 13
  universal and Ubuntu 24.04/RHEL 10 package definitions; Linux per-user archive
  plus managed DEB/RPM routes. Core-only build and policy/host tests pass 2/2.
  Windows/Linux native qualification remains M8, not inferred from definitions.
- AC2 implementation/available-host pass: the unchanged authoritative macOS
  release pipeline's new local prepare route produces a clean tests-disabled
  bundle/ZIP, exact 43-file SPDX inventory, pinned complete corresponding source,
  notices and checksums. Both-slice floor/dependency checks, offline ad-hoc seal,
  relocated launch, all installed hashes and artifact checksums pass. Real release
  trust, native CRT/license review and owner distribution approval remain M9.
- AC3 pass: the final source's fixture comparison and two-window retirement/
  clear/purge/recovery course passes. The exact tests-disabled package at
  `build/releases/m6-da12f556/` now also reaches PR 17 → Start review at
  `review.cpp`: native AX and screenshot show exact earlier 11–16/later 21–26
  values, one replacement, one Diff correspondence map and one Shared comparison
  position scrollbar. Normal Quit returns exit 0; the qualification guard passes
  with unchanged live files. The earlier locked-desktop timeout is preserved in
  the ledger, not counted as a pass. No human review verdict is required at M6.

All independent commands are complete. No Developer ID signing, Keychain change,
notarization, tag, push, upload, publication or remote station mutation occurred.
Ordinary offline ad-hoc sealing is the explicitly authorized local verification
exception. BPR-0957 becomes Ready for a fresh M7 owner; this slice stops at M6.

## M2 allocation for the successor

The [source/dependency allocation](../../architecture/release-readiness.md)
records G1–G4/G7. Reconcile the production MCP helper's emitted name, platform
suffix, build dependency and copy/install path; tests currently inject an
absolute path that masks the missing production composition. Inventory the
agent supervisor and Mac updater helper too. Add only the missing Windows/Linux
installers and per-user routes; reuse the existing macOS skill/scripts.

Account for static SQLite/KSyntaxHighlighting/Quill/Glaze/LibArchive and all
embedded definitions, TLS/plugins, platform runtimes and source offers in the
actual closure; the generated CMake dependency audit alone is not an SBOM.
Pin/inspect the macOS 13 deployment floor on every Mach-O slice. The declared
Linux targets are glibc 2.39, Ubuntu 24.04 LTS DEB and RHEL 10 RPM; record the
RHEL CPU floor, actual C++23 toolchain/GLIBCXX requirements and reconcile any
incompatibility explicitly instead of silently raising the baseline. None of
these target declarations is native acceptance. Publication remains unauthorized.

Under owner amendment r2, the authoritative macOS packaging path is already
`tools/release.sh` as documented by the project `publish-release` skill. Reuse
its universal tests-disabled build, restricted deployment, signing/notarization,
smoke, DMG/ZIP and checksum flow. Complete demonstrated gaps and Windows/Linux
work; do not construct a second equivalent macOS release pipeline. The
[source inventory](../../verification/RFC-BPR-0014.md#owner-amendment-r2-existing-macos-release-workflow)
keeps actual past evidence and unverified final acceptance distinct.

Owner amendment r3 modernizes the installation prescription above while
preserving the full package matrix and core dependency-isolation outcome.
Account for every required shipped third-party component, including static
dependencies, in the minimal runtime closure, SBOM and license/source-offer
analysis. The live RFC records this rationale; no native acceptance is waived.

## M5 presentation handoff

Candidate `13a7d0522137eeb4ab147ff9d563de62aa978035` mounts actual profile
Settings diagnostic preview/export. Read the
[M5 contract](../../architecture/release-maintenance.md#m5-settings-presentation-and-bounded-composition)
and [M5 evidence/station ledger](../../verification/RFC-BPR-0014.md#milestone-5-qt-presentation-adapter).
Controlled clear/purge presentation is complete; live destructive capabilities
are false. M6 must compose the retained-lease barrier: close ingress, retire
all affected windows/generations, drain producers and Git/workspace/MCP recovery,
close stores/logger, retain an independent vault worker for exact credential-
before-metadata cleanup, and end the purged runtime. Cancellation cannot silently
restart producers; partial cleanup needs real recovery before fresh admission.
Ordinary shutdown or a passing widget test does not prove this composition.
Do not simply flip the two flags in `diagnostic_storage`.

The shared Settings panel follows invoking-window pages, and diagnostic work
drains during profile shutdown. Preserve that behavior and unchanged retention
ownership while completing tests-disabled package selection. JaegAW Windows and
WSL2 routes are reachable but older dirty candidates, not native qualifications;
reconcile exact station/lease/toolchain before authorized work. M8 owns their
native acceptance. Existing release/publish paths remain authoritative; M5 ran
no publication, signing or notarization and grants no such authority.
