# BPR-0967 Mac startup measurement inventory — 2026-09-25

Assignment `/root/bpr0967_startup`, mandate `r4-standalone-mac-20260925`,
source candidate `6c2d531c63b55626bfaebde3876f3ac67904989a` on `main`.
The assigned stopping boundary is reached: the current public executable
cannot select the large-review fixture, and the separate benchmark has no
process/Qt launch or native presentation timestamp. This is a bounded partial
result. True launch, separate first metadata and first visible frame remain
unmeasured; no new latency budget is justified by this inventory.

The coordinator's clarification requires verifying the public fixture/refusal
path and recording this limitation without adding a production fixture or
claiming compositor presentation. Product code, test code, CMake registrations,
measurement boundaries and thresholds are unchanged. Plan/claim commits advance
the repository to `187c6cba267bc6cc70c0055e9040a810e67906c8` before the public
controls; the source inputs remain identical to the baseline. The earlier
[108.992 ms load observation](BPR-0967-mac-load-gap-2026-09-25.md) retains its
exact candidate and remains a failure against the 100 ms input target.

## Candidate and station

Apple M4 Pro, 48 GiB (51,539,607,552 bytes), 14 logical processors;
macOS 26.6.2, build 25G83, arm64; Debug, Qt 6.11.2.
The exercised courses use `QT_QPA_PLATFORM=offscreen` and Fusion.
The large-review window is 1600 × 1000 at device scale 1. These are local
Mac executable observations, with no native desktop/compositor or packaged
release acceptance.

Both targets were built against the unchanged source candidate using
`cmake --build --preset dev --target app_better_pull_review bench_better_pull_review_large_review`.
The application was relinked; the benchmark bytes are identical to the previous
milestone's verified executable. Subsequent commits contain documentation and
bookkeeping only.

| Executable, relative to the source root | SHA-256 |
| --- | --- |
| `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review` | `45221c2952b3f28b30b3c312bdabb419d6a2235395006536b5735f9682eac6bb` |
| `build/dev/tests/end_to_end/better_pull_review_large_review` | `bf93be054115add71cdfea2980f7017c21141c075892b711da985284bb0a704b` |

## Public selection control

The tests-enabled [application entry point](../../src/app/main.cpp) selects
only `github-com`, `ghes-3.18` and `uiforetw-74` at its `--github-fixture`
branch. Every other value prints `PUBLIC_PATH_FAILED` and returns failure
before desktop composition. `QApplication` already exists at that refusal.
The large fixture is available to the separate benchmark through
`github_application_fixture_access::start(large_review_v1, ...)`; that does
not make it a public executable fixture selector. The large replay is guarded
by `BPR_BUILD_TESTING` and is absent from the production build graph.

Two fresh process controls use the same rebuilt application, the repository's
macOS network-denial runner, zero allowed TCP loopback ports and the automated
test boundary. The supported fixture uses the existing disposable private
fixture roots; no account or real pull request is selected.

| Selection | Actual terminal | Evidence |
| --- | --- | --- |
| `--github-fixture large-review-v1` | Exit 1; `PUBLIC_PATH_FAILED`; runner confirms child exit 1 | No render file and no success marker |
| `--github-fixture github-com` | Exit 0; `PUBLIC_PATH_OK`, `GITHUB_FIXTURE_OK:github.com`, `RENDER_OK`; runner confirms child exit 0 | Full-window widget capture exists; ordinary composed fixture completes |

This pair distinguishes an unsupported fixture from a broken executable or
an inability of the isolation runner to run the normal supported course.
It does not establish a large-review launch duration. The supported fixture's
`PUBLIC_PATH_BUDGET` is its automated workflow duration, not a launch metric.

The refused invocation is reproducible from any working directory:

```sh
QT_QPA_PLATFORM=offscreen BPR_AUTOMATED_TEST_BOUNDARY=1 \
  /opt/homebrew/opt/python@3.14/bin/python3.14 -I -S \
  /Users/jaeger/Documents/GitHub/BetterPullReview/scripts/run_network_isolated_test.py \
  --sandbox-exec /usr/bin/sandbox-exec \
  --profile-template /Users/jaeger/Documents/GitHub/BetterPullReview/cmake/sandbox/macos-network-deny.sb \
  --loopback-port-count 0 -- \
  /Users/jaeger/Documents/GitHub/BetterPullReview/build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
  --github-fixture large-review-v1 \
  --render-evidence /Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0967-startup/large-review-refusal.png \
  --automated-run-budget-ms 40000
```

