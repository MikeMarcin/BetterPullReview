---
schema: "implement-rfc.card.v1"
id: "BPR-0640"
title: "Finish Glaze test helpers and migration integration"
status: "Ready"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0639"]
---

# Finish Glaze test helpers and migration integration

## Bounded acceptance and ownership

Own the remaining repository-owned C++ test/helper Qt JSON inventory, associated
test target Glaze links and final migration bookkeeping. Replace helpers with
Glaze and retain independently authored raw malformed-input fixtures and literal
canonical-byte expectations. Audit src/tests for all Qt JSON entry points; only
intentional documentation and boundary guards may remain. Run one focused
integration pass covering migrated auth/read/pagination/mutation/recovery and
public composition; refresh affected parser-sensitive evidence before resuming
performance work. Record platform limitations without reopening unrelated RFC
acceptance. No Python/CMake JSON tooling or third-party migration.

Migration anchor: BPR-0612. Migration base `4e6a6013f3589711f614fba7b9c46b8967313796`.
Preserve core/host boundaries and pinned dependency provenance. All execution
is local with synthetic fixtures; no Actions, push, paid CI or production
account/process operations. Full migration acceptance remains on BPR-0612.
