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
`NSAccessibility.h`. This identifies a read and change-notification mechanism;
it is not yet target-native runtime evidence. Installed Qt 6.11.2 exposes
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

## Acceptance ledger

| Obligation | Current result | Direct evidence |
|---|---|---|
| BPR-0758 / B3 | Accepted within this card's preference scope | Source `6f07c343`; focused host/store/Qt tests and fixture-only composed gesture/restart above. Actual mascot rendering remains pending. |
| BPR-0759 / B4 and B9 | Pending dependency | Native/injected observer and shutdown checks pending; no platform support claim yet. |
| BPR-0760 / B1, B2, B9 and B10 | Pending dependency | Fixture catalog, sequencer, cache, widget lifecycle and budget checks pending; production resource selection remains for BPR-0757/0761. |
| `runtime-foundation:exit:1` | Passed for the current appearance path | `6f07c343`; focused migration/fan-out/failure checks and composed gesture/restart above. |
| `runtime-foundation:exit:2` | Pending | Requires honest observer states and fixture-only retained playback/lifecycle evidence. |
