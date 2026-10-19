---
schema: "implement-rfc.card.v1"
id: "BPR-1654"
title: "Bind discarded SDK import metadata in PE release audit"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"b914b56f6aab8c51f632eafa863f890c80539865","reservation":"docs/issues/_reservations/BPR-1654--BPR-1654--8dea387504db.json","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["scripts/verify_pe_release.py","scripts/verify_release_package.py","scripts/verify_release_security.py","tests/scripts/verify_release_security_tests.py","tests/scripts/verify_release_package_tests.py","docs/development/release-packages.md",".codex/skills/publish-release/references/windows.md"],"boundary":"Coordinator explicitly invalidates source freeze for bounded verifier-only correction; no C++/CMake/runtime/linkage/numeric-gate change or production rerun before common-source coordination"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Final release source dd6d3cec6026f32dd50428238bbfe9aaa80926fe; private stableLatest release417726205; root postdownload-complete-dd6 receipt hashes independently rechecked. Original b914 MCP map/image failure and actual bcrypt import/name absence remain retained in the existing card/native R3 evidence; no historical library-selection premise is restored.","receipt":".mask/sessions/release-v0.3.0/resumed-after-rss-exception/postdownload-complete-dd6.json","producerHandoffSHA256":"70a23079a0dedf68e79077a6c9dd951e93d2c3f4d88e1852c0df6b80ad2ef8ea"}},{"criterion":"AC2","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Final release source dd6d3cec6026f32dd50428238bbfe9aaa80926fe; private stableLatest release417726205; root postdownload-complete-dd6 receipt hashes independently rechecked. The reviewed full-file SDK specimen and exact static-map/PE absence predicates are retained in committed dd6. Missing/wrong specimen and all named hostile metadata controls pass; every normal image/symbol/fixture assertion remains.","receipt":".mask/sessions/release-v0.3.0/resumed-after-rss-exception/postdownload-complete-dd6.json","producerHandoffSHA256":"70a23079a0dedf68e79077a6c9dd951e93d2c3f4d88e1852c0df6b80ad2ef8ea"}},{"criterion":"AC3","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Final release source dd6d3cec6026f32dd50428238bbfe9aaa80926fe; private stableLatest release417726205; root postdownload-complete-dd6 receipt hashes independently rechecked. Existing native R3 MCP positive plus23 hostile controls retained. Final dd6 native Windows public package/security audits complete successfully on fresh MSI/runtime,29 file hashes and21 PE images, with5 native publisher/timestamp checks. Actual downloaded MSI plus4 first-party images independently pass native trust and equal final hashes.","receipt":".mask/sessions/release-v0.3.0/resumed-after-rss-exception/postdownload-complete-dd6.json","producerHandoffSHA256":"70a23079a0dedf68e79077a6c9dd951e93d2c3f4d88e1852c0df6b80ad2ef8ea"}},{"criterion":"AC4","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Final release source dd6d3cec6026f32dd50428238bbfe9aaa80926fe; private stableLatest release417726205; root postdownload-complete-dd6 receipt hashes independently rechecked. Independent changed-surface review at1a71 and exact final technical acceptance retained. No C++/CMake/runtime/linkage or numeric-predicate changes. Earlier17security/7package/8dependency controls and final documentation gate retained; final common-source/download reconciliation now complete. Broad qualification and BPR1653 remain open.","receipt":".mask/sessions/release-v0.3.0/resumed-after-rss-exception/postdownload-complete-dd6.json","producerHandoffSHA256":"70a23079a0dedf68e79077a6c9dd951e93d2c3f4d88e1852c0df6b80ad2ef8ea"}}]
---

# BPR-1654: Bind discarded SDK import metadata in PE release audit

The actual signed Windows b914 candidate passes production, native manifest,
fresh administrative extraction, all 29 runtime-file hashes and five native
signature/publisher/timestamp checks. Its package/security audit fails with
pe-map-symbol-address-mismatch. Preserve that failed audit and all seven final
assets; extraction is not product installation or native runtime qualification.

