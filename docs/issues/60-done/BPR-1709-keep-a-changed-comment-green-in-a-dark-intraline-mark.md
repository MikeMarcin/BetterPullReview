---
schema: "implement-rfc.card.v1"
id: "BPR-1709"
title: "A changed comment stays green, brighter, in a dark intraline mark"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1709--BPR-1709--577d067613cc.json","kind":"ux","origin":"owner report 2026-10-21","owner_note":"On dark theme intra-line highlights on comments change the text from green to white. I'd prefer if it was just brighter green.","governing_rfc":"RFC-BPR-0006","related_cards":["BPR-0323","BPR-1156","BPR-1334","BPR-1533"],"regressed_by":"af4e2d386","requested_skill":"frontend-design"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3ed01a5eb","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^qt_diff_workbench_scale_(100|125|200)$'","detail":"test_dark_intraline_keeps_a_comment_green: a changed comment, classified syntax_comment, selected through the ribbon, in light then dark. For every mark in the base pane, head pane and Line details it reads the ink its layers give the first changed character and, at 2x, the colour the pane actually paints there (the most common non-fill pixel of the mark). It requires painted == layer ink, the emphasis floor, and for an ink that moved: the comment's hue within 6 degrees, at least 0.9 of its HSV saturation, and 4.5:1. Dark after: head pane #8ffe7d on #117277 at 4.50:1; base pane #66b559 kept on #58376a (3.83:1); Line details #66b559 kept on #672323 (4.51:1) and #104a17 (4.13:1). Light after: #393f47 on #a8a5cd, #3a4048 on #73b5ba, #555e69 kept on #f1b9b4 and #9effb5, each painted as layered. Failing-before control: with src/ at ab1698599 (the four product files are the same at a18cfe3ad) and this test, all three runs fail; 100% and 125% at 'and its saturation: a brighter green, not white' (dark head pane #d6ebd2), 200% at 'base pane: the changed comment is painted in its layer's ink, not the normal ink' (light, painted #1f2328 where the layer is #393f47; the before render shows dark marks painted #dcdcdc). macOS 26 arm64, Qt offscreen, dev preset Debug built -O0 -g0."}},{"criterion":"AC2","outcome":"pass","revision":"3ed01a5eb","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^core_appearance_contrast$'","detail":"test_emphasis_brightens_along_its_hue_before_tinting: #66b559 on #117277 (dark) misses the 3.5:1 emphasis floor, moves to 4.5:1 with its channels in proportion and its largest under 255, so brightening alone reached the floor; #ff8000 at full value tints toward white and stays orange (red 255, green above blue); the light toward-black case is pinned at its earlier value, #555e69 on #73b5ba gives #3a4048. The existing readable-ink and selection-ink tests pass unchanged."}},{"criterion":"AC3","outcome":"pass","revision":"3ed01a5eb","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(architecture_boundaries|command_correctness_registry_contract|core_appearance_contrast|qt_code_sample_painter|qt_theme_preview_strip|qt_diff_workbench_scale_.*|qt_markdown_code_.*|application_diff_workbench_public_path|application_line_details_public_path)$'","detail":"13 of 13 pass on the rebased candidate. Before the rebase the wider appearance set also passed: core_appearance_policy, application_appearance_host, application_appearance_profile_consumers, qt_appearance_settings_panel, qt_appearance_role_display, qt_colour_role_list, qt_appearance_profile, qt_suggestion_editor_1_0/2_0, qt_review_cycle_view_1_0/2_0, qt_source_search_panel_scale_1/2, application_appearance_theme_customization_contract, application_appearance_older_theme_contract, application_diff_workbench_shutdown_public_path and application_cpp_identifier_appearance. The comment cards' suggested-change renders (qt_markdown_code capture at 2x, dark and light) are pixel-identical before and after: that surface lays word fills as layout formats, which Qt merges, and no default syntax colour misses the floor on the unselected fills."}},{"criterion":"AC4","outcome":"pass","revision":"3ed01a5eb","evidence":{"kind":"command","command":"cmake --build build/dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths src/core/src/appearance_contrast.cpp src/adapters/qt/src/code_sample_painter.cpp tests/core/appearance_contrast_tests.cpp tests/adapters_qt/diff_workbench_tests.cpp; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"clang-format 22.1.8 passes on all files; clang-tidy passes on the four changed translation units with warnings as errors. The command-correctness registry's pin for tests/adapters_qt/diff_workbench_tests.cpp is resealed (b2e16ad40957 -> f6e1be6897c1); the round-trip contract's pins are unchanged, and src/app is untouched. Both documentation gates pass with the card under a stand-in allocated ID; with the provisional ID the gate refuses only the card's ID format."}}]
---

# BPR-1709: A changed comment stays green, brighter, in a dark intraline mark

## Owner note

The owner, 2026-10-21, verbatim:

> On dark theme intra-line highlights on comments change the text from green
> to white. I'd prefer if it was just brighter green.

"Comments" here are code comments, the green `syntax_comment` role of the dark
profile (`#66B559`), where an edit marks changed words with the intraline fill.

## Diagnosis

Rendered offscreen at 2x, before: inside every intraline mark of the
comparison panes and Line details, in both themes and on both sides, the
glyphs were painted in `syntax_normal` (`#DCDCDC` in dark, `#1F2328` in light).
The comment colour showed only outside the marks. There were two causes.

1. **The token layers were never painted.** `append_intraline_emphasis_span`
   (`code_sample_painter.cpp`, shared by the panes and Line details) laid the
   mark as one extra selection with the fill and the normal ink, then one
   foreground-only selection per classified token, so a token keeps its
   colour. Qt's `QTextLayout::draw` skips a selection that sets a foreground
   and no background wherever an earlier selection already drew the text. The
   selection list said green; the pixels were the normal ink. This has been
   so since `af4e2d386` (2026-09-20). The tests read the selection list only,
   so they passed.
2. **A colour that had to move lost its hue.** On a selected row the mark is
   derived from the washed band ([[BPR-0323]]), `#117277` on the added side
   in dark. The comment green reads 4.13:1 on the accepted fill but under the
   3.5:1 emphasis floor on that mark, so `core::readable_emphasis_foreground`
   moves it. It composited toward white and stopped at `#D6EBD2`, which reads
   as white.

The removed side has the same layering flaw, which fix 1 corrects. In the
default dark palette no red-side ink has to move: comment green on the
selected removed mark `#58376A` reads 3.83:1. The fix for cause 2 applies
equally to any colour on either side that does have to move.

Light theme had cause 1: a changed comment was painted near-black instead of
its own grey. It never had cause 2. Every light-theme fill wants dark ink, and
shading toward black already keeps hue and saturation.

The comment cards' suggested-change boxes (`markdown_code.cpp`) lay the word
fill and token colours as layout formats, which Qt merges. They kept the
comment green before and are unchanged.

## Change

- `append_intraline_emphasis_span` gives each token layer the mark's own fill
  under its ink, so Qt paints it. The fill is the one already there, so only
  the ink changes.
- `core::readable_emphasis_foreground`, on a fill that wants light ink, first
  scales every channel of the colour by one factor, keeping hue and
  saturation, until it reads at the text floor or its largest channel is full.
  Only then does it tint toward white. The toward-black path and the
  high-contrast variants are unchanged, and so is the 3.5:1 emphasis floor,
  which the owner accepted in an attended review. A colour that already reads
  is still kept exactly.

Colours, dark, selected added row: comment `#66B559` on `#117277` was
`#D6EBD2` (4.50:1, HSV saturation 0.11) and is now `#8FFE7D` (4.50:1,
saturation 0.51, the role's own). Unselected, the comment keeps `#66B559`:
4.13:1 on `#104A17`, 4.51:1 on `#672323`. These are above the accepted
emphasis floor and below the 4.5:1 text floor, as before. No palette value
changed.

`docs/ux/appearance-and-accessibility.md` describes the rule.

## Acceptance

- AC1: In the panes and Line details, a changed comment is painted in the ink
  its layers give it. In dark, where it must move, it keeps the comment's hue
  and saturation and reads at 4.5:1. The test fails before the change.
- AC2: The core rule brightens along the hue before tinting toward white and
  leaves the toward-black path as it was.
- AC3: The focused appearance, diff, Markdown and public-path checks pass.
- AC4: Format, tidy, registry reseal and documentation gates pass.

## Owner look (pending)

The owner has not yet looked. The coordinator shows the offscreen renders
inline. No attended session was opened and no build was frozen. Renders at 2x,
outside the repository:
`before-dark-2x.png`, `after-dark-2x.png`, `before-light-2x.png`,
`after-light-2x.png` (the selected changed comment in the panes and Line
details), and `comment-card-suggestions-{dark,light}-2x-unchanged.png`.

## Checkpoint

Product commit `3ed01a5eb` on branch `worktree-agent-acd0bdd3f5b96c890`,
rebased onto main `a18cfe3ad`. This card follows in its own commit. Not landed
on main. The coordinator allocates the ID and renames `BPR-1709` in
`src/core/src/appearance_contrast.cpp`,
`src/core/include/bpr/core/appearance_contrast.hpp`,
`src/adapters/qt/src/code_sample_painter.cpp`,
`src/adapters/qt/include/bpr/qt/code_sample_painter.hpp`, the two test files
and the appearance doc. The rename reseals the registry's
`diff_workbench_tests.cpp` pin.

Changed paths: `src/core/src/appearance_contrast.cpp`,
`src/core/include/bpr/core/appearance_contrast.hpp`,
`src/adapters/qt/src/code_sample_painter.cpp`,
`src/adapters/qt/include/bpr/qt/code_sample_painter.hpp`,
`tests/adapters_qt/diff_workbench_tests.cpp`,
`tests/core/appearance_contrast_tests.cpp`,
`tests/command_correctness_registry.json`,
`docs/ux/appearance-and-accessibility.md`.

The build tree was removed at the end.
