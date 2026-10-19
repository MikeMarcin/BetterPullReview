---
schema: "implement-rfc.card.v1"
id: "BPR-1658"
title: "Audit approved stripped Linux Qt images"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"assigned_implementer":"/root/pc_final_dd6","mandate":"release-v030-coordinated-r1; separate audit-tool correction","baseline":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"boundary":"Standalone read-only verifier/tests/docs only; preserve all producer/package/source inputs and signed dd6 artifacts. Root owns independent acceptance and publication."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"49b07a96ca01f40c7d3ab0771035d3a1c1ed0001","evidence":{"kind":"artifact","detail":"Exact separate draft tool 2a12927c1d0d5eb1c41ab5f810499bb26b9d172f4028915c0cd1e53cf0553bcd; 12 actual Qt producer SPXD checksums and exact relocated-byte matches on both preserved dd6 roots; no generic fallback"}},{"criterion":"AC2","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"observation","detail":"Focused9package/18security controls and actual native changedQt/unownedstripped/firstpartystripped refusals PASS; unchanged full symbol/anchor and full package/security predicates; original failure retained"}},{"criterion":"AC3","outcome":"pass","revision":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","evidence":{"kind":"artifact","detail":"Complete4 actual expanded DEB/archive package+security entry points PASS472 firstparty units/19ELF/12Qt each; exactdrafttoolsha/producerROOT/fullCDB bound; all13 signedasset hashes and allcritical shipped/build/package/source inputs unchanged. Docs gate and67 controls PASS. Final toolcommit supplied by canonical finish; independent release/tool adoption remains root-owned."}}]
---

# BPR-1658: Audit approved stripped Linux Qt images

The signed dd6 Ubuntu package contains twelve pinned prebuilt Qt ELF libraries/plugins
without regular symbol tables. The current audit refuses them before their genuine
dynamic symbols can be inspected. First-party and source-built ICU images retain
full symbol tables. The original native failure remains preserved.

## Acceptance

- AC1: Admit dynamic defined-symbol evidence only for an explicitly declared Linux Qt library/plugin bound to its selected canonical producer, actual upstream SPDX file/checksum and byte-identical relocation. No generic fallback, filename-only authority or unknown image is admitted.
- AC2: Keep first-party full-symbol/production-anchor checks, all byte/fixture/dependency predicates and native hardening intact. Empty, malformed, unowned, changed-producer and first-party-stripped controls refuse.
- AC3: Run focused controls and the complete package/security audits on both preserved actual dd6 expanded roots using the exact separate draft tool and explicit actual source/build bindings. Bind the final tool commit/hash and prove all shipped/build/package/source-material inputs unchanged before release adoption. Preserve original failures and native qualification limits.

## Exact verification and separation

The draft verifier SHA-256 is
`2a12927c1d0d5eb1c41ab5f810499bb26b9d172f4028915c0cd1e53cf0553bcd`.
Against both preserved dd6 expanded roots, package and security entry points
pass with472 first-party compile units,19 ELF images and12 exact upstream Qt
bindings each. The native driver explicitly binds verifier/runtime source context
to the actual clean producer and keeps its full compilation graph. All13 signed
asset hashes remain unchanged. Original native failure and the first draft
wrapper's missing patchelf PATH refusal remain preserved.

Nine focused package controls and18 security controls pass. Native disposable
copies establish6731 genuine QtCore dynamic definitions, changed-Qt refusal,
unowned stripped-image refusal and stripped-first-party refusal. Empty,
ABS-only, undefined-only, malformed, changed producer, missing record and escaped
producer cases refuse in focused controls.67 documentation controls pass.

Actual evidence lives under the coordinator's ignored
`.mask/sessions/release-v0.3.0/pc-final-dd6-production/`:
`linux-corrected-audit-evidence/draft-audit-r2-receipt.json`, both pairs of
`draft-r2-*-package.json`/`draft-r2-*-security.json`,
`native-symbol-controls.json`, and the original
`linux-diagnostic-evidence/audit-receipt.json`. These are mechanical observations,
not native desktop/vault, vulnerability, material or publication approval.
The coordinator owns independent adoption and postpublication integration of
this audit-only branch. Main, producer commits and signed artifacts remain dd6.
