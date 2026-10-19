---
schema: "implement-rfc.card.v1"
id: "BPR-1654"
title: "Bind discarded SDK import metadata in PE release audit"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"b914b56f6aab8c51f632eafa863f890c80539865","reservation":"docs/issues/_reservations/BPR-1654--BPR-1654--8dea387504db.json","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["scripts/verify_pe_release.py","scripts/verify_release_package.py","scripts/verify_release_security.py","tests/scripts/verify_release_security_tests.py","tests/scripts/verify_release_package_tests.py","docs/development/release-packages.md"],"boundary":"Coordinator explicitly invalidates source freeze for bounded verifier-only correction; no C++/CMake/runtime/linkage/numeric-gate change or production rerun before common-source coordination"}
---

# BPR-1654: Bind discarded SDK import metadata in PE release audit

The actual signed Windows b914 candidate passes production, native manifest,
fresh administrative extraction, all 29 runtime-file hashes and five native
signature/publisher/timestamp checks. Its package/security audit fails with
pe-map-symbol-address-mismatch. Preserve that failed audit and all seven final
assets; extraction is not product installation or native runtime qualification.

## Acceptance

- AC1: Preserve the original audit failure and exact MCP producer image/map, matching timestamp/base/entry, SDK import-library and actual PE import evidence. Record that bcrypt is absent from the helper's import table and DLL-name payload; a positive import-corroboration premise is falsified.
- AC2: Admit only the proven discarded SDK import-name metadata with explicit proof bound to the selected producer library: exact static-table .idata$6, no f/i flags, named owner, image-base address, exact 32-bit wrap from a declared non-executable section, and absent import/name payload. Missing/malformed/mismatched proof refuses. Preserve full symbol/fixture scans and every normal identity/address assertion; no generic modulo or .idata wildcard.
- AC3: Prove the named positive and missing/wrong proof, owner/name/flags/offset/RVA/section/executable-section/ordinary-function negative controls, retain timestamp/base/entry/image/fixture refusals, and run the actual complete native MSI package/security audit with the bounded verifier. Old artifacts retain b914 source identity; final common-source production remains coordinator-owned.
- AC4: Complete appropriate focused script/documentation checks, independent changed-surface review, exact source-input impact and intentional upkeep. Change no C++/CMake/runtime/linkage, memory budget or qualification predicate. Retain all existing memory/history/native/vault/security/source-access gaps and require fresh final frozen/artifact reconciliation.

## Actual evidence and scope

Only the MCP helper has one rejected row among 1,833 map records:
`0002:fffe5000 .idata$6 0000000140000000 bcrypt:bcrypt.dll`.
Its image SHA-256 is
`6137ae12ff1f7c08713950af5273f434e025d0d4b2a6323973d7cc0256b2c4fd`;
the full map is
`c433f2cc82e61d229f3ec6b05cc4a8b32fafa0947920d1299e4d95cb33e9e3f9`.
Timestamp 6ad61e4c, base 0x140000000 and entry 0001:00017ca0 match.
Section VA 0x1b000 plus offset 0xfffe5000 equals exactly 2**32; its flags are
0x40000040, non-executable initialized data. All other first-party records pass.

The selected SDK bcrypt.lib SHA-256 is
`9fe31df255d17b0339391ff80a71bf2c5d442cc301bd2fe6eeea00b626f10785`.
Independent byte parsing identifies member 6974's static/type-zero/value-zero
.idata$6 symbol in section 3, whose 12 bytes are bcrypt.dll followed by two NULs.
Actual helper imports and both ASCII/wide DLL-name bytes lack bcrypt. The core
links bcrypt transitively, while the helper's map retains SDK metadata for its
unused contribution. The prior positive-import proposal cannot pass and is not
silently adopted. All native data, original failure and orthogonal checks remain
under ignored final-audit evidence, separate from distribution assets.

The coordinator authorizes a revised absence-proof verifier correction after
independent review of this exact counterexample. Source freeze is explicitly
invalidated; no artifact is relabelled. General PE infrastructure, product
linkage changes and a new runtime dependency are outside this slice.
