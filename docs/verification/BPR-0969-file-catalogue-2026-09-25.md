# BPR-0969 historical changed-file catalogue — 2026-09-25

Assignment `/root/bpr0969_review_content`, mandate
`r4-standalone-review-content-20260925`, native Codex `rfc-implementer` leaf,
coordinator-implementer recipe revision 2. Exclusive source/tracker writer at
`/Users/jaeger/Documents/GitHub/BetterPullReview`, branch `main`, baseline
`a85f0b166619dc82cc8fdc40593694ac4df473fc`. The coordinator explicitly accepted
the bounded catalogue continuation; full AC2/AC3 remain unfinished. RFC0013's
immutable original baseline remains `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`.
Inherited preflight is **incomplete / state.drift / host_load=not_verified**.
No tool-state repair, readiness claim or observed host-loading claim is made.

## Result and boundary

Source candidate `ea5736156e311ddbc897bcf92d906ed2f23aa1cb` adds an optional
historical changed-file catalogue to the existing accepted overview payload.
Current and previous paths, change kinds and provider additions/deletions remain
bound to that one observation's complete profile-revision/host/account/destination,
numeric repository/PR and exact base/head. Counts are GitHub's reported changes;
they do not assert a recomputed base-tip comparison. No retained catalogue grants
source, current snapshot, epoch, markdown-link, reading or mutation authority.

The core validates 1,024 unique paths maximum, 4,096 bytes per path and a
conservative 128 KiB catalogue budget. The host omits excessive content in full
while retaining the new overview. A present empty catalogue means zero observed
files; absent means unavailable. SQLite atomically saves the catalogue and
overview under one digest; total payload is at most 416 KiB, within the unchanged
ten-record/2048-page store. Original payloads have no extension and remain
unavailable. New payloads carry version two; unknown extensions fail closed.
Older readers reject the extension instead of guessing its meaning.

Qt shows the historical catalogue without activating files, hides unknown
conversation/progress columns and restores the current columns/activation on a
fresh server observation. It explicitly labels freshness unknown and source,
conversations and review state unavailable. The existing authentication and
transport-failure admission stays required. A fully disconnected startup still
cannot establish its account or expose cached content.

## Direct verification

All courses use synthetic public fixtures, private temporary roots and denied
network access on macOS arm64 with Qt 6.11.2/offscreen. No live account or remote
mutation was involved. Runtime logs and rasters remain ignored under
`build/bpr0969-review-content`; the durable handoff records their SHA-256 digests.

| Check | Observation |
|---|---|
| Failing before | New public restart assertion against unchanged baseline product failed at “durable cached file catalogue survives process restart” in 19.43 s. Only the test assertion differed; baseline product was not replaced or restored. |
| Exact candidate integration | Rebuilt candidate passed 7/7 in 105.04 s: core window read 0.36 s, application window host 0.26 s, SQLite cache 0.27 s, composed Qt window scale 100 3.59 s, Qt Overview 1.86 s, actual restart inventory 37.26 s, account isolation 61.40 s. |
| Actual process boundary | Five-process inventory preserves Git pack hashes and exact epoch authority/anchors through seed, authenticated offline Resume, legacy payload Resume, corrupt payload refusal and fully offline authentication refusal. The four-process account course checks both switch directions, delayed old identity, exact account catalogue after restart and legacy Recent ownership. |
| Rendered catalogue | Final full-window account 910001 raster inspected: `review.cpp` and `secondary.cpp`, each modified +6/−6, unknown freshness, source/conversations/progress unavailable. Public activation produces no source or accepted snapshot. Legacy raster inspected: no rows and explicit catalogue unavailable. |
| Host controls | Exact rename/current/previous paths and counts; off-lane persistence; cancellation, retired session, wrong account, permission failure and shutdown cannot publish cached content. Changed base/head and file catalogue replace atomically on server recovery. Observed empty stays present; 1,025 files omit the optional catalogue instead of truncating or carrying older content. |
| Core/storage controls | Foreign account/profile fingerprint/repository/PR, expected numeric identity and malformed base/head refuse. Duplicate paths, inconsistent counts, oversized previous-path/aggregate data, unknown change kind and allocation counts refuse. Cancelled writes preserve the prior complete observation; a completed COMMIT wins late cancellation. Digest corruption/unknown extension/schema refuse. Exact recreation, observed empty, legacy unavailable and replacement are asserted. |
| Required core checks | `core-only` configure/build succeeded; core window read and both architecture-boundary suites passed 3/3 in 120.30 s. The subsequent header comment-only change has no compiled behavior; the dev build/test uses the exact committed candidate. |
| Format/tidy | All eleven touched C++ files pass canonical clang-format 23.1.1. Strict tidy on the core validator and SQLite codec passed, with eight suppressed non-user warnings. `git diff --check` passes. |

