---
schema: "implement-rfc.card.v1"
id: "BPR-0638"
title: "Migrate GitHub read and pagination JSON to Glaze"
status: "In Progress"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0637"]
---

# Migrate GitHub read and pagination JSON to Glaze

## Bounded acceptance and ownership

Own github_read_contract.cpp, github_read_normalizer.cpp and github_paged_read.cpp
under src/adapters/github/src, plus corresponding contract/provider tests.
Replace all Qt JSON usage with Glaze, favoring typed wire records. Preserve
nullable/unknown semantics, exact integers, bounded full-input validation,
canonical request bytes, authority and pagination exhaustion, atomic failures,
rate resource tuples and sanitized diagnostics. Verify anonymous/authenticated
reads, paging/provider and the public application read path with stored fixtures.

Migration anchor: BPR-0612. Migration base `4e6a6013f3589711f614fba7b9c46b8967313796`.
Preserve core/host boundaries and pinned dependency provenance. All execution
is local with synthetic fixtures; no Actions, push, paid CI or production
account/process operations. Full migration acceptance remains on BPR-0612.
