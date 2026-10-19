---
schema: "implement-rfc.card.v1"
id: "BPR-1656"
title: "Ship a standalone xdiff recipient rebuild recipe"
status: "Done"
rfcs: ["RFC-BPR-0038", "RFC-BPR-0082"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"dfb5f95f3a6fc02ca58148d1d99b7ba7cf4d4bbd","reservation":"docs/issues/_reservations/BPR-1656--BPR-1656--025e2086fd06.json","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["packaging/dependencies.json","packaging/package_runtime.py","third_party/git-xdiff/rebuild/CMakeLists.txt","third_party/git-xdiff/rebuild/README.md","third_party/git-xdiff/PROVENANCE.md","tests/scripts/package_runtime_tests.py","tests/scripts/xdiff_recipient_rebuild_tests.py","docs/development/release-packages.md"],"boundary":"Coordinator explicitly reopens the freeze for the standalone corresponding-source recipe; no production C/C++/provider/hardening change, producer mutation or runtime qualification"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Final release source dd6d3cec6026f32dd50428238bbfe9aaa80926fe; private stableLatest release417726205; root postdownload-complete-dd6 receipt hashes independently rechecked. Original DFB source-material gap/assets retained. Final exact source ZIPs carry pinned original Git archive and six standalone build helpers/instructions, preserving seven C11 source/output/export/hardening settings and modified recipient-source allowance.","receipt":".mask/sessions/release-v0.3.0/resumed-after-rss-exception/postdownload-complete-dd6.json","producerHandoffSHA256":"70a23079a0dedf68e79077a6c9dd951e93d2c3f4d88e1852c0df6b80ad2ef8ea"}},{"criterion":"AC2","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Final release source dd6d3cec6026f32dd50428238bbfe9aaa80926fe; private stableLatest release417726205; root postdownload-complete-dd6 receipt hashes independently rechecked. Retained actual Mac universal build/arm64 consumer and30exports, Windows11 x64/MSVC consumer/import-library and33exports, Ubuntu26.04 WSL2 original/modified consumer and30exports plus all original refusal controls. Current selected xdiff component/sixhelpers/source material are byte-identical and independently reviewed. Seven of9 global recipe inputs match; two Linux packaging inputs changed for ICU. No all9 equality/nativeIntel/desktop/application-replacement claim.","receipt":".mask/sessions/release-v0.3.0/resumed-after-rss-exception/postdownload-complete-dd6.json","producerHandoffSHA256":"70a23079a0dedf68e79077a6c9dd951e93d2c3f4d88e1852c0df6b80ad2ef8ea"}},{"criterion":"AC3","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Final release source dd6d3cec6026f32dd50428238bbfe9aaa80926fe; private stableLatest release417726205; root postdownload-complete-dd6 receipt hashes independently rechecked. Fresh dd6 all-platform package production and matching exact separate source/notices/offers/provenance/SPDX complete native public audits. All28 actual downloaded files equal finalproducer hashes; Mac13 downloaded checks/Windows5native trust/Linux6public-only signature checks pass. Historical DFB artifacts remain unchanged; human material decisions are scoped to exact proven material.","receipt":".mask/sessions/release-v0.3.0/resumed-after-rss-exception/postdownload-complete-dd6.json","producerHandoffSHA256":"70a23079a0dedf68e79077a6c9dd951e93d2c3f4d88e1852c0df6b80ad2ef8ea"}},{"criterion":"AC4","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Final release source dd6d3cec6026f32dd50428238bbfe9aaa80926fe; private stableLatest release417726205; root postdownload-complete-dd6 receipt hashes independently rechecked. Focused21 source-asset and24package-runtime passes with original configuredskip, native component courses, independent changed-surface review, canonical source binding and prefreeze documentation retained. Final operational receipts stayed outside source until root authorized this postpublication closeout. All broader qualification and ongoing sourceaccess/retention obligations remain explicit.","receipt":".mask/sessions/release-v0.3.0/resumed-after-rss-exception/postdownload-complete-dd6.json","producerHandoffSHA256":"70a23079a0dedf68e79077a6c9dd951e93d2c3f4d88e1852c0df6b80ad2ef8ea"}}]
---

