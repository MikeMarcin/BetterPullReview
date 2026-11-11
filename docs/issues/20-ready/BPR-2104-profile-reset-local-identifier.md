---
schema: "implement-rfc.card.v1"
id: "BPR-2104"
title: "Disambiguate the profile reset fixture's retained identity binding"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2104--BPR-2104--9d8fb20461e1.json","source_baseline":"70324127b116e45988b8ffa3a77370209ba7aef4","mandate_revision":"release58-conditional-identifier-r1","milestone_plan":{"id":"bpr-2104-profile-reset-identifier","path":"docs/rfcs/plans/bpr-2104-profile-reset-identifier/milestone-plan.json"}}
---

# Disambiguate the retained identity

Windows58's source gate reports three uses of the valid POSIX structured-binding local `before` as stale spellings of the map parameter `aBefore`. The lexical gate does not recognize this binding declaration. macOS compiles the branch and does not police it.

Rename only that local and its two uses to `storeIdentity`. Keep the scanner, retained descriptors, reset gestures, assertions, conditions and budgets unchanged. No production or configuration change is authorized.

## Acceptance

- AC1: Preserve the actual Windows58 failure and demonstrate the exact original file's three Windows-model findings, then zero for the corrected file.
- AC2: Existing gate contract tests and planted stale-name controls still reject real errors; current-source platform models and scoped format/documentation checks pass.
- AC3: Commit the exact three-identifier correction through main, preserve foreign work and counters, and hand back source/evidence identities and a scoped release-evidence bridge proposal without claiming new native qualification.
