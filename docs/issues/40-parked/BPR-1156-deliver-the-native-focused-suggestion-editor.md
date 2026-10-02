---
schema: "implement-rfc.card.v1"
id: "BPR-1156"
title: "Deliver the native focused suggestion editor"
status: "Parked"
rfcs: ["RFC-BPR-0055", "RFC-BPR-0061"]
depends_on: ["BPR-1154", "BPR-1155"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC55-07", "RC55-09", "RC61-04"], "pack_alias": "PACK-CYCLE-14", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"RC55-07","outcome":"pending","revision":"fd5ca4412","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share passes on macOS arm64 at fd5ca4412 (core_suggestion_text_editing, qt_suggestion_editor_1_0/2_0, application_review_cycle_desktop_public_path): the gestures of b486149a5, with virtual space now only part of a box selection (a click, drag, Right, Up and Down stay on the text; Up and Down keep the column aimed for), and double and triple click that keep their word or line when the pointer drifts, driven through the window system in both modes. Owner judgment pending: BPR-1156-editor-gestures-v004 on the frozen candidate /Users/jaeger/bpr-cycles-selection-review.Ev9QTV (app sha256 af38e32d...f9c5538), superseding the undecided v003."}},{"criterion":"RC55-09","outcome":"pass","revision":"0d0872fbd5bfce9f3ff405b42ad649b7ed5ea5cb","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Card share: the composer hands exact replacement bytes, intent and rationale to its owner with no model, summarizer or second prompt and no file write (qt_suggestion_editor, application_review_cycle_desktop_public_path). Round preparation is BPR-1158."}},{"criterion":"RC61-04","outcome":"pending","revision":"fd5ca4412","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Owner rejected composer-layout v001 (event 713), v002 (719), v003 (742, an owner amendment in RFC-BPR-0055), v004 (765) and v005 (776: selection darker than usual, always in virtual space, double click not selecting the word). At fd5ca4412 the code view fills a selection with the solid Highlight every text box uses, under syntax colours kept readable on it (core::readable_selection_ink), ending at the text; only a box reaches past a line's end; and a double click keeps its word under pointer drift (qt_suggestion_editor_1_0/2_0, qt_review_cycle_view_1_0/2_0, application_review_cycle_desktop_public_path offscreen and under cocoa, through the window system). Owner judgment pending: BPR-1156-composer-layout-v006 on the frozen candidate /Users/jaeger/bpr-cycles-selection-review.Ev9QTV (app sha256 af38e32d...f9c5538)."}}]
---

Pack alias: **PACK-CYCLE-14** (live `BPR-1156`, allocated at import).

# BPR-1156: Deliver the native focused suggestion editor

## Outcome and bounded work

Integrate prefilled Suggest change into the shared native composer with source/range identity, before/after preview, ordinary undo and IME. Inventory actual multi-caret/virtual-space component; implement only missing narrow editing behavior, not an IDE or renderer replacement.

## Acceptance and direct evidence

Attended/automated native tests for Alt+Shift+Down then Shift+End, virtual space, box/multi-caret copy/paste, tabs, CRLF, deletion, undo, IME and stale-source refresh while composing; no checkout writes or model call.

Criteria: RC55-07, RC55-09, RC61-04.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), Qt 6.11.2, `build/dev` freshly configured at `2631bcf31`, and the fix `384525fb9`. The Qt rows run as registered: `QT_QPA_PLATFORM=offscreen` with `QT_STYLE_OVERRIDE=Windows`.

Evidence only; the card stays parked for the owner. `core_suggestion_text_editing`, `qt_suggestion_editor_1_0` and `_2_0`, and `application_review_cycle_desktop_public_path` pass. The editor course drives the editor with key events and checks focus offscreen. That is automated keyboard and focus evidence on Linux, not a native xcb or Wayland session, and it does not answer the owner's feel question.

The build is clean under GCC with no warnings. The logs are station records in `build/logs/bpr-station-authoring/` and are not committed.

## Windows — bpr-main-win, 2026-09-29

At `2631bcf3`, with `f42d4e3a` and the Linux station's `384525fb`,
`build/windows-dev` (MSVC 19.51) builds with no warning. `f42d4e3a` renamed
two locals in `suggestion_editor.cpp` for C4458. `384525fb` fixed the
course's read of an emptied optional in `review_cycle_workbench`, the same
fix this station had made. The authoring rows pass offscreen: the six core rows,
`sqlite_review_cycle_storage`, `review_cycle_{feedback,suggestion,workbench}`,
`qt_suggestion_editor_*`, `qt_review_cycle_view_*` and
`application_review_cycle_desktop_public_path`. The four new uncontained pins
are measured and confirmed. The details are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"Windows station, authoring".

No native Windows attended run was made. This entry adds evidence only, and the card stays Parked.

## Owner verdict on the composer, and syntax colours — macOS, 2026-10-01

In attended session `cbd48b91000b00f1e72e1ad62c7c384c` the owner rejected
`BPR-1156-composer-layout-v001` at event 713 with the note "There's no syntax
highlighting." `BPR-1156-editor-gestures-v001` is undecided and unchanged:
the colours alter no gesture, and its frozen build is untouched.

`f3327b74b` colours the suggestion editor and its before/after preview with
the review panes' packaged classifier, role colours and span-to-format rule.
It adds no second highlighter. The core splices the proposal into its whole
file (`core::splice_text`). The composer asks for the original and the
proposed text and adopts only spans of the text it still shows.
`review_cycle_desktop` classifies on the appearance effect lane through its
own small `syntax_classification_application_host`. The details and the
checks are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"BPR-1156: syntax colours in Suggest change".

RC61-04 asks again as `BPR-1156-composer-layout-v002` (session cursor 718),
bound to the frozen candidate `/Users/jaeger/bpr-suggestion-syntax-review.w9G5mr`
(app sha256 `6c3f20c0…dbef85c`, revision `f3327b74b`). The v001 protection is
released and its verdict kept. The card stays Parked on human-review.

The cycle page's own source pane is still plain text. That pane is
BPR-1157's, and this change does not touch it.

## Linux: bpr-main-linux, 2026-10-01

At `37b0b192b` (includes `f3327b74b`): WSL2, GCC 15.2, Qt 6.11.2, `build/dev`
Debug, offscreen with the Windows style that the Linux Qt rows use. The dev
build has 0 `warning:` lines. `ctest --preset dev -j4 --output-on-failure -R
'^(core_suggestion_text_editing|core_review_cycle_(feedback|suggestion|tabs)|qt_suggestion_editor_|qt_review_cycle_view_|review_cycle_workbench|application_review_cycle_desktop_public_path|application_review_round_trip_composition_contract|syntax_classification_contract|architecture_boundaries)'`:
**12 of 13 pass**.