# BPR-1656: Ship a standalone xdiff recipient rebuild recipe

Both preserved DFB source ZIPs contain the original xdiff acquisition provider
and two compatibility headers. The provider includes absent AitoCpm/Hardening
helpers, requires CPM and application-root paths, and supplies no standalone
entry point. Exact mechanical inventory checks pass their declared three files,
but do not prove recipients can rebuild the separately replaceable LGPL library.
The coordinator explicitly invalidates the source freeze for this bounded repair.

## Acceptance

- AC1: Retain original final assets and the actual source-material gap. Ship pinned original archive, compatibility headers, standalone recipient CMake project/instructions and every local helper it needs, without a BetterPullReview checkout, CPM, shared cache or network acquisition. Preserve the original seven C11 shared-library sources, output/export and hardening settings; original archive pins must not prohibit modifying recipient-extracted source.
- AC2: From only the generated component source asset, configure/build in fresh disposable roots, compare defined exports against the exact delivered library and link/run a consumer through the public xdl_diff C interface. Modify recipient source, rebuild/relink and prove the modification executes while the original API remains compatible. Corrupt original archive, missing helper/header/source and incorrect expected results must refuse. Record actual platform/architecture and retain independent changed-surface review; no native-desktop or other-platform execution claim follows.
- AC3: Reproduce affected final assets at the next coordinated common source, including matching separate source ZIP, installed notices/offers/provenance/SPDX and actual complete package/security audits. Preserve DFB assets unchanged and every existing native/signing/legal/security/test obligation. This remains pending external producer acceptance at the engineering freeze.
- AC4: Complete focused source/package tests, canonical criterion binding, documentation and intentional allocated-card/plan/ledger upkeep before the source freeze. Keep later operational acceptance receipts outside source until the coordinator's postrelease administrative boundary.

## Engineering evidence and preproduction boundary

The source-input declaration now carries six pinned build files: the original
provider, two compatibility headers, standalone CMake/README and unchanged
hardening helper. The standalone entry point resolves its helper inside the
asset and uses an explicit extracted source path; it includes no acquisition
provider or application tree. Installed source offers and the guide identify
that entry point and permit modified recipient source. This is technical recipe
work, with no UI change or new core infrastructure.

The actual Mac r2 assessment finishes in 5.662 seconds. From the verified
archive and freshly generated xdiff-component source asset, it builds both
arm64 and x86_64 slices, verifies the exact seven C11 compile commands with
canonical hardening options, and compares all 30 defined arm64 exports with
the preserved signed DFB library. A separately linked C consumer exercises
xdl_diff with Myers/indent and exact expected output. Editing recipient xutils.c
adds a callable marker, preserves every original export and repeats the correct
API result after rebuild/relink. The corrupt-archive writer, missing helper,
header/source and incorrect-result controls all refuse. Temporary source and
build roots are removed; the delivered library hash is unchanged.

The receipt is `.mask/sessions/release-v0.3.0/xdiff-recipient-r2/receipt.json`.
Its sourceRevision is the then-current claimed HEAD; recipe/declaration bytes
were under assessment before integration, not a new release asset. Input hashes
and the final commit binding accompany the coordinator handoff. This is an
actual universal build and arm64 execution/export comparison, with no native
Intel desktop or Windows/Linux recipe-execution claim. Windows import-library
and API controls were still pending at that Mac observation; the subsequent
native course is recorded below.

Focused package-runtime verification passes 24/25 with its configured Linux
baseline skip on Mac; separate source-asset verification passes all 21. The
coordinator owns final all-platform source/SPDX/notices/package reproduction
and complete public audits. The card stays In Progress for that acceptance;
all old assets and broader test/native/vault/privacy/signing/legal/security
obligations remain. No claim or receipt from the component-only assessment
repairs the old asset or certifies the application replacement course.

## Native Windows recipient course

