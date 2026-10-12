---
schema: "implement-rfc.card.v1"
id: "BPR-1411"
title: "Recognize C++ namespace and enum context"
status: "In Progress"
rfcs: ["RFC-BPR-0072"]
depends_on: []
priority: "P1"
metadata: {"mandate_revision":"cpp-identifier-context-r1","milestone_plan":{"id":"cpp-identifier-context-v1","path":"docs/rfcs/plans/cpp-identifier-context-v1/milestone-plan.json"}}
---

# BPR-1411: Recognize C++ namespace and enum context

RFC base: `6ebd33e57479a24aed91b930624bab030a436045`.

## Acceptance

- **AC1:** Namespace declarations, aliases and using directives have namespace roles across capitalization (RFC AC01).
- **AC2:** Scoped and unscoped enumerators are distinguished from nested initializer expressions and following code (RFC AC02).
- **AC3:** Whole-blob and hunk roles agree, with grammar-protected comments, strings and directives (RFC AC03).
- **AC4:** Existing macro conventions and other language roles remain intact (RFC AC04).
- **AC5:** Old identifier cache keys are retired and theme changes reuse classification (RFC AC05).
- **AC6:** Scoped declaration evidence recognizes qualified uses conservatively, with neutral unknown terminal names and no global spelling propagation (RFC AC07).