The supported control changes only the fixture value to `github-com` and the
image path to
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0967-startup/supported-fixture.png`.
The runtime receipt retains exact argument arrays, return codes, output and
executable digest. The refusal is an expected negative control, never a
successful large-review launch.

## What the existing clocks observe

The [large-review benchmark](../../tests/end_to_end/large_review_benchmark.cpp)
establishes these boundaries:

| Requested dimension | Current observation | Missing fact |
| --- | --- | --- |
| Process launch to first frame | No parent launch clock or process-entry timestamp | Process launch, loader and pre-main work, paired with a visible-frame observation |
| QApplication to first frame | `main` constructs `QApplication` before scheduling `run_benchmark` | Timestamp before construction and a native presentation endpoint |
| Composition to Home | `CompositionStartupMs` starts just before fixture `access::start`; ends after `show` and synchronous `window.grab()` | Process/QApplication work, native exposure and compositor presentation |
| Fixture Connect | Real Connect action to fixture account-connected/open-enabled state | This occurs after composition and Home capture; it is authentication-fixture work |
| Initial metadata | No separate benchmark phase or timestamp | First provisional metadata publication and its rendered presentation, distinct from complete snapshot acceptance |
| Metadata plus Git ready | Home Open action to 515 file rows, one smart fetch, ready epoch, navigation readiness and quiet jobs | Cannot isolate early metadata, accepted metadata, or first paint from the aggregate |
| First large source | File selection to exact selected result, quiet jobs, measured scroll geometry and synchronous window capture | Native presentation time; also excludes the preceding Home, Connect, load and Read steps |

The first source course checks both panes are visible in Qt and independently
compares exact blob identities and all 20,000 source lines on each side. The
captured pixels prove a widget render. The benchmark contains no native
exposure/paint completion or compositor timestamp. Selecting the Cocoa plugin
alone would not fill either missing clock boundary. Adding elapsed time around
the entire runner would mix isolation, launch, the complete workflow and
shutdown, so it would not supply the requested launch endpoint either.

## First metadata path

An early metadata path already exists; missing measurement does not mean the
product waits for all Git work to publish metadata. In
[github_paged_read.cpp](../../src/adapters/github/src/github_paged_read.cpp),
the provider validates repository and pull-request records and emits one
`overview_observed` observation before constructing the full paged snapshot.
[github_window_read_host.cpp](../../src/application/src/github_window_read_host.cpp)
posts that observation to the decision lane, checks request/token/session
policy and publishes `LoadingOverview`. The
[workbench window](../../src/adapters/qt/src/workbench_window.cpp) passes it
to [Overview](../../src/adapters/qt/src/pull_request_overview_page.cpp) only
while the initial pull request is loading. That presentation explicitly labels
the metadata provisional and keeps review entry disabled.

The current benchmark's ready predicate waits for the later complete
515-file/1,000-thread snapshot and Git/catalog/navigation readiness. Its
aggregate duration and input gap cannot be assigned to the earlier observation
or its first paint. No duration for that early path was measured in this slice.

## Unchanged public course

`caffeinate -i -s ctest --preset dev -R '^application_large_review_responsiveness$' --output-on-failure`
passes 1/1 in 24.12 seconds (24.14 seconds total). The process-scoped power
assertion and test exit normally. The executable digest matches the earlier
candidate, and the report passes all 20 existing completion/dispatch/source
input gates. Its boundaries remain those above:

| Observation | Elapsed ms | Maximum complete input interval ms |
| --- | ---: | ---: |
| Composition to Home widget capture | 702.718 | Not measured by this clock |
| Fixture Connect | 321.412 | 6.081 |
| Initial metadata/Git-ready aggregate | 10,235.913 | **108.139** |
| First large-source selection/capture | 1,909.081 | 82.893 |
| Large-source revisit/capture | 618.457 | 77.545 |

`InputBudgetFailures` still contains `paged_review_and_git_load`. The
108.139 ms interval is a new observation of unchanged code and exceeds 100 ms
by 8.139 ms; it neither supersedes nor accepts the previous 108.992 ms result.
The 1,000 ms composition capture limit remains scoped to that existing fixture
clock. It is not adopted as a process launch or native frame budget.

## Disposition and continuation

No product repair or timing harness change is made. Core policy, host
concurrency/persistence/lifecycle, provider authority, Qt behavior, public
commands, packaging and release configuration have no semantic change.
The exact source/cache/history and lifetime-peak evidence in the
[previous handoff](../rfcs/plans/bpr0967-mac-responsiveness/handoff-load-gap.json)
is reused at its recorded candidate: no cache, resource or timing ceiling
changes, and no repeated resource/history suite is needed for documentation.

A successor would need an explicitly scoped safe entry through the real
application for this workload, process/pre-QApplication timestamps and a
declared native first-frame observer with a control that suppresses or delays
presentation. It must distinguish provider observation, provisional metadata
publication, complete snapshot acceptance, Git readiness and exact source
render. Only actual samples from those boundaries can justify their budgets.
The present assignment stops at the demonstrated missing route/observer.

BPR-0967 returns to Backlog. Remaining initial-load input, true launch,
separate metadata/native frame, active-work input/cancellation,
generated/pathological fallback and peak live heap obligations remain open.
Windows/Linux measurements are explicitly deferred. No human, independent,
native desktop, package or release acceptance is supplied. No attended review
is ready; the coordinator-owned RFC0052 14-question session and its candidate,
state, server and waiter are untouched.

The [durable handoff](../rfcs/plans/bpr0967-mac-responsiveness/handoff-startup.json)
binds commands, evidence digests, checkpoint, repository writers and the next
action. The documentation gate reports `DOCUMENTATION_OK`; all 64 documentation tests
pass, the scoped board validates 1,186 cards and the diff check passes. The
milestone checker returns `action=stop`, with engineering acceptance explicitly
not evaluated by the helper. Raw logs, captures and control receipts remain in
ignored `build/bpr0967-startup/`.