At clean committed source `1d62aadb33bf5e3e094fc75c2098afcc143713cd`, the
same archive-only course passes on native Windows 11 x64/MSVC in 8.082 seconds.
All seven sources compile with the packaged canonical helper. The rebuilt DLL
has exactly the preserved DFB DLL's 33 defined exports. Its generated import
library actually links a separate C consumer, which runs the exact public
xdl_diff comparison. After editing recipient xutils.c, a new exported marker
returns the expected value while all original exports and the original diff
result remain. Every original tamper/missing-input/wrong-result control passes.

The complete receipt, actual compile commands and raw output are retained at
`.mask/sessions/release-v0.3.0/windows-xdiff-recipient-r1-native/`.
The 1d62 tool/assessment identity and DFB comparison-artifact identity remain
separate. The own task retires after completion with clean source and no
build/test processes; the PC lease returns to the coordinator. A transfer-only
PowerShell array-path failure is preserved, then corrected without rerunning or
altering the successful course. No delivered artifact or signing state changes.
The later Mac r3 record additionally hashes exact assessment inputs for final
commit reconciliation. Neither course is complete application replacement or
clean-account qualification. The subsequent Ubuntu course below retains its
separate baseline-artifact and source-status limits.

## Ubuntu archive-only recipient course

The same course passes on Ubuntu 26.04 x86_64 under WSL2 in 91.923 seconds.
The seven-source Release build matches all 30 defined ELF exports of the
unchanged retained Ubuntu Release library, and the original/modified consumer,
marker, tamper and refusal controls pass. The library's before/after SHA-256 is
`de56293200d8bb3c4815e53a3437015db3109a2424d37f335775bcb713d66132`.
This comparison is an existing build library, not a final delivered Linux
package; no app build, package signing, desktop, vault or credentials are used.
The WSL application checkout, source and benchmark/binaries stay untouched.

The receipt is under
`.mask/sessions/release-v0.3.0/linux-xdiff-recipient-r1-native/`.
It preserves Linux Git's dirty view of the mounted Windows source, while
native Windows Git remains clean at 1d62. No clean-Linux-Git claim is made.
All nine actual input hashes (script/runtime/declaration plus six shipped build
files) independently equal committed 1d62 bytes across Mac, Windows and Linux.
The source-status mismatch is retained separately from that exact byte proof.
The own scheduled task retires and the shared PC/WSL lease returns before the
coordinator assigns any later work. All three platform recipe observations
remain component assessments; final reproduced public-artifact acceptance is
still pending outside source.


## Final dd6 artifact acceptance, 2026-10-19

All three final platform sets are reproduced from
`dd6d3cec6026f32dd50428238bbfe9aaa80926fe`. Their separate source ZIPs,
installed notices/offers/provenance and SPDX pass the complete native public
audits. All 28 actual uploaded/downloaded files equal final producer hashes;
downloaded Mac checks (13), Windows native trust checks (five) and Ubuntu
public-only signatures (six) pass. The exact retained source trio is read-only
and equal to the downloaded source assets. Root's ignored
`resumed-after-rss-exception/postdownload-complete-dd6.json` binds the result.

The selected xdiff component, its six shipped helpers and practical source
material remain byte-identical to the independently reviewed native original/
modified consumer courses above. Seven of nine global recipe inputs match;
two Linux packaging inputs changed for ICU. No all-nine equality is claimed.
Mac/Windows reuse their exact approved inventory/source-ZIP/material scope by
independently verified equality; Ubuntu's changed ICU material has its own
human acceptance, decision `c8d8112a-b9c2-442b-bdbc-9e7310a61a73` in session
`5ccca36b4b292dc93c0c56a65bd62290`, hand-back cursor 5.

This completes AC3's deferred final-asset acceptance and AC4's postpublication
upkeep boundary. AC1's original material gap/DFB assets and AC2's actual native
component controls remain retained, with their recorded architecture and
source-status limits. No full suite or component course is repeated. Ongoing
no-extra-charge equivalent source access, original-channel/corporate/offline
fallback and indefinite immutable retention continue. This supplies no native
Intel/desktop, complete application replacement, vault, crash/privacy or broad
RFC0038 qualification, and BPR-1653 remains open.
