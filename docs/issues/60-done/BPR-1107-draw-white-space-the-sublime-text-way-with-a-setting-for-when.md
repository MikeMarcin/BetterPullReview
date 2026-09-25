---
schema: "implement-rfc.card.v1"
id: "BPR-1107"
title: "Draw white space the Sublime Text way, dots for spaces and lines for tabs, with a setting for when"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"origin":"owner rejection of attended candidate BPR-0986-rendered-panes-v001, session cbd48b91000b00f1e72e1ad62c7c384c, decision 6b7243c3-f194-4396-a3b9-0f8309dabfcf, 2026-09-24","origin_rfc":"RFC-BPR-0034","related_rfcs":["RFC-BPR-0006","RFC-BPR-0018"],"related_cards":["BPR-0986","BPR-0790","BPR-0791"],"owner_reference":"docs/verification/BPR-1107-white-space-drawing/owner-sublime-reference-2026-09-24.png","reservation":"docs/issues/_reservations/BPR-1106--BPR-1110--72437bc3b205.json","owner_decisions":{"date":"2026-09-25","session":"7083474240820608cdf9ff551ddd298f","drawing":"5a19ba24-8687-4719-bdca-294d1b1a02c7 accept (tab is a line, not line + dot)","choices":"0eb6d682-db01-4e68-be37-a16bd03745ba accept, default renamed Selection"},"design_page":"docs/design/BPR-1107-white-space-v2.html"}
acceptance: [{"criterion":"AC1 A design page shows the drawing and the setting on the four themes, with the owner's reference beside it, rendered and inspected headless","outcome":"pass","revision":"c0268e85","evidence":{"kind":"artifact","detail":"docs/design/BPR-1107-white-space-v2.html, the page as accepted with the owner's two corrections: his own text enlarged in Light and Dark, the panes in all four themes, the Type card in all four themes showing Selection, each choice in Light and Dark, the default and what is left out. Every image is a real capture from the gated courses (BPR_WHITE_SPACE_CAPTURE_DIR in qt_diff_workbench_scale_200 and qt_appearance_settings_panel) under docs/verification/BPR-1107-white-space-drawing/v2/. Rendered headless with Chrome --headless=new --screenshot at 1360 px and inspected. v1 (eb9a94a9) is kept as reviewed, with a pointer.","path":"docs/design/BPR-1107-white-space-v2.html"}},{"criterion":"AC2 The review panes and the preview draw spaces as dots and tabs as lines to their stop, through one shared painter, for each setting value offered; qt_diff_workbench_scale_100/125/200, qt_code_sample_painter, qt_type_preview and qt_theme_preview_strip pin it, and each fails with the drawing switched off","outcome":"pass","revision":"c0268e85","evidence":{"kind":"command","detail":"At eb9a94a9: paint_white_space is the one renderer (panes and preview through paint_white_space_of_block, tiles through paint_code_sample); the core decides which characters (core::white_space_drawn); the six rows pass and each fails with the drawing switched off. At c0268e85 the painter is unchanged and qt_code_sample_painter adds test_the_owners_example_draws_a_tab_as_a_line_alone on the owner's text byte for byte: 29 marks, each tab lit across its cell and at most two rows tall, each space a centred dot at least three rows tall, nothing on a letter. A painter that adds a dot near a tab's end fails it ('a tab is a line alone: no dot anywhere on it'). The six rows pass on macOS at c0268e85.","path":"docs/issues/60-done/BPR-1107-draw-white-space-the-sublime-text-way-with-a-setting-for-when.md#the-two-corrections-built-mac-2026-09-25-c0268e85"}},{"criterion":"AC3 The setting is a live control on the Appearance page and persists; qt_appearance_settings_panel, qt_appearance_profile, sqlite_appearance_settings_storage and the appearance end-to-end contracts pass","outcome":"pass","revision":"c0268e85","evidence":{"kind":"command","detail":"White space row beneath Tab width dispatches the typography intent; the default choice is labelled Selection (qt_appearance_settings_panel pins the six labels) and the preview caption says 'white space drawn in the selection' (qt_type_preview). Appearance store version 8 round-trips every choice, migrates version 7 to the default and refuses a value past the CHECK; the stored value of the renamed choice is unchanged (ordinal 1, stable name selection). 22 of 22 appearance, settings-window and end-to-end rows pass at c0268e85.","path":"docs/issues/60-done/BPR-1107-draw-white-space-the-sublime-text-way-with-a-setting-for-when.md#the-two-corrections-built-mac-2026-09-25-c0268e85"}},{"criterion":"AC4 The owner accepts the rendered drawing and the setting in an attended session: one question for the drawing and one for the setting's choices and default","outcome":"pass","revision":"c0268e85","evidence":{"kind":"observation","detail":"Owner verdicts, attended session 7083474240820608cdf9ff551ddd298f, packet docs/verification/reviews/owner-decisions-20260924-scoped-v001, both actor human with fingerprints equal to the prepared candidates' in session-identity.json. The drawing: candidate BPR-1107-white-space-drawing-v001 (eb9a94a9), fingerprint 141b09411933203f185dfb4abe4953ee86cc4a39e4e6cf5457ef5642cd054357, decision 5a19ba24-8687-4719-bdca-294d1b1a02c7, sequence 30, 2026-09-25T00:42:22Z: accept, note ending 'Just be sure tab renders as line not a line + dot, dot is space'. The choices: candidate BPR-1107-white-space-choices-v001 (eb9a94a9), fingerprint d787c4e10d188e9943f9c92792f9faf7317dfcfec0ac872bd3cdfeab08ca69a9, decision 0eb6d682-db01-4e68-be37-a16bd03745ba, sequence 29, 2026-09-25T00:16:27Z: 'Accept the six choices, default Under the selection', note 'Just call it selection.' Both corrections are made at c0268e85: the default is labelled Selection, and a tab is a line alone, checked on the owner's own text. The drawing is unchanged, so no further confirmation is asked.","path":"docs/issues/60-done/BPR-1107-draw-white-space-the-sublime-text-way-with-a-setting-for-when.md#owner-verdicts-2026-09-25-the-drawing-and-the-six-choices-accepted-with-two-corrections-attended-session-70834742"}}]
---

