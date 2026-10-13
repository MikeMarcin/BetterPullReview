---
schema: "implement-rfc.card.v1"
id: "BPR-1412"
title: "Verify and close RFC-BPR-0072"
status: "Done"
rfcs: ["RFC-BPR-0072"]
depends_on: ["BPR-1411"]
priority: "P1"
metadata: {"mandate_revision":"cpp-identifier-context-r1","milestone_plan":{"id":"cpp-identifier-context-v1","path":"docs/rfcs/plans/cpp-identifier-context-v1/milestone-plan.json"}}
closeout_for: ["RFC-BPR-0072"]
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"bc4b4aafe6f51020aa469c519e936d616e124e89","evidence":{"kind":"observation","detail":"Exact Git-backed composed comparison passes 14 declaration/use/macro sites in each pane, Light/Dark formats plus visible raster pixels. All 374 published contrast pairs pass in four built-ins. See m2-focused.json and docs/verification/RFC-BPR-0072.md."}},{"criterion":"AC2","outcome":"pass","revision":"bc4b4aafe6f51020aa469c519e936d616e124e89","evidence":{"kind":"observation","detail":"Five fresh OS processes pass legacy stored flags/values, independent namespace/member picker edits in four variants, old/new theme exchange and two restarts. Host defers omitted additive sentinels; explicit unreadable namespace remains rejected. m2-focused.json and m2-core-host.json bind exact candidate."}},{"criterion":"AC3","outcome":"pass","revision":"bc4b4aafe6f51020aa469c519e936d616e124e89","evidence":{"kind":"artifact","detail":"M1 inputs unchanged, all AC01-AC08 reconciled against immutable RFC_BASE_SHA. Owning appearance/architecture contracts and RFC/index updated; focused integration, format22, tidy and final documentation gate pass. m2-verification.json and m2-documentation.json retain results and scoped broader font-test limitation."}}]
---

# BPR-1412: Verify and close RFC-BPR-0072

RFC base: `6ebd33e57479a24aed91b930624bab030a436045`.

## Acceptance

- **AC1:** Composed offscreen comparison shows declarations and recognized qualified uses in both panes across light/dark, with genuine macros and audited high-contrast defaults (RFC AC06).
- **AC2:** Independent per-variant customization survives restart and exchange; older stored/imported palettes preserve all existing values and flags while defaulting new roles (RFC AC08).
- **AC3:** Reconcile every RFC criterion against the immutable baseline, update owning contracts and complete focused integration/documentation checks.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** Simple and nested namespace declarations, namespace aliases and `using namespace` receive the defined namespace treatment with identical roles for uppercase and mixed-case spellings; the reported `GCT` is not a macro. | Pass — accepted M1 public-classifier/cache evidence remains valid by source hashes; exact M2 public-classifier integration passes. See the RFC qualification ledger. |
| **AC02:** The supplied enum gives all four enumerator names the enum-member role. Scoped and unscoped declarations, multiline bodies and nested initializers distinguish names from expression operands, and classification resumes normally after the enum closes. | Pass — accepted M1 public-classifier/cache evidence remains valid by source hashes; exact M2 public-classifier integration passes. See the RFC qualification ledger. |
| **AC03:** A hunk beginning after the namespace/enum opener has exactly the roles projected from the complete blob. Lookalike text in comments, strings and directives does not alter that context. | Pass — accepted M1 public-classifier/cache evidence remains valid by source hashes; exact M2 public-classifier integration passes. See the RFC qualification ledger. |
| **AC04:** Existing macro invocations, unqualified standalone references, directive operands and predefined macros keep their roles, including a macro inside an enum initializer. Other language definitions retain their roles. | Pass — accepted M1 public-classifier/cache evidence remains valid by source hashes; exact M2 public-classifier integration passes. See the RFC qualification ledger. |
| **AC05:** Old identifier-rule cache keys cannot supply stale spans. Theme changes continue to reuse lexical classification. | Pass — accepted M1 public-classifier/cache evidence remains valid by source hashes; exact M2 public-classifier integration passes. See the RFC qualification ledger. |
| **AC06:** A composed offscreen comparison displays declaration and recognized use-site categories in both panes, including uppercase examples, across light and dark themes. New defaults meet the existing contrast floor, including high-contrast variants; a neighboring genuine macro retains its presentation. | Pass — exact composed layout/pixels in both panes Light/Dark, macro control, 374 contrast pairs in all four variants; m2-focused.json. |
| **AC07:** Qualified uses of namespaces and enum members declared in the blob receive their category roles. An unrelated identically named member is not recolored by global spelling, and the defined uncertain-use fallback keeps an unknown qualified uppercase value out of the macro role. | Pass — accepted M1 public-classifier/cache evidence remains valid by source hashes; exact M2 public-classifier integration passes. See the RFC qualification ledger. |
| **AC08:** Each new color can be customized independently per variant and survives restart and export/import. Loading an older palette or theme supplies both new defaults while preserving all existing customized values and stored states. | Pass — four-variant picker edits, old/new exchange and two fresh-process restarts; m2-focused.json and m2-core-host.json. All old values and stored flags checked. |

## Verification and scope

Product candidate: `bc4b4aafe6f51020aa469c519e936d616e124e89`.
The [qualification ledger](../../verification/RFC-BPR-0072.md) maps all eight
criteria and retains the scoped baseline comparison. [M2 verification](../_checkpoints/cpp-identifier-context-v1/m2-verification.json)
records exact inputs, five passing integration rows, Qt-free host correction,
format/tidy and documentation evidence. All required criteria are accepted;
no human/native/independent acceptance is required or inferred.

The host now defers omitted additive color sentinels while still refusing
explicit unreadable ink. Existing global theme contrast admission is preserved.
The broader profile suite's inherited font-resize reading-position assertion
failed before this repair and remains recorded outside the assigned scope.
No scrolling work was performed. Historical palette obligations remain intact.
