---
schema: "implement-rfc.card.v1"
id: "BPR-1157"
title: "Integrate local entry and source-first tabs with the accepted chrome"
status: "Parked"
rfcs: ["RFC-BPR-0061"]
depends_on: ["BPR-1146", "BPR-1151"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC61-01", "RC61-02", "RC61-03", "RC61-09"], "pack_alias": "PACK-CYCLE-15", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"RC61-01","outcome":"pass","revision":"028882c3a","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share as before (application_review_cycle_desktop_public_path and review_cycle_workbench on macOS arm64; Linux and Windows at 2f5894786). The owner accepted BPR-1157-home-section-v002 (028882c3a build, event 791) and BPR-1157-local-identity-v001 (c2a0df54a build, event 792) in session cbd48b91000b00f1e72e1ad62c7c384c, both with empty notes, and handed back at 795."}},{"criterion":"RC61-02","outcome":"pending","revision":"c2a0df54a7b2ca0b48f76777afc6975728392e51","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"The page replaces the review surface in place with the review bar and tabs above the panes, Updates folded, PR-only title-row controls and status line hidden; a pull request arriving takes the surface back and Home returns to the cycle. Owner judgment pending: BPR-1157-surface-rows-v001, still unanswered at hand-back 795; 73b2160fc changes nothing it asks (observation at event 801)."}},{"criterion":"RC61-03","outcome":"pass","revision":"c2a0df54a7b2ca0b48f76777afc6975728392e51","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Full source is one pane from R0; unchanged, whole-file and older-revision notes accepted; comparison gestures on cycle tabs refused so no all-added diff or empty baseline appears (core_review_cycle_tabs, qt_review_cycle_view, review_cycle_workbench). Comparison views are BPR-1163."}},{"criterion":"RC61-09","outcome":"pending","revision":"73b2160fc","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Owner rejected BPR-1157-native-keyboard-v001 (events 793/794): no visible focus; asked for Return to comment and Shift+Return to suggest in the read-only code pane; narrow width fine. 73b2160fc: a two-pixel focus ring on every Tab stop (Highlight lifted to the 3:1 indicator floor), Return notes the selection or the caret's line and Shift+Return begins Suggest change; qt_review_cycle_view_1_0/_2_0 and application_review_cycle_desktop_public_path drive window-system Tab/Return/Shift+Return, offscreen and natively under cocoa (17 stops ringed); five negative controls fail. Owner observation pending as BPR-1157-native-keyboard-v002; Linux and Windows runs of the changed rows owed."}}]
---

Pack alias: **PACK-CYCLE-15** (live `BPR-1157`, allocated at import).

# BPR-1157: Integrate local entry and source-first tabs with the accepted chrome

## Outcome and bounded work

Add Open Project, cycle recents/title badge and R0 full-source view in the RFC0052 scaffold. Preserve title/review bar, fold, per-tab exact selection and tray. Prepare a small native design course for genuinely unresolved layout choices without replacing accepted art.

## Acceptance and direct evidence

No-account entry to first note, native width/height/focus/key checks, tab/source restoration, no extra permanent row, unchanged-file comment action and subject-switch regression to a real PR.

Criteria: RC61-01, RC61-02, RC61-03, RC61-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), Qt 6.11.2, `build/dev` freshly configured at `2631bcf31`, and the fix `384525fb9`. The Qt rows run as registered: `QT_QPA_PLATFORM=offscreen` with `QT_STYLE_OVERRIDE=Windows`.

Evidence only; the card stays parked for the owner. At `2631bcf31`, `review_cycle_workbench` aborted on Linux: `std::optional<review_cycle_projection>::operator->` asserted on an empty optional. GCC 15's libstdc++ checks this in the unoptimized dev build; libc++ on macOS does not by default. The course held `resumed` as a reference to the host's live projection, called `close()`, which empties `Cycle`, and then read `resumed.Cycle->Subject`. That is undefined behaviour in the test, not the product. `384525fb9` copies the subject before closing. After it, `review_cycle_workbench` (tabs, revision, path and reading line restored after restart; resume from recents), `core_review_cycle_tabs`, `qt_review_cycle_view_1_0` and `_2_0` (900-pixel compact width and named regions, offscreen), and `application_review_cycle_desktop_public_path` (six 1280x800 renders produced; not inspected) pass. For RC61-09 this is the Linux automated share of keyboard and focus. It ran offscreen, not in a native desktop session; the owner's Mac observation and the Windows run remain.

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

