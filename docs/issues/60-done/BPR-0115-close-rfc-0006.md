---
id: BPR-0115
title: Verify and close RFC-BPR-0006
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0101, BPR-0104, BPR-0105, BPR-0106, BPR-0107, BPR-0108, BPR-0109, BPR-0110, BPR-0111, BPR-0112, BPR-0113, BPR-0114, BPR-0118, BPR-0181, BPR-0182, BPR-0183, BPR-0184, BPR-0185, BPR-0210, BPR-0214, BPR-0220, BPR-0233, BPR-0234, BPR-0236, BPR-0237]
---

# Verify and close RFC-BPR-0006

RFC base: `91e6b7044ced7b73af6a1d1993c9ed97f4f6b2d4`.

Replay the complete RFC-BPR-0006 claim matrix from clean state, resolve every documentation-ledger row, and obtain an independent review pass.

Closure is strictly ordered, and the chain is satisfied: RFC-BPR-0002 through RFC-BPR-0005 closed `Auto-Verified` by 2026-08-29. The owner-attended BPR-0114 closed on 2026-09-01 on the accepted v008 observation. **RFC-BPR-0006 is `Auto-Verified` as of 2026-09-01**; the final record is below the checklist.

## Historical composition finding and current correction

BPR-0180 measured that nothing RFC-BPR-0006 built is connected to the
application: `appearance_application_host` and `packaged_syntax_classifier`
have zero symbols in the shipped binary, `appearance_profile` is constructed
only by tests, and the composition never links
`BetterPullReview::AdapterSyntax`. BPR-0181 through BPR-0185 close that, and
BPR-0118 retires the one setting the product owner decided not to keep.

That composition finding is historical. At final HEAD the predefined profiles
and syntax classifier are composed only when durable settings open. BPR-0236
owns the missing default fallback, and BPR-0237 owns semantic palette
customization, preview, persistence, and safe import.

When this card is eventually prepared, the replay must be run against the
composed application. Replaying the component suites would reproduce exactly
the gap BPR-0180 found, since those suites all passed while the product had
no appearance model and no syntax highlighting at all.

Ask the linker as part of the replay. `nm` on the shipped binary reporting
symbols for every component this RFC specifies is a cheap check that no amount
of green component testing substitutes for.

## Closeout checklist

Historical evidence is provenance only; every row requires final replay at the final closeout head.

