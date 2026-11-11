---
schema: "implement-rfc.card.v1"
id: "BPR-2104"
title: "Disambiguate the profile reset fixture's retained identity binding"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2104--BPR-2104--9d8fb20461e1.json","source_baseline":"70324127b116e45988b8ffa3a77370209ba7aef4","mandate_revision":"release58-conditional-identifier-r1","milestone_plan":{"id":"bpr-2104-profile-reset-identifier","path":"docs/rfcs/plans/bpr-2104-profile-reset-identifier/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"cee8fe23fd5c8d7e71efa1e178b4096c837709eb","evidence":{"kind":"artifact","detail":"Exact original Windows58 failure retained/hash-bound. Original file reproduces three Windows-model findings at126/127/131; corrected file none; three-token reverse substitution restores originalbytes."}},{"criterion":"AC2","outcome":"pass","revision":"cee8fe23fd5c8d7e71efa1e178b4096c837709eb","evidence":{"kind":"artifact","detail":"All2175sourcefiles pass unchanged Windows/Darwin/Linux gate models; unchanged26gate contracts pass and planted stale-name still found. Format22/diffcheck/documentation gate/67documentation contracts pass; no native claim."}},{"criterion":"AC3","outcome":"pass","revision":"cee8fe23fd5c8d7e71efa1e178b4096c837709eb","evidence":{"kind":"artifact","detail":"Canonicalsource31fc9e391 and proof63596c536 pushed/remoteverified, exact TU0bce35b96123 matches proof. Foreign2103 preserved onlycanonical, not release slice. Root retains original58qualification and owns scoped bridge/newcohort. All own commands terminal; no native/producer writes."}}]
---

# Disambiguate the retained identity

Windows58's source gate reports three uses of the valid POSIX structured-binding local `before` as stale spellings of the map parameter `aBefore`. The lexical gate does not recognize this binding declaration. macOS compiles the branch and does not police it.

Rename only that local and its two uses to `storeIdentity`. Keep the scanner, retained descriptors, reset gestures, assertions, conditions and budgets unchanged. No production or configuration change is authorized.

## Acceptance

- AC1: Preserve the actual Windows58 failure and demonstrate the exact original file's three Windows-model findings, then zero for the corrected file.
- AC2: Existing gate contract tests and planted stale-name controls still reject real errors; current-source platform models and scoped format/documentation checks pass.
- AC3: Commit the exact three-identifier correction through main, preserve foreign work and counters, and hand back source/evidence identities and a scoped release-evidence bridge proposal without claiming new native qualification.