Commands used the real `test_offline_inventory_public_path`,
`test_better_pull_review_application_github_window_read_host_tests`,
`test_better_pull_review_github_window_read_tests`,
`test_cached_review_overview_storage`,
`test_better_pull_review_qt_pull_request_overview_page_tests`, and
`test_better_pull_review_qt_github_tests` targets. The integration selector was:

```sh
BPR_OFFLINE_RENDER_DIR=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0969-review-content ctest --preset dev -R '^(core_github_window_read|application_github_window_read_host|sqlite_cached_review_overview|qt_pull_request_overview_page|qt_composed_github_scale_100|application_offline_inventory|application_recent_account_isolation)$' --output-on-failure
ctest --preset core-only -R '^(core_github_window_read|architecture_boundaries|architecture_boundary_contract)$' --output-on-failure
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors --paths src/core/src/cached_review_overview.cpp src/adapters/sqlite/src/cached_review_overview_store.cpp
```

## Self-review and remaining acceptance

Self-review traced accepted provider data through core admission, the serial
SQLite worker, decision-lane stale/cancellation checks and the narrow Qt
projection. It checked one-record atomic revision binding, allocation/byte bounds,
legacy/empty distinction, inert cached navigation, tooltip/column cleanup on
reconnection, current identity invalidation and host shutdown. Extracting cached
row presentation kept the existing incremental file model and avoided a parallel
file-navigation authority. No new process, network, provider or Qt dependency
entered the core. No composition-root or purge wiring change was needed because
the existing overview store and lifecycle already own the payload.

The first local build caught and corrected an enum-typed loop counter. The first
six-test pass then passed in 94.99 s. The final seven-test pass covers the added
legacy public course and simplified presentation at the committed source. The
plan helper initially refused duplicate AC3 work as a separate milestone; the
checked amendment instead preserves all original AC3 milestone exits and adds
four continuation exits. Prior overview acceptance is reused within its actual
scope, while full canonical AC3 remains pending.

Source blobs, conversations/threads, disconnected authentication, arbitrary
saved reading/selected epoch restoration, full changed-revision public recovery
and full AC2/AC3 remain pending. The catalogue cannot fulfil a saved selected
revision or prove current numeric ownership from a locator. Oversized optional
content is unavailable; one pending save per window can still skip a subsequent
observation. No crash/native compositor/accessibility/human or Windows/Linux
acceptance is inferred. Windows/Linux remain owner-deferred.

No new attended session was created. The protected RFC0052 candidate, session,
server and waiter were untouched. The next eligible bounded content milestone
can retain conversations with explicit collection availability under the same
current-process authentication, or define read-only exact retained-source
composition without manufacturing live review authority.

The documentation gate passed (`DOCUMENTATION_OK`), all 64 documentation
regression tests passed in 1.715 s, and scoped board validation passed for 1,186
cards. The shared checkpoint checker returns `action=stop` and
`engineering_acceptance=not-evaluated`, preserving AC3 pending. Its first check
caught a missing top-level reference to reused overview evidence; that listing
was corrected and rechecked. Exact command quiescence and evidence hashes are recorded
in the [durable handoff](../rfcs/plans/bpr0969-offline-inventory/review-content-handoff.json).
