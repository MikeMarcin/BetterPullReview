---
schema: "implement-rfc.card.v1"
id: "BPR-1835"
title: "Keep cancellation diagnostics in the composed agent oracle"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-1827"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"release_issue":"BPR-1811","mandate":"release-v041-full-stable-N8-oracle-followup","external_state":"/Users/jaeger/.codex/release-runs/v0.4.1-20261029","frozen_release_source":"d9d4a822dbf603c7bb22c83574f5a64d2bee5645","integration":"Separate future-main test-oracle repair; frozen publisher, source and producers remain D9.","frontend_design":"Existing frontend-design guidance loaded; no product words, controls, layout or policy change."}
---

# Keep cancellation diagnostics in the composed agent oracle

N8 macOS Release records a failed cancellation public course. The expected
cancelled status has arrived, but the old empty-output assertion rejects a
53-byte log before the workspace-cleanup wait is entered. BPR-1827 deliberately
shows a bounded diagnostic summary for a stopped run that wrote stdout. This
course first requires the canary to write, so empty output is no longer its
accepted public contract. The raw failure and its unreached cleanup checks
remain preserved; the immediate cleanups=0 snapshot is not a cleanup timeout.

## Acceptance

- AC1: Correct only the test's cancellation-output oracle to require the accepted
  bounded diagnostic summary without raw canary/private content. Keep staleness
  output expectations and production behavior unchanged, and add meaningful
  positive and hostile controls for the corrected predicate.
- AC2: Replay the complete public cancellation contract against the exact
  unchanged product/helper inputs and original budgets. Retain typed terminal
  state, descendant death, exactly one workspace cleanup, quiescence and shutdown
  assertions; no earlier failure or unreached assertion receives passing credit.
- AC3: Bind any private native test build to the original configuration, compiler,
  headers, libraries and helper hashes, with only its test translation unit,
  object and output paths differing. Complete focused format/documentation
  checks and leave a separately reviewable future-main commit, unpushed until
  stable publication compatibility is reconciled.

Terminal diagnostics is a separate unresolved failure, outside this card.

## Prepared correction

The cancellation predicate accepts only the bounded stopped-run summary with a
positive stdout count, zero stderr bytes and an optional integer exit code.
It rejects empty output, zero-byte claims, nonzero stderr, raw canary output,
private text before or after the summary and an overlong count. Two valid and
seven hostile cases execute in the cancellation course. Staleness still requires
empty output. All existing cleanup, descendant-stop and shutdown waits remain
unchanged, including their original budgets.

This is prepared test source, not yet native acceptance. Private compilation
and complete public replay remain pending; no original failed or unreached
check is marked passed.
