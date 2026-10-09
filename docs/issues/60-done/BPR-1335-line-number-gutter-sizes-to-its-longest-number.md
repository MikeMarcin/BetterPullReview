---
schema: "implement-rfc.card.v1"
id: "BPR-1335"
title: "The line-number gutter sizes to the longest number it shows"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","found_by":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1335--BPR-1335--645a64497526.json","related_cards":["BPR-0455","BPR-1290","BPR-1212","BPR-1264"]}
acceptance: [{"criterion":"BPR-1335:cause","outcome":"pass","revision":"54c5a5988","evidence":{"kind":"artifact","detail":"Card section Cause records the four reasons the gutter was wider (padding, digit floors, one digit width, staged adoption)."}},{"criterion":"BPR-1335:width","outcome":"pass","revision":"54c5a5988","evidence":{"kind":"command","detail":"qt_diff_workbench_scale_{100,125,200}: 9/99/1557/100000-line files give digits x widest digit advance + 8 px (16/24/40/56 px)."}},{"criterion":"BPR-1335:control","outcome":"pass","revision":"54c5a5988","evidence":{"kind":"command","detail":"The same test against the source before the change fails at all three scales (9-line file: gutter=28 expected=16)."}},{"criterion":"BPR-1335:stable","outcome":"pass","revision":"54c5a5988","evidence":{"kind":"command","detail":"Width holds scrolling the 100000-line file to its end, widest number painted whole, font change resizes the gutter."}},{"criterion":"BPR-1335:rows","outcome":"pass","revision":"b864e319d","evidence":{"kind":"command","detail":"qt_diff_workbench_scale_{100,125,200}, qt_workbench_window_scale_{100,125,200}, qt_workbench_conversation_reveal, qt_workbench_line_draft: 8 of 8 pass on v002 (macOS); Linux station passes at e602e2cf9 rows except the colour sample fixed in v002."}},{"criterion":"BPR-1335:markers","outcome":"pass","revision":"frozen-b864e319d-v002","evidence":{"kind":"observation","detail":"Owner Accept on BPR-1335-add-button-v002, session b4b76c70c8e54f1933410b582189d0e8, decision e7d697dd-056d-418f-bc6d-ffa10b11d687, note: 'bug: Why is this damned tooltip far away again. This is a consistent problem.' (filed as BPR-1354)."}},{"criterion":"BPR-1335:base-side","outcome":"pass","revision":"answer-b864e319d-v001","evidence":{"kind":"observation","detail":"Owner Accept on BPR-1335-base-side-v001, session b4b76c70c8e54f1933410b582189d0e8, decision aa39b2f7-bdc6-4ac0-acb9-66417c610e42; follow-up filed as BPR-1353."}}]
---

# BPR-1335: The line-number gutter sizes to the longest number it shows

## Owner note

Owner, chat, 2026-10-08, verbatim: "Our line numbers pane looks to be winder
then necessary. It should size to the longest line number".

## Scope

The code panes' line-number gutter (`source_pane` in
`src/adapters/qt/src/diff_workbench.cpp`): the two split panes and the one-pane
(unified) view.

- The number column is as wide as the longest line number the pane shows:
  its digit count times the widest digit advance of the pane's font, plus a
  small pad on each side.
- The thread-marker and "+" column (BPR-1290, BPR-1212) keeps its own width
  to the right of the numbers. It is not narrowed. (Amended by the owner's
  updates and v001 verdict below: the column is gone; a thread tints its
  number and the "+" is a button at the start of the code.)
- The width is recomputed when the document, the font or the scale changes.
  It does not change while scrolling.

## Acceptance

- `BPR-1335:cause`: the card records why the gutter was wider than its numbers.
- `BPR-1335:width`: at 1.0x, 1.25x and 2.0x, 9-, 99-, 1557- and 100000-line
  files give a gutter of exactly digits × widest digit advance + padding, with
  the marker column unchanged when it is on.
- `BPR-1335:control`: the same test fails against the code before the change.
- `BPR-1335:stable`: the width holds while scrolling the longest file to its
  end, the widest number is painted whole, and a font change resizes the
  gutter.
