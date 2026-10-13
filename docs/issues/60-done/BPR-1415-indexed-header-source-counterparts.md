---
schema: "implement-rfc.card.v1"
id: "BPR-1415"
title: "Index closest header and source counterparts"
status: "Done"
rfcs: ["RFC-BPR-0074"]
depends_on: []
priority: "P1"
metadata: {"mandate_revision":"header-source-navigation-r4","milestone_plan":{"id":"header-source-navigation-v1","path":"docs/rfcs/plans/header-source-navigation-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9492bef2f732703779c59fa40190af95f7e695ce","evidence":{"kind":"observation","detail":"Core and public-host checks pass all suffixes/case behavior, exact stems, exact-directory priority, clarified src/inc/project suffix proximity, unique zero-overlap and equal-best no-op.","artifact":"docs/issues/_checkpoints/header-source-navigation-v1/m1-verification.json","command_evidence":["docs/issues/_checkpoints/header-source-navigation-v1/m1-focused.json","docs/issues/_checkpoints/header-source-navigation-v1/m1-format.json","docs/issues/_checkpoints/header-source-navigation-v1/m1-tidy.json","docs/issues/_checkpoints/header-source-navigation-v1/m1-documentation.json"]}},{"criterion":"AC2","outcome":"pass","revision":"9492bef2f732703779c59fa40190af95f7e695ce","evidence":{"kind":"observation","detail":"Public host query and ordinary exact selection reach a deduplicated withheld entry and resolve back. Removed/Unicode paths work; renamed PreviousPath and differently cased or unsupported stems are excluded. Unobserved and refused catalogues are unavailable.","artifact":"docs/issues/_checkpoints/header-source-navigation-v1/m1-verification.json","command_evidence":["docs/issues/_checkpoints/header-source-navigation-v1/m1-focused.json","docs/issues/_checkpoints/header-source-navigation-v1/m1-format.json","docs/issues/_checkpoints/header-source-navigation-v1/m1-tidy.json","docs/issues/_checkpoints/header-source-navigation-v1/m1-documentation.json"]}},{"criterion":"AC3","outcome":"pass","revision":"9492bef2f732703779c59fa40190af95f7e695ce","evidence":{"kind":"observation","detail":"Both 32- and 4096-entry catalogues deliberately share one basename. One hundred queries visit exactly 400 directory nodes in each, with no indexing, selected-diff reads or projection publications. Exact invalidation/replacement retires membership and candidates.","artifact":"docs/issues/_checkpoints/header-source-navigation-v1/m1-verification.json","command_evidence":["docs/issues/_checkpoints/header-source-navigation-v1/m1-focused.json","docs/issues/_checkpoints/header-source-navigation-v1/m1-format.json","docs/issues/_checkpoints/header-source-navigation-v1/m1-tidy.json","docs/issues/_checkpoints/header-source-navigation-v1/m1-documentation.json"]}}]
---

# BPR-1415: Index closest header and source counterparts

RFC base: `6a310fc57ae2d52360bfec252cf25aa1350d8aa2`. Owner amendment r4 settles Q1; no chooser or cycling.

## Acceptance

- **AC1:** Pure matching accepts the eight suffixes without ASCII extension case sensitivity, compares nonempty basenames exactly, prefers same-directory then longest trailing-directory agreement, and makes equal-best ambiguity a no-op.
- **AC2:** The workbench host indexes the complete accepted catalogue including withheld entries, deduplicates stable file identity, uses current/removed paths without rename aliases, and distinguishes unavailable from observed-empty.
- **AC3:** Repeated large-catalogue queries visit indexed directory nodes, read no blobs and publish no full file projection; lifecycle replacement/invalidation cannot reuse retired catalogue entries.