## Acceptance

- AC1: Preserve the original audit failure and exact MCP producer image/map, matching timestamp/base/entry, SDK import-library and actual PE import evidence. Record that bcrypt is absent from the helper's import table and DLL-name payload; a positive import-corroboration premise is falsified.
- AC2: Admit only the proven discarded SDK import-name metadata with an explicit full-file pin for the independently reviewed SDK metadata specimen, without claiming actual linked-library selection: exact static-table .idata$6, no f/i flags, named owner, image-base address, exact 32-bit wrap from a declared non-executable section, and absent import/name payload. Missing/malformed/mismatched proof refuses. Preserve full symbol/fixture scans and every normal identity/address assertion; no generic modulo or .idata wildcard.
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

The independently inspected SDK bcrypt.lib specimen SHA-256 is
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

## Explicit proof-boundary amendment

The initial selected-library premise is withdrawn after independent review:
CMAKE_RC_COMPILER selects the resource compiler, while LINK response paths,
/LIBPATH, LINK options and LIB ordering can select another import library.
The preserved historical producer did not record that resolution. The
coordinator explicitly authorizes the narrower honest specimen role: the
reviewed full-file hash establishes the reserved static DLL-name metadata
class; this artifact's strict map/PE predicates establish the absence of a
mapped implementation address. No selected-library provenance is inferred.
The independent reviewer finds this classification sufficient in the named
scope; actual implementation, controls and native audit remain required.

## Implementation checkpoint before common-source production

Implementation aaadffc76d0a66cbccd201e07e1e9518b1c41427 passes 17 security,
7 package and 8 dependency tests. Independent review finds no blocking
changed-surface issue. Native R2 on the unchanged b914 MSI retains the expected
no-specimen map refusal, passes all 32 focused unit cases, then advances past
MCP metadata and fails undeclared-pe-producer at the separate Qt module
coverage defect BPR-1655. Native R3 passes the actual MCP positive plus 23
hostile controls, preserving all original image/map bytes and complete symbol
scans. These observations do not constitute a complete package audit pass.

The preserved-failure, metadata and narrow changed-verifier criteria have evidence. The complete
native package/security acceptance remains pending coordinator-owned fresh
common-source production after BPR-1655; its packaging change intentionally
makes old-source build-drift checks refuse. Administrative card closeout follows
actual final artifact/download verification, without moving the publishing
source after freeze. The old seven b914 assets and both failed audit attempts
remain immutable. The Windows operator reference now exposes the specimen
option and its refusal/provenance boundary under explicit coordinator ownership.

Independent changed-surface review is complete at 1a71 after the separate Qt
correction; no remaining blocker is found in this bounded source change.
Native R3 evidence and all hostile controls remain binding. This is source
review completion, not final common-source package/security acceptance.


## Final dd6 artifact acceptance, 2026-10-19

The fresh final Windows MSI/runtime at
`dd6d3cec6026f32dd50428238bbfe9aaa80926fe` passes the complete native package
and security entry points, all 29 runtime-file hashes and 21 PE image checks.
Five native publisher/chain/timestamp checks pass. The downloaded MSI and four
first-party images independently pass native trust and equal the final hashes.
The private stable v0.3.0 release contains these exact bytes; root's
`resumed-after-rss-exception/postdownload-complete-dd6.json` binds the completed
28-file download verification.

This completes the previously deferred AC3 public-audit and AC4 final binding.
AC1's original failure and falsified positive-import premise remain retained;
AC2's reviewed specimen is metadata-class evidence, with no claim that LINK
selected that historical library. Existing positive/23-hostile controls and
independent review remain valid. No new native desktop, vault, memory, privacy
or vulnerability qualification is claimed; BPR-1653 and RFC0038 remain open.