# BPR-1107: Draw white space the Sublime Text way, dots for spaces and lines for tabs, with a setting for when

## Why

[[BPR-0986]] made the review panes paint the Appearance preview's tab markers
on every leading tab (`55e9405f`). On 2026-09-24 the owner rejected that
rendering in attended session `cbd48b91000b00f1e72e1ad62c7c384c` (packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
candidate `BPR-0986-rendered-panes-v001`, revision `55e9405f`, fingerprint
`5b87471b…`, decision `6b7243c3-f194-4396-a3b9-0f8309dabfcf`, sequence 580,
19:12:17Z, `actor: human`): **reject**, with this note, verbatim:

> Should follow the sublime text model with dots and lines and have settings to control white space. We don't need to support all their settings but at least easy and impactful ones.

The rest of the note is Sublime Text's own documentation of its
`draw_white_space` setting, pasted verbatim (tabs as the owner pasted them;
trailing blank lines omitted):

```text
	// Controls when white space is drawn. Any of the following options may be
	// combined:
	// - "selection": Draw white space under the current selection.
	// - "leading": Draw any white space between the beginning of a line and the
	//              first character.
	// - "enclosed": Draw white space enclosed by other characters.
	// - "trailing": Draw white space following the last character on a line.
	// - "isolated": Draw white space on lines containing no other characters.
	// - "all": All of the above, ie. always draw white space.
	//
	// These options may be further refined by appending any of the following
	// separated by an underscore:
	// - "none": Don't draw this kind of white space.
	// - "tabs": Only draw tabs here.
	// - "spaces": Only draw spaces here.
	// - "mixed": Only draw white space that does not match the indentation
	//            style. For example if "translate_tabs_to_spaces" is true only
	//            draw tabs.
	// - "mixed_tabs": Like "mixed" but only draw tabs.
	// - "mixed_spaces": Like "mixed" but only draw spaces.
	// - "all": Draw both tabs and spaces. This is the default.
	//
	// Note that options are applied in sequence. So a later option may override
	// an earlier one.
	//
	// Examples:
	// - ["selection", "trailing", "isolated"]:
	//     Draw white space at the end of any lines and under the selection.
	//
	// - ["all_tabs", "selection"]:
	//     Draw tabs anywhere and any white space under the selection.
	//
	// - ["all_mixed"]:
	//     Draw any white space that does not match the indentation style.
	//
	// - ["leading_mixed", "isolated_mixed"]:
	//     Draw any indentation that does not match the indentation style.
	//
	// - ["selection_mixed_tabs"]:
	//     Draw only tabs under the selection and only if the indentation style
	//     is spaces.
	//
	// - ["all_tabs", "selection"]:
	//     Draw all tabs and any white space under the selection.
	//
	// - ["all", "selection_none"]:
	//     Inverse of the default. Draw white space everywhere except under the
	//     selection.
	"draw_white_space": ["selection"],
```

