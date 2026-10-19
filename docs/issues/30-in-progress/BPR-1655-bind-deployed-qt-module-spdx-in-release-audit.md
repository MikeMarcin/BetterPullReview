---
schema: "implement-rfc.card.v1"
id: "BPR-1655"
title: "Bind deployed Qt module SPDX in release audit"
status: "In Progress"
rfcs: ["RFC-BPR-0038", "RFC-BPR-0084"]
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"aaadffc76d0a66cbccd201e07e1e9518b1c41427","reservation":"docs/issues/_reservations/BPR-1655--BPR-1655--3342d7aa4b89.json","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["packaging/package_runtime.py","scripts/verify_pe_release.py","tests/scripts/package_runtime_tests.py","tests/scripts/verify_release_security_tests.py","docs/development/release-packages.md"],"boundary":"Coordinator explicitly authorizes bounded deployed-module metadata correction; no C++/CMake/runtime/module pin change, signing or production before final common-source coordination"}
---

# BPR-1655: Bind deployed Qt module SPDX in release audit

The actual aaad-tool/b914-artifact native package audit advances past the
MCP metadata row, then fails undeclared-pe-producer. A separate native
observation reproduces the refusal for Qt6Svg.dll and qwebp.dll. Their exact
configured producer qtsvg and qtimageformats SPDX documents exist. The PE
verifier reads only qtbase; provenance selects qwebp's document but its QtSvg
substring misses the Windows Qt6Svg.dll basename. The old installed provenance
therefore omits Svg. Preserve the failed attempt and old signed asset bytes.

## Acceptance

- AC1: Preserve the actual R2 failure, R3 module evidence and all old artifacts; bind exact configured/resolved Qt producer and separate module document hashes. Treat raw upstream survey's xdiff refusal separately from its already-declared first-party route.
- AC2: Share bounded deployed-module SPDX selection between generated Qt provenance and native PE producer inspection. Recognize approved Windows, Darwin and Linux Svg/WebP names, require their exact configured module documents, and retain exact upstream bytes/signing normalization, unknown/duplicate/path escape and checksum refusals. No filename alone supplies producer authority or broad module fallback.
- AC3: Prove cross-platform module closure, missing-document and hostile producer controls; preserve the actual old-candidate full audit and any build-drift refusal, validate exact existing PE producers, then require complete native MSI package/security acceptance on freshly reproduced common-source artifacts. Preserve tool/artifact source identities. This audit cannot repair missing metadata in old assets; generate final affected provenance/SPDX/assets only through coordinator-owned common-source production.
- AC4: Complete focused checks, independent changed-surface review and intentional card/plan/ledger/package-guide upkeep. Preserve runtime/linkage/module pins, all numerical gates and existing release gaps; final frozen/artifact binding adoption remains explicit and coordinator-owned.

## Implemented scope and acceptance boundary

The shared selector reads only qtbase and the exact module documents required
by canonical deployed Svg/WebP names; the PE route retains full upstream
file/checksum/signature checks and every unknown/duplicate/escape refusal.
The cross-platform fixture now exercises actual joined closure generation,
installed notices/source offers/provenance/final SPDX, and refuses an omitted
Svg installed provenance record. The separate source-asset archive and exact
pinned source/legal checks remain mandatory. No old asset is repaired in place.

The full package-runtime suite also exposed an older lexical test selecting
the assessment helper's first PREPARE_ONLY branch. The fixture now selects the
unique main ASSET_BASE branch, preserving no-notary/no-security/no-identity-sign,
ad-hoc-sign and exit assertions. Product release.sh bytes remain unchanged.

AC3's final complete native public audit remains pending fresh coordinator
production. A packaging-source change must retain package-build-source-drift
on old b914 artifacts. No guard is relaxed to obtain an old-candidate pass.

Independent review supplies a real Windows case-only counterexample at 9579:
QT6SVG.DLL could pass PE ownership while provenance repeated the Svg omission.
Windows file identity is now consistently case-insensitive; Darwin/Linux file
identity remains exact. The public generation course covers uppercase Windows
Svg/WebP/core files, actual platform-specific legal layout and final SPDX, then
proves omitted installed Svg provenance refuses. No filename authorizes bytes.

Native R4 at 9579 passes all 21 exact PE producer/map/export/payload/dependency/
hardening inspections and verifies the complete old corresponding-source asset
and installed source references. Its corrected Qt closure contains 11 files.
Both maintained public auditors actually refuse old b914 with
package-build-source-drift; the old installed provenance separately refuses
qt-runtime-provenance-drift. Original assets and extracted runtime hashes remain
unchanged. These partial observations do not close the final public-audit AC.

Native R5 at 1a71 passes the exact29-file runtime private-copy uppercase course:
the same 11-file producer closure, exact QtSvg/WebP upstream PE binding, and
changed-copy byte refusal. The temporary copy is removed; all original runtime
and asset hashes remain equal. Native package-runtime 23 pass/2 configured
platform skips (Linux baseline and POSIX ldd fixture), and security 18 pass. All four owned verifier tasks retire at
2026-10-19T14:33:58Z with no producer/build/test processes. The coordinator
receives the PC lease; no production or install/launch was performed.

The explicit preproduction stopping boundary leaves final common-source public
audit AC pending. No postfreeze card or ledger commit may move publishing HEAD;
actual production/audit/publication observations remain in ignored evidence
until postrelease administrative closeout. A new artifact failure requires an
explicit new source invalidation and reproduction.

Independent changed-surface review completes at 1a71 with no remaining blocker.
The reviewer independently confirms identical canonical/uppercase 11-file,
65-package closure against the actual inventory and all three producer SPDXs,
and reconciles native R4/R5, source/notices/relink and fixture evidence.
This completes bounded source review only; fresh-artifact public audits,
new SPDX/Grype applicability and every other release gate remain pending.
