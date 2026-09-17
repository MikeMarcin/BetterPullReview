---
schema: "implement-rfc.card.v1"
id: "BPR-0807"
title: "architecture_boundaries stops on three findings main never saw"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: []
priority: "P1"
metadata: {"found_by":"BPR-0666 neighbour selectors and the BPR-0806 critical review","found_at":"49566de8","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
acceptance: [{"criterion":"architecture_boundaries and architecture_boundary_contract pass on macOS at the fixing commit","outcome":"pass","revision":"a5667197","evidence":{"kind":"command","detail":"ctest --test-dir build/r2-dev -R ... serial integrated pass: architecture_boundaries 10.62 s pass, architecture_boundary_contract 72.69 s pass; build/r2-logs/int2-ctest.log"}},{"criterion":"The Accounts & hosts hint still tells a person not to type a scheme and its rendered course passes","outcome":"pass","revision":"a5667197","evidence":{"kind":"command","detail":"Hint reads 'without a scheme or a path'; qt_workbench_window_scale_100/125/200 (scheme refusal) and qt_surface_capture_course_scale_* (Accounts states) pass in int2-ctest.log"}},{"criterion":"BPR-0806's architecture_boundaries row can be checked at a head containing both","outcome":"pass","revision":"a5667197","evidence":{"kind":"observation","detail":"a5667197 contains the BPR-0806 merge c0f2fa31 and architecture_boundaries passes there"}}]
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

## Evidence, 2026-09-17 (run-20260916-stations, dispatch dsp-001-r2)

The fix is `a5667197`. A non-fatal full pass
(`build/r2-logs/bpr0807-baseline-all-findings.log`) showed **eight** findings,
not three. Five come from `cdf6175c` ("Bridge opted-in gh reads to bounded Git
objects", RFC-BPR-0015). None came from BPR-0797; the gate had been stopping
before it reached those rules.

| # | Finding | Decision |
|---|---|---|
| 1 | `https?://` in the Accounts & hosts hint | Harmless text reworded to "without a scheme or a path", matching the field's accessible description; no test asserts the string |
| 2 | `QDesktopServices` in a device authorization panel comment | Comment reworded |
| 3 | Production CLI factory no longer nullary | Source drift: `make_github_cli_process_runner()` restored exactly; the bridge's three sites use new `make_github_cli_git_acquisition_process_runner(file_limits)`, which refuses a zero limit, a relative store, an out-of-store spool and an existing spool |
| 4 | Worker one-owner rule did not admit the `FileLimits` value member | Rule out of date; amended with a reason (a value owns no worker) |
| 5 | File-redirected channel `setStandardOutputFile` | RFC-BPR-0015's bounded stdout spool admitted exactly once, only inside the factory's refusal body and followed by the `RLIMIT_FSIZE` child modifier, matched on comment-free code |
| 6 | `"PATH"` built in the Git bridge | Moved into the transport as `compile_gh_cli_local_git_request`; fields and order unchanged (verified field by field in review) |
| 7 | `".git"` literal in the bridge | Shared `gh_cli_git_clone_store(root, index)` |
| 8 | Four pre-existing clang-format lines in `workbench_window.cpp` (present at `53a0a747`) | Formatted |

**Independent critical review** found no blocker. Its two should-fix items were
fixed before commit. First, rule 5's guards could be satisfied by comments,
dead code or uncalled functions: they are now span-anchored, with negatives for
each of those shapes plus a deleted refusal. Second, the acquisition factory's
refusals were untested: `test_git_acquisition_factory_refuses_unbounded_stores`
was added, and removing the zero-limit refusal makes it fail. The review's
notes are hardening candidates, not defects:

- no rule constrains `github_cli_process_file_limits`' members;
- `RLIMIT_FSIZE` is Unix-only, while the bridge is macOS-only at run time;
- there is a same-user TOCTOU on the spool path inside a 0700 temporary
  directory, pre-existing.

These rule changes are security-relevant for [[BPR-0788]] (RFC-BPR-0015
closeout).

**Integrated pass at `a5667197`** (`build/r2-logs/int2-ctest.log`, serial):
39 of 40 selected tests pass, including:

- `architecture_boundaries` (10.6 s) and `architecture_boundary_contract` (72.7 s);
- `github_cli_process_runner`, `gh_cli_transport` and `gh_cli_git_smart_transport`;
- `qt_workbench_window_scale_*`, `qt_surface_capture_course_scale_*`,
  `qt_settings_window*`, `qt_composed_github_scale_*` and
  `qt_device_authorization_scale_*`.

The only failure, `test_network_isolation_manifest`, reports exactly
[[BPR-0802]]'s `gh_cli_git_smart_transport` and raw `add_test` findings.
`application_device_authorization_panel_course` fails independently of this
card: it also fails at a clean `53a0a747` worktree. It is [[BPR-0815]].