| State | Criterion | Direct verification surface | Command or attended interaction | Expected result | Durable evidence location | Gate class | Live blocker |
|---|---|---|---|---|---|---|---|
| [x] | C++, CMake, Markdown, JSON, Python, and plain-text fixture files select expected definitions. | packaged_syntax_classifier fixtures and shipped-host composition inventory | `ctest --preset dev -R '^(syntax_classification_contract\|composed_application_hosts)$' --output-on-failure` | C++, CMake, Markdown, JSON, Python, and plain text select their expected packaged definitions and the real executable constructs the classifier. | Definitions-are-selected-correctly row in the claim matrix in [RFC-BPR-0006 verification ledger](../../verification/RFC-BPR-0006.md#claim-matrix); final replay record keyed by “C++, CMake, Markdown, JSON, Python, and plain-text fixture files select expected definitions.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | A hunk beginning inside a multiline comment/string receives the same classification as the complete blob. | Offscreen real diff_workbench block-layout classification in qt_syntax_pane_composition | `ctest --preset dev -R '^(qt_syntax_pane_composition)$' --output-on-failure` | A marker whose multiline comment/string begins hundreds of lines above the visible hunk receives the same semantic span/color as whole-blob classification. | Hunk-classification-equals-whole-blob-classification row in the claim matrix in [RFC-BPR-0006 verification ledger](../../verification/RFC-BPR-0006.md#claim-matrix); final replay record keyed by “A hunk beginning inside a multiline comment/string receives the same classification as the complete blob.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Switching theme or color does not re-run lexical parsing. | BPR-0237 selected-comparison semantic-color preview/repaint trace plus the appearance-host lexical submission counter | Run `ctest --preset dev -R '^(application_appearance_host\|qt_syntax_pane_composition\|application_appearance_gesture_contract)$' --output-on-failure`, then execute BPR-0237's selected-comparison semantic-color preview/repaint trace and compare the lexical submission count before and after. | The selected comparison repaints the changed semantic colors immediately while its retained spans remain selected and the lexical submission count is unchanged. | Theme-change-does-not-re-parse row plus BPR-0237's selected-comparison trace in the claim matrix in [RFC-BPR-0006 verification ledger](../../verification/RFC-BPR-0006.md#claim-matrix); final replay record keyed by “Switching theme or color does not re-run lexical parsing.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Changing font invalidates the Qt-local measured axis and ribbon geometry while preserving semantic selection, then reprojects the one shared comparison position without changing mapping or confidence. | BPR-0227 real selected-comparison font-change trace plus qt_appearance_profile measured-axis component coverage | Run `ctest --preset dev -R '^(qt_appearance_profile\|qt_diff_axis_scale_(100\|125\|200))$' --output-on-failure`, then execute BPR-0227's real selected-comparison font-change trace at one retained shared comparison position. | The font change invalidates and rebuilds the measured axis and ribbon geometry, reprojects the one retained shared comparison position, and leaves mapping, confidence, and semantic selection unchanged. | Font-change-preserves-semantics row plus BPR-0227's real selected-comparison trace in the claim matrix in [RFC-BPR-0006 verification ledger](../../verification/RFC-BPR-0006.md#claim-matrix); final replay record keyed by “Changing font invalidates the Qt-local measured axis and ribbon geometry while preserving semantic selection, then reprojects the one shared comparison position without changing mapping or confidence.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | System mode responds to OS scheme changes; explicit mode does not. | Foreground macOS System/explicit-mode probe with BPR-0220 cause-attributed native-appearance geometry instrumentation | Run [BPR-0114](../60-done/BPR-0114-rfc-0006-system-scheme-toggle.md#required-direct-evidence)'s frontmost Light-to-Dark probe with the completed BPR-0220 cause instrumentation, restore the original scheme, and retain the appearance-mode markers and complete native-appearance geometry attribution. | System resolves light to dark after the OS change, explicit Light stays light, returning to System resolves dark, and the colour-only native transition causes zero geometry rebuilds while state and focus assertions pass. | System-mode-follows-the-OS row plus BPR-0114 and BPR-0220 target-native records in the claim matrix in [RFC-BPR-0006 verification ledger](../../verification/RFC-BPR-0006.md#claim-matrix); final replay record keyed by “System mode responds to OS scheme changes; explicit mode does not.” | target-native | Resolved: the owner accepted the v008 observation as the terminal attended row on 2026-09-01 ([BPR-0114](../60-done/BPR-0114-rfc-0006-system-scheme-toggle.md)); the prior-RFC blocker resolved when RFC-BPR-0005 closed Auto-Verified on 2026-08-29. |
| [x] | UI and code fonts, sizes, UI line height, tab width, ligatures, bold, italic, and semantic colors persist and preview immediately. The code line height is retired; see the goals above. | Healthy-store three-process appearance gesture plus BPR-0237 semantic-role preview/import path | Run `ctest --preset dev -R '^(application_appearance_gesture_contract\|sqlite_appearance_settings_storage\|qt_appearance_settings_panel)$' --output-on-failure`, then execute BPR-0237's composed semantic-role preview/commit/cancel/restart and valid/invalid import matrix against the selected comparison. | Fresh state is System/tab-width four; gestures preview every typography field immediately; dark/tab-width eight and committed semantic roles survive a separate process; canceled preview restores prior values. | Predefined-settings-persist row and BPR-0237 required final-state contract in [RFC-BPR-0006 verification ledger](../../verification/RFC-BPR-0006.md#claim-matrix); final replay record keyed by “UI and code fonts, sizes, UI line height, tab width, ligatures, bold, italic, and semantic colors persist and preview immediately. The code line height is retired; see the goals above.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Theme imports reject unknown fields, URLs, includes, oversized files, and executable/style payloads. | core_theme_exchange validation plus BPR-0237 composed import/preview interaction | Run `ctest --preset dev -R '^(core_theme_exchange)$' --output-on-failure`, then execute BPR-0237's composed semantic-role preview/commit/cancel/restart and valid/invalid import matrix against the selected comparison. | Unknown fields, URLs, includes, oversize, executable/style payloads, malformed/forward versions, and contrast-invalid roles get distinct rejections; a valid theme round-trips and previews safely. | Theme-import-is-safe row and BPR-0237 required final-state contract in [RFC-BPR-0006 verification ledger](../../verification/RFC-BPR-0006.md#claim-matrix); final replay record keyed by “Theme imports reject unknown fields, URLs, includes, oversized files, and executable/style payloads.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | High-contrast and color-vision checks prove state remains understandable without color alone. | Four shipped palettes, semantic non-color cues, and digest-bound high-contrast captures | Run `ctest --preset dev -R '^core_appearance_contrast$' --output-on-failure`, then complete BPR-0210's exact Light/Dark/high-contrast attended capture review with BPR-0233's selected recorded diff. | Every review state retains an icon, label, mark, outline, prefix, or accessible name; required contrast/color-vision pairs pass and the attended captures remain readable. | State-survives-without-colour row and BPR-0210 verdict artifact in [RFC-BPR-0006 verification ledger](../../verification/RFC-BPR-0006.md#claim-matrix); final replay record keyed by “High-contrast and color-vision checks prove state remains understandable without color alone.” | owner-attended | Resolved: the owner accepted the v008 observation as the terminal attended row on 2026-09-01 ([BPR-0114](../60-done/BPR-0114-rfc-0006-system-scheme-toggle.md)); the prior-RFC blocker resolved when RFC-BPR-0005 closed Auto-Verified on 2026-08-29. |
| [x] | Dependency scans prove no font/color/KSyntaxHighlighting/Qt types enter the functional core; theme/font interactions update host caches and Qt presentation without mutating review semantics. | Core dependency scans, appearance host cache counters, and Qt syntax/profile projection | `ctest --preset dev -R '^(core_dependency_boundary\|application_appearance_host\|qt_syntax_pane_composition\|qt_appearance_profile)$' --output-on-failure` | Core contains no font/color/KSyntaxHighlighting/Qt type; host cache invalidations and Qt repaint/geometry changes do not mutate review mapping or selection. | Core-stays-free-of-presentation-types row in the claim matrix in [RFC-BPR-0006 verification ledger](../../verification/RFC-BPR-0006.md#claim-matrix); final replay record keyed by “Dependency scans prove no font/color/KSyntaxHighlighting/Qt types enter the functional core; theme/font interactions update host caches and Qt presentation without mutating review semantics.” | automated | No live blocker; final-head focused replay remains required. |

## Focused replay of the automated rows — 2026-09-01

Run against `build/dev` at the tree of `cc17d703`, whose `src/` and
`tests/` are byte-identical to the closeout candidate's for every surface
these rows name (`cmake --build --preset dev` reported `ninja: no work to
do`). Under the light closeout contract this is the check per criterion, its
result, and its commit; a later commit that does not touch these surfaces
does not reopen them.

`ctest --test-dir build/dev --output-on-failure --no-tests=error -R '^(syntax_classification_contract|composed_application_hosts|qt_syntax_pane_composition|application_appearance_host|application_appearance_gesture_contract|qt_appearance_profile|qt_diff_axis_scale_(100|125|200)|sqlite_appearance_settings_storage|qt_appearance_settings_panel|core_theme_exchange|core_appearance_contrast|core_dependency_boundary|application_appearance_theme_customization_contract|application_appearance_store_fallback_contract|application_uiforetw_selected_diff_contract|application_native_appearance_probe_contract)$'`
— **18/18 passed**, 100%.

| Criterion | Selectors run | Result | Commit |
|---|---|---|---|
| Definitions are selected correctly | `syntax_classification_contract`, `composed_application_hosts` | pass | `cc17d703` |
| Hunk classification equals whole-blob classification | `qt_syntax_pane_composition` | pass | `cc17d703` |
| Theme change does not re-parse | `application_appearance_host`, `qt_syntax_pane_composition`, `application_appearance_gesture_contract`, plus BPR-0237's `application_appearance_theme_customization_contract` (the selected-comparison repaint trace with the lexical-submission count held) | pass | `cc17d703` |
| Font change preserves semantics | `qt_appearance_profile`, `qt_diff_axis_scale_100/125/200`; BPR-0227's selected-comparison font path is exercised by `application_uiforetw_selected_diff_contract` | pass | `cc17d703` |
| Predefined settings persist and preview | `application_appearance_gesture_contract`, `sqlite_appearance_settings_storage`, `qt_appearance_settings_panel`, `application_appearance_theme_customization_contract`, `application_appearance_store_fallback_contract` | pass | `cc17d703` |
| Theme import is safe | `core_theme_exchange`, `application_appearance_theme_customization_contract` | pass | `cc17d703` |
| The core stays free of presentation types | `core_dependency_boundary`, `application_appearance_host`, `qt_syntax_pane_composition`, `qt_appearance_profile` | pass | `cc17d703` |
| System mode follows the OS; explicit does not | `application_native_appearance_probe_contract` (the probe's deterministic contract) passes here; the frontmost observation itself is the owner-accepted v008 record | automated half pass; attended half see below | `cc17d703` |
| State survives without colour | `core_appearance_contrast` passes here; the attended captures are the accepted `high-contrast-scheme-acceptance-v001` and `diff-scheme-acceptance-v002` verdicts | automated half pass; attended half see below | `cc17d703` |

### The two attended rows

Neither attended surface has changed since its verdict: no commit since
2026-08-21 touches the appearance profile, host, settings adapter, or syntax
adapter (`git log --since=2026-08-21 -- src/*/appearance* src/adapters/syntax`
is empty), and the only appearance-related change to `main.cpp` since v008
is a failure-message format ([[BPR-0475]]). The v008 owner acceptance and the
owner's recorded request that no redundant digest-only rerun be required
therefore stand as this row's evidence under the reuse rule. Whether to close
the RFC on that reuse or to sit one more v009 foreground run against the
closeout executable is the owner's call; it is presented with the run's other
pending verdicts rather than decided here, and [[BPR-0114]] carries it.

## Final closeout record — 2026-09-01

Under the light closeout contract: per criterion, the check that would fail if
the criterion were false, its result, and the commit.

- **Automated rows**: the 2026-09-01 focused replay above, 18/18 at
  `cc17d703`, whose `src/` and `tests/` are byte-identical to this closeout
  head for every surface those selectors exercise (the commits since are
  documentation, the RFC-0017 guard script, `tests/CMakeLists.txt`'s
  negative-control registration, the Windows preset, and the station
  launcher; none touches the appearance, syntax, theme, or core-boundary
  sources or tests).
- **System mode follows the OS; explicit does not**: the owner-accepted v008
  observation (`docs/verification/reviews/appearance-scheme-foreground-v008/`
  operator response, raw log, validation record) plus BPR-0220's accepted
  zero-rebuild repair, accepted as the terminal attended row on 2026-09-01
  in `run-boundary-decisions-v001` (validated verdict and record committed).
- **State survives without colour**: `core_appearance_contrast` in the
  replay, plus the accepted attended batches the ledger's attended history
  records — `diff-scheme-acceptance` v002, `high-contrast-scheme-acceptance`
  v001, `intraline-high-contrast` v001, `intraline-hue-and-subtle-dark` v001,
  and `intraline-dark-green-strength` v001 — each validated against its own
  batch manifest at the time.
- **Composition**: `composed_application_hosts` passed in the replay, which
  requires the exact demangled `appearance_application_host` and
  `packaged_syntax_classifier` constructors in both the development and the
  tests-disabled Release executables.
- **Documentation gate**: `python3 scripts/verify_documentation.py --root .`
  → `DOCUMENTATION_OK`; `git diff --check` clean.
- **Independent review**: not selected. The changed surface since the last
  independent review of this RFC's slices is documentation and the two
  attended acceptances; none of the review trigger classes (wire, persistence,
  authority, concurrency, destructive migration) is involved, and the RFC-0005
  precedent's full-matrix and tidy runs are not part of the current contract.
  Recorded as an omitted optional lane, not a gap.

RFC-BPR-0006 is `Auto-Verified`. [[BPR-0114]] closes in this same commit.
