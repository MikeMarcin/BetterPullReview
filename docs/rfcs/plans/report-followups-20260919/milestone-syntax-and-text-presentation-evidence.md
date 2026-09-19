# Syntax and text-presentation evidence

Run `report-followups-run-20260919`, dispatch
`report-followups-dispatch-001`, producer `/root/report_followups`, mandate
revision 1. The accepted integrated source candidate is
`4e409bc967a9814e5a492f3eaf644254352e1df8`.

## Diagnosed causes and implemented behavior

- KSyntaxHighlighting reported language types such as `void` through
  `DataType`, while BetterPullReview mapped that role to the same yellow type
  used for inferred user types. The corrected adapter maps only explicit C/C++
  language type tokens to `syntax_builtin`; PascalCase types and upstream
  typedef-style `widget_t`/`value_type` spans remain `syntax_type`.
- KSyntaxHighlighting 6.28.1 has no Windows-resource grammar and its TOML
  grammar claims `*.rc`, producing the broad yellow capture and incorrect
  C-style comments. Case-insensitive `.rc` now selects a stable versioned
  `Windows Resource` identity backed by the packaged C lexical grammar. A
  bounded post-pass assigns resource keywords, ALL_CAPS resource macros and
  ordinary identifiers distinct roles without entering strings, comments or
  directive operands.
- The apparent diff indentation is authored content. The epoch fixture uses
  one- and two-tab rows; the recorded UIforETW source uses one, two and three
  tabs. `diff_workbench` publishes the decoded documents directly and syntax
  ranges affect format, not text. Natural vertical displacement is required by
  RFC-BPR-0004; inserting rows or reindenting would corrupt identity.
- A Qt regression contract now measures corresponding first-glyph positions
  in each pane, verifies exact `toPlainText` and semantic mappings, applies
  styled spans, then changes tab width from four to eight columns. Geometry
  changes while source, comparison, mapping and font identity remain intact.

Implementation commits are `f6cb5662e63e94b45bf78b7349313428659315ed`
(tab correspondence), `c098f5ea3725cab1a00d3d178936b33f36a8f98e`
(syntax/resource roles), and
`4e409bc967a9814e5a492f3eaf644254352e1df8` (critical-review correction for
typedef-style user types).

## Verification and review

At corrected candidate `4e409bc9`, the integration selector passed 16/16 in
97.67 seconds. It included `core_diff_compute`, `core_diff_semantics`,
`core_appearance_contrast`, `syntax_classification_contract`,
`qt_syntax_pane_composition`, all three `qt_diff_workbench_scale_*` variants,
`qt_appearance_profile`, the focused restoration stores/hosts, recorded
UIforETW loading, anonymous public paths and the attended replay course.
`coding-format-check` checked 700 files successfully in 16.972 seconds and
`git diff --check` passed.

The critical reviewer found one blocking counterexample in the first candidate:
upstream ISO C++ marks `*_t` and `*_type` names as `DataType`, so a blanket
conversion made user types blue. A real packaged-classifier probe reproduced
the error while the original contract still passed. The correction and exact
declaration/use regression assertions passed; the reviewer then reported no
remaining blocker. Review and test-runner commands were read-only and quiesced.

## Evidence limits

The Windows-resource support is a bounded lexical refinement, not a complete
resource compiler grammar. Deterministic macOS/headless Qt tests, source-image
inspection and recorded fixtures were used; no new native visual verdict was
required or claimed. The supplied screenshots do not reveal the originating
persisted font family or tab-width value, so the contract proves the supported
geometry rather than inferring those settings from pixels. Image and original
report paths plus SHA-256 digests are retained on BPR-0930 and BPR-0931.

