---
schema: "implement-rfc.card.v1"
id: "DRAFT-0006"
title: "Measure composed allocator behavior and preserve tool paths"
status: "Backlog"
rfcs: ["RFC-DRAFT-0028"]
depends_on: ["DRAFT-0002", "DRAFT-0003", "DRAFT-0004"]
---

# Measure composed allocator behavior and preserve tool paths

## Scope and acceptance

Run representative product workflows and allocator-boundary stress after
native interception is known (AC06). Verify sanitizer/fuzzer selection and
ordinary allocation-failure behavior. Compare matched Release workloads for
launch, large-review navigation and memory/close/prune on the same hardware
with the developer baseline switch (AC08). Re-run unchanged existing memory
budgets and keep BPR-0967's pre-existing failures distinct. Record conditions,
candidate identities, raw outcomes and limitations; no speedup is presumed.
