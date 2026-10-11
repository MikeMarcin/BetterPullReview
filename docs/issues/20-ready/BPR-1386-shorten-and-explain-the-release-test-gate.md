---
schema: "implement-rfc.card.v1"
id: "BPR-1386"
title: "Shorten and explain the release test gate"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"corrective","origin":"Owner requests optimization and an explanation of the 74-minute macOS release suite, 2026-10-10","reservation":"docs/issues/_reservations/BPR-1386--BPR-1387--aad26e4d7bc0.json","milestone_plan":{"id":"bpr1386-release-test-suite","path":"docs/rfcs/plans/bpr1386-release-test-suite/milestone-plan.json"}}
---

# Shorten and explain the release test gate

## Mandate and baseline

The owner asks where the time goes, whether redundant tests can be removed, and
for corrective cards worked directly in this chat. The retained 0.1.7 run is
685 sequential CTest rows, 4,450.03 seconds, eight failures and one configured
skip. The 20 slowest rows consume 2,136.64 seconds; all 98 core rows consume
68.21 seconds. Compilation before CTest is additional time.

The composition contract creates and destroys a complete tests-disabled Release
build on every invocation. Log-age coverage sleeps for three real minutes.
The release script gives the full suite no explicit scheduling or timing report.
Interactive Qt courses must stay headless and serial, and performance evidence
must not be taken while competing test work perturbs it.

## Acceptance

- AC1: Produce a reproducible timing breakdown from actual CTest results, separating wall time, summed test time, slow rows, failures and skips; document the measured baseline and optimized run.
- AC2: Remove avoidable repeated setup or safely overlap independent checks while preserving the registered coverage, isolation, resource locks, serial interactive Qt courses and performance gates. Any deleted or consolidated coverage has an explicit equivalent check.
- AC3: Wire the optimized procedure into the existing macOS release path and document focused development versus complete release verification. A failed test still stops the release gate; no blanket exclusions or automatic acceptance of failures.
- AC4: Run focused regressions for scheduling/setup/reporting and one complete native integration pass. Record actual elapsed time, cold/warm build assumptions, failures and configured skips without claiming native evidence for other hosts.

## Scope

Keep the existing release producer and test isolation boundary. No signing,
publication, live GitHub mutation, desktop driving or new owner acceptance is
needed. BPR-1387 owns the observed test failures. Native release qualification
and unrelated in-progress cards retain their existing obligations.
