---
schema: "implement-rfc.card.v1"
id: "BPR-1459"
title: "Record the vendored-piece and gitoxide desk checks and cost records"
status: "In Progress"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1454"]
priority: "P2"
platforms: ["mac"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
---

# BPR-1459: Record the vendored-piece and gitoxide desk checks and cost records

Candidates V and G of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md). Under the owner's Q1
answer, a vendored matcher or reftable reader in core is eligible only pending
the owner's approval of that code; it may be measured as a test-only oracle.

It needs only [[BPR-1454]] to start; V's oracle measurement runs in the harness or fuzz target once [[BPR-1455]] or [[BPR-1457]] lands.

## Acceptance criteria

- **BPR-1459:AC1:** V's cost record determines the license of libgit2's `wildmatch.c` from its header and COPYING, measures a reference wildmatch as a test-only oracle in the harness or fuzz target, and records its eligibility pending owner approval.
- **BPR-1459:AC2:** G's desk check records its license, toolchain, FFI, crate-graph and packaging costs and the reasons it is rejected; nothing is linked.

## Continuation

Imported 2026-10-13 under mandate `git-metadata-readers-r1`; nothing is measured or accepted yet.
