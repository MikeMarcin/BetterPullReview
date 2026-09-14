# RFC-BPR-0031 runtime foundation evidence

This record belongs to the authored `runtime-foundation` milestone
(BPR-0758/0759/0760). The selected production Dif catalog is still the
historical three-clip revision. Synthetic ten-clip data used here may prove
runtime contracts, but cannot select or accept production art. The neutral
identity decision remains with `BPR-0755-neutral-identity-v001` in attended
session `cbd48b91000b00f1e72e1ad62c7c384c`.

## Composed path baseline

At `08cf545b`, before runtime source edits, the installed `dev` preset ran
`ctest --preset dev -R 'application_appearance_theme_customization_contract|appearance_gesture_contract' --output-on-failure`.
The theme-customization contract passed in 23.49 seconds through the actual
app composition. The gesture contract timed out after its 120-second internal
budget at epoch fixture stage 4: its expected six conversation roots did not
arrive; four were presented. This is a negative pre-change observation and
does not establish a Dif regression or an accepted Dif gesture.

## Platform motion observation

The installed macOS AppKit SDK declares
`NSWorkspace.accessibilityDisplayShouldReduceMotion` and
`NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification` in
`NSAccessibility.h`. The `e337c1fa` native source reads the property and
registers for its notification through the shared workspace's notification
center. A direct `QT_QPA_PLATFORM=cocoa` probe returned
`motion-native-probe:observed` for a Full or Reduced current-state read; an
offscreen probe returned Unsupported. Injected source tests cover changed
values, deduplication, two subscribers, same-delivery unsubscribe, destroyed
subscriber lifetimes and handler release. The app shares one observer across
profile windows and unregisters each window when it closes. An actual OS
preference change/notification was not exercised, so target-native change
delivery remains open for later verification. Installed Qt 6.11.2 exposes
appearance-color hints but no reduced-motion hint. Windows API correspondence
and notification behavior are unproved here. No reliable common Linux API has
been identified, so unsupported must remain a first-class result. No OS
preference has been changed during this milestone.

## BPR-0758 direct result

`6f07c343` adds the closed core values, accepted profile host, schema-6
SQLite migration, and the real Appearance controls. The focused `dev` build
and five selected behavior tests passed: `core_appearance_policy`,
`application_appearance_host`, `application_appearance_profile_consumers`,
`sqlite_appearance_settings_storage`, and `qt_appearance_settings_panel`.
The corresponding `core-only` build and six checks passed. Tests cover
default/System resolution, explicit Full/Reduced/hidden, v5 migration and
corrupt rollback, stale/detached completions, same-profile fan-out, other
profile isolation, session-only failure labels and unchanged classification
cache identity. All touched C++ files passed `clang-format --dry-run --Werror`
and `git diff --check`.

The selected `dev` app target built. A fixture-only direct process used
`--github-fixture github-com --epoch-fixture-phase single
--appearance-gesture-probe` with an owner-private root under
`build/dif-runtime/appearance-proof`. It exited zero with `PUBLIC_PATH_OK`,
`APPEARANCE_GESTURE_OK`, `APPEARANCE_REFUSAL_OK`,
`DIF_PREFERENCES:hidden:full:full`, and one durable write. A separate
`--self-test --test-application-data-root` process reopened that exact root,
exited zero and reported `DIF_PREFERENCES:hidden:full:full` with zero new
writes. A fresh-root self-test reported
`DIF_PREFERENCES:shown:system:reduced` without a write. The local logs are
`build/dif-runtime/appearance-proof-gesture.log`,
`appearance-proof-restart.log`, and `appearance-fresh.log`; they contain
fixture output and are not source artifacts.

The existing `application_appearance_theme_customization_contract` passed
again after the change. The `architecture_boundary_contract` passed. The
`architecture_boundaries` gate did not pass: its root-wide recursive scan
included an unrelated `.claude/worktrees` checkout, and, when that false
positive was temporarily excluded, the gate exposed a separate existing
GitHub CLI factory-signature assertion that predates this change. Neither
unmanaged checkout nor CLI contract was modified. The earlier 120-second
appearance-gesture epoch-fixture timeout remains an unrelated negative
baseline; the single-phase direct gesture above exercises this new path.

## BPR-0759/0760 integrated result

`e337c1fa` adds the macOS observation edge, profile-window fanout, and an
explicitly shared Qt cache/sequencer/widget with ten synthetic RGBA8 PNG
strips. The app, `qt_motion_observer`, `qt_dif_runtime`, and selected
appearance host/panel/composed checks built and passed. The native Cocoa
current-state probe passed. The fixture player test covers every clip index,
frame rate, loop/one-shot return, gutter rectangle, malformed geometry/digest,
late/backward time, static interval, hidden/reduced/modal/deactivated gates,
timer shutdown and shared retained pixels. It counted no repaint during a
350 ms static interval and at most five timer wakeups in a 400 ms animation
interval. Two widgets shared one ten-strip decode; fixture pixels were
16,960 bytes, accounted retained memory 34,160 bytes, and estimated
conversion peak 37,534 bytes. The opaque Qt/allocator allowance is an
estimate; native backend allocations remain unmeasured.

After observer integration, the direct offscreen `github-com` fixture with
`--epoch-fixture-phase single --appearance-gesture-probe` and a fresh
absolute root passed `PUBLIC_PATH_OK`, `APPEARANCE_GESTURE_OK`,
`APPEARANCE_REFUSAL_OK`, and `DIF_PREFERENCES:hidden:full:full` with one
durable write using a 30-second internal budget. An initial attempt omitted
the required `--epoch-fixture-root` and was rejected at configuration. The
corrected 8-second default-budget attempt reached the pull request but
expired at stage 2; increasing the explicit internal budget allowed the
same path to complete in 10.7 seconds. `DOCUMENTATION_OK`, touched C++ format
checks and `git diff --check` passed. The independent changed-surface reviewer
found no blocking issue; its two follow-ups (unconditional observer checks
and same-delivery unsubscribe fencing) were implemented and retested.
Production art/resource selection, actual OS preference toggle and native
backend memory remain outside this fixture-only result.

## Acceptance ledger

| Obligation | Current result | Direct evidence |
|---|---|---|
| BPR-0758 / B3 | Accepted within this card's preference scope | Source `6f07c343`; focused host/store/Qt tests and fixture-only composed gesture/restart above. Actual mascot rendering remains pending. |
| BPR-0759 / B4 and B9 | Accepted for the observer edge; target-native change verdict remains open | `e337c1fa`; injected delivery/shutdown test, native Cocoa current-state read, offscreen Unsupported result and composed fixture path above. |
| BPR-0760 / B1, B2, B9 and B10 | Accepted for fixture-only retained playback/lifecycle | `e337c1fa`; synthetic ten-clip cache, sequencer, timer and paint checks above. Production selection remains for BPR-0757/0761; native backend memory for BPR-0766. |
| `runtime-foundation:exit:1` | Passed for the current appearance path | `6f07c343`; focused migration/fan-out/failure checks and composed gesture/restart above. |
| `runtime-foundation:exit:2` | Passed within the authored fixture boundary | Unsupported/Failed/Unknown resolve conservatively; native macOS read and injected change delivery are separately identified. Ten-strip fixture playback/lifecycle tests pass. Actual OS change and production resources remain open later. |
