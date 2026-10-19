---
schema: "implement-rfc.card.v1"
id: "BPR-1619"
title: "Move pure diff and identifier computation into core"
status: "Ready"
rfcs: ["RFC-BPR-0082", "RFC-BPR-0072", "RFC-BPR-0081"]
depends_on: []
priority: "P1"
metadata: {"mandate_revision":"core-boundaries-r1","reservation":"docs/issues/_reservations/BPR-1619--BPR-1619--a6fd8340b17d.json","milestone_plan":{"id":"core-boundaries-v1","path":"docs/rfcs/plans/core-boundaries-v1/milestone-plan.json"}}
---

# Move pure diff and identifier computation into core

Owner, 2026-10-18: move xdiff into core while keeping its separate dynamic
library; move identifier refinement if modest; permit atomic/mutex with explicit
caller-owned/referenced state and `<filesystem>` for portable lexical path
manipulation, keeping filesystem access out of core. Mandate `core-boundaries-r1`.

## Acceptance

- **AC1:** Move pure xdiff matching and its provider-free interface into core;
  remove the adapter/target/wiring. Preserve separate dynamic linkage, packaging,
  line identities, whitespace mapping, memory and application-owned abandonment.
- **AC2:** Permit explicit referenced atomic/mutex state and lexical paths;
  reject filesystem access (including current directory, canonicalization and
  directory iteration). Add positive and negative boundary courses; no promise
  that platform-native path conventions are identical.
- **AC3:** Extract pure identifier refinement with modest value-contract cleanup;
  preserve syntax semantics/version, bounds and cancellation. Qt grammar,
  template helper and resources stay adapter-owned; no parser redesign.
- **AC4:** Focused core-only/real xdiff and public host/identifier courses,
  architecture/dependency/package, format22 and documentation gates pass.
  Historical RFC platform and other acceptance obligations remain separate.
