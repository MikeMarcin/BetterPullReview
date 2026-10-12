---
schema: "implement-rfc.card.v1"
id: "BPR-1412"
title: "Verify and close RFC-BPR-0072"
status: "Ready"
rfcs: ["RFC-BPR-0072"]
depends_on: ["BPR-1411"]
priority: "P1"
metadata: {"mandate_revision":"cpp-identifier-context-r1","milestone_plan":{"id":"cpp-identifier-context-v1","path":"docs/rfcs/plans/cpp-identifier-context-v1/milestone-plan.json"}}
closeout_for: ["RFC-BPR-0072"]
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
| **AC01:** Simple and nested namespace declarations, namespace aliases and `using namespace` receive the defined namespace treatment with identical roles for uppercase and mixed-case spellings; the reported `GCT` is not a macro. | Pending qualification. |
| **AC02:** The supplied enum gives all four enumerator names the enum-member role. Scoped and unscoped declarations, multiline bodies and nested initializers distinguish names from expression operands, and classification resumes normally after the enum closes. | Pending qualification. |
| **AC03:** A hunk beginning after the namespace/enum opener has exactly the roles projected from the complete blob. Lookalike text in comments, strings and directives does not alter that context. | Pending qualification. |
| **AC04:** Existing macro invocations, unqualified standalone references, directive operands and predefined macros keep their roles, including a macro inside an enum initializer. Other language definitions retain their roles. | Pending qualification. |
| **AC05:** Old identifier-rule cache keys cannot supply stale spans. Theme changes continue to reuse lexical classification. | Pending qualification. |
| **AC06:** A composed offscreen comparison displays declaration and recognized use-site categories in both panes, including uppercase examples, across light and dark themes. New defaults meet the existing contrast floor, including high-contrast variants; a neighboring genuine macro retains its presentation. | Pending qualification. |
| **AC07:** Qualified uses of namespaces and enum members declared in the blob receive their category roles. An unrelated identically named member is not recolored by global spelling, and the defined uncertain-use fallback keeps an unknown qualified uppercase value out of the macro role. | Pending qualification. |
| **AC08:** Each new color can be customized independently per variant and survives restart and export/import. Loading an older palette or theme supplies both new defaults while preserving all existing customized values and stored states. | Pending qualification. |
