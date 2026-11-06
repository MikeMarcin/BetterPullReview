---
schema: "implement-rfc.card.v1"
id: "BPR-1895"
title: "Show Overview checks before remaining status"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1893"]
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1895--BPR-1895--a728bb8ad4fb.json","milestone_plan":{"id":"responsiveness-latency-v9","path":"docs/rfcs/plans/responsiveness-latency-v9/milestone-plan.json"},"frontend_design":"frontend-design:frontend-design and desktop guidance applied; retain the existing Checks section and partial loading plate."}
---

# BPR-1895: Show Overview checks before remaining status

The owner asked for Overview's Status tab to load faster when opening a PR,
allowing fast sections to appear before slow sections with explicit loading.
Continue direct implementation, preserve other behavior, record one journal
line and land each verified round on main. Work resumes from rewritten history.

The provider already has complete check runs and commit statuses before it reads
merge assessment, pending reviewers and branch policy. Overview currently waits
for the entire snapshot before showing any of those results. Publish the check
results independently, without changing request order or spending extra requests.

## Acceptance

- AC1: An observed authenticated first open publishes complete check results
  before remaining merge/reviewer details. Refresh, signed-out and unobserved
  reads retain their behavior; request counts, ordering, shared bounds,
  optional-failure handling and final snapshot validation are preserved.
- AC2: Core/host accept one bounded check observation after metadata only for
  the exact pending read. Check shape, head, identities and text/record bounds
  are validated; duplicate, stale, cancelled and terminal observations are
  refused. This publishes no accepted snapshot, epoch or mutation authority.
- AC3: In the composed offscreen app, Status shows selectable check results
  while merge assessment is held, with remaining status clearly loading.
  Check details stay readable, final acceptance retains selection/disclosure,
  and superseded or failed opens clear provisional results. The old-reader
  control fails the early-results assertion.
- AC4: Measure optimized before/after latency with equal request counts; pass
  focused core/host/provider/Qt/public-path and architecture/coding/documentation
  checks, add one journal line and land the verified card on main.

## Plan

Reuse the overview observation channel with one independent check collection;
project it with the existing check-result policy and no merge-policy claims.
Reuse the current Checks section with a partial loading plate for the remaining
Status data. Browser actions keep waiting for the accepted review authority.
Exercise the provider, exact host lifecycle, widget state and actual window
interaction. No layout redesign, new worker or transport concurrency is needed.

## Implementation

The provider publishes completed check runs and commit statuses immediately after
both collections finish, before merge assessment, reviewer requests and branch
policy. This is one additional bounded observation on the existing serialized
lane; requests, ordering and final snapshot validation remain unchanged.

Core admission requires the exact pending window/read after metadata and permits
checks once, independently of the file catalogue. The host validates availability,
head, record identity, record/text bounds and shape before projecting results with
the existing check-result policy. Early results grant no snapshot, epoch or link
authority; required-check classifications explicitly remain loading.

Overview reuses the Checks section and partial loading plate. Results can be
expanded and selected while reviewers, merge rules and progress still load.
The selected check and disclosure survive final acceptance. A failed, cancelled
or replacement read clears provisional results. The shared check-row types live
in one small projection header; no transport, worker or request was added.

Direct implementation applies the frontend-design skill and its desktop guidance
to the existing surface. A composed offscreen test holds merge assessment, opens
Status, clicks a check and reads its details before releasing the remaining load.
The owner's desktop is not accessed. A separate two-check composed-window course
covers a nondefault selection across acceptance and clears on replacement,
failure and cancellation.

## Controls and corrections

The old-reader control fails the intended held-merge assertion. A stronger
selection control also exposed the window clearing early results immediately
before full acceptance; the handoff now retains the selected check until the
accepted overview replaces it. The original single-check fixture could not
distinguish selection preservation from selecting its sole row again.

Early test corrections are retained in external logs: provider cancellation
argument/variant mismatches, a widget assertion before the normal loading reveal
delay (now a controlled clock), an unbuilt optional regression executable, and
missing type/include declarations in the stronger selection test. These are not
passing evidence. Final verification and measurements are recorded below.

Evidence root:
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-responsiveness-vvxyjjsj`.

## Measurements and verification

Optimized `qualification-release`, macOS arm64, AppleClang 21, Qt 6.12,
three serialized local fixture runs per side with 330 ms per simulated read.
The public-path fixture has two files and one check, at 1.0x scale; the separate
selection course has two checks and runs at 1.0x/1.25x/2.0x:

| Median | Previous reader | Independent checks |
|---|---:|---:|
| First PR: checks visible | 6,355 ms | 3,339 ms |
| First PR: full review accepted | 6,355 ms | 6,355 ms |
| First PR: requests | 19 | 19 |
| Second PR: checks visible | 6,040 ms | 3,357 ms |
| Second PR: full review accepted | 6,040 ms | 6,032 ms |
| Second PR: requests | 21 | 21 |

This measures earlier useful content, not faster total acquisition or live-network
latency. The second open includes the existing additional history reads; its
request count is also unchanged. `status-baseline-commands.json`,
`status-final-measurement-commands.json` and `status-final-timing-summary.json`
retain commands, exit codes and every sample. The old reader's held-merge
control fails the expected early-checks assertion (`status-old-reader-control.log`).

All eleven focused Release courses pass (`status-functional-final.log`): core
window reads, application read host, provider paging, application overview,
overview widget, composed window at 1.0x/1.25x/2.0x, overview chips at 1.25x/2.0x,
and the progressive-open public path. Core-only configure, focused build and
`core_github_window_read` also pass (`status-core-{configure,build,functional}.log`).
No dedicated Debug timing course was run or counted.

The final strict held-merge course passes, including a real check-row click and
readable details with the remaining loading plate visible
(`status-final-held-public-path.log`). The offscreen full-window render
`status-final-render/status-checks-before-details.png` is inspected. The stronger
two-check course first failed the selection handoff before the correction
(`status-selection-control-valid.log`), then passed at all three scales,
including replacement, failure and cancellation clearing.

Both `architecture_boundaries` and `architecture_boundary_contract` pass
(`status-architecture.log`, 2/2). clang-format 22 passes all 21 changed C++
files (`status-format.log`). The documentation gate, its 67 regression tests,
scoped board validation and `git diff --check` pass (`status-docs.log`,
`status-doc-tests.log`, `status-board-precommit.json`).

The direct simplification review reuses the existing observer, check-result
policy, row types and pending presentation. It also found and verified the
selection handoff correction; no unrelated behavior or new dependency is added.

Full clang-tidy 22 covered all fourteen changed translation units
(`status-tidy.log`). Thirteen passed; the window file exposed two existing
warnings: the local `finish_choice` name and a queue-filter dereference on a path
that had already allowed an absent filter model. The local now uses the project
naming style, and that initialization path waits for the filter model before
updating queue status. Fully constructed-window behavior is unchanged. These
small gate corrections are included in this card, with the initial failure
retained and a focused rebuild/lint/functional recheck.

After those corrections, the rebuilt composed window, Home filter and strict
progressive-open course all pass (`status-window-functional-final.log`, 3/3).
The architecture ownership gate, clang-format 22 and documentation gate also
pass on the correction (`status-window-architecture-final.log`,
`status-window-format-final.log`, `status-docs-final.log`). The earlier timing,
core/host/provider, scale and architecture-contract evidence is reused: the
corrections change neither acquisition nor fully constructed-window behavior.

The corrected window passes full clang-tidy 22 with warnings as errors
(`status-window-tidy-final.log`). Together with the other thirteen unchanged
passing translation units, all touched C++ is lint-clean.
