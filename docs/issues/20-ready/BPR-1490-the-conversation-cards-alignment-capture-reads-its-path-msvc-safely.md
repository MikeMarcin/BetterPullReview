---
schema: "implement-rfc.card.v1"
id: "BPR-1490"
title: "The conversation cards alignment capture reads its path the MSVC-safe way"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1486--BPR-1491--162208d134c7.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) fails on main at cd4860e30, owner release request 2026-10-13","related_cards":["BPR-0519"]}
---

# The conversation cards alignment capture reads its path the MSVC-safe way

`msvc_secure_crt_contract` fails on main:
`tests/adapters_qt/conversation_cards_view_tests.cpp:1784` calls
`std::getenv("BPR_DISCUSS_ALIGNMENT_PNG")` without the scoped C4996
suppression or a `_dupenv_s` branch that [[BPR-0519]] requires, so MSVC
refuses it under warnings as errors.

## Acceptance

- AC1: The read uses the repository's established MSVC-safe environment
  pattern, and `msvc_secure_crt_contract` passes.
- AC2: The conversation cards Qt tests still build and pass on macOS.

## Boundaries

Test source only.