## Home section rejection: a launcher fault, and the window it exposed — macOS, 2026-10-01

The owner rejected `BPR-1157-home-section-v001` at session event 720: "The
window I got looks not correct." The attached screenshot shows an empty
"Open Project · Local cycle" page with "Open a project to review it locally."
and no Home.

Diagnosis. The v001 launcher set only `HOME`, which macOS ignores for
Application Support. The frozen `c2a0df54a` app therefore opened the owner's
own profile, not the fresh one its item promised; its `home/` stayed empty.
Five minutes earlier the `f3327b74b` candidate had migrated that profile's
`review-cycles.sqlite3` to schema 7, and the schema-4 build refused it. Listing
recents failed at start, and the refusal took Home over with a page whose only
text was the generic notice. The reason was in the folded Updates line.

Fix at `028882c3a`. A refusal before any cycle opens takes the page only after
the reviewer asks for a cycle (Open Project or a recent row); at start Home
stays. The page states the refusal in the source pane. A schema mismatch reads
"This profile's Review Cycles were saved by a different version of
BetterPullReview; they were left as they are." The desktop course marks a
profile's store newer (`PRAGMA user_version=99`) and checks that Home stays at
start and that the refusal is shown after Open Project. Dropping the guard
fails "Home stays in place at start". The course's render of that page
matches the owner's screenshot.

Launchers. `CFFIXED_USER_HOME` moves the macOS profile; an offscreen probe of
both frozen apps wrote the profile under the given home and left
`~/Library/Application Support/BetterPullReview` untouched. The new candidate's
launcher sets it. The shared v001 launcher
`/Users/jaeger/bpr-cycles-authoring-review.xpXmIl/launch.py` now sets it as
well, so the four pending v001 items open their own profile. Their app is
unchanged, and each item has an observation recording this (events 730-733).
The owner's own profile keeps the schema-7 store.

Asked again as `BPR-1157-home-section-v002` on
`/Users/jaeger/bpr-cycles-syntax-review.rMJBTD` (app sha256
`85492e75…ebef307`). The item says what the owner should see and carries
native renders of Home, of Home with a recent, and of the refusal page, with
the v001 screenshot for comparison. The card stays Parked.

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

## Owner verdicts at hand-back 795 and native-keyboard v002 — macOS, 2026-10-02

In attended session `cbd48b91000b00f1e72e1ad62c7c384c` the owner decided
three of this card's four open items and handed back at event 795:

| Candidate | Criterion | Event | Verdict |
|---|---|---|---|
| `BPR-1157-home-section-v002` (028882c3a build) | RC61-01 | 791 | Accept |
| `BPR-1157-local-identity-v001` (c2a0df54a build) | RC61-01 | 792 | Accept |
| `BPR-1157-native-keyboard-v001` (c2a0df54a build) | RC61-09 | 793, then 794 (the note completed) | Reject |
| `BPR-1157-surface-rows-v001` | RC61-02 | — | Not answered |

The rejection, verbatim (event 794):

> Tab does cycle between things but it's not obvious at all what has focus.
> Also we need a good shortcut key to add comment. Like maybe when focused on
> teh code pane in the read-only mode enter adds comment, shift+enter suggests
> edit?
>
> Resizing the window to be narrow worked fine.

RC61-01 passes: both of its questions are accepted. The narrow window needs no
change.

### Why focus did not show

A native run of the composed window walked the Tab chain on the cycle page:
the title row's Home, Account and Settings, the Updates disclosure, the review
bar's enabled buttons, the document tab bar and New tab, the files list, the
code pane and the notes (with macOS Keyboard navigation off, plain buttons are
not stops). None of them showed focus clearly:

- The review bar's buttons marked focus with one pixel of `Highlight` inside
  their border. Under the profile that is the selection colour, `#28518C` on
  the dark surface `#1C2026`, about 2.06:1, and on a checked toggle (Files,
  Notes) it was the same colour as the toggle's own border.
- The Updates disclosure, the tab bar and the lists asked the style for a
  focus rectangle, which macOS styles do not draw at all.
- The code pane only changed its caret from dimmed to the text colour.

The reading pane was also announced to accessibility as "Suggested code" until
the first suggestion ended. It is now "Source" from the start, and its
description names Return and Shift+Return.

