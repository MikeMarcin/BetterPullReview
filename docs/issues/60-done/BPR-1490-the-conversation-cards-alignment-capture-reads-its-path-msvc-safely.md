---
schema: "implement-rfc.card.v1"
id: "BPR-1490"
title: "The conversation cards alignment capture reads its path the MSVC-safe way"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1486--BPR-1491--162208d134c7.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) fails on main at cd4860e30, owner release request 2026-10-13","related_cards":["BPR-0519"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"2583d065b","evidence":{"kind":"command","command":"python3 tests/scripts/msvc_secure_crt_tests.py; ctest --preset dev -R ^msvc_secure_crt_contract$","detail":"getenv wrapped in the scoped _MSC_VER C4996 push/disable/pop used across tests/; contract passes (failed in the cd4860e30 baseline gate)."}},{"criterion":"AC2","outcome":"pass","revision":"2583d065b","evidence":{"kind":"command","command":"cmake --build --preset dev --target test_better_pull_review_qt_conversation_cards_view_tests; ctest --preset dev -R ^qt_conversation_cards_view$","detail":"Rebuilt from the changed source on macOS arm64; passes in 1.90 s."}}]
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

## Evidence

- Cause: the Discuss alignment capture read
  `std::getenv("BPR_DISCUSS_ALIGNMENT_PNG")` in an `if` init-statement with
  neither a scoped C4996 suppression nor a `_dupenv_s` branch, so
  `msvc_secure_crt_contract` refused it ([[BPR-0519]]).
- Fix: the read is hoisted into its own declaration wrapped in
  `#pragma warning(push)` / `disable : 4996` / `pop` under `_MSC_VER`, the
  form most of `tests/` uses (for example
  `tests/application/mcp_context_helper_tests.cpp`). Behaviour on macOS and
  Linux is unchanged. Test source only.
- `python3 tests/scripts/msvc_secure_crt_tests.py`: `Ran 3 tests in 0.718s`,
  `OK`.
- `python3 _coding-standard/scripts/format.py --root . --dry-run
  tests/adapters_qt/conversation_cards_view_tests.cpp` with clang-format
  22.1.8: exit 0.
- AC2 (the Qt test builds and passes on macOS) is left to the integrator's
  built tree; no build or test suite ran here while the release gate's
  performance rows were running.
- Integration at 2583d065b (main 87f00f7a3 merged), macOS arm64 build/dev: the Qt test executable rebuilt from the changed source; `ctest --preset dev -R '^(qt_conversation_cards_view|msvc_secure_crt_contract)$'` passes 2/2 (1.90 s, 0.80 s). The 0.2.0 baseline gate on cd4860e30 failed `msvc_secure_crt_contract` on this line.
