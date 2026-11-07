---
schema: "implement-rfc.card.v1"
id: "BPR-1901"
title: "Reduce repeated Windows compilation"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"milestone_plan":{"id":"windows-rebuild-latency-v1","path":"docs/rfcs/plans/windows-rebuild-latency-v1/milestone-plan.json"},"baseline":"d4c4272cd493abb0bf4be745738dfd66a3d6ba50","mandate":"Owner requested direct implementation, before/after measurements, unchanged required coverage, main integration and a one-line journal on 2026-11-06."}
---

# Reduce repeated Windows compilation

Investigate the repeated Windows compilation recorded during release candidate
retries and remove demonstrated redundant work. Use an isolated native checkout;
the separately paused release candidate and its historical evidence stay intact.

## Acceptance

- AC1: Identify the redundant work and measure a representative native Windows
  retry before and after with exact configuration, commands and work counts.
- AC2: Preserve required Release and Debug coverage, diagnostic options, source
  and artifact identity; real changed inputs still rebuild correctly.
- AC3: Pass focused native build/invalidation controls and relevant tooling checks.
- AC4: Land the verified change, completed card and one-line journal entry on main.

## Investigation

The retained candidate-21 Windows Debug build took 2,619.375 seconds. Its log
names nine compiled C++ sources twice (module scanning and compilation), not
eighteen separate compilations. The recorded process lifetimes put `link.exe`
on the wall-clock path for 2,301.28 seconds and `cl.exe` for 95.57 seconds;
those intervals overlap and are not additive. Removing implicit vcpkg input
is tracked separately by [[BPR-1903]] and did not reduce warm retry time.

A fresh native syntax dependency build then failed after 76.969 seconds:
parallel Visual Studio projects tried to restore the same CMake generation
stamp and reran configuration concurrently. The parent deliberately discards
an incomplete staging tree on retry, repeating completed compilation. The
correction is tracked by [[BPR-1904]]: the existing source/option signature
continues to own invalidation. Warm dependency retries did not get faster;
the separate optimization target is narrowing volatile build fingerprints to
the translation units that consume them, preserving their values and coverage.
