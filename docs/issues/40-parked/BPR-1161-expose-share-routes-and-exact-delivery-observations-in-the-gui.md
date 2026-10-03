---
schema: "implement-rfc.card.v1"
id: "BPR-1161"
title: "Expose share routes and exact delivery observations in the GUI"
status: "Parked"
rfcs: ["RFC-BPR-0056", "RFC-BPR-0061"]
depends_on: ["BPR-1156", "BPR-1157", "BPR-1159"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "portable-rounds", "acceptance": ["RC56-07", "RC56-10", "RC61-05", "RC61-06"], "pack_alias": "PACK-CYCLE-19", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}, "related_issues": ["BPR-1212"]}
parked_on: "human-review"
acceptance: [{"criterion":"RC56-07","outcome":"pass","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 core_review_cycle_share, sqlite_review_cycle_storage, review_cycle_workbench, qt_review_cycle_view_* and the desktop course: copied/saved/retrieved/accepted/acknowledged admitted only from channels that can show them, for this round and digest; copied only when the platform clipboard returns the same text (else uncertain); saved under the file name only; each listed in its own words with 'none means received, read, acted on or accepted'; no note status changes; persisted across restart. Negative controls fail."}},{"criterion":"RC56-10","outcome":"pass","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Every route equals authorize's reviewer export over 225 classification/channel/setting rows, and a granted harness's retrieval gets the same answer; a clipboard denied for an unclassified plain folder copies, records and freezes nothing, says so, writes no file instead, and Save keeps its own decision (core_review_cycle_share, review_cycle_workbench, qt_review_cycle_view)."}},{"criterion":"RC61-05","outcome":"pending","revision":"3b5f405be","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"v001 (the Compose feedback page) rejected at event 834; direction A chosen at 860; v002 (the Share notes sheet, 324c17826) rejected at event 895 because a note's code was not aligned, not syntax coloured and not visibly in a monospace face. Since then the excerpt and the whole-file view use the profile's code font (bundled JetBrains Mono NL), tabs expand from the code's first column at the panes' width, and both are coloured by the panes' classifier and role colours (3b5f405be). Automated: qt_review_cycle_view_1_0/_2_0 (rendered column alignment on every row kind, profile font, role/text rule) and application_review_cycle_desktop_public_path (real classifier) pass with four negative controls. Asked again as BPR-1161-compose-share-v003 (session cbd48b91000b00f1e72e1ad62c7c384c, added 907, ready 915) on the self-contained frozen build in /Users/jaeger/bpr1161-share-review-v003.WQEATE (app sha256 af8f9975...6bce0c4c, --code-font-probe CODE_FONT_OK); the owner's judgment is pending."}},{"criterion":"RC61-06","outcome":"pass","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Owner Accept of BPR-1161-delivery-words-v001 on the frozen app dc64a2c2...0f2128 (session cbd48b91, event 859, decision d63bf276-9711-4c39-8866-556a938dfd35, empty note): each delivery observation said apart in its own words, none claiming more. Automated share passes (each observation kind worded apart; external route only when reachable, shows the exact invocation, records and starts nothing). The rebuilt share sheet keeps these words and the caption verbatim."}},{"criterion":"BPR-1161:boundary","outcome":"pass","revision":"3b5f405be","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"At 3b5f405be on macOS: the 18-row BPR-1161 selection incl. architecture_boundaries and test_network_isolation_manifest, and all 118 qt_ rows pass; clang-format and clang-tidy clean on every changed unit. Earlier at 324c17826: the 52-row review-cycle selection and core-only's 24 core_review_cycle rows. Linux and Windows runs at 3b5f405be owed (commands on the card)."}}]
---

Pack alias: **PACK-CYCLE-19** (live `BPR-1161`, allocated at import).

# BPR-1161: Expose share routes and exact delivery observations in the GUI

## Outcome and bounded work

Compose feedback and Share feedback in the accepted native layout. Copy and Save are first-class, with selection/order/context preview; optional routes are capability-driven. Freeze+delivery may be one action without confusing copied/saved/read/acknowledged states.

## Acceptance and direct evidence

No-agent native share course, identical round Copy then Save, amendment after share, explicit destination/bytes, denied channel, oversized output and no per-item confirmation/second prompt. No automatic executable discovery or network on preview.

Criteria: RC56-07, RC56-10, RC61-05, RC61-06.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Implemented — macOS, 2026-10-02

