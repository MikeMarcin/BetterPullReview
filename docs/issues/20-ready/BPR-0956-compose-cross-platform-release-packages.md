---
schema: "implement-rfc.card.v1"
id: "BPR-0956"
title: "Compose cross-platform release packages"
status: "Ready"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0952","BPR-0955"]
milestone: "M6 Composition and public-path integration"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
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

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.

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