- `BPR-1335:rows`: the diff workbench rows pass at all three scales.
- `BPR-1335:markers` (added by the owner's updates below): the owner chooses
  how a line shows its thread once the marker column is gone from both panes.
  The "+" and the thread marker stay hit-testable and accessible. The v001
  verdict chose the tinted number and asked for GitHub's "+"; v001 of
  `BPR-1335-add-button` asks for Accept or Reject on both together. Its
  verdict kept the look and the place and asked that the "+" be drawn only
  while the pointer is over its column; v002 asks that alone.
- `BPR-1335:base-side` (added by the owner's v001 note): the owner has the
  reason the base (left) pane offers no new comment, and chooses whether the
  follow-up that would make it possible goes ahead.

## Cause

`source_pane::gutter_width()` sized the number column as
`2 × 6 px + advance('9') × max(digits, 2)` in a split pane, and
`2 × (2 × 6 px + advance('9') × max(digits, 4))` in one pane. Four things
made it wider than its numbers:

1. **Padding.** Six pixels either side of the numbers, twelve in all. With
   the marker column on (it always is in the application), the number's
   right edge sat another two pixels from the marker box.
2. **Digit floors.** A split pane reserved at least two digits, so a file of
   nine lines or fewer got a column twice the width of its numbers. One pane
   reserved at least four digits per column.
3. **One digit width, not the widest.** It measured `'9'` only. With a
   monospace code font that is every digit, so this did not widen anything;
   with a proportional font it could clip.
4. **A staged file kept the empty pane's gutter.** Files over 4096 lines on
   both sides together are filled into detached documents and adopted with
   `setDocument` (BPR-1264). That emits no `blockCountChanged`, so nothing
   recomputed the width: a 100000-line file kept the two-digit width of the
   empty pane (28 px) until something else, such as the marker column
   toggling, recomputed it. This one made the gutter too narrow, not too
   wide, and clipped six-digit numbers.

The marker and "+" column (22 px, BPR-1290 and BPR-1212) was not the cause.
It stays the same.

## Change

- The number column is the longest number's digits times the widest digit
  advance of the pane's font, plus 4 px either side. No digit floor.
- One pane: each of the two columns is the larger of that and the width of
  its `base` / `head` heading, plus the same padding. Its longest number is
  the larger of the last base and head numbers, not the row count.
- The width is cached and recomputed when the document is replaced (including
  staged adoption), the block count changes, or the font, device pixel ratio,
  unified rows, marker column or number visibility changes. The layout is
  touched only when the width or heading row actually changed. Scrolling
  recomputes nothing.
- A file that ends in a newline still numbers the empty line after it, as
  VS Code does and as BPR-1291's scroll-past-end course expects. That number
  counts toward the width. A 99-line file ending in a newline therefore shows
  "100" and gets three digits.

## Open question for the owner

GitHub does not number the empty line after a final newline. This app does.
Dropping that number would save one digit for files of 9, 99, 999 … lines
that end in a newline. It would also change the scroll-past-end behaviour,
so it was not done here.

## Owner updates, 2026-10-08

Relayed by the coordinator, verbatim:

> Update on line numberwidth, what I thought was pure waste I think is
> reserved space for comment markers. I just think that space wastes too much
> across both panes

> I thought we had moved comment markers to the scroll bar area in our
> mock-up

The coordinator asked for options (a) a column on the head pane only, (b)
the marker overlaid on the number, (c) a half-width column, and (d) no
per-line column at all, with threads shown by the scroll-bar ticks, a tint on
the line and the drawer notes, and the "+" on hover in the number area. (d)
leads, and goes to the owner against the leanest of (a) to (c).

**The mock-up.** Both BPR-1290 inline-drawer prototypes
(`docs/design/BPR-1290-inline-drawer-v1/index.html` and `-v2/index.html`)
drew a 24 px marker column in the gutter (`.ln .m` with `.pin` markers) *and*
a 14 px scroll-bar ruler with a tick per thread. Markers were never moved to
the scroll bar alone. BPR-1290:navigation, which the owner accepted, still
names "its marker in the code's gutter" as one way to open a thread.

**Leanest of (a) to (c).** (b) gives back the whole 22 px column in both
panes (44 px). (a) gives back 22 px in the base pane only, and (c) 11 px per
pane. (b) is the leanest, and it reclaims as much as (d).

## Slice 1: the number column (on main, 54c5a5988)

Offscreen test font, one digit 8 px, logical pixels, the same at 1.0x, 1.25x
and 2.0x. Files end without a newline, so the longest number is the line
count.

| File | Before | After |
|---|---|---|
| 9 lines | 28 | 16 |
| 99 lines | 28 | 24 |
| 1557 lines | 44 | 40 |
| 100000 lines | 28 (stale; 60 once recomputed) | 56 |
| one pane, 100000 lines | 120 | 112 |

With the marker column on, add 22 px to each split-pane figure, before and
after. Control: the final test against the source before the change fails
at all three scales ("9-line file: … gutter=28 expected=16").

## Slice 2 candidate: no marker column (branch `bpr-1335-markers`, db1b346a3)

Not on main until the owner chooses. `thread_marker_style` has three values:
`column` (today's, still the default), `in_numbers` (option b) and
`number_tint` (option d). Without a column:

- Both gutters are their numbers only. In the candidate renders each goes from
  54 px to 32 px; 22 px of that is the column, in each pane.
- `in_numbers`: the thread's marker (label mark, new dot, open ring) takes
  the number's place, sized to the row.
- `number_tint`: the number stays and its cell is tinted amber, blue when
  new, lighter when resolved, dashed outline for my draft. Pressing anywhere
  on the cell opens the thread.
- In both, the "+" takes the number's place under the pointer on a head line
  without a thread. The tooltips are unchanged. The scroll-bar ticks and the
  open thread's line wash are unchanged.
- In every style, a selected line with a thread says so in its accessible
  description ("Thread: … The context menu opens it"), and the code's context
  menu has **Open thread**. Return still adds a comment.

Checks on db1b346a3, macOS arm64 Debug, offscreen, load average about 70:
`qt_diff_workbench_scale_{100,125,200}` and
`qt_workbench_window_scale_{100,125,200}` passed. `test_thread_marker_candidates`
measures each style's widths in light and dark and presses the thread
marker, the tinted cell, the base side's marker and the "+". It also checks
the accessible description with and without a thread. The window test opens
line 22's thread from the context menu, and finds no Open thread on a line
without one.

## Owner look on v001

Session `3c9017b5e4768e3a76a25c14bf81ee21`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1335-markers-v001`,
checkout `/Users/jaeger/Documents/GitHub/bpr-1335`, candidate
`BPR-1335-thread-markers-v001` (revision `bpr-1335-markers@db1b346a3`,
fingerprint `329a11fa8045…`), ready at event cursor 3. One option-list
question: Option 1, tinted line number (d), or Option 2, marker in the
number's place (b), with today, Option 1 and Option 2 rendered at 2x in light
and dark. Offline page: `review.html` in the state directory.

## Next

On the owner's choice: delete the losing style and the `column` style, make
the chosen one the only presentation, and recheck the
`qt_workbench_window` "+" test: it aims 8 px inside the gutter's right
edge, which is in the number cell once the column is gone. Update the
specification's gutter text and BPR-1290's navigation wording as an owner
amendment. Then merge to main, rerun the two rows at three scales, and
finish.

## Owner verdict on v001, 2026-10-09

Session `3c9017b5e4768e3a76a25c14bf81ee21`, candidate
`BPR-1335-thread-markers-v001` (revision `bpr-1335-markers@db1b346a3`,
fingerprint `329a11fa8045…`): **Reject** (decision
`f922d012-da88-402c-be96-e92425943667`, 01:39:00Z, handed back at event 5;
cursor 3 to 5 read). Note, verbatim:

> I like option 1 but I don't like the + replacing the line number, i think
> github actual does it better

The owner attached a screenshot of a GitHub split diff (`src/parse.cpp`,
dark theme). It is kept in the session file, not committed. In it, GitHub's
"+" is a blue rounded square sitting on the boundary between the number
gutter and the code, half over the start of the code. The line's number
("2") stays visible to its left.

Read as: option 1 (tinted line number, `number_tint`) is chosen. The "+"
must not take the number's place. It should float at the gutter's right
edge, overlapping the start of the code as GitHub's does, with the number
still visible. Nothing else in v001 was rejected. BPR-1335:markers is asked
again on v002.

## Slice 2: tinted numbers and GitHub's "+" (on main, 034e84ad9, with the docs in 7de324815)

What the owner's v001 verdict asked for, on `bpr-1335-markers` rebased onto
`1a2a003e5` (BPR-1334's line details, BPR-1336's zoom and BPR-1212's drawer
drafts underneath) and merged to main. The frontend-design skill was loaded
for this slice.

- **One presentation.** `thread_marker_style`, its setter, the `column` and
  `in_numbers` styles, the label-mark glyphs and `set_mark_theme` on the
  comparison are gone. A thread's number keeps its place and its cell is
  tinted: amber, blue when new, lighter when resolved, dashed outline for my
  draft; the open thread's number is barred amber. Pressing anywhere on the
  cell opens the thread. Both gutters are their numbers only, with threads
  shown or not.
- **The "+" is GitHub's.** A child button of the head pane
  (`diffLineAddButton`, a `QAbstractButton`), raised above the gutter and the
  code: a blue rounded square (the theme's new-thread blue) with a white
  cross, the row's height up to 20 px. It shows while the pointer is on a
  head line without a thread, over its code or its number, and sits at the
  start of the line's code across the boundary with the number. Its left
  edge is 2 px into the gutter's 4 px right padding, so it never covers a
  digit; centring it on the boundary, as GitHub's wider number cell allows,
  would cover half of the last digit here. Its tooltip is "Add comment (↩)",
  its accessible name "Add comment", its description names the line, and an
  accessibility press adds the comment. It takes no focus: Return and the
  context menu stay the keyboard's way, as before.
- **Text selection.** A press on the button never reaches the code, so it
  neither starts nor changes a text selection. A press, double-click or drag
  in the code is the text's and hides the button until the pointer moves
  again with no button held. Scrolling hides it too.
- **Ranges by drag, and BPR-1212's drawer drafts.** Dragging the button
  washes the lines from where it started to the one under the pointer, in
  the same blue at the same strength as a draft being written, and on release
  dispatches the first and last lines. The window selects them
  (`restore_source_range`) and takes the one path Return and Shift+Return
  use: `core::decide_comment_range` decides, a refusal says why in the status
  line, and the drawer draft opens on "Lines a–b" with its bracket across
  them. On a line that already has my kept draft the draft takes the new
  range. A click is one line, as before.
- **Not offered.** No "+" on a line with a thread or a kept draft (its tinted
  cell opens it), on the base side, or in the one-pane view, as before
  BPR-1335.

### Controls (failing before)

The new and changed tests, built against main's Qt adapter (`c8ae21f08`,
the `column` presentation and the gutter "+"):

- `qt_diff_workbench_scale_100`: "9-line file: showing the threads keeps the
  gutter its numbers' width" fails (the column adds 22 px). With that test
  skipped (`BPR_1335_CONTROL=1`, a local edit not committed), the new test
  fails at "light: with threads shown, both gutters are their numbers only,
  with no column".
- `qt_workbench_conversation_reveal`: "the pointer on line 5's code shows its
  "+"" fails.
- `qt_workbench_line_draft`: "the pointer on line 4's code shows its "+""
  fails.

### Checks on the candidate

macOS arm64 Debug, offscreen, load average 46 to 220. The rows were run
again on the final rebase (7de324815): 11 of 11 pass.

- `qt_diff_workbench_scale_{100,125,200}`,
  `qt_workbench_window_scale_{100,125,200}`, `qt_workbench_conversation_reveal`,
  `qt_workbench_line_draft` and `qt_composed_inline_drawer_scale_{100,125,200}`
  pass.
- `test_tinted_numbers_and_the_add_button`, light and dark: no column; the
  thread's cell amber (`#e7d7b5` light, `#4d3f21` dark), the new one's blue,
  a plain one untinted, and the tinted cell still inked with its number; the
  tinted cell opens its thread at either end and in the base pane. Line 2's
  button at x 31 against the code's edge at 33, 15 by 15 on a 15 px row,
  centred on it, blue face `#0969da` and white cross; line 2's number band
  identical pixel for pixel with and without the button; no button on line
  4 (thread); the same button from the number; Leave hides it. Click: lines
  (2, 2), no text selection, selection unchanged. Drag 2 to 6: washed while
  it lasts, (2, 6), wash gone after; upward 6 to 3: (3, 6). A drag in the
  code selects text, hides the button and adds nothing. The thread note in
  the line details' description, and none on a plain line.
- The compose course clicks line 5's "+" into a drawer draft; the line-draft
  course drags 4 to 6 into the draft "Your draft on lines 4–6" with the
  bracket across them, its range sent as one edit.
- The integration pass over the `qt_workbench_*`, `qt_composed_*` and
  `qt_diff_workbench_*` rows these binaries own: 34 of 37 pass.
  `qt_workbench_home_discovery_scale_*` fails at "the empty Needs me lane is
  not shown while nothing is saved"; BPR-1338 recorded that as pre-existing.
- The round-trip composition pins (`full_application`,
  `production_app_manifest`, `production_mutation_branch`) match the merged
  tree at 7de324815 (`BPR_PRINT_PINNED_DIGESTS`). BPR-1335 changes nothing
  under `src/app`. BPR-1331's 7dfde8915 had left two pins unsealed on main;
  a BPR-1335 reseal was written and then dropped, because BPR-1328's
  1a2a003e5 resealed the same change first.
- The specification's gutter, "+" and thread-marker text and BPR-1290's
  navigation criterion carry the owner's amendment.

## Owner look on `BPR-1335-add-button-v001` (pending)

One new short session, one question, Accept or Reject: "tinted numbers +
GitHub-style add button".

- Session `6541cdcdee84a0c761d8b7a1410a2073`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1335-add-button-v001`,
  repo root the main checkout, candidate `BPR-1335-add-button-v001` (revision
  `frozen-7de324815-v001`, fingerprint `16e3f3ee2d3a…`), criterion
  `BPR-1335:markers`, ready at event cursor 3. The host runs detached; the
  offline page is `review.html` in the state directory. Light and dark 2x
  renders of the "+" on line 2 and of a drag from line 2 to line 6 are inline.
- Frozen root `/Users/jaeger/bpr1335-add-button-v001.jo2yocwy`, made by
  `scripts/freeze_attended_build.py freeze` from 7de324815 (tests-on
  `app-release`, source read-only), executable SHA-256 `c5084cec428b…`.
  Fixture `inline-drawer-v1`, file `src/ui/conversation_cards_view.cpp`. The
  freeze's offscreen `--check` and one native `--check --native` both reached
  `ATTENDED_FILE_SHOWN` on head `89df5e59…`, and the native window closed.
- Launch: `python3 /Users/jaeger/bpr1335-add-button-v001.jo2yocwy/launch.py`.
- v001's session `3c9017b5…` was handed back with its host stopped. It is
  released (`resume-development --all`, sequence 6) with its Reject kept, and
  its checkout, the `bpr-1335` worktree, and its build tree are removed.

## Owner verdict on `BPR-1335-add-button-v001`, 2026-10-09

Session `6541cdcdee84a0c761d8b7a1410a2073`, candidate
`BPR-1335-add-button-v001` (revision `frozen-7de324815-v001`, fingerprint
`16e3f3ee2d3a…`): **Reject** (decision
`15f36203-cc0e-4bad-b3aa-85fe8018c24f`, 04:02:28Z, handed back at event 5;
cursor 3 to 5 read). Note, verbatim:

> It is the github style like I asked for, and I like the positioning. But i
> find it somewhat distracting always being drawn. Maybe we only draw it when
> the cursor is over the column where it would go?
>
> And why can't I add a comment from the left pane? Seems like that would be
> reasonably useful, is there a technical reason I can't?

Read as: the tinted numbers, the button's look and its position stay. The
"+" is drawn only while the pointer is over its own column, the strip across
the number and code boundary where it is drawn, not anywhere on the line.
Its place and the drag across lines stay. The second paragraph asks whether
the base (left) pane can take a new comment, and if not, why not.

The session is released (`stop`, then `resume-development --all`, sequence
6) with its Reject kept. Frozen root `bpr1335-add-button-v001.jo2yocwy` was
removed with `freeze_attended_build.py remove`.

## Linux station run (bpr-main-linux, 2026-10-08)

At `e602e2cf9`, WSL2 Ubuntu 26.04, GCC 15.2.0, `build/dev` (Debug), offscreen, network-isolated runner:
- **Pass:** `qt_workbench_window_scale_{100,125,200}` and
  `qt_diff_workbench_scale_100`, plus `qt_workbench_conversation_reveal`,
  `qt_workbench_line_draft` and `qt_composed_inline_drawer_scale_{100,125,200}`.
- **Fail:** `qt_diff_workbench_scale_125` and `qt_diff_workbench_scale_200`,
  at "light: the button is blue with a white cross". The cross pixel read
  `#3785e1` at 1.25× and `#c2daf6` at 2×. The face was `#0969da` at both.
- **Diagnosis:** a test-sampling fault, not a rendering defect.
  - A temporary capture, reverted and not committed, shows a correct blue
    rounded button with a centred white cross at both scales.
  - The button is 14×14 logical at (29,20). `QRect::center()` is (35,26),
    half a pixel before the true centre (35.5,26.5). Scaled to device pixels,
    it lands on the cross's anti-aliased edge instead of its white core.
  - At 100% the sample still hits white, and the Mac's metrics evidently
    give a size or position where it does too.
  - The proposed test-only fix samples at the exact centre
    (`QRectF(place).center()`), or takes the brightest pixel within one
    device pixel of it. It is awaiting the coordinator.

Windows is still owed.

## v002: the "+" only in its column (on main), and the base pane's answer

The frontend-design skill was loaded for this slice. Worktree
`/Users/jaeger/Documents/GitHub/bpr-1335-v002`, branch `bpr-1335-v002`.

### The "+" column

- The head pane's "+" is drawn only while the pointer is over its own
  column: the button's own horizontal extent, the strip across the number
  and code boundary where it sits, on a line it is offered on (a head line
  no thread is on). On the rest of the line's code, and on the digits,
  nothing is drawn. The part of the column in the number's padding shows it
  as the part over the code does. Moving on into the code, leaving the code,
  a press or drag in the code, or scrolling hides it, as before.
- The look, the place, the tooltip, the accessible button, the click, the
  drag across lines and its wash are unchanged. A drag in progress keeps its
  button while the pointer is out on the code.
- `source_pane::add_button_rect` is the one place the button's rectangle is
  computed; `add_line_under` takes a point of the pane and answers the line
  whose column it is in, for the code's viewport and the gutter alike.
- The Linux station's colour-sample failure at 1.25x and 2x (above) is fixed
  in the test as the coordinator asked: the cross is sampled at
  `QRectF(place).center()`, as the brightest device pixel within one of it.

### Controls (failing before)

The new `test_tinted_numbers_and_the_add_button`, built against main's Qt
adapter (`a5cba14fb`): `qt_diff_workbench_scale_{100,125,200}` fail at
"light: the pointer on line 2's code, away from the "+" column, draws no
"+"".

### Checks on the candidate

macOS arm64 Debug, offscreen, load average 70 to 320 (other agents' builds).

- `qt_diff_workbench_scale_{100,125,200}`, `qt_workbench_conversation_reveal`,
  `qt_workbench_line_draft` and `qt_workbench_window_scale_{100,125,200}`:
  8 of 8 pass. At 2x the button is at x 31 against the code's edge at 33,
  15 by 15, face `#0969da`, cross `#ffffff`, light and dark.
- The test: away from the column on line 2's code, and on its digits, no
  "+"; in the column over the code, and in the column's part in the
  number's padding, the same "+"; just past the button on the code it goes;
  back in the column it comes back; leaving the code hides it. Click, drags
  both ways and the text-selection checks as before, from the column.
- Offscreen renders at 2x, light and dark: the pointer on line 2's code
  (nothing drawn), in its column (the "+"), and a drag from line 2 to 6.
- The round-trip pins (`full_application`, `production_app_manifest`,
  `production_mutation_branch`) are unchanged (`BPR_PRINT_PINNED_DIGESTS`);
  nothing under `src/app` changed.
- `clang-format` 22.1.8, the gate's pinned major (BPR-1352), is clean on the
  changed files after the rebase onto `dcb317bd7`. That rebase brought only
  BPR-1352's format-only changes and board edits, so the rows above, run
  before it, stand.

### Why the base pane takes no new comment

The owner asked: "why can't I add a comment from the left pane? ... is there a
technical reason I can't?" Yes, there is one, and GitHub is not it.

- **GitHub takes base-side comments.** Checked live on the owner's test
  repository, `MikeMarcin/BPR-Test` #42 (opened, probed and closed by this
  slice; its two branches deleted). With `commit_id` the head, `side: LEFT`
  was accepted on a removed line (line 10), on a base context line (line 8)
  and as a range (`start_line` 8, `line` 11, `start_side: LEFT`). A base
  line outside the diff (line 2) was refused with 422, "pull_request_review_
  thread.line could not be resolved".
- **BPR's base pane is not GitHub's base side.** GitHub numbers `LEFT` lines
  in the merge base of the base branch and the head. BPR's base pane shows
  the base branch's accepted tip. When the base branch has moved on since
  the pull request branched, the two differ, and a line number read off
  BPR's base pane names other text on GitHub, or nothing. The provider
  contract carries no merge-base identity, so BPR cannot tell the two cases
  apart today. `docs/architecture/github-host-implementation.md` records
  this, and the composition sets `leftHasCurrentDiffRevision = false` for
  the same reason (`github_application.cpp`, "GitHub's current PR diff uses
  a merge-base coordinate on LEFT ..."). That is also why existing base-side
  threads are never pinned to the base pane. A derived merge base is
  RFC-BPR-0029 Decision 4's "derived comparison" candidate, which that RFC
  leaves unvalidated.
- Offering the "+" on the base pane now would post comments on the wrong
  lines whenever the base has moved. That is line identity, so it was not
  shipped.

**Proposed follow-up, if the owner accepts.**
1. The host computes, with the local Git cache and no fetch, whether the base
   pane's commit is an ancestor of the head (the existing `is_ancestor` /
   `derive_comparison_base` port). When it is, the merge base is that commit,
   and the base pane's numbering is GitHub's.
2. That one fact sets the window's `LeftConversationEndpointCurrent`.
   Existing base-side threads are then pinned to the base pane, and new
   base-side comments are offered, together. Otherwise both stay off, and
   the base pane says why.
3. The mutation host requires the same fact for a new base-side comment.
4. Proof: a fixture with a moved base, and one live round on BPR-Test.

The base-side half is already written and kept on the local branch
`bpr-1335-base-side-wip` (`cf85998b9`; not pushed, not built beyond its
controls, not for main). It holds:
- the core's base-side rule (`decide_comment_range` and `base_outside_hunk`);
- `side: LEFT` in the mutation host and the GitHub contract, with no
  file-comment fallback;
- a version-2 recovery record that keeps each child's side;
- the base pane's own "+";
- tests that fail on main: `core_review_round_trip`,
  `core_review_comment_range`, `application_review_mutation_application_host`,
  `sqlite_review_mutation_recovery_storage` and `github_mutation_contract`.

It still lacks the window's reasons and names, the composition's merge-base
gate and a composed publish course.

## Owner look on v002

One new short session, two candidates, one Accept or Reject question each.

- Session `b4b76c70c8e54f1933410b582189d0e8`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1335-v002`,
  repo root the main checkout, ready at event cursor 6. The host runs
  detached; the offline page is `review.html` in the state directory.
- `BPR-1335-add-button-v002` (revision `frozen-b864e319d-v002`, fingerprint
  `d70e6b786fd7…`), criterion `BPR-1335:markers`: the "+" only in its
  column. The 2x renders are inline: the pointer on line 2's code, in its
  column (light and dark), and a drag from line 2 to line 6.
- `BPR-1335-base-side-v001` (revision `answer-b864e319d-v001`, fingerprint
  `68f67a508103…`), criterion `BPR-1335:base-side`: the reason above, and
  Accept to go ahead with the proposed follow-up.
- Frozen root `/Users/jaeger/bpr1335-add-button-v002.a9imrgo8`, made by
  `scripts/freeze_attended_build.py freeze` from `b864e319d` (fixture
  `inline-drawer-v1`, file `src/ui/conversation_cards_view.cpp`). Its own
  `--check` reached `ATTENDED_FILE_SHOWN` on head `89df5e59…`.
- Launch: `python3 /Users/jaeger/bpr1335-add-button-v002.a9imrgo8/launch.py`.
- v001's session `6541cdcd…` is released with its Reject kept, and its frozen
  root is removed.

## Owner verdict on v002, 2026-10-09

Session `b4b76c70c8e54f1933410b582189d0e8`, handed back at event 9; cursor 6
to 9 read.

- `BPR-1335-add-button-v002` (revision `frozen-b864e319d-v002`, fingerprint
  `d70e6b786fd7…`), criterion `BPR-1335:markers`: **Accept** (decision
  `e7d697dd-056d-418f-bc6d-ffa10b11d687`, 05:06:32Z). Note, verbatim:

  > bug: Why is this damned tooltip far away again. This is a consistent problem.

  The owner attached a screenshot (kept in the session file, not committed):
  the "+" sits on line 608, and its "Add comment (↩)" tooltip shows about
  three lines lower, near line 611, and to the right of the button. The "+"
  itself is accepted; the tooltip placement is filed as [[BPR-1354]], which
  fixes it for every control, not only this one.
- `BPR-1335-base-side-v001` (revision `answer-b864e319d-v001`, fingerprint
  `68f67a508103…`), criterion `BPR-1335:base-side`: **Accept** (decision
  `aa39b2f7-bdc6-4ac0-acb9-66417c610e42`, 05:08:01Z), no note. The follow-up
  goes ahead as [[BPR-1353]], with the plan above and the branch
  `bpr-1335-base-side-wip` (`cf85998b9`), now pushed to `origin`. Later the
  same day [[BPR-1353]] was closed as superseded: [[BPR-1212]] v002 (branch
  `bpr-1212-v002`, `d785a3f13`) already carries base-side comments with the
  merge-base check, `side: LEFT` publication and base threads on the left
  pane. Nothing is built from the WIP branch.

The session is released (`stop`, then `resume-development --all`, sequence
10) with both Accepts kept. Frozen root `bpr1335-add-button-v002.a9imrgo8`
was removed with `freeze_attended_build.py remove`.

## Close-out

Every criterion is met: `cause`, `width`, `control`, `stable` and `rows` by
slice 1 and the rows rerun on each later slice; `markers` by the v002 Accept;
`base-side` by its Accept. Windows runs of the two rows are still owed to the
station; platform eligibility is metadata, so they do not hold the card open.

## Linux re-run after v002 (bpr-main-linux, 2026-10-09)

At `65f1f6904` (contains v002, `b864e319d`, with the exact-centre and brightest-pixel
sampling), WSL2 Ubuntu 26.04, GCC 15.2.0, `build/dev`, offscreen:
`qt_diff_workbench_scale_100`, `qt_diff_workbench_scale_125` and
`qt_diff_workbench_scale_200` pass, including "the button is blue with a white
cross" at 125% and 200%. The Linux half of the owed runs is discharged; Windows
is still owed.

## Windows station run (bpr-main-win, 2026-10-09)

At `7672d5a8b`, Windows 11, MSVC 19.50, `build/windows-dev-tour-cli` (Debug), offscreen,
network-isolated runner, `-j4`:

- **Pass:** `qt_workbench_window_scale_{100,125,200}`,
  `qt_diff_workbench_scale_{100,125,200}`, `qt_workbench_conversation_reveal`,
  `qt_workbench_line_draft` and `qt_composed_inline_drawer_scale_{100,125,200}`.
- The Windows half of the owed runs is discharged.
