---
schema: "implement-rfc.card.v1"
id: "BPR-2047"
title: "Normalize gutter ink against its reference background"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-2045"]
priority: "P1"
platforms: ["win"]
metadata: {"milestone_plan":{"id":"bpr-2047-background-aware-gutter-ink","path":"docs/rfcs/plans/bpr-2047-background-aware-gutter-ink/milestone-plan.json"},"mandate_revision":"release54-background-aware-ink-r1","source_baseline":"899056e7de7187edc0ee206a7dc499a1f5487af4","point_source":"5a9a503f74bb72d53b0ff3a94acc252a84dc1dc7","reservation":"docs/issues/_reservations/BPR-2047--BPR-2047--8c355069f064.json","allocation_main":"db44a3474de4cc366ad68f3c7357f16a2353f726"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3783652920a81b04e62d6a7afbbed004193d9c01","evidence":{"kind":"observation","detail":"Observed125RGB/PNG failure binds a present fringe pixel to fixed-distance classification. Test-local normalized contrast validates blank padding and same original reference row, bounded65025products, original reference32mask,575messages, gestures and1px geometry unchanged.","ledger":"docs/verification/BPR-2047-background-aware-gutter-ink.md","external_evidence":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer54-windows-diff-workbench-triage-r1/background-positive-r1"}},{"criterion":"AC2","outcome":"pass","revision":"3783652920a81b04e62d6a7afbbed004193d9c01","evidence":{"kind":"observation","detail":"Same-code observed/background/lowcontrast/missing/shift controls and exhaustive scalar reference-mask identity pass. Combined original100/125/200native engineering courses pass once12.877/14.556/19.313s; actualCL/module/link/rows match except privateEXE/output namespace. NativeWX, LLVM22format/fullTUtidy0, numeric/static, explicitplan/docs/board/diff pass. No release qualification credit.","ledger":"docs/verification/BPR-2047-background-aware-gutter-ink.md","external_evidence":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer54-windows-diff-workbench-triage-r1/background-positive-r1"}},{"criterion":"AC3","outcome":"pass","revision":"3783652920a81b04e62d6a7afbbed004193d9c01","evidence":{"kind":"observation","detail":"Own sourcec4c6 lands canonicalmain378365292 with ownplan/claim only, preserving foreignsource/counterNEXT2048. All1742producer inputs/candidate exact, source899clean, forests empty, actualtaskReady0/idletrue; PClease returned. Original54failures retained and fresh common-source native qualification remains root responsibility.","ledger":"docs/verification/BPR-2047-background-aware-gutter-ink.md","external_evidence":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer54-windows-diff-workbench-triage-r1/background-positive-r1"}}]
---

# Normalize gutter ink against its reference background

The original125course rejects a painted fringe pixel when selection changes
its background. PenRGB91/101/112; reference118/130/142 on208/224/242 passes
raw distance30; current126/134/143 on240/242/245 fails at35. Both captured
frames contain the fringe, and neighboring rows move exactly10devicepixels.
The original geometry predicate and one-device-pixel contract stay unchanged.

## Acceptance

- AC1. Preserve original failures and captured RGB/PNG evidence. Add only a
  test-local bounded integer normalized-contrast classifier. Sample documented
  blank padding inside each column; validate reference bounds and original row,
  fail closed on invalid/low contrast, never clamp to another row. Preserve the
  original reference mask,575messages, all gestures and2045point correction.
- AC2. Same-code controls recognize observed pixels and reject background-only,
  low contrast, missing visible one/two-plus pixel ink and +/-2pixel shifts,
  while retaining +/-1. Combined uninstrumented100/125/200courses pass once each
  under original flags, sandbox, properties,32reference threshold,1pxgeometry
  limit and120s deadline; only private EXE/output namespace differ. Scoped
  format/static/docs checks pass. No source54qualification reuse or Qt waiver.
- AC3. Land only owned test/workflow slices on main, preserving foreign work and
  counter highwater. Producer source899/images/objects/cache/PNGs remain exact;
  commands and leases are reconciled. Fresh common-source release gates remain
  required, and raw original failures retain their meaning.
