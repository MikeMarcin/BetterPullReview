---
schema: "implement-rfc.card.v1"
id: "BPR-1836"
title: "Validate diagnostic record categories by their operation family"
status: "In Progress"
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

## Prepared correction

The test helper now independently compares category with the first operation
component. The production vocabulary's 23 operation names are listed explicitly
in focused controls with their expected families, including the newer agent,
credential, updater, storage, workspace, search, image, notification, tour and
cycle families. Every case rejects a wrong category, and each dotted operation
also rejects its entire operation name as category. This does not call the
production category helper or change any other JSON record predicate.

Native builds and affected full public courses remain pending. Historical
failures are not classified as recovered by this source change alone.

The original failed category assertion prints no operation/category values, so
the specific first offending family is unknown. The earlier terminal wrapper
also discarded its child's stderr and exact exit value. Neither historical gap
is reconstructed or attributed to agent events; the complete documented
first-component rule and all 23 operation controls bind the correction.