### What changed at `73b2160fc`

- **One focus ring.** `keyboard_focus_ring` (new, `src/adapters/qt`) is a
  transparent overlay that draws a two-pixel rounded ring inside its control's
  edge while the control has focus. It keeps the ring while a popup opened from
  the control is up. It takes no pointer input and no focus, and it tells
  accessibility it is invisible decoration, so the control keeps its own
  accessible focus. The cycle page puts one on every stop Tab can reach outside
  the review bar (tabs, New tab, files, code, comparison, notes, the tray's
  buttons and editors, and the composer's controls). The review bar's buttons
  (through the strip style, so the title row's too) and the Updates disclosure
  draw the same two-pixel ring over their border. A checked toggle keeps its
  one-pixel border; focus adds the thicker ring. The tour editor's private copy
  of the same overlay is replaced by the shared one.
- **Its colour, decided by the core.** `core::visible_focus_indicator` keeps
  `Highlight` where it reaches the 3:1 indicator floor on the surface and
  otherwise moves it toward white or black by the least amount that does, the
  rule `readable_selection_ink` already applies at the text floor. On the
  standard dark profile the ring is about `#486B9D`, 3.01:1; the hue stays the
  theme's.
- **Return and Shift+Return, the owner's suggestion recorded as the decision.**
  The code view hands Return or keypad Enter, alone or with Shift only, to its
  reader while it is read-only (`suggestion_editor::Activated`). The cycle page
  answers Return with a note on the selection, or on the caret's line when
  nothing is selected (`core::line_text_at`: the line's bytes without its
  ending, an empty line being an insertion point), and Shift+Return with
  Suggest change at the caret. Editing, the code view never offers the key:
  Return breaks the line with the core's indentation, as before. Ctrl+Return
  and Alt+Return do nothing in the reading pane.
- **Hints.** The source pane's context menu now holds its own Comment and
  Suggest change, which show Return and Shift+Return beside them (`↩`/`⇧↩` on
  macOS, Enter/Shift+Enter elsewhere). They are labels only: an action of a
  standalone menu never matches a key, so nothing is registered in the window.
  The menu's Comment acts where the caret is, as Return does. The review bar's
  Comment still notes the whole file when nothing is selected, and its tooltip
  and Suggest change's name the keys.

Conflicts checked. No action in the composed window claims Return or
Shift+Return; the desktop course scans every action for them. The window's
`Alt+C` (Comment on a pull request), `Alt+R` and the tour's `Alt+Return` are
untouched. The workbench's duplicate-key scan (`qt_workbench_window_*`) still
passes.

### Verification

macOS arm64, `build/dev`, Qt 6.11.2, dark scheme.

- `cmake --build --preset dev`: full build, no `warning:` line.
  `clang-format --dry-run --Werror` and `_coding-standard/scripts/tidy.py
  --warnings-as-errors` on the changed files are clean. The `core-only` preset
  builds and passes `core_appearance_contrast` and `core_review_cycle_feedback`.
- Integration pass:
  `ctest --preset dev -j 8 -R "^qt_|^core_|^application_(workbench|composed|review_cycle|review_tour|review_round_trip|diff_workbench_public_path|workbench_composition_contract)|^review_cycle|^sqlite_review_cycle|^syntax_classification_contract$|^architecture|network_isolation"`
  passes **236 of 236**.
- `qt_review_cycle_view_1_0` and `_2_0` walk the page with window-system Tab
  on the dark profile. At every stop the ring appears where it was absent, by
  the local-colour rule over a ring-wide band inside the left and right edges.
  It leaves the stop focus left, and accessibility receives a focus event for
  the stop, which has a name. Ringed stops: the code, the tray's two buttons,
  the notes, Updates, the review bar's buttons, the tab bar, New tab and the
  files. Return with only a caret opens the note composer, focused, on
  `src/parser.cpp · line 3`, and saves exactly bytes 12–29 without the CRLF;
  keypad Enter on a selection notes it exactly. Shift+Return begins Suggest
  change with the caret where it was. Editing, Return gives
  `{\r\n\t\r\n` and opens no note.
- `application_review_cycle_desktop_public_path` does the same in the composed
  window through its real shortcut map: 19 stops ringed offscreen, title row
  included. Run natively as
  `QT_QPA_PLATFORM=cocoa build/dev/tests/end_to_end/review_cycle_desktop_public_path`,
  it passes with 17 stops (Keyboard navigation off). One native run crashed
  when another application kept the window from activating, so nothing had
  focus. `ce0ea8368` makes the course fail at "the window is active, so keys
  reach it" instead.

Negative controls, each a temporary source change (none committed), each
failing where it should:

| Control | Fails at |
|---|---|
| No rings on the page (`ring_keyboard_focus` call removed) | "the focus ring appears on the stop Tab reaches" (view), "every stop Tab reaches shows the focus ring" (desktop) |
| The old one-pixel strip ring | the same two checks, on a review-bar and a title-row button |
| No `Activated` handler | "Return opens the note composer, focused"; "Return in the reading pane opens a note on the caret's line" |
| The code view offering Return while editing | "editing, Return breaks the line and indents, and opens no note"; "Enter after a brace indents one tab, CRLF kept, and opens no note" |
| The ring left at raw `Highlight` | `core_appearance_contrast` "lifted to the indicator floor"; the view's "the ring is lifted off the dark selection colour" |

The view course also hides the code view's ring and checks the rule then finds
none.

### Asked again

Asked as `BPR-1157-native-keyboard-v002` (RC61-09). It is one Accept/Reject
question that states what to see: a ring on each stop, and Return and
Shift+Return in the read-only pane. Four native renders are attached. The
candidate is frozen at `/Users/jaeger/bpr-cycles-focus-review.NGa8CN` (app
sha256 `f92f544b8994d0ef5ecdf624edd41259f73bd9315e42bd7fed0be1808b8761d8`, a
copy of the dev bundle at `73b2160fc`). Its launcher sets `HOME` and
`CFFIXED_USER_HOME` to the root's `home/`. An eight-second offscreen trial
start wrote its profile there and left
`~/Library/Application Support/BetterPullReview` untouched. Session cursor 805.

Protections were released by candidate name only (events 798–800):
`native-keyboard-v001`, `home-section-v002` and `local-identity-v001`, whose
verdicts stand. `surface-rows-v001` and `BPR-1241-retarget-key-v001` stay
protected. This change touches their files but nothing either asks, as recorded
in observations 801 and 802.

Owed: Linux and Windows runs of the changed rows. The card stays Parked on
RC61-02 (`surface-rows-v001`) and RC61-09 (`native-keyboard-v002`).

```sh
ctest --preset dev -j4 --output-on-failure -R "^qt_|^core_appearance_contrast$|^core_review_cycle_feedback$|^application_review_cycle_desktop_public_path$|^application_review_tour_composition_|^review_cycle_workbench$|^architecture_boundaries$"
```

```powershell
ctest --preset windows-dev -j4 --output-on-failure -R "^qt_|^core_appearance_contrast$|^core_review_cycle_feedback$|^application_review_cycle_desktop_public_path$|^application_review_tour_composition_|^review_cycle_workbench$|^architecture_boundaries$"
```

Limits: the rendered checks run offscreen and natively on macOS only. On
macOS with Keyboard navigation off, plain push and tool buttons are not Tab
stops, as in other Mac applications. The comparison pane is ringed but has no
Return gesture.

### Linux at `862e31a93`: focus rings and Return/Shift+Return (bpr-main-linux, 2026-10-02)

Includes `73b2160fc`. WSL2, GCC 15.2, Qt 6.11.2, `build/dev` Debug, offscreen.
`cmake --preset dev && cmake --build --preset dev` exits 0 with 0 `warning:`
lines, and there is **no link error**: `tests/support/window_system_keys.hpp`,
used by `review_cycle_view_tests` and `review_cycle_desktop_public_path`,
links against QtGui. The requested `ctest --preset dev -j4` selection passes
**117 of 123** in 89.93 s.

The 6 failures are [[BPR-1255]]'s rows, `qt_workbench_title_row_scale_{100,125,200}`
and `qt_workbench_window_scale_{100,125,200}`. Each fails only at `no
review-bar control folds into an overflow menu at any width`, which is parked
on the owner's choice. No other row failed: `qt_review_cycle_view_*`,
`application_review_cycle_desktop_public_path`, the three
`application_review_tour_composition_*` rows, `core_appearance_contrast`,
`core_review_cycle_feedback`, `review_cycle_workbench` and
`architecture_boundaries` all pass, so no pixel check failed at 1×. The
[[BPR-1236]] row is not in this selection. No remote mutation.