He attached a Sublime Text window as the reference, kept here cropped to its
text (the full 3648x2216 original, SHA-256
`192322df1f0049d75f671ddaddc0ebd4c597ddfd0b164953fbaf805339e40616`, stays in
the session's decision record):

![Sublime Text drawing spaces as dots and tabs as lines](../../verification/BPR-1107-white-space-drawing/owner-sublime-reference-2026-09-24.png)

In it every space, leading, enclosed or trailing, is a small centred dot. Each
tab is a thin horizontal line running from where the tab starts to its tab
stop, so a tab that begins mid-stop is shorter ("Another tab, smaller because
of spacing"). Faint vertical guides mark the indentation stops. The editor's
status bar reads "Tab Size: 4".

## Scope

- **Draw.** In the review panes (both panes and one pane), draw a space as a
  dot and a tab as a line to its tab stop, in the white-space colour, at the
  pane's own tab width. The Appearance page's preview draws the same thing
  through the same painter. RFC-BPR-0034 C4 makes the preview "the review
  window's painter in miniature", and the shared helper stays the only
  renderer. It replaces `paint_tab_markers`' current leading-tab markers.
- **When.** A setting chooses when white space is drawn. At least the easy and
  impactful choices from the owner's list: none, selection, leading,
  trailing, and all. Consider enclosed, isolated and the tabs-only,
  spaces-only and mixed refinements, and include the ones that are cheap. Say
  which are left out and why. The default is proposed with the design and
  confirmed by the owner.
- **Where the setting lives.** On the Appearance page's Type card beside Tab
  width, as one more one-line row with an info glyph, in RFC-BPR-0034's row
  language. It is persisted with the typography, per profile, through the
  existing appearance store. A schema change is a migration with a test.
- **The colour.** Reuse the role the markers use today
  (`code_line_number_text`), unless the design shows it fails on some theme.
  A new role is an RFC-BPR-0006 decision, and this card does not take it.
- **Line identity is untouched.** Drawing white space changes no text, no
  selection, no copy and no line mapping.

## Acceptance

- AC1 A design page shows the drawing and the setting on the four themes, with
  the owner's reference beside it, rendered and inspected headless.
- AC2 The review panes and the preview draw spaces as dots and tabs as lines to
  their stop, through one shared painter, for each setting value offered.
  `qt_diff_workbench_scale_100/125/200`, `qt_code_sample_painter`,
  `qt_type_preview` and `qt_theme_preview_strip` pin it, and each fails with
  the drawing switched off.
- AC3 The setting is a live control on the Appearance page and persists.
  `qt_appearance_settings_panel`, `qt_appearance_profile`,
  `sqlite_appearance_settings_storage` and the appearance end-to-end contracts
  pass.
- AC4 The owner accepts the rendered drawing and the setting in an attended
  session: one question for the drawing and one for the setting's choices and
  default.

## Boundaries

The core decides, the host performs, Qt presents. The setting is appearance
state owned by the appearance domain. The painter only reads it. No
stylesheet and no hard-coded palette. [[BPR-0986]] keeps the current-line band
and the two hunk-header roles; this card owns white space.

## Built, 2026-09-24 (Mac batch 7)

**The drawing.** `paint_white_space` in `code_sample_painter` replaces
`paint_tab_markers`. It is now the only white space renderer.
- A space is a small dot at the middle of its cell.
- A tab is a thin line from where it starts to its tab stop, a hair short of
  each end, so a tab that begins partway to a stop is shorter.
- Both use `code_line_number_text` at the old markers' faint alpha. No new
  role is added (the RFC-BPR-0006 boundary holds).

Three surfaces draw through it:
- the review panes (both panes and one pane) and the Appearance preview,
  through `paint_white_space_of_block`, which places each mark where the
  editor put the character (`cursorRect`), so the tab stops are the editor's
  own;
- the theme tiles, through `paint_code_sample`.

A line is under the selection when the pane's selection wash holds it: the
two panes pass their washed lines, and the one pane its unified selection.

**Which characters: the core decides.** `core::white_space_drawn`
(`src/core/src/white_space.cpp`) takes one line's characters, classified as
space, tab or other, plus the choice and whether the line is selected. It
answers which characters are drawn. Leading and trailing are measured from the
first and last other character, and a line of white space alone is both.

**The setting.** `core::white_space_drawing`, carried on
`code_font_settings::WhiteSpace`, has six choices:

| Choice | Sublime Text | Draws |
|---|---|---|
| None | `[]` | nothing |
| Under the selection (default) | `["selection"]`, Sublime's default | every white space character on a selected line |
| Leading | `["leading"]` | indentation |
| Trailing | `["trailing"]` | from the last other character to the end |
| Leading and trailing | `["leading", "trailing"]` | both, so also a line of white space alone (Sublime's "isolated") |
| All | `["all"]` | everything, enclosed white space too |

- **Where it lives.** `validate_typography` refuses a value outside the
  vocabulary (`typography_field::white_space`). The Type card gains a row,
  **White space**, directly beneath Tab width: a one-line row with an info
  glyph and a choice, dispatched as the typography intent the other rows use.
- **Where it persists.** The appearance store is now schema **version 8**. It
  adds `code_white_space INTEGER NOT NULL CHECK(... BETWEEN 0 AND 5)`. A
  version-7 file is carried across a fresh table and takes the fresh default.
  A value written past the CHECK is refused whole on load.
- **Who reads it.** The panes read it from the profile's typography in
  `diff_workbench::adopt_appearance`, the preview from its adopted
  typography, and the tiles from the panel. The preview's caption states the
  choice.
- **The RFC.** RFC-BPR-0034's Type paragraph and C3 now say twelve rows, with
  a dated note that does not reopen C1 to C6.

**Left out, and why** (also on the design page):
- Combining choices: one at a time keeps the row one line. Leading and
  trailing is the one combination with its own entry.
- Enclosed and isolated on their own: enclosed alone answers nothing All does
  not, and isolated is already drawn by the leading and trailing choices.
- The `_tabs`, `_spaces` and `_none` refinements would triple the list.
- The `_mixed` refinements need an indentation style the review window does
  not have.
- Indentation guides are a separate Sublime setting and are not white space.

**AC1, the design page.**
[BPR-1107-white-space-v1.html](../../design/BPR-1107-white-space-v1.html) shows:
- the owner's reference beside the panes;
- the panes in all four themes;
- the Type card in all four themes;
- each choice in Light and Dark;
- the proposed default and what is left out.

Every picture is a real capture, written by gated courses:
`BPR_WHITE_SPACE_CAPTURE_DIR` runs `test_white_space_design_captures` in
`qt_diff_workbench_scale_200` and `test_white_space_row_design_captures` in
`qt_appearance_settings_panel`. The captures are stored under
`docs/verification/BPR-1107-white-space-drawing/`. The page was rendered
headless with Chrome (`--headless=new --screenshot`) at 1360 px and
inspected: every image loads and the four themes read. The offscreen host
resolves the code font to a proportional face. The page says so, because the
marks follow whatever font is in effect.

**Verification** (macOS 26.6.2 arm64, `build/dev`):
- `core_appearance_policy` has two new tests.
  `test_white_space_drawing_follows_the_choice` covers each choice on the
  reference's first line, tabs, a line of white space alone, empty and
  white-space-free lines, and an out-of-vocabulary value.
  `test_white_space_vocabulary_is_closed` covers names, the default, bounds
  and the malformed refusal.
- `qt_code_sample_painter`:
  - `test_white_space_is_dots_and_lines_to_the_stop` reads pixels: a tab's
    line at the row's middle, a space's dot, and a partial tab ending at its
    stop;
  - `test_each_choice_draws_the_white_space_it_names` covers each choice on a
    painted sample, and an editor line through `paint_white_space_of_block`.
- `qt_type_preview` counts 5 under the selection (the selected row's two tabs
  and three spaces), 5 for leading, 21 for all and 0 for none, and the caption
  states the choice.
- `qt_theme_preview_strip`: all draws on the tiles; none and under the
  selection do not (the tile snippet selects nothing).
- `qt_diff_workbench_scale_100/125/200` count:
  - 12 for leading (six indenting tabs a side);
  - 32 for all;
  - 0 for trailing;
  - 2 under the selection (the selected change's two spaces);
  - 0 for none.
- `qt_appearance_settings_panel`: `test_white_space_is_a_row_beside_tab_width`
  checks the six items in order with their labels, that adoption shows the
  choice without dispatching, that the row sits beneath Tab width, and that a
  choice leaves as a typography intent carrying the rest. The Type card now
  has twelve rows.
- `sqlite_appearance_settings_storage`:
  `test_white_space_round_trips_migrates_and_refuses` round-trips every
  choice, migrates version 7 with every value kept and white space defaulted,
  checks that the CHECK refuses 6, and that a value written past the CHECK is
  refused as `integrity_failure`. The down-edit helper learned version 7.
- **Negative control (AC2).** With the drawing switched off in
  `paint_white_space`, all six named rows fail: `qt_code_sample_painter`
  ("one mark per white space character"), `qt_theme_preview_strip` ("all draws
  the snippet's spaces on the tiles"), `qt_type_preview` ("under the
  selection, the selected row's two tabs and three spaces"), and
  `qt_diff_workbench_scale_100/125/200` ("every choice but none draws
  something on these lines"). Restored, they pass.
- Also passing, 34 of 34:
  - the core contrast and theme-exchange rows;
  - `qt_settings_window` at every scale, `qt_surface_capture_course_scale_*`
    and `qt_workbench_window_scale_*`;
  - `qt_composed_github_scale_100` and `qt_syntax_pane_composition`;
  - `application_appearance_host` and the four appearance end-to-end
    contracts;
  - `application_large_review_benchmark`, `_resources` and `_loaded_close`.

  `application_large_review_history`, run beside a build on the same
  machine, failed its 100 ms input-pulse bound once (a 1026 ms pulse in
  epoch refresh 6). Every refresh stayed inside its 12 s budget. The serial
  reruns are recorded under "application_large_review_history, rerun alone"
  below.
- clang-format 23.1.1 passes every touched file. The project tidy script's two
  findings on new lines were fixed: string concatenation in the version-8
  migration, and a lambda name in the painter test. What remains is
  pre-existing, on untouched lines:
  - `theme_preview_strip.cpp:461` and `appearance_settings_database.cpp:168`
    and `:1229`;
  - `appearance_settings_panel_tests.cpp:1069`;
  - `appearance_settings_storage_tests.cpp:781`.

## Owner asks — parked on human-review (AC4, two questions)

Prepared for a later session, not asked. The candidate is this card's commit.
Show the design page with the panes rendered on the build.

1. **The drawing.** Section 1 of the design page shows the panes in four
   themes beside your Sublime Text reference: a dot per space and a line to
   the tab stop per tab, faint, in the line-number colour. **Do you accept
   this as how the review window draws white space?**
2. **The choices and the default.** Section 2 shows one row on the Type card,
   White space, beneath Tab width. It offers None, Under the selection,
   Leading, Trailing, Leading and trailing, and All, and the proposed default
   is Under the selection (Sublime's own). **Do you accept these six choices
   with Under the selection as the default?** (The alternative default worth
   naming is Trailing.)

## application_large_review_history, rerun alone (2026-09-24, bpr-main-mac)

Run at `a7dd1cd2`, whose product source is `f6d38c58`'s: nothing under `src/`
or `tests/` changed between them. The build was current, and nothing else was
running except the editor's background indexer (load average 2.9 to 3.8).

- **First run: failed** its 100 ms pulse bound, in one refresh out of 31.
  `epoch_refresh_8`'s largest gap was 143.0 ms, and that refresh took 9.8 s
  against 8.3 to 8.7 s for the rest. The other 30 refreshes' largest gaps
  have a median of 68.4 ms and a maximum of 71.4 ms. Every refresh stayed
  inside 12 s.
- **Second run, straight after: passed**, 318 s. The 31 refreshes' largest
  gaps have a median of 68.2 ms and a maximum of 72.4 ms. The refreshes took
  8.5 to 8.7 s, and the source selections' largest gap was 34.2 ms.

The refresh gaps are where [[BPR-1043]] left them (median 68.8 ms, maximum
75.3 ms), so the white-space drawing adds nothing measurable. A single
refresh 1.2 s slower than the rest, with one long gap, looks like the
process being descheduled rather than work the refresh does. It did not
recur. The row is a sampled Debug observation, and one outlier in 31 is not
a regression. It is recorded here and not carded.


## Linux at `45191097` (bpr-main-linux, 2026-09-24)

**AC2's and AC3's rows pass on Linux, 16 of 16, including the appearance
store's version 8.** No macOS or Windows run was made from this station.

The run was in the station's main checkout, with a clean tree, at
`45191097`, which contains `eb9a94a9`. WSL2 Ubuntu 26.04, GCC 15.2, Qt
6.11.2, `build/dev` (Debug, `-Werror`), rebuilt at `9ee4ee50` with no
`warning:` line. `45191097` changes nothing under `src/`, `tests/` or
`cmake/` from it. `ctest --test-dir build/dev -j1 --output-on-failure`,
offscreen, with `DISPLAY`, `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS`
unset and fresh absolute `HOME` and XDG roots.

| Row | Result |
| --- | --- |
| `core_appearance_policy` | pass, 0.00 s |
| `qt_code_sample_painter` | pass, 0.02 s |
| `qt_type_preview` | pass, 0.04 s |
| `qt_theme_preview_strip` | pass, 0.09 s |
| `qt_diff_workbench_scale_100` | pass, 2.75 s |
| `qt_diff_workbench_scale_125` | pass, 2.98 s |
| `qt_diff_workbench_scale_200` | pass, 3.51 s |
| `qt_appearance_settings_panel` | pass, 0.22 s |
| `qt_appearance_profile` | pass, 0.26 s |
| `sqlite_appearance_settings_storage` | pass, 0.37 s |
| `application_appearance_host` | pass, 0.02 s |
| `application_appearance_profile_consumers` | pass, 0.01 s |
| `application_native_appearance_probe_contract` | pass, 2.90 s |
| `application_appearance_theme_customization_contract` | pass, 5.85 s |
| `application_appearance_gesture_contract` | pass, 6.53 s |
| `application_appearance_store_fallback_contract` | pass, 3.38 s |

The runs added nothing to `/tmp` and no `bpr-` name to `/run/user/1000`.
The negative control (the drawing switched off) was not repeated here; it is
the macOS run above. `application_large_review_history` was not run.

The acceptance record above is unchanged, and the card stays parked on AC4.
The log is a station record under
`build/logs/bpr-main-linux-20260924/batch7-linux/bpr-1107-rows.log` and is
not committed.

## Windows at `3f8f954d` (bpr-main-win, 2026-09-24): 13 of 16 pass, and the three workbench rows fail on [[BPR-1032]]'s change, not this one

`3f8f954d` contains `eb9a94a9`. The tree is `build/dev` (MSVC 19.51 Debug,
Qt 6.11.2), rebuilt at `3f8f954d`, `ninja` exit 0. The rows ran with
`ctest --preset dev -j1 --output-on-failure`, offscreen, each contained or
Windows-uncontained as registered.

| Row | Result |
| --- | --- |
| `core_appearance_policy` | pass, 0.04 s |
| `qt_code_sample_painter` | pass, 0.18 s |
| `qt_type_preview` | pass, 0.39 s |
| `qt_theme_preview_strip` | pass, 2.13 s |
| `qt_diff_workbench_scale_100` | **fail**, 2.16 s |
| `qt_diff_workbench_scale_125` | **fail**, 2.32 s |
| `qt_diff_workbench_scale_200` | **fail**, 2.66 s |
| `qt_appearance_settings_panel` | pass, 4.14 s |
| `qt_appearance_profile` | pass, 1.99 s |
| `sqlite_appearance_settings_storage` | pass, 6.56 s; the store's version 8 |
| `application_appearance_host` | pass, 0.05 s |
| `application_appearance_profile_consumers` | pass, 0.03 s |
| `application_native_appearance_probe_contract` | pass, 15.05 s |
| `application_appearance_theme_customization_contract` | pass, 27.59 s |
| `application_appearance_gesture_contract` | pass, 36.05 s |
| `application_appearance_store_fallback_contract` | pass, 23.54 s |

**The three failures are not this card's.** Each stops at the suite's first
numbering check, [[BPR-0455]]'s "past a one-sided block the two panes show
different numbers on the same row". That check runs before any white-space
check. A reverted trial names the cause. At `3f8f954d`, with only
`measured_alignment_axis.cpp` put back as it was before [[BPR-1032]]'s
`27c35729`, the suite passes in full (`diff-workbench-tests:pass`). That run
includes `test_white_space_design_captures` and the white-space choices in
`test_selected_composition`, at scale 1.0. On Windows, BPR-1032's
measurement stops the shared scrollbar about a quarter of the way down; its
post-close note has the numbers. The fix needs a follow-up card, whose title
has gone to the integrator. Until it lands, this card's AC2 rows cannot pass
on Windows.

`%TEMP%` gained no entry from the batch. The negative control (the drawing
switched off) was not repeated here; it is the macOS run above.
`application_large_review_history` was not run.

The acceptance record above is unchanged, and the card stays parked on AC4.
The logs are station records under `build/logs/batch7-main-win/`
(`rows-3f8f954d.log`, `trial-revert-1032-*.log`, `probe-*.log`), and are not
committed.

## Owner verdicts, 2026-09-25: the drawing and the six choices accepted, with two corrections (attended session 70834742)

Source: attended session `7083474240820608cdf9ff551ddd298f`, packet
[owner-decisions-20260924-scoped-v001](../../verification/reviews/owner-decisions-20260924-scoped-v001/operator-packet.md),
hand-back at sequence 32. Both verdicts are `actor: human` and carry their
prepared candidate's fingerprint in
[session-identity.json](../../verification/reviews/owner-decisions-20260924-scoped-v001/session-identity.json).

**Question 1, the drawing.** Candidate `BPR-1107-white-space-drawing-v001`,
revision `eb9a94a9`, fingerprint `141b0941…`, decision
`5a19ba24-8687-4719-bdca-294d1b1a02c7`, sequence 30, 2026-09-25T00:42:22Z:
**accept**, with this note. Its first line, verbatim:

> I think I had inadventently put a space after the tab in my sublime example

Then this example, byte for byte (the indents on the second and third lines
are tab characters):

```text
     4 spaces.          8 more spaces
		2 tabs.
	1 tab.	Another tab, smaller because of spacing
```

And its last line, verbatim:

> Just be sure tab renders as line not a line + dot, dot is space

**Question 2, the choices and the default.** Candidate
`BPR-1107-white-space-choices-v001`, revision `eb9a94a9`, fingerprint
`d787c4e1…`, decision `0eb6d682-db01-4e68-be37-a16bd03745ba`, sequence 29,
2026-09-25T00:16:27Z: **select**, verbatim:

> Accept the six choices, default Under the selection (proposed; Sublime's own
> default)

Note, verbatim:

> "Under the selection" is weird phrasing, made me want to look at lines
> beneath the selection. Just call it selection.

**What they settle.** AC4 is accepted on both questions, with two corrections
to make before Done:

1. **The name.** The default choice is called **Selection** in the product and
   in the docs. The stored value does not change: ordinal 1, stable name
   `selection`, appearance store version 8. So no migration is needed.
2. **A tab is a line alone.** The owner's reading is right. v1's design
   captures copied the reference's stray spaces (`\t\t 2 tabs.` and
   `\t 1 tab.`), so each tab run in them ended in a dot. The painter draws
   one mark per character, a line for a tab and a dot for a space, so the
   drawing itself needs no change. The captures are retaken with the owner's
   text above, and a check on that exact text pins a tab as a line with no
   dot.

The card leaves human-review for **Ready** on macOS for those two
corrections.

## The two corrections, built (Mac, 2026-09-25, `c0268e85`)

The change was built and verified as `c664ead1`, then rebased onto
bpr-main-win's claim of [[BPR-0974]] and pushed as `c0268e85`. The two carry
the same patch (patch-id `36a7a528`). Their trees differ only in that claim's
two documentation files, so the runs below hold for `c0268e85`.

**The name.** The White space row's second item says **Selection**. The
preview's caption says "white space drawn in the selection". The source
comments and test messages that named the choice use the same word. The
stored value is unchanged: ordinal 1, stable name `selection`, appearance
store version 8. So nothing migrates, and a profile saved with the old label
still reads as Selection. RFC-BPR-0034's follow-on note names it too.

**A tab is a line alone.** The painter was already right, and it is
unchanged: `paint_white_space` draws one mark per white space character, a
line for a tab and a dot for a space. The dots after the tabs in v1's
captures came from the capture fixture, which had copied the reference
picture's stray spaces (`\t\t 2 tabs.` and `\t 1 tab.`).

- **The fixture.** `test_white_space_design_captures` now uses the owner's
  text from his note, byte for byte. Two trailing spaces are still added to
  its first line, so Trailing has something to draw.
- **The check.** New in `qt_code_sample_painter`:
  `test_the_owners_example_draws_a_tab_as_a_line_alone`. It lays out the
  owner's three lines in a `QPlainTextEdit` and paints them with All through
  `paint_white_space_of_block`, the panes' own path. It then reads every
  character's cell at the row's middle:
  - a tab is lit all the way across its cell, and never more than two pixel
    rows tall (a line, with no dot on it);
  - a space is lit at its centre, at least three rows tall, and not at its
    edges (a dot, not a line);
  - a letter's cell has nothing drawn in it, so no dot follows a tab where
    the owner's text has none.

  It counts 29 marks: 18 spaces, then two tabs and a space, then two tabs and
  six spaces. The offscreen host answers the fixed font with the proportional
  system face (a 6 px space), so the check sets 12 px of letter spacing to
  give every cell room.
- **Negative controls.** A painter that also draws a dot near a tab's end
  fails the new check at "a tab is a line alone: no dot anywhere on it". A
  dot drawn just past a tab's stop fails the existing
  `test_white_space_is_dots_and_lines_to_the_stop`. Restored byte for byte,
  both pass.

**Rendered.** [Design page v2](../../design/BPR-1107-white-space-v2.html),
with its captures under
[`BPR-1107-white-space-drawing/v2/`](../../verification/BPR-1107-white-space-drawing/v2),
written by the same gated courses (`BPR_WHITE_SPACE_CAPTURE_DIR`). It opens
with the owner's lines enlarged, in Light and Dark. Line 2 is two lines and
then "2 tabs.", and line 3 is a line, "1 tab.", a short line to the stop, and
"Another tab". The Type card shows Selection. The page was rendered headless
with Chrome (`--headless=new --screenshot`) at 1360 px and inspected: every
image loads. [v1](../../design/BPR-1107-white-space-v1.html) is kept as the
owner reviewed it, with a pointer to v2.

**Verification** (macOS 26.6.2 arm64, `build/dev`, Qt 6.11.2, at `c0268e85`):

- 22 of 22 pass: `core_appearance_policy`, `qt_code_sample_painter`,
  `qt_type_preview`, `qt_theme_preview_strip`,
  `qt_diff_workbench_scale_100/125/200`, `qt_appearance_settings_panel`,
  `qt_appearance_profile`, `sqlite_appearance_settings_storage`,
  `qt_syntax_pane_composition`, `application_appearance_host`,
  `application_appearance_profile_consumers`,
  `application_native_appearance_probe_contract`, the three appearance
  end-to-end contracts, `qt_settings_window` at 100/125/150/200 and
  `qt_surface_capture_course_scale_100`.
- `qt_appearance_settings_panel` pins the six labels, Selection second.
  `qt_type_preview` pins the caption's "white space drawn in the selection",
  and that it never says "under the selection".
- clang-format 23.1.1 passes all eleven touched files. The project tidy
  script reports nothing on the touched lines. Its two findings are the
  pre-existing ones in `appearance_settings_database.cpp`, at 168 and 1229.

**AC4 holds on the owner's verdicts, with both corrections made.** The
drawing itself did not change, and his condition, that a tab is a line
alone, is now checked on his own text. The rename is exactly what he asked
for. No further confirmation is asked. Every criterion is met, and the card is
**Done**.

**Owed elsewhere, ordinary confirmation.** Linux passed AC2 and AC3's rows at
`45191097`, before this change. A rerun of `qt_code_sample_painter`,
`qt_type_preview` and `qt_appearance_settings_panel` there would cover the
new check and the label. On Windows, `qt_diff_workbench_scale_100/125/200`
fail on [[BPR-1032]]'s measurement, not on white space (see the Windows
record above). That follow-up is reserved as BPR-1123 for bpr-main-win.

## Post-close: Linux at `85e522d8` (bpr-main-linux, 2026-09-24)

**The Linux rerun owed above passes, 3 of 3.** Nothing else is owed from
Linux. The Linux note above, at `45191097`, predates `c0268e85`.

The run was in the station's main checkout, with a clean tree, at
`85e522d8`, which contains `aac61b27` and `c0268e85`. WSL2 Ubuntu 26.04, GCC
15.2, Qt 6.11.2, `build/dev` (Debug, `-Werror`), rebuilt at `85e522d8` with
no `warning:` line. `ctest --test-dir build/dev -j1 --output-on-failure`,
offscreen, with `DISPLAY`, `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS`
unset and fresh absolute `HOME` and XDG roots. The names were resolved with
`ctest -N`. The three rows ran together with [[BPR-0644]]'s three.

| Row | Result |
| --- | --- |
| `qt_code_sample_painter` | pass, 0.02 s |
| `qt_type_preview` | pass, 0.04 s |
| `qt_appearance_settings_panel` | pass, 0.20 s |

The run added nothing to `/tmp` and no `bpr-` name to `/run/user/1000`. The
log is a station record, not committed:
`build/logs/bpr-main-linux-20260924/batch7-linux/closeout-1107-0644-rows.log`,
with `build-dev-85e522d8.log`.

## Post-close: Windows at `2ff7e20c` and `fbab68c9` (bpr-main-win, 2026-09-25)

**The Windows rerun owed above passes, and the three workbench rows that
failed on Windows now pass.** Nothing else is owed from Windows. The Windows
note above, at `3f8f954d`, predates `c0268e85`.

JaegAW, Windows 11 25H2, MSVC 19.51, Qt 6.11.2, `build/dev` Debug (Ninja),
offscreen. Each row ran contained as registered, serially
(`ctest --preset dev -j1 --output-on-failure`).

| Row | Revision | Result |
| --- | --- | --- |
| `qt_code_sample_painter` | `2ff7e20c`, which contains `c0268e85` | pass, 0.23 s. It includes `test_the_owners_example_draws_a_tab_as_a_line_alone` |
| `qt_type_preview` | `2ff7e20c` | pass, 0.39 s |
| `qt_appearance_settings_panel` | `2ff7e20c` | pass, 4.06 s |
| `qt_diff_workbench_scale_100` | `fbab68c9` ([[BPR-1123]]) | pass, 15.90 s |
| `qt_diff_workbench_scale_125` | `fbab68c9` | pass, 16.82 s |
| `qt_diff_workbench_scale_200` | `fbab68c9` | pass, 17.92 s |

**The workbench rows.** They failed at `3f8f954d` on [[BPR-1032]]'s
measurement, before any white-space check ran. BPR-1123 found why. Since
BPR-1032, every re-measure shaped every line again, and Windows Debug's
debug Qt made that slow enough for the suite's geometry to arrive after its
jumps. The panes now keep their measured heights, and the suite waits for
geometry after a full first pass. So on Windows the whole suite runs,
including `test_white_space_design_captures` and the white-space choices in
`test_selected_composition`. With the 13 rows that passed at `3f8f954d`,
this card's Windows set is complete.

The acceptance record above is unchanged. The logs are station records under
`build/logs/bpr-1107-main-win/` and `build/logs/bpr-1123-main-win/`, not
committed.
