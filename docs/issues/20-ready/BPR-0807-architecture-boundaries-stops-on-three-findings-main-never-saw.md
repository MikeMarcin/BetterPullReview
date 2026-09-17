---
schema: "implement-rfc.card.v1"
id: "BPR-0807"
title: "architecture_boundaries stops on three findings main never saw"
status: "Ready"
rfcs: ["RFC-BPR-0032"]
depends_on: []
priority: "P1"
metadata: {"found_by":"BPR-0666 neighbour selectors and the BPR-0806 critical review","found_at":"49566de8","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0807: architecture_boundaries stops on three findings main never saw

## Defect

`architecture_boundaries` fails on `main` in every tree on every platform.
It stops at the first finding, so earlier failures hid the rest. bpr-win saw
the first finding identically at `f2b8e682`, and the BPR-0806 critical review
masked each finding in turn in a scratch copy to reach the next. At
`49566de8` and `c0f2fa31` there are three:

1. `Provider data crossed the core/Qt boundary in src/adapters/qt/src/workbench_window.cpp: https?://`
   (`cmake/VerifyArchitectureBoundaries.cmake:1043`, the
   `provider_free_projection_patterns` list). The match is the user-facing
   hint `Enter a hostname such as github.example.com, without https:// or a path.`
   at `workbench_window.cpp:3403`, last changed in `ed2b3f14`.
2. `Qt adapter effect-boundary violation in src/adapters/qt/src/device_authorization_panel.cpp: QDesktopServices`
   (`VerifyArchitectureBoundaries.cmake`, `qt_forbidden_patterns`). The match
   is a comment naming `QDesktopServices::openUrl` at
   `device_authorization_panel.cpp:105`, added by `b1f89533` ([[BPR-0797]]).
3. `GitHub CLI production factory must remain nullary while the explicit test factory and asynchronous worker retain shared seam ownership`
   (`VerifyArchitectureBoundaries.cmake:3478`), a source-shape rule over the
   GitHub CLI process runner that `b1f89533`'s run-time factory move
   plausibly changed. This is not yet confirmed.

The nested `.claude/worktrees` checkout that used to fail the gate first was
removed from the tree in this run, which is why these are now visible.

## Scope

- For each finding, decide whether the source crossed the boundary or the
  rule matched text that carries no provider data or effect. Fix the source in
  the first case. In the second, change the text rather than the rule unless
  the rule is demonstrably wrong. Rewording a hint or a comment is cheaper and
  keeps the gate's literal patterns meaningful.
- For finding 3, read the rule against the current
  `make_github_cli_process_runner` / `_for_test` declarations and
  definitions. Restore the shape the rule protects, or amend the rule in the
  same commit with the reason, if [[BPR-0797]]'s run-time move legitimately
  changed it. Do not weaken the nullary-production-factory property.
- Keep going until the gate reaches its end. Fix any further findings it then
  reports, or card them if they are unrelated.
- Owned paths: the three sources above, and
  `cmake/VerifyArchitectureBoundaries.cmake` only where a rule is
  demonstrably wrong.

## Acceptance

- `architecture_boundaries` and `architecture_boundary_contract` pass on
  macOS at the fixing commit.
- The Accounts & hosts hint still tells a person not to type a scheme, and its
  rendered course still passes.
- [[BPR-0806]]'s acceptance row "architecture_boundaries passes" can then be
  checked at a head that contains both.
