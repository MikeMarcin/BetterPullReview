---
schema: "implement-rfc.card.v1"
id: "BPR-1655"
title: "Bind deployed Qt module SPDX in release audit"
status: "Done"
rfcs: ["RFC-BPR-0038", "RFC-BPR-0084"]
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"aaadffc76d0a66cbccd201e07e1e9518b1c41427","reservation":"docs/issues/_reservations/BPR-1655--BPR-1655--3342d7aa4b89.json","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["packaging/package_runtime.py","scripts/verify_pe_release.py","tests/scripts/package_runtime_tests.py","tests/scripts/verify_release_security_tests.py","docs/development/release-packages.md"],"boundary":"Coordinator explicitly authorizes bounded deployed-module metadata correction; no C++/CMake/runtime/module pin change, signing or production before final common-source coordination"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Final release source dd6d3cec6026f32dd50428238bbfe9aaa80926fe; private stableLatest release417726205; root postdownload-complete-dd6 receipt hashes independently rechecked. Actual R2 undeclared-pe-producer failure, R3 module evidence, exact configured Qt producer/module hashes and original b914 assets are preserved; first-party xdiff remains separately declared.","receipt":".mask/sessions/release-v0.3.0/resumed-after-rss-exception/postdownload-complete-dd6.json","producerHandoffSHA256":"70a23079a0dedf68e79077a6c9dd951e93d2c3f4d88e1852c0df6b80ad2ef8ea"}},{"criterion":"AC2","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Final release source dd6d3cec6026f32dd50428238bbfe9aaa80926fe; private stableLatest release417726205; root postdownload-complete-dd6 receipt hashes independently rechecked. Committed bounded shared QtBase/Svg/WebP selection retains cross-platform native filename rules and full upstream bytes/signing-normalization/duplicate/path/checksum refusals. Exact producer ownership was independently reviewed; filenames grant no authority.","receipt":".mask/sessions/release-v0.3.0/resumed-after-rss-exception/postdownload-complete-dd6.json","producerHandoffSHA256":"70a23079a0dedf68e79077a6c9dd951e93d2c3f4d88e1852c0df6b80ad2ef8ea"}},{"criterion":"AC3","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Final release source dd6d3cec6026f32dd50428238bbfe9aaa80926fe; private stableLatest release417726205; root postdownload-complete-dd6 receipt hashes independently rechecked. Cross-platform and uppercase Windows closure/refusal courses, old-candidate package-build-source-drift and old-provenance refusal remain retained. Newly produced dd6 Windows MSI/runtime complete native public package/security audits with29 file hashes/21PE images. Final Mac public audits and both Ubuntu roots complete package/security calls PASS; all28 uploaded/downloaded files equal producer hashes.","receipt":".mask/sessions/release-v0.3.0/resumed-after-rss-exception/postdownload-complete-dd6.json","producerHandoffSHA256":"70a23079a0dedf68e79077a6c9dd951e93d2c3f4d88e1852c0df6b80ad2ef8ea"}},{"criterion":"AC4","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Final release source dd6d3cec6026f32dd50428238bbfe9aaa80926fe; private stableLatest release417726205; root postdownload-complete-dd6 receipt hashes independently rechecked. Focused package/security controls and independent1a71 changed-surface review retained; exact final SPDX/application/caller technical dispositions adopted separately. New common-source production, source/SPDX/notices and downloaded trust are reconciled without runtime/module pins, numerical gates or broader qualification changes.","receipt":".mask/sessions/release-v0.3.0/resumed-after-rss-exception/postdownload-complete-dd6.json","producerHandoffSHA256":"70a23079a0dedf68e79077a6c9dd951e93d2c3f4d88e1852c0df6b80ad2ef8ea"}}]
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

The final complete native public audit remains pending fresh coordinator
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


## Final dd6 artifact acceptance, 2026-10-19

The newly produced Windows MSI/runtime at
`dd6d3cec6026f32dd50428238bbfe9aaa80926fe` passes both complete native public
auditors, with all 29 runtime-file hashes, 21 PE images and exact QtBase/Svg/
WebP producer binding. The final Mac public package/security audits and all
four package/security calls on the two actual Ubuntu expanded roots pass.
For stripped upstream Linux Qt images, the independently accepted separate
BPR-1658 read-only verifier is implementation commit
`7301ff8eb4ce63d1a64488e4d848c7153bb627f9`; producer and asset source stays dd6.

All 28 actual uploaded/downloaded files equal the final producer hashes.
Downloaded Mac checks (13), Windows native trust checks (five) and Ubuntu
public-only signature checks (six) pass. Final SPDX and exact source/caller
applicability decisions are separately reconciled, with raw findings and
coverage flags retained. Root's `postdownload-complete-dd6.json` under the
ignored resumed release session binds these results.

This completes AC3's deferred common-source public-audit acceptance and AC4's
final binding. Earlier cross-platform/uppercase positive and hostile controls,
independent review, old-source drift refusals and original R2 failure remain.
No old signed inventory is repaired in place. No full suite is rerun, and no
native desktop, vault, privacy, complete vulnerability coverage or other broad
RFC0038 qualification follows from this metadata/audit completion.
