---
schema: "implement-rfc.card.v1"
id: "BPR-1156"
title: "Deliver the native focused suggestion editor"
status: "Parked"
rfcs: ["RFC-BPR-0055", "RFC-BPR-0061"]
depends_on: ["BPR-1154", "BPR-1155"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC55-07", "RC55-09", "RC61-04"], "pack_alias": "PACK-CYCLE-14", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"RC55-07","outcome":"pending","revision":"c2a0df54a7b2ca0b48f76777afc6975728392e51","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share passes on macOS arm64 (core_suggestion_text_editing, qt_suggestion_editor_1_0/2_0): Alt+Shift+Down x2 then Shift+End one caret per line to its own end, typing at every caret with CRLF kept, undo, virtual space, Tab, box copy/paste with marker, cut, Alt+click, IME preedit and commit. Owner judgment pending: attended item BPR-1156-editor-gestures-v001 on the frozen candidate (revision c2a0df54a)."}},{"criterion":"RC55-09","outcome":"pass","revision":"0d0872fbd5bfce9f3ff405b42ad649b7ed5ea5cb","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Card share: the composer hands exact replacement bytes, intent and rationale to its owner with no model, summarizer or second prompt and no file write (qt_suggestion_editor, application_review_cycle_desktop_public_path). Round preparation is BPR-1158."}},{"criterion":"RC61-04","outcome":"pending","revision":"028882c3a","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Owner rejected BPR-1156-composer-layout-v001 (event 713: \"There's no syntax highlighting.\") and v002 (event 719: \"The main file pane stll has no syntax highlight.\"). At 028882c3a the cycle page's source pane, the composer's editor and its before/after preview are all coloured by the review panes' packaged classifier and role colours (qt_review_cycle_view_1_0/2_0 and qt_suggestion_editor_1_0/2_0 rendered pixels, application_review_cycle_desktop_public_path with the real classifier); the earlier share (prefill, before/after in context, newer material stated without moving, related location, ambiguous/deleted/unmapped words) still passes. Owner judgment pending: BPR-1156-composer-layout-v003 on the frozen candidate /Users/jaeger/bpr-cycles-syntax-review.rMJBTD (app sha256 85492e75...7ebef307)."}}]
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
