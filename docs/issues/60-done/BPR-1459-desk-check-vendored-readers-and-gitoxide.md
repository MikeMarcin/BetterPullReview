---
schema: "implement-rfc.card.v1"
id: "BPR-1459"
title: "Record the vendored-piece and gitoxide desk checks and cost records"
status: "Done"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1454"]
priority: "P2"
platforms: ["mac"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
acceptance: [{"criterion":"BPR-1459:AC1","outcome":"pass","revision":"f3a351729","evidence":{"kind":"observation","detail":"V: wildmatch.c's header (GPL-2.0 with linking exception) and COPYING lines 1074-1098 (Rich Salz's BSD-style terms with a binary acknowledgement) both bind; not public domain. tool_wildmatch_differential (evaluation-only) agrees with core::wildcard_match on 7,000,000 pairs in the C locale with and without folding; under en_US.UTF-8 it disagrees on Latin-1 bytes (486 exact and 1,096 folded per million) and it is exponential on star-heavy patterns. Recorded as not eligible as is (H6) and at best eligible pending the owner's approval of that code plus an abort fix and ASCII-only classes; no reftable source exists to vendor.","path":"docs/issues/60-done/BPR-1459-desk-check-vendored-readers-and-gitoxide.md"}},{"criterion":"BPR-1459:AC2","outcome":"pass","revision":"f3a351729","evidence":{"kind":"observation","detail":"G's desk check records MIT OR Apache-2.0, a second Rust toolchain on three platforms, a BPR-maintained FFI with no type crossing, a crate graph needing per-crate SPDX/notices/corresponding source, fuzzing outside the CMake and ctest presets, and the rejection reasons; nothing downloaded or linked.","path":"docs/issues/60-done/BPR-1459-desk-check-vendored-readers-and-gitoxide.md"}}]
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

Done on `f3a351729` (macOS); evidence in the
[verification ledger](../../verification/RFC-BPR-0083.md#bpr-1459-candidates-v-and-g).