**`qt_suggestion_editor_1_0` fails, deterministically (3 of 3 reruns), at
`the editor paints the keyword and the number in their role colours`.**
`qt_suggestion_editor_2_0` passes. A probe, not committed, printed the two
counts right after the check's own `pixels_near` calls:

| Row | `keyword` (`pixels_near(editor, #a40e26)`, needs `> 10`) | `number` (`#0550ae`, needs `> 0`) | Editor font | DPR |
|---|---|---|---|---|
| scale 1.0 | **8** | 10 | `monospace` 9 pt (fontconfig resolves it to DejaVu Sans Mono) | 1 |
| scale 2.0 | 154 | 43 | same | 2 |

So the colours do arrive: the keyword has 8 pixels within ±24 of the
control-flow colour, and the number has 10. The issue is that `return` at 9 pt
and 1× in DejaVu Sans Mono has thin, anti-aliased strokes. Fewer than 11 of its
pixels fall within the ±24 tolerance, which the `> 10` threshold assumes. This
looks like the check's calibration to the platform font, not a painting
defect. I changed nothing. The choices are a threshold relative to the glyph's
ink, or a bolder or larger sample. That decision belongs to the card's owner.

Passing rows: `core_suggestion_text_editing`, `core_review_cycle_feedback`,
`_suggestion` and `_tabs`, `qt_suggestion_editor_2_0`,
`qt_review_cycle_view_1_0` and `_2_0`, `review_cycle_workbench`,
`application_review_cycle_desktop_public_path`,
`application_review_round_trip_composition_contract`,
`syntax_classification_contract` and `architecture_boundaries`.

### The keyword checks no longer count pixels (bpr-main-linux, 2026-10-01)

Test only. The card stays Parked. In `tests/adapters_qt/suggestion_editor_tests.cpp`,
the four `pixels_near(…, flow) > 10` checks are replaced with a local, colour
rule. Inside the keyword's own rectangle there must be at least one pixel
within ±24 of the role colour, and none within ±24 of the plain text colour
(`QPalette::Text`). Anti-aliasing blends the role colour toward the
background, never toward the text colour. So a painted keyword passes at any
scale and face, and an unpainted one has many text-coloured pixels. The
replaced checks were "the editor paints the keyword…", "and paints it"
(preview), "coloured again" and "the moved line keeps its colours".

- `span_painted` (proposal editor): the region lies on the caret's line,
  between the x where `point_at` moves past column 0 and the x where it moves
  past column 5. Those are the middles of `r` and `n`, so the region holds
  only `return`'s ink.
- `word_painted` (preview `QPlainTextEdit`): the region is the
  `cursorRect`-bounded rectangle of the first `return`.
- The number keeps `pixels_near(editor, number) > 0`. It had 10 pixels at 1×.
  The two `== 0` checks before classification and after an edit are
  unchanged.

| Check (Linux, GCC 15.2, `build/dev`, offscreen) | Result |
|---|---|
| `qt_suggestion_editor_1_0` and `_2_0`, 3 runs each | 6 of 6 pass (were: `_1_0` failed at `keyword=8`, needing `> 10`) |
| Negative control: `suggestion_editor.cpp`'s colour lookup disabled (`formats = nullptr` in place of `syntax_of(line)`, not committed) | Both scales fail at "the editor paints the keyword and the number in their role colours" |
| The same control with that check reduced to `span_painted` alone | Both scales fail, so the new keyword rule fails without the number's count |
| `/usr/bin/clang-format` 22 `--dry-run --Werror`, `git diff --check`, `tidy.py --paths` on the file | Clean, `tidy=0` |

## Windows (bpr-main-win, 2026-10-01)

