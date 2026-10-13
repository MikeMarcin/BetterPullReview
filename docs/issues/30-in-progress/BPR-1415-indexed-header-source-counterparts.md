---
schema: "implement-rfc.card.v1"
id: "BPR-1415"
title: "Index closest header and source counterparts"
status: "In Progress"
rfcs: ["RFC-BPR-0074"]
depends_on: []
priority: "P1"
metadata: {"mandate_revision":"header-source-navigation-r4","milestone_plan":{"id":"header-source-navigation-v1","path":"docs/rfcs/plans/header-source-navigation-v1/milestone-plan.json"}}
---

# BPR-1415: Index closest header and source counterparts

RFC base: `6a310fc57ae2d52360bfec252cf25aa1350d8aa2`. Owner amendment r4 settles Q1; no chooser or cycling.

## Acceptance

- **AC1:** Pure matching accepts the eight suffixes without ASCII extension case sensitivity, compares nonempty basenames exactly, prefers same-directory then longest trailing-directory agreement, and makes equal-best ambiguity a no-op.
- **AC2:** The workbench host indexes the complete accepted catalogue including withheld entries, deduplicates stable file identity, uses current/removed paths without rename aliases, and distinguishes unavailable from observed-empty.
- **AC3:** Repeated large-catalogue queries visit indexed directory nodes, read no blobs and publish no full file projection; lifecycle replacement/invalidation cannot reuse retired catalogue entries.
