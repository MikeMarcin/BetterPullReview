---
schema: "implement-rfc.card.v1"
id: "BPR-1469"
title: "Qualify the adopted diff stack on Windows"
status: "Backlog"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1467", "BPR-1485", "BPR-1481", "BPR-1480"]
priority: "P2"
platforms: ["win"]
metadata: {"mandate_revision":"diff-engine-evaluation-r4","rescoped":"2026-10-13: owner delegated the decision (\"decide\"); the coordinating session rescoped candidate measurement to qualification of the adopted stack","original_title":"Measure the diff-engine candidates on Windows"}
---

# BPR-1469: Qualify the adopted diff stack on Windows

Part of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md).

## Rescope, 2026-10-13

This card first ran the harness of
[BPR-1466](../60-done/BPR-1466-opt-in-diff-engine-evaluation-harness.md) on the
Windows station, measuring every candidate against the first-party baseline.
That comparison is moot: the macOS decisions adopted linked xdiff
([BPR-1485](../60-done/BPR-1485-adopt-linked-xdiff-as-the-primary-line-matcher.md)),
dtl intraline ([BPR-1481](../60-done/BPR-1481-compute-intraline-ranges-with-dtl.md))
and rapidfuzz-cpp pairing
([BPR-1480](../60-done/BPR-1480-compute-line-pair-similarity-with-rapidfuzz.md)),
the first-party primary matcher is deleted, and the opt-in evaluation build is
retired. Asked whether to keep or rescope this card, the owner answered
"decide" on 2026-10-13; the coordinating session decided to rescope it to
qualifying the adopted stack on Windows. The
[owner delegation](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md#owner-delegation-on-linux-and-windows-qualification--2026-10-13)
is recorded in the RFC. The original acceptance is kept below, superseded.

## Acceptance

- **AC1:** On Windows, xdiff builds only as its own shared library
  (`bpr_xdiff.dll`) and the application links it dynamically; dtl and
  rapidfuzz-cpp build as pinned header-only libraries linked only by the core.
  The platform's toolchain and hardware are recorded.
- **AC2:** The diff suites (`core_diff_*`, `core_review_comment_range`,
  `core_review_round_trip`, `core_review_cycle_*`, `application_diff_host`) and
  both architecture-boundary checks pass on Windows.
- **AC3:** The BPR-1485 equivalence tool
  (`tests/evaluation/xdiff_equivalence`, `compare_with_l_myers_indent.py`)
  run over the evaluation corpus reproduces the macOS digests in
  `docs/verification/RFC-BPR-0082/macOS-L-raw.json` for every pair (segments,
  pairs and intraline), showing cross-platform determinism, and reports GitHub
  hunk agreement 78 of 78.
- **AC4:** The package and release audits pass on Windows:
  `package_runtime_contract`, `dependency_modernization_contract`,
  `release_package_audit_contract`, `release_security_audit_contract`,
  `runtime_audit_contract` and `binary_dependency_audit_contract`, and the
  platform's package carries the xdiff shared library with its LGPL notice,
  offer and corresponding source.

## Superseded acceptance (before the 2026-10-13 rescope)

- **AC1:** Windows results for every size class and every measurement item,
  including P against Git 2.39 and the current release and stop-to-return with
  process kill and reap, are committed beside the macOS results, with the
  baseline in the same runs and the hardware recorded.
- **AC2:** Cross-platform determinism is checked against the macOS outputs, and
  the report's gate table and decisions are updated with the Windows results.
