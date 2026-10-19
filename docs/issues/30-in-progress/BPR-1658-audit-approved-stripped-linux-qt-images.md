---
schema: "implement-rfc.card.v1"
id: "BPR-1658"
title: "Audit approved stripped Linux Qt images"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"assigned_implementer":"/root/pc_final_dd6","mandate":"release-v030-coordinated-r1; separate audit-tool correction","baseline":"dd6d3cec6026f32dd50428238bbfe9aaa80926fe","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"boundary":"Standalone read-only verifier/tests/docs only; preserve all producer/package/source inputs and signed dd6 artifacts. Root owns independent acceptance and publication."}
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