At `7a385389c`. Compose feedback opens the cycle's latest round draft, or
starts the first with every open note, in the centre of the cycle page (Back
and Escape leave it, as on a pull request's Compose review). Copy to
Clipboard, Save to File… and Save as Bundle… are buttons on the page; each
freezes the previewed revision and shares it in one click. Use in external
agent and Send to Agent appear only when the host reaches them, which this
build does not. Every route is decided by `core::authorize`; a refused one is
shown refused, shares nothing and is never replaced by another. Delivery
observations are admitted by the core per channel, kept in the store's new
`deliveries` table (schema 8) and listed on the page in their own words.
Owned paths, commands, the 49-row pass, ten negative controls and the
compatibility decision are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"BPR-1161: Compose feedback, Share feedback and delivery observations".

RC56-07 and RC56-10 pass on the automated evidence. RC61-05 and RC61-06 have
their automated share and wait for the owner.

## Pending owner judgment

One Accept/Reject question per criterion in session
`cbd48b91000b00f1e72e1ad62c7c384c`, added at sequence 826:

- `BPR-1161-compose-share-v001` (RC61-05): can one complete round be shared
  in one gesture, with Copy and Save first-class?
- `BPR-1161-delivery-words-v001` (RC61-06): are the round and each delivery
  observation legible, said apart, with nothing claiming more?

Candidate: `/Users/jaeger/bpr-cycles-share-review.3aySQ6/BetterPullReview-cycles-share.app`,
executable SHA-256 `dc64a2c2956914f3774607908888680abd7959f7a58d31e153ab8d9820d2f128`,
built at `7a385389c`, launched with
`python3 /Users/jaeger/bpr-cycles-share-review.3aySQ6/launch.py` (own profile
in `home/`, `HOME` and `CFFIXED_USER_HOME` set; saves may go to `shared/`).
`7a385389c` landed as `8cbdc1477` with identical BPR-1161 source.

## Owner verdict at hand-back 835: the share page is rejected (2026-10-02)

`BPR-1161-compose-share-v001` (RC61-05) was **rejected** at event 834,
decision `bf77d66c-1ec3-4cca-9ee7-6d6df2d036e4`, fingerprint
`2056fe1c…efe3`, on the frozen app above. The owner's note, verbatim:

> There is a lot going on in this screen and it's not a very pretty experience. I have no idea how I got to this screen, why I would wnat to do any of these actions, and what is important.

The session was handed back at 835. RC61-05 now records `fail` at
`7a385389c`. The automated share evidence still stands; what failed is how
the page presents it. `BPR-1161-delivery-words-v001` (RC61-06) was left
undecided and is still open. The rejected candidate was released by name at
836 (`resume-development --candidate BPR-1161-compose-share-v001`). Its
frozen app stays protected for the delivery-words question.

### Design round v1 (2026-10-02)

The owner asked for this design iteration to use `frontend-design`. Product
source was not touched in this round, because BPR-1169 was in progress in
`src/`. [The overview](../../design/BPR-1161-share-round-v1.html) audits the
rejected page from the candidate's own render. It pins 11 findings under the
owner's three complaints. The entry is an unlabeled pencil, and the tray's
"Next round" reads like the way to share. The only context is the title
"F2". Three equal buttons sit above the choices that shape them. A facts
line, Move Up and Move Down, and an empty "Shared so far" box add clutter.
The largest element is a raw-Markdown preview full of hashes. The file list
and the Notes tray repeat the notes beside the page. The overview links
three directions, each drawn at 1280×800 in the app's light and dark
palettes, before and after Copy:

- [A: Share sheet](../../design/BPR-1161-share-round-A-sheet-v1.html): a
  labelled Share notes button opens a sheet over the code with Copy round 2
  as its default.
- [B: Share from Notes](../../design/BPR-1161-share-round-B-tray-v1.html):
  no page. The Notes tray holds the round, and its foot holds Copy round 2.
- [C: The round as a letter](../../design/BPR-1161-share-round-C-letter-v1.html):
  the page renders the round as its reader gets it, with one side panel to
  send it.

All three keep RC61-05's one-gesture freeze, first-class Copy and Save, no
AI setup, second prompt or per-note confirmation, and the exact text. The
design commit is `57293c334`, with the audit image under
`docs/verification/BPR-1161-share-round-v1/`.

**Why this work lives here and not on [[BPR-1212]].** BPR-1212 is the later
rethink of a pull request's Compose review (RFC-BPR-0037 and RFC-BPR-0052).
It is in Backlog and covers summary and disposition, inline comments,
replies, kept drafts and the AI workshop. This page is Review Cycles' share
step. It is bound to RC61-05 and RC61-06 on this card, which is parked on
exactly this judgment. Moving the work there would either pull an unrelated
backlog card into the Review Cycles pack, or make this card wait on it. The
two are linked instead. Whichever direction is chosen here gives BPR-1212 a
starting vocabulary, and BPR-1212 now says so.

**Asked:** `BPR-1161-share-direction-v001` in session `cbd48b91`, added at
sequence 837 and ready at 839, revision `57293c334`, fingerprint
`e177ae92…cbc6b`. It is one option-list question, because A, B and C are
named alternatives, and it allows a note. It asks which direction the share
page takes. It says the delivery-words question is still open. The next
round reads `status --after 839`, builds the chosen direction, and asks
RC61-05 again on a new frozen candidate.

## Owner verdicts at hand-back 869: delivery words accepted, direction A chosen (2026-10-02)

Read with `status --after 858` in session `cbd48b91000b00f1e72e1ad62c7c384c`:

- `BPR-1161-delivery-words-v001` (RC61-06) was **accepted** at event 859,
  decision `d63bf276-9711-4c39-8866-556a938dfd35`, fingerprint
  `41f27ec2…eb6a4ba`, with an empty note, on the frozen app
  `dc64a2c2…0f2128`. RC61-06 now passes at `7a385389c`. The delivery words
  and the caption under them ("These say only what happened to the bytes.
  None of them means the round was received, read, acted on or accepted.")
  are kept verbatim by the rebuild.
- `BPR-1161-share-direction-v001` was answered with **select "A: Share sheet
  over the code"** at event 860, decision
  `c8b738f5-2ec5-457f-b132-60a58dd2b2f3`, fingerprint `e177ae92…cbc6b`. The
  owner's note, verbatim:

  > I think I like A the best but it does lose quite a bit of code context compared with the others, maybe we could had a disclosure expand or lightbox or some way to get that deeper context back when we want it while keeping it short and sweet by default.

This is an owner decision about how RC61-05 is presented, not an amendment
of the criterion. The next round builds
[direction A](../../design/BPR-1161-share-round-A-sheet-v1.html) in Qt in
place of the rejected page, keeps the tested share and delivery core and
[[BPR-1169]]'s route semantics, adds a short-by-default way to see each
note's code with a deeper view on request, and asks RC61-05 again as
`BPR-1161-compose-share-v002` on a new frozen build. The session was handed
back at 869. Both answered items were released by candidate name after this
record.

## Direction A built: Share notes, a sheet over the code (macOS, 2026-10-02)

At `a426c66a0` and `324c17826`. The rejected Compose feedback page is gone.
The review bar's pencil is now a labelled **Share notes** button that carries
the number of open notes. It opens a window-modal sheet. On macOS AppKit
attaches the sheet to the window; on other platforms it hangs from the review
bar. The file the reviewer was reading stays behind it.

The sheet has these parts, in order:

- **Share round N**, then one sentence that says what a round is and that
  nothing has left the window yet.
- **Ask them to** (Discuss, Plan, Implement), with a line saying what the
  chosen one asks for, and an optional note for the whole round.
- The notes, two lines each, in the order they will be read. Each has a check
  box and a grip.
- One default button, **Copy round N**, with **Save as File…** beside it. Each
  has a line saying when to use it.
- In the foot, Read the Exact Text, Save as Bundle… and any route this
  installation reaches.

After a share the title says what happened ("Round 1 is on your clipboard").
The text under it says what comes next. Shared so far lists every
observation in the words the owner accepted, under the same caption, and
Done becomes the default. [[BPR-1169]]'s routes are unchanged: Use in
External Agent appears only when the host reaches a local interface, and it
grants the round and shows the invocation.

### Code context: short by default, more on request

The owner's note asked for "a disclosure expand or lightbox or some way to
get that deeper context back when we want it while keeping it short and sweet
by default". The `frontend-design` pass chose two steps. Both are native
controls and both inherit the palette.

1. **A disclosure per note.** Click the arrow, or press Right on it, to open
   the note's code in place. Left closes it. The code shows the lines the
   note is about and each suggested change in place (`-` and `+` on the
   review panes' row fills), with two unchanged lines either side, numbered,
   in the code font, at most 14 rows. Under it is one line of facts
   ("src/parser.cpp at R1, lines 12–19 of 31."). **Show All Code** opens or
   closes every note at once, which the owner asked for on BPR-1213. A note
   stays open across refreshes.
2. **A larger view, the lightbox.** **Show the Whole File…** opens a
   window-modal panel over the sheet, nine tenths of the window's size. It
   shows the note's whole file at the revision the note names, with each
   change in place, scrolled to the note, and has Previous Note and Next
   Note. Escape or Done closes only this view, and focus returns to the
   button that opened it.

Why this and not something else:

- An always-visible excerpt per note is direction C. It would lengthen the
  sheet that A was chosen to keep short.
- A hover tooltip neither the keyboard nor a screen reader can reach.
- "Show in code" closes the sheet. That is the cost direction A itself
  listed, and the owner's note asks to avoid it.

The disclosure answers "what is this note about" without leaving the list.
The lightbox answers "what is around it" without leaving the sheet.

The layering: `core::excerpt_note` cuts every excerpt. The host reads each
listed note's file at the revision its target names, only content that
revision pinned, one note at a time, so a share never waits behind them
(`application::read_note_context`). The whole file is read only on request
(`review_cycle_workbench::show_note_code`) and let go when the view closes.
Qt presents both. A change whose bytes the file no longer holds at its
place is counted and not shown.

### Keys

These follow BPR-1157's conventions:

- Return presses the default button. In the round note, Return saves a
  changed note and shares nothing.
- Shift+Return does nothing on the sheet.
- Escape is Cancel. On macOS Control-C is Cancel too, as in the code.
- Option+Up and Option+Down (Alt elsewhere), or dragging a grip, reorder
  the notes.
- Every Tab stop shows the focus ring.

Two fixes came out of the round:

- Unsaved round-note text is no longer replaced by a refresh while the sheet
  is not the active window. Before, the BPR-1168 handoff course froze over
  the typed text.
- A focus ring left on a control whose window was hidden no longer reappears
  when the window opens again (`keyboard_focus_ring`).

### Owned paths

- `src/core/include/bpr/core/review_cycle_share.hpp` and `.cpp`:
  `excerpt_note`.
- `src/application/include/bpr/application/review_cycle_projection.hpp`:
  `review_cycle_note_context`, `Contexts` and `Larger`.
- `src/application/include/bpr/application/review_cycle_share.hpp` and `.cpp`:
  `read_note_context`.
- `src/application/include/bpr/application/review_cycle_runtime.hpp` and
  `.cpp`: `note_context_selected`.
- `src/application/include/bpr/application/review_cycle_workbench.hpp` and
  `.cpp`: the note-code cache, `show_note_code` and `close_note_code`.
- `src/adapters/qt/include/bpr/qt/review_cycle_share_sheet.hpp` and `.cpp`,
  which replace `review_cycle_share_page`.
- `review_cycle_view.hpp` and `.cpp`, and `keyboard_focus_ring.cpp`.
- `src/app/src/review_cycle_desktop.cpp`: the dispatch, and the save dialog
  opened over the sheet.
- The tests named below, and `docs/architecture/domain-and-state-model.md`.

No new `src/app` file, so the round-trip inventory needs no entry.

### Verification (macOS arm64, dev Debug, offscreen)

- `core_review_cycle_share`: excerpts, CRLF and lone CR, insertions,
  deletions, stale and overlapping changes, the cut, and the whole file.
- `review_cycle_workbench`: the host reads each listed note's code at its
  revision, not the folder as edited since, and reads the whole file on
  request.
- `qt_review_cycle_view_1_0` and `_2_0`: real window-system key and mouse
  events, and rendered pixels. The sheet is window-modal, and the window
  behind it takes no click. Copy is the one default and Return copies.
  Removed and added rows are on the panes' fills (flat-fill rule). The
  gutter's ink is the placeholder colour over the fill and never the text
  colour, while the code's ink is the text colour (the local role/text
  rule). Escape closes the larger view only. The sheet fits a 900×600
  window with every route.
- `application_review_cycle_desktop_public_path`: through the real
  composition, the code is read, the whole file opens, Copy and Save work,
  and Escape closes the sheet.
- `application_profile_cycle_handoff_public_path` (`build/dev-tour-cli`).

The 52-row review-cycle selection passes, including `architecture_boundaries`
and `test_network_isolation_manifest`, as do all 117 `qt_` rows, the three
tour-composition rows and core-only's 24 `core_review_cycle` rows.
clang-format and clang-tidy are clean.

Eight negative controls each fail their row:

- the noted line off by one;
- Save made the default;
- Return in the round note left to the default button;
- the disclosure showing nothing;
- the unfocused round note overwritten;
- the stale focus ring;
- Escape in the larger view also closing the sheet;
- the host reading no note code.

A native cocoa probe opened the sheet as a real sheet, expanded a note, and
opened and closed the larger view. The other native courses stop at window
activation, so attachment under the window's own title area is what the owner
sees in the attended check.

`application_review_round_trip_composition_contract` was red during the run.
The cause was another writer's then-uncommitted `github_application.cpp`
(BPR-1250), not this card, and the contract passes at `324c17826`.

## Pending owner judgment: compose-share v002

`BPR-1161-compose-share-v002` (RC61-05), one Accept/Reject question, in
session `cbd48b91000b00f1e72e1ad62c7c384c`. It was added at sequence 885 and
was ready at 887, with fingerprint `a7516bac…8f622d`. The question: does
Share notes let you share one complete round in one gesture, with Copy and
Save first-class, short by default and with each note's code there when you
want it?

Candidate: `/Users/jaeger/bpr-cycles-sheet-review.yiVapS/BetterPullReview-cycles-sheet.app`,
executable SHA-256 `94724b6c64bba9aadb6eba7734163e0cfebeb9c0dfc356be256bbf43ac8a28a1`,
built at `324c17826`. Launch it with
`python3 /Users/jaeger/bpr-cycles-sheet-review.yiVapS/launch.py`. It has its
own profile in `home/`, with `HOME` and `CFFIXED_USER_HOME` set. The sample
project is `sample-project/`, and saves may go to `shared/`. RC61-05 is the
only open criterion, so the card stays Parked on human-review.

`a426c66a0` and `324c17826` landed as `d46d37cf7` and `00f7165cd` after a
rebase over `cfa40e2ec`, which touched one BPR-1250 card and no source. The
frozen candidate's source is therefore identical to `00f7165cd`, and the
acceptance records name that commit.

## Owed platform runs

Not gating. Linux station (`build/dev`):
`cmake --build --preset dev && ctest --preset dev -j4 --output-on-failure -R "^core_review_cycle_share$|^core_review_cycle_policy$|^sqlite_review_cycle_storage$|^review_cycle_(workbench|round|export|bundle|verification)$|^qt_review_cycle_view_|^application_review_cycle_desktop_public_path$|^architecture_boundaries$"`.
Windows station: the same selector with `--preset windows-dev` plus
`|^test_network_isolation_manifest$`.

### Linux at `dbe6966ea`: share page and delivery records (bpr-main-linux, 2026-10-02)

Includes `8cbdc1477`. WSL2 (i9-10900KF, 20 logical processors), GCC 15.2,
Qt 6.11.2, `build/dev` Debug, offscreen. `cmake --build --preset dev` exits 0
with 0 `warning:` lines. The requested `ctest --preset dev -j4` selection
passes **12 of 12** in 32.96 s:

- `core_review_cycle_share` and `core_review_cycle_policy`
- `sqlite_review_cycle_storage`
- `review_cycle_{round,export,bundle,verification}`
- `qt_review_cycle_view_1_0` and `_2_0`
- `application_review_cycle_desktop_public_path`, 2.82 s. The real clipboard
  works offscreen on Linux; there was no clipboard-owner failure.
- `architecture_boundaries`

**`review_cycle_workbench`, which renders the 16 MiB round, takes 32.94 s at
-j4, and 30.94 s and 31.00 s alone.** The row sets no `TIMEOUT` property, so
it runs under CTest's default. No remote mutation.

## Windows (bpr-main-win, 2026-10-02)

At `dbe6966ea`, which contains `8cbdc1477`. Windows 11 Home 10.0.26200, MSVC
14.51, Qt 6.11.2, `build/windows-dev` Debug. The 10 executables these rows
run were deleted first, and the build rebuilt every one. It printed no
`warning C` or `warning LNK` line. The sources changed since `ce2b02c0b`
declare no `near`, `far` or `small` identifier.

```powershell
ctest --preset windows-dev -j4 --output-on-failure -R "^core_review_cycle_share$|^core_review_cycle_policy$|^sqlite_review_cycle_storage$|^review_cycle_(workbench|round|export|bundle|verification)$|^qt_review_cycle_view_|^application_review_cycle_desktop_public_path$|^architecture_boundaries$|^test_network_isolation_manifest$"
```

12 of 13 pass, in 53.87 s:

- `review_cycle_workbench`, which renders the 16 MiB round, passes in
  38.13 s. It has no `TIMEOUT` of its own, so CTest's 1,500 s default bounds
  it.
- `application_review_cycle_desktop_public_path` 15.67 s.
- `qt_review_cycle_view_1_0` 8.82 s and `_2_0` 17.60 s, against 60 s.
- `review_cycle_round` 1.62 s, `_verification` 2.56 s, `_bundle` 1.37 s and
  `_export` 0.54 s.
- `sqlite_review_cycle_storage` 3.39 s.
- `core_review_cycle_share` and `core_review_cycle_policy`.
- `architecture_boundaries` 32.92 s.

`test_network_isolation_manifest` fails, and not on this card's rows. All 58
of its lines are "uses `bpr_windows_uncontained` but is not in the pinned
Windows set". No `review_cycle_*` row or other row of this card is among
them.

- Most of them are [[BPR-1237]]'s unmeasured pins: the private-review,
  tour-publication, logging, scale and other rows that card lists.
- The 11 `application_large_review_*` rows are not on [[BPR-1237]]: active
  work, cache bytes and entries, history and its session race, launch, loaded
  close, pathological, provider cancel, resources and responsiveness.
  `cef11b982` (BPR-0967) registered them `WINDOWS_UNCONTAINED` without adding
  them to `scripts/verify_test_network_isolation.py`'s Windows set. They are
  BPR-0967's to pin.

## `review_cycle_workbench` bound, and the manifest (bpr-main-win, 2026-10-02)

`review_cycle_workbench` now sets `TIMEOUT 120`. That is three times the
slowest isolated run, Windows Debug's 38.13 s, rounded up
(`docs/development/build-and-test.md`). Linux, about 31 s, and macOS, about
18 s, sit well inside it. On Windows it passes in 34.91 s under the new
bound. `test_network_isolation_manifest`, red above on other cards' rows, now
passes on Windows ([[BPR-1237]]).

## Owed platform runs at `324c17826` (the share sheet)

Not gating. Linux station (`build/dev`):

```sh
cmake --build --preset dev && ctest --preset dev -j4 --output-on-failure -R "^core_review_cycle_share$|^core_review_cycle_policy$|^sqlite_review_cycle_storage$|^review_cycle_(workbench|round|export|bundle|verification)$|^qt_review_cycle_view_|^qt_suggestion_editor_|^application_review_cycle_desktop_public_path$|^application_review_tour_composition_|^architecture_boundaries$"
```

Windows station (`build/windows-dev`):

```powershell
cmake --build --preset windows-dev; ctest --preset windows-dev -j4 --output-on-failure -R "^core_review_cycle_share$|^core_review_cycle_policy$|^sqlite_review_cycle_storage$|^review_cycle_(workbench|round|export|bundle|verification)$|^qt_review_cycle_view_|^qt_suggestion_editor_|^application_review_cycle_desktop_public_path$|^application_review_tour_composition_|^architecture_boundaries$|^test_network_isolation_manifest$"
```

Where a build has the tour CLI with Qt and tests, add
`^application_profile_cycle_handoff_public_path$`. The sheet is a top-level
window-modal dialog. On Linux it is placed under the review bar by
`review_cycle_share_sheet::place`; on Windows the same, inside the window's
own frame.

### Linux at `ef058e602`: share sheet, direction A (bpr-main-linux, 2026-10-02)

WSL2, GCC 15.2, Qt 6.11.2, `build/dev` Debug, offscreen with the Windows style.
`cmake --build --preset dev` exits 0 with 0 `warning:` lines. The requested
`ctest -j4` selection passes **15 of 17** in 34.49 s.

**`qt_review_cycle_view_1_0` and `_2_0` fail, deterministically (4 of 4 at
1×), at `one focus ring, on Copy: none left from before the sheet closed`**,
in the compact-window step after the sheet reopens. A probe, not committed,
listed every `cycleKeyboardFocus` ring under the sheet at that check:

- The focus widget is `QPushButton cycleShare_copy_document` ("Copy round 1"),
  and its ring is visible, as expected.
- **The ring of `QLineEdit cycleShareGoal` (the goal field, 720×20) is also
  visible**, with `isHidden() == false`, although the field does not have
  focus.
- All 24 other rings are hidden.

So the goal field's ring, left over from before the sheet closed, is not
cleared when the sheet reopens with focus on Copy. This is a real focus-ring
defect on Linux, not a pixel threshold. Nothing was changed or loosened.
macOS passes the same row, so the cause may depend on how the line edit's
focus-out is delivered under this platform and style; that was not traced
further here.

Passing rows: `core_review_cycle_{share,policy}`, `sqlite_review_cycle_storage`,
`review_cycle_{round,export,bundle,verification}`, `review_cycle_workbench`
(34.47 s), `qt_suggestion_editor_1_0` and `_2_0`,
`application_review_cycle_desktop_public_path`, the three
`application_review_tour_composition_*` rows and `architecture_boundaries`.
No remote mutation.

### The stale goal-field ring, fixed (bpr-main-linux, 2026-10-02, `0a1053c11`)

**Cause.** A temporary log of the goal field's ring (`QLineEdit
cycleShareGoal`, not committed) showed the last reopening of the sheet:
`Show` (ring hidden, correct), then a **`FocusIn` (Tab reason) delivered
while the field's `hasFocus()` was already false**. Focus had moved on to
"Copy round 1" by then, and no `FocusOut` followed. `keyboard_focus_ring`'s
`FocusIn` branch called `show()` unconditionally, so the stale event left
the ring lit. The earlier fix covered the `Show` path (a window hidden with
focus inside), not a late focus-in.

**Fix (`src/adapters/qt/src/keyboard_focus_ring.cpp`).** On `FocusIn` the
ring calls `setVisible(parentWidget()->hasFocus())`, so the control's current
focus decides, not the event. Only that branch changed. The `FocusOut`
(popup-reason exception), `Resize` and `Show` paths, and the share sheet,
are unchanged.

| Check (Linux, GCC 15.2, `build/dev`, offscreen) | Result |
|---|---|
| `qt_review_cycle_view_1_0` and `_2_0`, 3 runs each | 6 of 6 pass |
| Negative control: the previous `keyboard_focus_ring.cpp`, rebuilt (not committed) | Both fail at `one focus ring, on Copy: none left from before the sheet closed` |
| The requested selection plus every `qt_` row at -j6 | 122 of 128 pass. The 6 failures are [[BPR-1255]]'s title-row and window rows at their overflow check. |
| `cmake --build --preset dev`; clang-format 22; `git diff --check`; `tidy.py --paths` on the file | 0 warnings; clean |

Not run on macOS.

## Windows: the share sheet (bpr-main-win, 2026-10-03)

At `19829befa`, which contains `ef058e602` and the sheet (`d46d37cf7`).
Windows 11 Home 10.0.26200, MSVC 14.51, Qt 6.11.2, `build/windows-dev` Debug.
The selection's executables were deleted and rebuilt, and the build printed no
`warning C` or `warning LNK` line. No SDK-macro identifier is in the changed
sources.

The first run had 16 of 18 passing. `qt_review_cycle_view_1_0` and `_2_0`
failed at "every row in view, no scrolling", after the sheet's own checks (it
hangs under the review bar here) had passed. A probe, not committed, of the
opened note's `note_code_view` printed:

```
height=142 viewport=140 spacing=13 fmHeight=14 leading=-1 line0=14 blockSum=144 max=1
```

The face has negative leading. `QFontMetricsF::lineSpacing()` is 13, but the
text layout gives each line its height, 14. `fitted_height()` sized ten rows
at 13, so the tenth sat one line out of view. The fit now takes
`max(lineSpacing(), height())` per row
(`src/adapters/qt/src/review_cycle_share_sheet.cpp`), and macOS and Linux,
whose leading is not negative, are unchanged.

A separate observation, not changed: the excerpt's font family is
`monospace`, which resolves to Arial on Windows (`QFontInfo`), so the note's
code is not fixed-pitch there.

After the fix, the selection passes 18/18, in 70.97 s:

- `qt_review_cycle_view_1_0` 9.67 s and `_2_0` 17.40 s;
- `qt_suggestion_editor_1_0` and `_2_0`;
- `application_review_cycle_desktop_public_path` 16.66 s;
- `application_review_tour_composition_public_path` and `_scale_125` and
  `_200`;
- `review_cycle_workbench` 44.98 s, against its 120 s bound;
- `review_cycle_round`, `_export`, `_bundle` and `_verification`;
- `sqlite_review_cycle_storage`;
- `core_review_cycle_share` and `core_review_cycle_policy`;
- `architecture_boundaries`;
- `test_network_isolation_manifest`.

### The sheet's code on the panes' font (bpr-main-win, 2026-10-03)

The note excerpts and the whole-file panel took the appearance's code font
raw. On Windows its generic `monospace` family resolved to Arial, so the code
was not fixed-pitch. Now:

- `note_code_view::set_code_font(font, tab width)` puts the font through
  `fixed_pitch_code_font()`, the shared code view's helper, with no new
  lookup. It sets the panes' tab width as the tab stop, and expands the
  excerpt's tabs at that width.
- The share sheet and its viewer pass both on.
- `review_cycle_view` gives the sheet its `TabWidth` with the font.
- The row-height fix above stays.

Measured on Windows at `2cef0715d` plus this change (MSVC 14.51, Qt 6.11.2,
`build/windows-dev` Debug). A probe of the opened excerpt, not committed,
printed `resolved=Consolas fixed=1`, line spacing 14, leading 0, height 152,
and scroll maximum 0. The selection's executables were deleted and rebuilt
with no `warning C` or `warning LNK` line, and `clang-format` is clean. The
BPR-1161 selection passes 18/18, in 69.41 s: `qt_review_cycle_view_1_0`
9.10 s, `_2_0` 16.28 s, and `application_review_cycle_desktop_public_path`
16.97 s. The macOS and Linux reruns are the coordinator's.

### Linux at `07f853492`: the excerpt on the fixed-pitch code font (bpr-main-linux, 2026-10-03)

Includes `d922bbee8` and `07f853492`. WSL2, GCC 15.2, `build/dev` Debug,
offscreen. `cmake --build --preset dev` exits 0 with 0 `warning:` lines. The
requested `ctest -j4` selection passes **12 of 12** in 34.01 s:

- `core_review_cycle_share`
- `review_cycle_{round,export,bundle,verification}`, and `review_cycle_workbench`
  (34.00 s)
- `qt_review_cycle_view_1_0` (1.44 s) and `_2_0` (2.99 s), with
  `0a1053c11`'s focus-ring fix in place
- `qt_suggestion_editor_1_0` and `_2_0`
- `application_review_cycle_desktop_public_path` (4.04 s)
- `architecture_boundaries`

No remote mutation.

## Owner verdict 895: compose-share v002 rejected; the code aligned and coloured (macOS, 2026-10-03)

`BPR-1161-compose-share-v002` (RC61-05) was **rejected** at event 895,
decision `ab7105e5-2bd6-437e-870d-6f6ad54b9b1f`, on fingerprint
`a7516bac…8f622d`. The owner's note, verbatim:

> Something weird here these lines arent aligned, or syntax highlighted, is it usiing the proper monospace font?

The screenshot attached to the decision is the v002 media render of the
expanded excerpt. The frozen root's `home/` is empty and it has no
`owner-app.log`, so the owner judged the render. That render came from the
offscreen course at `324c17826`.

**Root cause.** Measured on that render, the first code ink sat at 405 pixels
on context rows, 408 on `-` rows and 410 on `+` rows. The tab-indented rows
sat at 423 and 425 pixels. There were two causes:

- **A proportional face.** The sheet took the appearance's code font raw, and
  its generic `Monospace` family resolved to a proportional face. The marks
  (`-`, `+` and the bullet) have different widths, so they pushed the code
  sideways by 3 to 5 pixels.
- **Tab stops counted from the row's first column.** The stops were counted
  from the line number, so one tab indented 3 cells, not 4.

Both were already fixed on main before this round. `07f853492` passes the font
through `fixed_pitch_code_font` and expands tabs from the code's first column.
BPR-1270 makes JetBrains Mono NL the bundled default. The same course at
`4c8bcda0b` puts every row at 418 and both tab rows at 447. What was still
missing was syntax colour.

**Change (`3b5f405be`).**

- The excerpt and the larger view are coloured as the cycle's file pane is,
  through `code_syntax_format_ranges_at` and the panes' role colours. Qt still
  runs no lexer.
- A shown note's code asks for two documents to be classified: its rows before
  the suggested changes and after them, each as shown.
- `review_cycle_desktop` classifies them one at a time on a second syntax host,
  over the panes' packaged definitions and lane. A view adopts only spans of
  the exact text it still shows. The gutter stays in the placeholder colour.

Details, checks and negative controls are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md) under "BPR-1161: a
note's code aligned, in the code font, and coloured". In short:

- `qt_review_cycle_view_1_0` and `_2_0` check rendered columns: an `X` at
  columns 0, 4 and 8 lands on the same pixel on every row kind, whether tabs,
  spaces or both reach it. A tab equals `code_tab_stop_distance()`. They also
  check the profile's font (JetBrains Mono NL) and the role/text rule.
- `application_review_cycle_desktop_public_path` checks the real classifier.
- Four negative controls fail as they should. The 18-row selection and all
  118 `qt_` rows pass on macOS.

## Pending owner judgment: compose-share v003

`BPR-1161-compose-share-v003` (RC61-05) is one Accept/Reject question in
session `cbd48b91000b00f1e72e1ad62c7c384c`. It was added at sequence 907,
ready at 915, with fingerprint `f7148a03…2e2837`. The content says what the
owner should now see:

- a note's code in the profile's code font, JetBrains Mono NL;
- every column aligned whatever the row's mark, line number or indentation;
- the code coloured as the code panes colour it, with the gutter quiet;
- the same in Show the Whole File.

It then asks the v002 question again. v002 was released by name at 906, and
three superseded BPR-1156 questions that an unscoped `prepare` re-readied were
returned to waiting (918–920). The consumed cursor is **920**.

The candidate is self-contained in `/Users/jaeger/bpr1161-share-review-v003.WQEATE`:

- `source/` is `git archive` of `3b5f405be`, built in `source/build/cand`
  (`cmake --preset dev`, sharing only the dependency cache).
- The executable's SHA-256 is
  `af8f9975cb9a02586402695c65dfb08486194d330684be836377747a6bce0c4c`.
- `--code-font-probe` prints `registered=yes`, `resolved="JetBrains Mono NL"`,
  `fixed_pitch=yes` and `CODE_FONT_OK`.

Launch it with:

```
python3 /Users/jaeger/bpr1161-share-review-v003.WQEATE/launch.py
```

The launcher sets `HOME` and `CFFIXED_USER_HOME` to the root's `home/`. It
checks the build and the sample project before starting, and it reports an
exit within twenty seconds as a launch failure. A launch on a separate
scratch profile opened "BetterPullReview — Default profile" and was closed.
The sample project is `sample-project/` (tab-indented `src/parser.cpp`), and
saves may go to `shared/`. The media are native cocoa renders of this build's
excerpt and whole-file view, and the offscreen alignment render. RC61-05 is
the only open criterion, so the card stays Parked on human-review.

### Owed platform runs at `3b5f405be`

Not gating. Linux station (`build/dev`):

```sh
cmake --build --preset dev && ctest --preset dev -j4 --output-on-failure -R "^core_review_cycle_share$|^core_review_cycle_policy$|^sqlite_review_cycle_storage$|^review_cycle_(workbench|round|export|bundle|verification)$|^qt_review_cycle_view_|^qt_suggestion_editor_|^application_review_cycle_desktop_public_path$|^application_review_tour_composition_|^architecture_boundaries$"
```

Windows station (`build/windows-dev`):

```powershell
cmake --build --preset windows-dev; ctest --preset windows-dev -j4 --output-on-failure -R "^core_review_cycle_share$|^core_review_cycle_policy$|^sqlite_review_cycle_storage$|^review_cycle_(workbench|round|export|bundle|verification)$|^qt_review_cycle_view_|^qt_suggestion_editor_|^application_review_cycle_desktop_public_path$|^application_review_tour_composition_|^architecture_boundaries$|^test_network_isolation_manifest$"
```

On both, `qt_review_cycle_view_*` prints `note code X inks: …` with the
measured columns. The rows at column 4 (and those at column 8) must be equal.

### Linux at `17691bc63`: coloured, aligned excerpt and whole-file view (bpr-main-linux, 2026-10-03)

Includes `3b5f405be`. WSL2, GCC 15.2, `build/dev` Debug, offscreen.
`cmake --build --preset dev` exits 0 with 0 `warning:` lines. The requested
`ctest -j4` selection passes **17 of 17** in 34.35 s, with
`qt_review_cycle_view_1_0` (1.73 s) and `_2_0` (3.55 s), both
`qt_suggestion_editor` scales, the desktop path, the three tour-composition
rows and `review_cycle_workbench` (34.33 s) among them.

`note code X inks:` lines, the X ink column of rows 0 to 7:

| Scale | Inks | Column 4 (rows 1, 2, 3, 7) | Column 8 (rows 5, 6) | Stop | Tab from column 0 |
|---|---|---|---|---|---|
| 1.0 (`_1_0`) | `47 76 76 76 -1 105 105 76` | **76, 76, 76, 76** | **105, 105** | 28.75 | 76−47 = 29, and 105−47 = 58 (2 × 28.75 = 57.5) |
| 2.0 (`_2_0`) | `95 153 153 153 -1 210 210 153` | **153, 153, 153, 153** | **210, 210** | 57.5 | 153−95 = 58, and 210−95 = 115 (= 2 × 57.5) |

Each group is equal, and each tab is within 1 pixel of the stop. Row 4 reports
`-1`, an X ink not found on that row, and no expectation covers it. The
courses resolve the bundled fixed-pitch family. No pixel check failed. No
remote mutation.

### Windows: the excerpt in syntax colours, aligned (bpr-main-win, 2026-10-03)

At `17691bc63`, which contains `3b5f405be`. Windows 11 Home 10.0.26200, MSVC
14.51, Qt 6.11.2, `build/windows-dev` Debug. The 12 executables the selection
runs were deleted first and rebuilt, and the build printed no `warning C` or
`warning LNK` line. The changed sources declare no SDK-macro identifier.

The selection passes 18/18, in 68.50 s. The view rows, run again with `-V`,
print:

```
qt_review_cycle_view_1_0: note code X inks: 46 74 74 74 -1 102 102 74 stop 28
qt_review_cycle_view_2_0: note code X inks: 93 149 149 149 -1 205 205 149 stop 56
```

- **Column 4** (inks 1, 2, 3 and 7: the noted, removed and added rows, by a
  tab, by spaces and by both) is one pixel at each scale: 74 at 1.0, 149 at
  2.0.
- **Column 8** (inks 5 and 6: two tabs and eight spaces) is one pixel: 102 at
  1.0, 205 at 2.0.
- A tab is the panes' stop from the first column. At 1.0, 74 − 46 = 28 and
  102 − 46 = 56 against a stop of 28. At 2.0, 149 − 93 = 56 and
  205 − 93 = 112 against a stop of 56.

The course's own check that the shown code is the profile's font, resolving to
the bundled fixed-pitch "JetBrains Mono NL", passes. So does
`fit_rows_at_negative_leading()`: it ran on Arial, since it prints only when
no face with negative leading exists.
