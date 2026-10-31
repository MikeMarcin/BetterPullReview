---
schema: "implement-rfc.card.v1"
id: "BPR-1836"
title: "Validate diagnostic record categories by their operation family"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-1828"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"release_issue":"BPR-1811","mandate":"release-v041-full-stable-N8-oracle-followup","external_state":"/Users/jaeger/.codex/release-runs/v0.4.1-20261029","frozen_release_source":"d9d4a822dbf603c7bb22c83574f5a64d2bee5645","integration":"Independent test-oracle follow-up only; frozen publisher, product sources, libraries and producers remain D9."}
---

# Validate diagnostic record categories by their operation family

The N8 macOS Release logging and capability-failure courses fail the shared
independent JSONL oracle's category assertion. It special-cases github.*
and otherwise requires the complete operation as category. The accepted
production vocabulary now includes agent.* and other dotted families; its
documented category is the first operation component. No producer correction
is authorized or implied by this stale test assumption.

The helper has four direct test consumers: application_logging_tests.cpp,
quill_logging_tests.cpp, diagnostic_controls_tests.cpp and
session_retention_tests.cpp. The terminal-diagnostic recovery subcourse uses
the first binary, but its historical child stderr is unavailable. That failure
remains ungrouped until a complete replay proves the cause.

## Acceptance

- AC1: Correct only the independent test category predicate to the documented
  operation-family rule. Preserve every complete-schema, UTC/time ordering,
  event identity, severity, build identity and privacy assertion.
- AC2: Add meaningful valid-family and wrong-category controls. Replay all
  affected real logging courses against exact unchanged D9 product libraries;
  retain original raw failures, child context and unreached assertions.
- AC3: Bind private native test builds to the original flags, compiler, headers,
  libraries and helpers with explicit input/output seals. Complete focused
  format/documentation checks and commit this separate future-main fix without
  pushing or changing the frozen publisher before stable compatibility is
  reconciled.
