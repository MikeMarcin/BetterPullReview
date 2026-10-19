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
- AC3: Prove cross-platform module closure, missing-document and hostile producer controls; run the actual complete native MSI package/security audits against unchanged b914 artifacts and preserve tool/artifact source identities. This audit cannot repair missing metadata in old assets; generate final affected provenance/SPDX/assets only through coordinator-owned common-source production.
- AC4: Complete focused checks, independent changed-surface review and intentional card/plan/ledger/package-guide upkeep. Preserve runtime/linkage/module pins, all numerical gates and existing release gaps; final frozen/artifact binding adoption remains explicit and coordinator-owned.