Windows 11 Home 10.0.26200, Visual Studio 18 (MSVC 14.51), Qt 6.11.2,
`build/windows-dev` Debug, at `3a69349d5`. That revision contains
`f3327b74b` and Linux's revised keyword check: a role-colour pixel present
and no text-colour pixel in the keyword's rectangle. `cmake --build --preset
windows-dev` exited 0 and printed no `warning C` line.

```powershell
ctest --preset windows-dev -j4 --output-on-failure -R "^(core_suggestion_text_editing|core_review_cycle_(feedback|suggestion|tabs)|qt_suggestion_editor_|qt_review_cycle_view_|review_cycle_workbench|application_review_cycle_desktop_public_path|application_review_round_trip_composition_contract|syntax_classification_contract|architecture_boundaries)"
```

All 13 rows pass, in 30.07 s:

- `qt_suggestion_editor_1_0` (0.44 s) and `_2_0` (0.89 s);
- `qt_review_cycle_view_1_0` (1.40 s) and `_2_0` (3.61 s);
- `review_cycle_workbench` (4.87 s);
- `application_review_cycle_desktop_public_path` (6.69 s);
- `application_review_round_trip_composition_contract` (11.74 s);
- `syntax_classification_contract` (0.43 s);
- `core_suggestion_text_editing`, and `core_review_cycle_feedback`,
  `_suggestion` and `_tabs`;
- `architecture_boundaries` (26.27 s).

## The file pane in syntax colours — macOS, 2026-10-01

The owner rejected `BPR-1156-composer-layout-v002` at session event 719: "The
main file pane stll has no syntax highlight." v002 had coloured the composer
and left the cycle page's source pane plain as BPR-1157's.

`028882c3a` colours that pane through the same path. The page asks for the
exact bytes it shows (`review_cycle_view::SourceSyntaxWanted`) and adopts only
spans of those bytes. `review_cycle_desktop` serves it from a third slot on the
composer's classifier host. The per-line mapping the composer used is now
shared in `code_sample_painter` (`classified_text`,
`code_syntax_format_ranges_at`), so the pane and the composer follow one rule.
A file the pane has already classified colours the composer at once from the
host's cache. The comparison view (View menu) still shows plain rows.
`57fff5a41` makes the desktop course paint in the platform's own colour
scheme, so native renders show dark-scheme colours on a dark window.

| Check (macOS arm64, `build/dev`) | Result |
|---|---|
| `qt_review_cycle_view_1_0` and `_2_0`: asks for the shown bytes, plain until adopted, refuses spans of other and earlier bytes, paints `return`, `café` and a number after a two-byte character in their role colours | Pass |
| `application_review_cycle_desktop_public_path`, offscreen and under `QT_QPA_PLATFORM=cocoa`: the real classifier colours `return` and `check` in the source pane | Pass |
| Negative control (not committed): `apply_source_syntax` returns at once | Both `qt_review_cycle_view` scales and the desktop course fail at their colour checks |
| 52 focused rows (`review_cycle`, `suggestion`, `syntax`, round-trip composition, `architecture_boundaries`), `coding-format-check`, `tidy.py --changed` | Pass |

The colour checks are local to each word's ink: at least one role-coloured
pixel and no plain-text-coloured pixel inside the word's rectangle, the rule
`3a69349d5` adopted for `qt_suggestion_editor`. They need no pixel count, so
they hold at 1x with Linux fonts. Linux and Windows runs of the two changed
rows are owed.

Asked as `BPR-1156-composer-layout-v003` on the frozen candidate
`/Users/jaeger/bpr-cycles-syntax-review.rMJBTD` (app sha256
`85492e75…ebef307`). Its launcher also sets `CFFIXED_USER_HOME`; see BPR-1157's
note of the same date for why. The card stays Parked.

### Linux at `2f5894786` (bpr-main-linux, 2026-10-01)

Includes the file-pane highlighting and `028882c3a`'s Home store-failure fix.
WSL2, GCC 15.2, Qt 6.11.2, `build/dev` Debug, offscreen. `cmake --build --preset
dev` exits 0 with 0 `warning:` lines. `ctest --preset dev --output-on-failure -R
"qt_review_cycle_view|application_review_cycle_desktop_public_path|qt_suggestion_editor|review_cycle_workbench"`
passes **6 of 6** in 3.90 s:

- `review_cycle_workbench`
- `qt_suggestion_editor_1_0` and `_2_0`
- `qt_review_cycle_view_1_0` and `_2_0`
- `application_review_cycle_desktop_public_path`

No remote mutation.

## Windows at `2f5894786` (bpr-main-win, 2026-10-01)

At first, `cmake --build --preset windows-dev` (MSVC 14.51, Qt 6.11.2, Debug)
failed in `tests/end_to_end/review_cycle_desktop_public_path.cpp`:

```
(147,12): error C2059: syntax error: 'const'
(177,23): error C2440: '?': cannot convert from 'const QColor' to 'bool'
```

The colour helper `028882c3a` added there was named `near`, which the Windows
SDK defines as an empty macro (`minwindef.h`). It is now `colour_near`, with a
comment saying why. No behavior changed. With the rename, the build exits 0
and prints no `warning C` line, and `clang-format --dry-run --Werror` is clean.

```powershell
ctest --preset windows-dev --output-on-failure -R "qt_review_cycle_view|application_review_cycle_desktop_public_path|qt_suggestion_editor|review_cycle_workbench"
```

All six rows pass on the rebuilt binaries, in 19.13 s:

- `review_cycle_workbench` (4.09 s);
- `qt_suggestion_editor_1_0` (0.42 s) and `_2_0` (0.83 s);
- `qt_review_cycle_view_1_0` (1.60 s) and `_2_0` (3.95 s);
- `application_review_cycle_desktop_public_path` (8.17 s).

Before the rename, the desktop row passed only because CTest ran the stale
executable.

## Owner amendment: Suggest change edits the file in place (2026-10-01)

In attended session `cbd48b91000b00f1e72e1ad62c7c384c` the owner rejected
`BPR-1156-composer-layout-v003` at event 742 and handed back at 743:

> If you're going to show +/- lines in the bottom right they should be colored
> like we do our diffs.
>
> I should be able to right click suggest change. I'm not sure why i can't
> suggest change without a selected block, I feel like a cursor should be
> sufficient.
>
> I don't want to edit in a tiny box on the right I want to edit in the main
> window. I don't want to be limited to changing the lines I initially selected
> either, can't I just suggest change and start editing in that whole file?

This is a product decision, not an observation. It changes the card's outcome
("prefilled Suggest change" in a focused composer) to: Suggest change from the
caret, including a right click; the whole file edited in the main pane; and a
before/after coloured like the diffs. The decision, how a suggestion's extent
is derived and what is delivered when changes are not contiguous are recorded
as the [owner amendment in RFC-BPR-0055](../../rfcs/RFC-BPR-0055-source-feedback-and-exact-suggested-edits.md#owner-amendment-2026-10-01-suggest-change-edits-the-file-in-place).
RC55-04 and RC61-04 are amended there and in their closeout rows.

## Suggest change in the main pane — macOS, 2026-10-01

Observations at `da459c4e1`.

- From the caret: Suggest change needs no selection, from the review bar and
  from the source pane's context menu (Comment, Suggest change, then the pane's
  own Copy and Select All). A right click outside the selection moves the caret.
- In place: the main pane becomes the multi-caret suggestion editor holding the
  whole captured file, at the caret or with the selection selected again, at
  the same scroll line. The tray keeps location, intent, rationale and the
  before/after. Save or Cancel gives the pane back; a later capture moves
  nothing. A file over 1 MiB is not opened (the editor model's new bound; undo
  is bounded by bytes as well as steps).
- Extent: `core::derive_edit_hunks` returns whole-line runs separated by at
  least one unchanged line, exact against the edited file; each run is one
  exact suggestion of one note (`review_cycle_suggestion_request::Edits`, added
  in order by the host). More than 64 runs or a run over 64 KiB is refused with
  the reason shown.
- Preview: `core::preview_edit_hunks` rows; removed and added rows wear the diff
  panes' fills (`code_sample_row_fill`), the header the hunk-header roles.

| Check (macOS arm64, `build/dev`, `da459c4e1`) | Result |
|---|---|
| `core_review_cycle_suggestion`: exact hunks with CRLF, deletion and insertion, CRLF to LF, no final newline, empty file, 300 random edits exact and separated, one exact hunk past the bound, the save refusals, preview rows and headers | Pass |
| `core_suggestion_text_editing` (`point_of`), `review_cycle_workbench` (two changes saved as one note's two suggestions) | Pass |
| `qt_suggestion_editor_1_0`/`_2_0`: whole file at the caret or selection; two separate changes with exact hunks; summary; undo back to nothing to save; editor and preview words by the local role-colour/text-colour rule; removed, added and unchanged row fills by a flat-fill pixel rule | Pass |
| `qt_review_cycle_view_1_0`/`_2_0`: Suggest change enabled with only a caret; context menu order and caret move; editor in the main pane, focused; one and two exact edits handed over; Cancel; a capture while editing moves nothing | Pass |
| `application_review_cycle_desktop_public_path`, offscreen and under `QT_QPA_PLATFORM=cocoa`, real classifier: Suggest change from the context menu, two changes, coloured editor and preview rows, diff fills, one note with `Suggested change · 2`, file on disk unchanged | Pass |
| 52 rows matching `review_cycle\|suggestion\|syntax\|round_trip_composition\|architecture_boundaries\|code_sample`; full `cmake --build --preset dev` with no warning; clang-format and `tidy.py --changed --warnings-as-errors` | Pass |
| Negative controls (not committed): preview row fills removed; the editor's syntax lines not set; Suggest change dropped from the menu; Suggest change enabled only with a selection | Each fails `qt_suggestion_editor_*`, the desktop course or `qt_review_cycle_view_*` at the matching check |

Attended: `BPR-1156-composer-layout-v004` (RC61-04) and
`BPR-1156-editor-gestures-v002` (RC55-07, superseding the undecided v001,
whose small tray editor no longer exists) on the frozen candidate
`/Users/jaeger/bpr-cycles-inplace-review.hnx9k9` (app sha256
`bdb77d98…6ca112c13`; its launcher sets HOME and CFFIXED_USER_HOME, and an
offscreen trial start wrote its profile only under the given home). Session
cursor 764. The card stays Parked on human-review.

A `resume-development` for v001 and v003 was given `--paths` as well as
`--candidate`; the tool releases the union, so it also released the four
undecided BPR-1157 items. Their frozen apps and questions are unchanged, so
they were prepared again from the same material and the reason recorded on
each; no verdict changed.

Owed: Linux and Windows runs of the changed rows at `da459c4e1` or later.

### Linux at `54c4154d4`: in-place Suggest change (bpr-main-linux, 2026-10-01)

Includes `da459c4e1`. WSL2, GCC 15.2, Qt 6.11.2, `build/dev` Debug, offscreen.
`cmake --preset dev && cmake --build --preset dev` exits 0 with 0 `warning:`
lines. The requested `ctest --preset dev -j4` selection passes **14 of 14** in
19.38 s:

- `core_suggestion_text_editing` and `core_review_cycle_{feedback,suggestion,tabs}`
- `qt_suggestion_editor_1_0` and `_2_0`
- `qt_review_cycle_view_1_0` and `_2_0`
- `review_cycle_workbench` and `review_cycle_suggestion`
- `application_review_cycle_desktop_public_path`
- `application_review_round_trip_composition_contract`
- `syntax_classification_contract` and `architecture_boundaries`

No pixel check failed at 1×. No remote mutation.

## Windows at `54c4154d4`, whole-file Suggest change (bpr-main-win, 2026-10-01)

At `54c4154d4`, which contains `da459c4e1`. Windows 11 Home 10.0.26200, MSVC
14.51, Qt 6.11.2, `build/windows-dev` Debug.

So that no stale executable could pass, the 11 executables these rows run
were deleted before the build:

- the four core test executables;
- `better_pull_review_suggestion_editor_tests.exe` and
  `better_pull_review_review_cycle_view_tests.exe`;
- `review_cycle_suggestion.exe` and `review_cycle_workbench.exe`;
- `review_cycle_desktop_public_path.exe`;
- `better_pull_review_syntax_classification_contract_tests.exe`;
- `better_pull_review.exe`.

`cmake --build --preset windows-dev` exited 0 and printed no `warning C` line,
and all 11 were rebuilt. No Windows-only compile fix was needed.

```powershell
ctest --preset windows-dev -j4 --output-on-failure -R "^(core_suggestion_text_editing|core_review_cycle_(feedback|suggestion|tabs)|qt_suggestion_editor_|qt_review_cycle_view_|review_cycle_workbench|review_cycle_suggestion|application_review_cycle_desktop_public_path|application_review_round_trip_composition_contract|syntax_classification_contract|architecture_boundaries)"
```

All 14 rows pass, in 28.02 s:

- `review_cycle_suggestion` (1.25 s) and `review_cycle_workbench` (4.59 s);
- `qt_suggestion_editor_1_0` (0.45 s) and `_2_0` (0.77 s);
- `qt_review_cycle_view_1_0` (2.03 s) and `_2_0` (5.70 s);
- `application_review_cycle_desktop_public_path` (9.66 s);
- `application_review_round_trip_composition_contract` (12.40 s);
- `syntax_classification_contract` (0.43 s);
- the four core rows;
- `architecture_boundaries` (27.96 s).

## Owner verdict on v004, and one code view — macOS, 2026-10-02

In attended session `cbd48b91000b00f1e72e1ad62c7c384c` the owner rejected
`BPR-1156-composer-layout-v004` at event 765 and handed back at 766:

> Much better but...
>
> The text editor changes in a weird way once i click suggest edit. Quite
> noticable under selection. For some reaosn the non-edit window loses color on
> higlighted lines. I don't understand why we don't have a consistent code text
> view rendering across the presentation surface.
>
> Selection also behaves oddly differently between the two modes.
>
> It would be great if things like auto indention on newlines while editng code
> in blocks would work like a modern text edtiro. I dunno if Qt has something
> built in or if there is a library that can help or if we have to roll our own.

These are observations about the candidate and one request (indentation on
Enter) within RC55-07's editing workflows; the outcome the v003 amendment set
is unchanged.

### How the code text views differed at `54c4154d4`

| Surface | Widget | Font | Rows | Selection | Syntax colour under a selection |
|---|---|---|---|---|---|
| Diff panes (two-pane and one-pane) | `QPlainTextEdit` (`source_pane`) | The profile's code font (`apply_code_font_to`) and tab stop | Text layout line with leading; 4 px document margin | Semantic line selection: full-width `Highlight` wash at `core::selection_wash_alpha` (72). A mouse text selection is Qt's: opaque `Highlight` with `HighlightedText` glyphs | Kept under the line wash; lost under a mouse text selection |
| Cycle file pane, reading | `QPlainTextEdit` (`cycleSource`) | Never given a code font: `review_cycle_view::set_code_font` had no caller, so it drew in the window's UI font with Qt's default 80 px tab stops | Text layout line; 4 px margin | Qt's: opaque `Highlight`, glyphs redrawn in `HighlightedText`; caret only with keyboard selection | **Lost**: a probe drew 248 role-coloured pixels unselected and 0 selected |
| Cycle file pane, Suggest change | `suggestion_editor` (separate widget) | `suggestion_code_font()` (platform fixed font) | `QFontMetrics::lineSpacing`, 6 px margin, one glyph per column cell | `Highlight` at alpha 110 focused, 60 unfocused, under the glyphs; virtual space; no double or triple click | Kept |
| Before/after preview | `QPlainTextEdit` | `suggestion_code_font()` | Text layout line | Qt's | Lost under a text selection; rows wear the diff fills |
| Comparison view (View menu) | `QPlainTextEdit` | UI font (same missing call) | Text layout line | Qt's | No syntax colours; fixed red/green row tints |

So entering Suggest change swapped a UI-font pane for a fixed-font one with
another pitch, margin and selection, and the reading pane repainted selected
text in one colour.

### What changed at `b486149a5`

- **One code view.** The file pane is `suggestion_editor` in read-only mode,
  and Suggest change makes the same widget editable; nothing is reloaded, so
  text, scroll, selection and colours stay and only the caret's colour follows
  focus. Save or Cancel reloads the captured bytes at the same top line.
  Read-only, every movement and selection gesture works and typing, paste,
  cut, undo and IME are refused.
- **The panes' typography.** `review_cycle_view::adopt_appearance` gives the
  view the profile's code font and tab width; rows use the panes' pitch (a
  laid-out line of the code font with leading) and the text document's margin.
  A proportional code font falls back through monospace families, because the
  view gives every column one cell.
- **The panes' selection.** `QPalette::Highlight` at
  `core::selection_wash_alpha` under the glyphs, in both modes and whatever the
  focus, so a selected keyword keeps its role colour and the selection is the
  one the contrast contract audits.
- **One selection behaviour.** Double-click selects a word and triple-click a
  line (`core::select_word_at`, `core::select_line_at`) in both modes; the
  context menu is the view's own (Comment, Suggest change, Copy, Select All
  while reading; Undo, Redo, Cut, Copy, Paste, Select All while editing).
- **Indentation on Enter** (the core decides): the blanks before the caret, a
  level more after `{`, `[` or `(`, and after `:` in Python and YAML
  (`core::indentation_for`, by extension; Markdown, reStructuredText, AsciiDoc
  and plain text open no level); between a bracket and its closer the closer
  goes on its own line; a closer typed on a line of blanks outdents one level.
  The unit is the file's own: a tab when more lines start with a tab,
  otherwise the most common step of leading spaces.
- **Kept whole and fast.** The view now shows any captured file, so a text over
  the 1 MiB editing bound is held whole and read, and no edit applies to it
  (Suggest change was already not offered). Caret-only gestures compare carets
  instead of copying the state, and line widths are measured only when the
  text changes.
- **Accessibility.** The view answers `QAccessibleTextInterface` (text,
  selection, caret, character rectangles) so a screen reader still reads the
  file the `QPlainTextEdit` used to expose.

Why not a library: `QPlainTextEdit` has no auto-indent, and KSyntaxHighlighting
provides indenters only through KTextEditor (KF6 Parts, a full editor
component with its own document, view and rendering), which would replace this
view rather than serve it and add a large dependency. QScintilla and
KTextEditor remain the options if the product later wants language-aware
indentation, folding or completion; this rule is about 60 lines in the core
model and fully tested.

| Check (macOS arm64, `build/dev`, `b486149a5`) | Result |
|---|---|
| `core_suggestion_text_editing`: indentation after brace, bracket pair, colon by language, inside the indentation, per caret, a typed closer and its undo; units for tabs, two and four spaces, alignment ignored, extension case and a dot in a folder; word and line selection; byte offsets; a text over the bound read but not edited | Pass |
| `qt_suggestion_editor_1_0`/`_2_0`: Enter through key events in C++ and Python; read-only gestures (nothing typed, pasted, cut or composed changes the text; Shift+End and Copy; double and triple click; the read-only menu; the accessible text and selection) | Pass |
| `qt_review_cycle_view_1_0`/`_2_0`: an 80-block file scrolled to line 30 with a selection, rendered before and after Suggest change: the same widget, scroll and selection; **0 differing pixels outside the caret columns**; the selected `return` keeps the control-flow colour by the local role/text pixel rule in both; a blank selected cell is `over(Highlight@72, Base)` and identical in both; Cancel returns with 0 differing pixels; a 1 MiB + 11 byte file read whole and not offered for Suggest change | Pass |
| `application_review_cycle_desktop_public_path`, offscreen and under `QT_QPA_PLATFORM=cocoa`, real classifier, the window in the profile's own palette as the application applies it: same view on Suggest change, Enter after `{` in `parser.cpp` gives `\r\n\t`, Save and Cancel back to the captured bytes; native renders of the window before and just after Suggest change differ nowhere in the file pane | Pass |
| 52 rows matching `review_cycle\|suggestion\|syntax\|round_trip_composition\|architecture_boundaries\|code_sample`, full `cmake --build --preset dev` with no warning, `coding-format-check`, `tidy.py --changed --warnings-as-errors` | Pass |
| Negative controls (not committed): Qt-style selection while reading (opaque fill, `HighlightedText` glyphs); wash alpha 110 while editing; glyphs one pixel right while editing; colours cleared when Suggest change begins; Enter without indentation | Fail at, in order: "reading: the selected keyword keeps its role colour"; "the selection fill is identical in both modes"; "the pane is the same pixel for pixel but for the caret" (13379 pixels at 1x); "editing: the selected keyword keeps its role colour"; the Qt and core indentation checks |

`qt_suggestion_editor_*` under cocoa in the dark scheme fails at the preview's
"the removed row is coloured…" check: that course leaves the native dark palette
under the light profile, so the light removed-row fill reads as text-coloured.
The preview is unchanged here and the row is registered offscreen.

Not changed, recorded as options: the diff panes, the before/after preview and
the comparison view are still `QPlainTextEdit`s whose mouse text selection is
Qt's opaque one, and the comparison view still uses fixed red/green tints and
the UI font. Moving them onto this view (with line bands, gutters and the
panes' scroll machinery) or reimplementing their paint to wash would finish
"consistent across the presentation surface"; that is a larger change across
BPR-1157/BPR-1163 and the diff workbench. The view draws one glyph per column
cell, so ligatures (off by default) are not drawn.

Attended: `BPR-1156-composer-layout-v005` (RC61-04) and
`BPR-1156-editor-gestures-v003` (RC55-07, superseding the undecided v002) on
the frozen candidate `/Users/jaeger/bpr-cycles-codeview-review.u1YDg8` (a copy
of the dev bundle, app sha256 `5b0770dfd5bccba66f20dfc19e18e5d1c1b528bec36f2b71686c5297b7ff3ec7`;
its launcher sets HOME and CFFIXED_USER_HOME, and an eight-second offscreen
trial start wrote its profile only under the given home). v004 and v002 were
released by candidate name only; the BPR-1157 items stay protected. Session
cursor 775. The card stays Parked on human-review.

Owed: Linux and Windows runs of the changed rows at `b486149a5` or later.

### Linux at `fbd4ab206`: one shared code view and auto-indent (bpr-main-linux, 2026-10-02)

Includes `b486149a5`. WSL2, GCC 15.2, Qt 6.11.2, `build/dev` Debug, offscreen.
`cmake --preset dev && cmake --build --preset dev` exits 0 with 0 `warning:`
lines. The requested `ctest --preset dev -j4` selection passes **14 of 14** in
19.25 s. That includes `qt_review_cycle_view_1_0` and `_2_0` with the new
before-and-after Suggest change pixel comparisons, as well as
`qt_suggestion_editor_1_0` and `_2_0`, `review_cycle_suggestion`, the
round-trip contract, `syntax_classification_contract` and
`architecture_boundaries`. No pixel check failed on the Linux fonts. No
remote mutation.

## Windows at `fbd4ab206`, the shared code view: the view rows time out (bpr-main-win, 2026-10-02)

At `fbd4ab206`, which contains `b486149a5`. Windows 11 Home 10.0.26200, MSVC
14.51, Qt 6.11.2, `build/windows-dev` Debug. The 11 executables these rows run
were deleted first, and the build rebuilt all of them. It exited 0 with no
`warning C` line, and no Windows-only compile fix was needed.

```powershell
ctest --preset windows-dev -j4 --output-on-failure -R "^(core_suggestion_text_editing|core_review_cycle_(feedback|suggestion|tabs)|qt_suggestion_editor_|qt_review_cycle_view_|review_cycle_workbench|review_cycle_suggestion|application_review_cycle_desktop_public_path|application_review_round_trip_composition_contract|syntax_classification_contract|architecture_boundaries)"
```

12 of 14 pass. The two failures are clock failures:
`qt_review_cycle_view_1_0` and `_2_0` each hit `***Timeout` at their 60 s
bound, with no output. They ran again alone with the same result. At
`54c4154d4` they passed in 2.03 and 5.70 s.

- **Not a hang.** Run directly and uncontained, with the row's environment
  (`QT_QPA_PLATFORM=offscreen`, `QT_SCALE_FACTOR=1.0`), the `1_0` course prints
  `review-cycle-view-tests:pass` and exits 0 after 66.5 s.
- **Where the time goes.** Stack samples of the running test, taken seconds
  apart, all sit in `bpr::qt::suggestion_editor::paintEvent` at
  `src/adapters/qt/src/suggestion_editor.cpp:599`. The process had used 36.7 s
  of CPU by then. That line is the per-glyph loop. For every character of
  every visible row, it builds a one-character `QString`, searches the line's
  format ranges linearly, sets the pen and calls `QPainter::drawText`. The loop
  is bounded to the visible rows, so the cost is per paint, and since
  `b486149a5` the shared code view in these courses paints far more often or
  far more text.
- No pixel-compare check failed. Nothing was loosened, and the 60 s bound was
  not changed.

Ways forward, for the view's owner:

- Paint each run of glyphs that share a colour in one call, still placed by
  column. With a fixed-pitch font this keeps the "one glyph per column cell"
  alignment.
- Or cache the shaped glyphs per line: `QStaticText`, or `QGlyphRun` with
  explicit positions.

Either keeps the rule and cuts the per-character `drawText` that Windows'
Debug Qt makes expensive. The other 12 rows pass:

- `review_cycle_suggestion` (1.19 s) and `review_cycle_workbench` (4.40 s);
- `application_review_cycle_desktop_public_path` (11.30 s);
- `application_review_round_trip_composition_contract` (13.65 s);
- `qt_suggestion_editor_1_0` (0.47 s) and `_2_0` (0.83 s);
- `syntax_classification_contract` (0.42 s);
- the four core rows;
- `architecture_boundaries` (31.93 s).

## The code view paints by cached glyph runs (bpr-main-win, 2026-10-02)

`suggestion_editor::paintEvent` drew every visible character with its own
one-character `QString`, a linear search of the line's format ranges, and a
`setPen` and `drawText` call. On Windows Debug that put
`qt_review_cycle_view_*` past their 60 s bound (above).

**Chosen: per-glyph `QGlyphRun` caching, not `drawText` over a run.** Drawing
a run with `drawText` would let a font apply kerning or ligatures across
characters, so it is pixel-identical only for some fonts. Instead:

- `shaped()` shapes each distinct character once, alone, with a `QTextLayout`
  in the code font on the viewport. That is how a one-character `drawText`
  shapes it. The cache keeps the glyph, its raw font and its baseline offset.
- A character that needs more than one glyph or a second font is still drawn
  with `drawText`.
- The cache is cleared when the font is measured and when the device pixel
  ratio changes.
- Each run of glyphs in one colour and one raw font is drawn with a single
  `drawGlyphRun`, every glyph still at its own cell's `column_x`.
- The colours come from a per-line table of UTF-16 units, filled from the
  ranges in reverse. So the first range in list order that covers a unit
  wins, exactly as the linear search chose.

**Pixel identity, measured.** A temporary switch, not committed, restored
the old path: per-glyph `drawText` with the linear search. The
`review_cycle_view` course ran both ways at scale 1.0 and 2.0 with
`BPR_CYCLE_RENDER_DIR`, and all 24 captured renders (12 per scale) are
byte-identical PNGs.

**Timing on Windows Debug (MSVC 14.51, Qt 6.11.2):**

| Row | Before (`fbd4ab206`) | After |
|---|---|---|
| `qt_review_cycle_view_1_0` | `***Timeout` at 60 s; 66.5 s run directly | 6.12 s |
| `qt_review_cycle_view_2_0` | `***Timeout` at 60 s | 10.38 s |

The 60 s bound is unchanged.

The 14-row selection above passes 14/14, in 28.60 s, with every
before/after-Suggest-change comparison and role/text colour check unchanged.
The build printed no `warning C` line, and `clang-format --dry-run --Werror`
is clean. The macOS and Linux reruns are the coordinator's.

### Linux at `3868375a7`: cached glyph runs (bpr-main-linux, 2026-10-02)

Includes `3868375a7`. WSL2, GCC 15.2, `build/dev` Debug, offscreen.
`cmake --preset dev && cmake --build --preset dev` exits 0 with 0 `warning:`
lines. The same 14-row `ctest --preset dev -j4` selection passes **14 of 14**
in 21.16 s, with every pixel check unchanged.

| Row | `53d27c5b2`, -j4 | `3868375a7`, -j4 | `3868375a7`, alone (-j1, 3 runs) |
|---|---|---|---|
| `qt_review_cycle_view_1_0` | 2.06 s | 1.00 s | 0.97, 0.96, 0.96 s |
| `qt_review_cycle_view_2_0` | 3.23 s | 3.00 s | 1.83, 1.82, 1.83 s |

The -j4 times depend on what ran beside them. The rows ran alone only at this
revision. No remote mutation.

## Owner verdict on v005: selection shade, virtual space, double click — macOS, 2026-10-02

In attended session `cbd48b91000b00f1e72e1ad62c7c384c` the owner rejected
`BPR-1156-composer-layout-v005` at event 776 and handed back at 777:

> Better but...
>
> I feel like selected text highlight is darker than usual. And we're
> seemingly always in virtual space mode when we should only enter that
> through the box-select edit mode. Also double click should select the word
> like a normal text editor but its not.

These are observations about the candidate. The second is also a refinement
of RC55-07's "virtual-space ergonomics": virtual space belongs to the box
gesture only. RFC-BPR-0055 records that.

### What was wrong at `b486149a5`

- **Shade.** The view laid `QPalette::Highlight` over the code at the diff
  panes' line-wash alpha, `core::selection_wash_alpha` (72/255). Every other
  text box in the window, `QPlainTextEdit` and `QLineEdit` alike, fills a
  selection with solid `Highlight`. In the dark profile the wash is `#152338`
  on `#0D1117` (1.20:1 against the pane). A text box's selection is `#28518C`
  (2.38:1). In the light profile the wash is `#DBE9F9` and a text box's
  selection is `#7FB0E8`.
- **Virtual space.** Every caret gesture could leave the caret past a line's
  end. A click or a drag snapped to a virtual column. Right at the end went
  one column further. Up and Down carried the column onto shorter lines. The
  box gesture, Alt+Shift+Arrow, is the only box mode the editor has. The mouse
  has no box gesture: Alt+click adds a caret.
- **Double click.** It is reproduced through QtGui's window-system entry
  (`qt_handleMouseEvent`, the path QTest drives), not only with events sent
  to the widget. A still double click selected the word, bytes 5 to 10 of
  `auto value = …`. If the pointer drifted one to three pixels while the
  button was down, `mouseMoveEvent` extended the selection by characters from
  the word's start to the pointer, giving bytes 5 to 8, in both modes. A
  triple click lost its line the same way (0 to 17 for 0 to 25). A trackpad
  click drifts like that routinely. A double click on the right half of a
  word's last letter also rounded to the next boundary and selected the
  following space.

### What changed at `fd5ca4412`

- **The selection a text box makes.** The view fills a selection with solid
  `Highlight` and ends it at the line's text, plus one cell where it includes
  the line break. The glyphs on the selection keep their role colours. A
  colour that does not reach the variant's text floor on the fill (4.5:1, or
  7:1 for high contrast) moves toward black or white by the least amount that
  does, so it keeps its hue. That is `core::readable_selection_ink`. In the
  dark profile, for example, the keyword's `#6DA9DB` becomes `#A1C8E8`. Every
  role of the four built-in profiles reads on its own selection. The
  required-pair audit is unchanged: its wash pairs measure the diff panes'
  line wash, which those panes still paint, and this ink meets the floor by
  construction wherever any ink can.
- **Virtual space only in a box.** A click, Shift+click, Alt+click or drag
  lands on the text. Right at a line's end goes to the next line, and Left
  from a box's virtual column returns to the line end. Up and Down aim for
  the column they started from and stop at a shorter line's end. The column
  is kept across a run of vertical moves as `suggestion_text_state::Goals`.
  Alt+Shift+Arrow still reaches past short lines, and typing there pads them
  with spaces.
- **A press keeps what it selected.** A drag with the button held grows from
  what the press selected, by the press's unit: characters after a click,
  whole words after a double click, whole lines after a triple click
  (`core::drag_selection`). A double click names the character under the
  pointer (`suggestion_editor::cell_at`), not the nearer edge.
- The painter is still the cached glyph runs of `3868375a7`. A selection only
  changes a glyph's colour, which splits a run.

| Check (macOS arm64, `build/dev`, `fd5ca4412`) | Result |
|---|---|
| `core_suggestion_text_editing`: Down stops at a short line's end and the column is reached again further on; Up likewise; Right at the end wraps; a click, Shift+click and Alt+click past the end land at the end; typing there adds no blanks; a box stands in virtual space, backspaces there, and pads on typing; Left after a box returns to the line end; word and line drags keep the double- or triple-clicked unit, grow by whole units both ways, and stop at a line's end by characters | Pass |
| `core_appearance_contrast`: the selection ink keeps a readable role exactly, lifts a faint one to the text floor (not the emphasis floor) by the least step, keeping its hue, and holds a high-contrast variant to its own floor | Pass |
| `qt_suggestion_editor_1_0`/`_2_0`, through the window system: double click with 0, 1 and 3 px of drift selects exactly the word, and a triple click the line, reading and editing; the right half of a word's last letter and the left half of its first; a drag after a double click by whole words both ways; End then Down, Up and Up again; Right wraps; a click and a drag past the end; a box past a short line; the solid `Highlight` fill at a blank selected cell, the base past the text, a selected line break one cell; every built-in profile's roles read on its selection | Pass |
| `qt_review_cycle_view_1_0`/`_2_0`: the selection is solid `Highlight` and differs from the wash; the selected `return` is in the selection's ink, not plain text; 0 differing pixels outside the caret before and after Suggest change, and after Cancel | Pass |
| `application_review_cycle_desktop_public_path`, offscreen and under `QT_QPA_PLATFORM=cocoa` (dark scheme, 2x), the composed window in the profile's palette: a double click with drift selects `parse` reading and `check` editing; a triple click selects the line; a click past the end puts the caret at the end; a drag past the end selects the text in `code_selection_background` and paints the base after it; the selected `check` has solid pixels in the function role's selection ink and none in the bare role colour | Pass |
| 64 rows matching `review_cycle\|suggestion\|syntax\|round_trip_composition\|architecture_boundaries\|code_sample\|appearance`, the network-isolation rows, `core-only` build and its suggestion/appearance rows, full `cmake --build --preset dev` with no warning, `coding-format-check`, `tidy.py --changed --warnings-as-errors` | Pass |
| Negative controls, not committed: (1) the 72/255 wash restored; (2) the bare role colour on the selection; (3) virtual columns kept by a click; (4) the old character drag after a double click; (5) a double click at the nearer edge; (6) Down into virtual space | Each fails where it should: (1) "the selection is the solid Highlight…" in three courses; (2) "a selected call keeps its role, in the ink…" and the cycle view's ink check; (3) "a click past the end puts the caret at the line's end" and "Down stops at a shorter line's end"; (4) "a double click … selects exactly the word" in the editor course and the composed window; (5) "a double click on the right half of the last letter…"; (6) "Down stops at a shorter line's end" in core and Qt |

The window-system mouse helper is `tests/support/window_system_mouse.hpp`.
It declares QtGui's exported `qt_handleMouseEvent` exactly as `qtestmouse.h`
does, so no new Qt module is linked.

Attended: `BPR-1156-composer-layout-v006` (RC61-04, one question on the shade,
line ends and double click) and `BPR-1156-editor-gestures-v004` (RC55-07,
superseding the undecided v003, whose wording offered virtual space past
short lines outside a box). Both are on the frozen candidate
`/Users/jaeger/bpr-cycles-selection-review.Ev9QTV`, a copy of the dev bundle
at `fd5ca4412` with app sha256
`af38e32d3aad3b66a80176b23033def563921af834b06c439f7311142c9f5538`. Its
launcher sets HOME and CFFIXED_USER_HOME. An eight-second offscreen trial
start wrote its profile only under the given home and changed nothing under
the account's own Application Support. v005 and v003 were released by
candidate name only. Session cursor 787, host on port 59998. The card stays
Parked on human-review.

Owed: Linux and Windows runs of the changed rows at `fd5ca4412` or later.

### Linux at `0dd841b40`: selection shade, virtual space, double click (bpr-main-linux, 2026-10-02)

Includes `fd5ca4412`. WSL2, GCC 15.2, Qt 6.11.2, `build/dev` Debug, offscreen.
`cmake --preset dev && cmake --build --preset dev` exits 0 with 0 `warning:`
lines, and there is **no link error**. `tests/support/window_system_mouse.hpp`'s
`qt_handleMouseEvent` resolves against QtGui. The requested 15-row `ctest
--preset dev -j4` selection passes **14 of 15**.

**`application_review_cycle_desktop_public_path` fails, deterministically (4 of
4), at `a selected call keeps its role, in the ink that reads on the
selection`.** The earlier window-system checks in the same course pass: the
double click, the triple click, the click past the end, the drag selection in
the solid selection colour, and "nothing past the text". A probe, not
committed, printed the counts that check uses, over bytes 41–46 after
`select_bytes(41, 46)`:

| Colour | ±4 (the check's `solid_pixels`) | ±12 | ±24 | ±48 |
|---|---|---|---|---|
| `functionInk` `#4e2b8e` (`readable_selection_ink`; check needs `> 0`) | **0** | 9 | 25 | 49 |
| `function` `#6639ba` (check needs `== 0`) | 0 | 0 | 30 | 80 |
| selection fill `#7fb0e8` | 311 | 329 | 342 | 369 |

Region: x 83.4–119.5, y 28–42 at DPR 1. The source pane's font is
`monospace` 9 pt, which fontconfig resolves to DejaVu Sans Mono, with text
`#1f2328`. The pixel closest to the ink is `#513193`, 6 off on its largest
channel.

So the selected call is drawn in the selection ink, not the role colour: no
role-coloured pixel is within ±12, and ink-tinted pixels are. But at 1× in
this face, no pixel of the thin strokes reaches the ink within ±4, which is the
"solid core" that `solid_pixels` assumes. This is the same calibration
question as the earlier editor check, now on the selection ink. I changed
nothing and loosened nothing.

The other 14 rows pass: `core_suggestion_text_editing`,
`core_appearance_contrast`, `core_review_cycle_{feedback,suggestion,tabs}`,
`qt_suggestion_editor_1_0` and `_2_0`, `qt_review_cycle_view_1_0` and `_2_0`,
`review_cycle_workbench`, `review_cycle_suggestion`, the round-trip contract,
`syntax_classification_contract` and `architecture_boundaries`. No remote
mutation.

### The selection-ink check no longer requires a ±4 solid core (bpr-main-linux, 2026-10-02)

Test only. The card stays Parked. In
`tests/end_to_end/review_cycle_desktop_public_path.cpp`, `solid_pixels` (±4 of
the ink) is replaced by `ink_painted`. Inside the selected call's cells there
must be at least one pixel nearer (squared RGB distance) to the selection ink
than to the role colour, the selection fill and the plain text colour, and
no pixel within ±12 of the role colour or of the text colour. The region is
inset one logical pixel from each side.

Two additions go beyond the integrator's two conditions, and both are
measured:

- **The text colour.** Without it, an unpainted call (plain text colour)
  passed at both scales. Anti-aliased blends of `#1f2328` toward the fill come
  nearer to the ink `#4e2b8e` than to the role colour or the fill. The text
  core within ±12 refuses it.
- **The inset.** At 2×, the next glyph `(` reached two device pixels (x
  237–238) across byte 46's cell edge at x 239, and put 56 text-coloured
  pixels in the region.

| Run (Linux, GCC 15.2, `build/dev`, offscreen) | Scale 1 | Scale 2 (`QT_SCALE_FACTOR=2`, same command) |
|---|---|---|
| Fixed | pass | pass |
| Control A: the selected call painted in its role colour (`colour = role` at `suggestion_editor.cpp:703`, not committed) | fails at the check | fails at the check |
| Control B: the selected call unpainted (`colour = palette().color(QPalette::Text)` inside the selection, not committed) | fails at the check | fails at the check |

The CTest row passes 3 of 3, and the 15-row selection from the previous
request passes 15 of 15. clang-format 22, `git diff --check` and `tidy.py`
(`tidy=0`) are clean on the file.
