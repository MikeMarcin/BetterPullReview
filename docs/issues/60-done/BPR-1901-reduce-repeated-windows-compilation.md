---
schema: "implement-rfc.card.v1"
id: "BPR-1901"
title: "Reduce repeated Windows compilation"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"milestone_plan":{"id":"windows-rebuild-latency-v1","path":"docs/rfcs/plans/windows-rebuild-latency-v1/milestone-plan.json"},"baseline":"d4c4272cd493abb0bf4be745738dfd66a3d6ba50","mandate":"Owner requested direct implementation, before/after measurements, unchanged required coverage, main integration and a one-line journal on 2026-11-06."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9d4654594fd8ce796b582eb02c492ccfa8b74358","evidence":{"kind":"observation","detail":"Native paired fingerprint-only compilation retries: seven objects to two; Release 35.433/35.255s to 4.927/4.902s; Debug 31.214/31.078s to 4.936/10.025s. Exact commands, configuration and phase limits retained.","artifact":"docs/verification/windows-build-latency.md"}},{"criterion":"AC2","outcome":"pass","revision":"9d4654594fd8ce796b582eb02c492ccfa8b74358","evidence":{"kind":"observation","detail":"Both 906-row native test inventories are identical; seven existing compiler commands in each configuration change only the relocated definition. Real Windows source changes rebuild; actual built metadata matches generated values and independently recomputed attended-source digest.","artifact":"docs/verification/windows-build-latency.md"}},{"criterion":"AC3","outcome":"pass","revision":"9d4654594fd8ce796b582eb02c492ccfa8b74358","evidence":{"kind":"observation","detail":"Native Debug/Release logging builds and six functional/policy courses each pass. Mac Release app links and five public-path/logging courses pass; architecture contracts, format22, focused tidy22 with warnings as errors and documentation pass.","artifact":"docs/verification/windows-build-latency.md"}},{"criterion":"AC4","outcome":"pass","revision":"9d4654594fd8ce796b582eb02c492ccfa8b74358","evidence":{"kind":"observation","detail":"Completed card, one-line journal and measured verification record accompany the source commits for the authorized main fast-forward and push; stop after this integration.","artifact":"docs/verification/windows-build-latency.md"}}]
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

## Result

Source `9d4654594` moves the changing app and diagnostic fingerprints into two
small private translation units. The digest algorithms and release-cohort
override stay intact, and the new app files join the attended source manifest.
Constant initialization preserves lifetime without adding allocation or startup
work. The logger's records and export capture are checked for the same identity.

Two native Windows retries at baseline `04c3f2212` and candidate `9d4654594`
measure seven compiled objects becoming two: Release 35.433/35.255 seconds becomes
4.927/4.902, and Debug 31.214/31.078 becomes 4.936/10.025. These are compilation
phase timings, excluding configuration and linking. A changed Windows source
still recompiles, both configurations retain identical 906-row test inventories,
and all seven existing compiler commands differ only by removing the relocated
fingerprint definition. The [verification record](../../verification/windows-build-latency.md)
holds the commands, measurements and evidence scope. The owner asked to stop
after landing this fingerprint correction; no further optimization is included.

Native Debug/Release metadata probes match the generated definitions and an
independently computed attended source digest; logging builds and six functional
or policy courses pass in each configuration. The macOS Release app links and
passes its offscreen public path and four logging courses. Architecture,
clang-format 22, focused clang-tidy 22 and documentation checks pass. Full
release-cycle elapsed time and complete release qualification remain unmeasured.
