---
schema: "implement-rfc.card.v1"
id: "BPR-1334"
title: "Line details: the current line, base over head, full width under the comparison"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner chat request, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1334--BPR-1334--7d299944dc95.json","related_cards":["BPR-0646","BPR-1264","BPR-1212","BPR-1330","BPR-1313"],"requested_skill":"frontend-design","acceptance":["BPR-1334:C1","BPR-1334:C2","BPR-1334:C3","BPR-1334:C4","BPR-1334:C5"]}
parked_on: "human-review"
---

# BPR-1334: Line details under the comparison

## Owner request

The owner, in chat on 2026-10-08, verbatim:

> Can we add something akin to the beyond compare text details view as an
> optional on by default view page below the side by side which shows just
> the current cursor's line as a full width single pane diff

and, later the same day:

> The detail pane might be it, I thought we had already built it but I don't
> see it in the app or view menu

Beyond Compare's Text Details pane sits under the side-by-side view. It shows
the current line's left version above its right version, full width, with the
character differences highlighted, so a long line can be read without
scrolling sideways.

## What exists today (inventory, `origin/main` 03ae5aa31)

- **The Detail pane exists and is composed, but hidden.** `diff_workbench`
  has built a `QPlainTextEdit` (`diffSelectedChangeDetail`, "Selected change
  detail") under the panes in its vertical splitter since RFC-BPR-0004.
  [[BPR-0646]] made it a *review detail*. It is off by default and shown only
  by **Review details** (`Alt+Shift+3`), together with the exact epoch
  anchors. That action is in the review bar's **Comparing …** button menu,
  not in the View menu. That is why the owner could not find it.
- **It was specified as this feature.** RFC-BPR-0005 asks for "a full-width
  Beyond Compare-inspired line-over-line lower detail pane", and RFC-BPR-0004
  feeds it from the same line mapping. The owner remembered it correctly.
- **When shown, it is not this feature.** It is at most 96 px tall and does
  not wrap. It holds plain text: a heading ("Change 3 of 11" or "Selected
  line 12"), then "Earlier" with the left text and "Later" with the right
  text. A side with no counterpart shows its whole opposing span, capped at
  64 lines by [[BPR-1264]]. It has no line numbers and no character
  highlights.
- **Core already computes intraline spans.** `diff_compute` runs a bounded
  character LCS over every *similar* line pair. The result is
  `computed_file_diff::Intraline`, given as byte spans relative to each line's
  content. `detail_for_line` and `detail_for_selection` return them in
  `line_detail_input::Intraline`. A pair whose LCS ran out of matrix budget
  stays similar but has no spans ([[BPR-0719]]). The panes already paint these
  spans (`append_intraline_emphasis`).
- **The line cursor.** [[BPR-1212]] slice 1 (a keyboard line cursor) is not on
  `origin/main`. The current line is the semantic selection's
  `PreferredLine`. A click, a change step, a conversation anchor or a restored
  reading position sets it, and `diff_workbench::current_line` reports it. The
  [[BPR-0986]] current-line band marks it in the panes.

## Change

The existing Detail pane becomes **Line details**. No second pane is added.

- It shows the current line twice, full width: the base version above the
  head version. Each row has its line number and a −/+ mark in a gutter, and
  the changed characters are highlighted. Rows do not wrap (v002, after the
  owner rejected v001's wrapping): a long row scrolls sideways under the
  strip's one horizontal bar, both rows together, and the gutter stays put.
  The bar covers neither a row nor the gutter, legacy or overlay (v003).
  A new line opens scrolled to its first changed character.
- Each row is coloured as its side's pane colours that line (v004, after
  the owner rejected v003: "there's no syntax highlighting"). The base row
  takes the base blob's classification and the head row the head blob's,
  the spans the panes already hold, through the same format ranges. The
  change highlight is a background under them, so a changed token keeps its
  colour, moved only as far as it must to stay readable on the fill, as in
  the panes. A cut row is coloured only as far as it is shown.
- A mouse selection that spans lines shows each selected line's pair, capped
  at 16 lines, followed by a line that says how many more were selected.
- Unchanged lines show both versions without highlights. A removed line shows
  its base row; the head row says the line is not in head. An added line is
  the mirror image. A changed line with no paired counterpart that replaces
  one line shows that line as its other row, without emphasis; one that
  replaces several names them.
- A line longer than 4,096 bytes is cut at a character boundary, followed by
  a marker that gives how many more bytes there are. Core decides the cut
  and clips the highlight spans to it. Qt draws what core gives it.
- It is on by default. **View > Line Details** turns it off or on, and the
  choice is a per-profile preference, also on Settings > Reviews and queue
  (a Comparison row).
  Review details no longer shows or hides it.
- Its top edge is the shared resize seam with `hold_resize_cursor`. The height
  the reader drags to is kept per window.

## Acceptance

- **C1 (direct).** In the production workbench, the current line's base and
  head rows appear under the comparison, each one unwrapped line under one
  shared horizontal bar with a fixed gutter, opening on the first changed
  character, with line numbers, the panes' syntax colours for the same
  line, and intraline highlights from core. They follow a click, a change step and a
  mouse selection across lines. Added, removed and unchanged lines each read
  sensibly.
- **C2 (direct).** The strip is shown in a fresh profile. View > Line Details
  hides it and shows it again, Settings shows the same state, and the choice
  survives a restart of the profile.
- **C3 (direct).** The seam resizes the strip, holds the resize cursor, and the
  dragged height survives a restart for that window.
- **C4 (direct).** A 100,000-byte line is truncated with its marker, a range is
  capped, and the optimized large-review rows keep every gated phase under
  100 ms.
- **C5 (owner).** The owner looks at a frozen build with long changed lines
  and answers Accept or Reject for "line details strip" (v001, rejected) and
  then "line details v002" (rejected: the frozen build did not start
  natively) and then "line details v003" (rejected: no syntax highlighting)
  and then "line details v004".

## Verification (2026-10-08, bpr-main-mac implementer)

Design: `frontend-design:frontend-design` was loaded before any UI work. The
strip inherits the panes' code font, gutter roles and diff colours and adds
nothing of its own. The gutter shows only the line number and −/+, every wrapped
line of a row wears the row's band, and the character emphasis is the one
strong mark. Offscreen renders were used for self-critique only. They found the
band covering only the first wrapped line and a one-line unpaired replacement
reading "No matching line"; both are fixed.

**Candidate.** `f6f247e8b` on `origin/main` `495f8752a`, which carries
BPR-1212 slice 1's keyboard line cursor and BPR-1336's zoom. The cursor moves
the selection through `select_line` and extends it through the pane's text
selection, so Line details follows both. Zoom gives the workbench its font, so
the strip scales with it.

**Frozen root.** `artifacts/bpr-1334/line-details-frozen-v001/` holds
`source/`, which is `git archive f6f247e8b` (sha256 `4e961eaa…`, `diff -rq`
clean against HEAD). Beside it is `build/`, a Release build with tests on
(Apple clang 21 `-O3`, Qt 6.11.2, macOS 26.6.2, M4 Pro), and `launch.py`
(sha256 `6cb2a7c4…`), which checks the fixture tree and the executable and
reports an exit within 30 s. The attended executable
`build/tests/end_to_end/line_details_public_path` (sha256 `dc5e412e…`)
reaches `LINE_DETAILS_ATTENDED_READY` offscreen in 1.4 s.

**Focused rows at the candidate (frozen build, offscreen).** 16 of 17 pass:
`core_diff_semantics`, `qt_diff_workbench_scale_{100,125,200}`,
`qt_workbench_window_scale_100`, `qt_workbench_focus_order_scale_100`,
`qt_settings_window`, `sqlite_workbench_settings_storage`,
`application_workbench_host`, `application_diff_workbench_public_path`,
`application_profile_window_layout_public_path`,
`application_line_details_public_path`, `github_recorded_fixture_contract`,
`github_contract_fixture_security`, `architecture_boundaries` and
`application_review_round_trip_composition_contract`.
`application_uiforetw_selected_diff_contract` fails only
`title_row_exact_order`. BPR-1338 (`e8e1567c4`) added
`workbenchOpenOnGithubAction` to the title row, and the probe pins the row at
17 actions; neither was updated. It passed at this card's pre-rebase
candidate (`bb3308e81`), whose Line details change put the strip's floors
behind View > Line Details (below). Documentation: `DOCUMENTATION_OK`;
`verify_documentation_tests` 67/67. Format check passes. clang-tidy on the
changed translation units reports only findings that predate this card, once
the cloned-branch finding in `line_details` was fixed.

**Failing-before controls** (each mutation reverted after it was run): range
ignored; cursor hold removed; emphasis dropped; strip tied to Review details;
profile column not written; held height dropped; UTF-8 back-off removed;
emphasis unclipped; similar pairs read as unchanged; one-line replacement rule
off. Each fails its named check.

**UIforETW height floors.** At `ed2ab5f7d` the capture failed
`UIFORETW_CAPTURE_LAYOUT_FAILED:985:728:open=694`. Line details, on by
default, took about eight text lines from the panes. The capture's focused
review now hides it through View > Line Details, beside the drawers and
Review details. The floors are unchanged, so a new chrome row still fails
them. At the default layout the panes are shorter by the strip's height. That
is the trade the owner's "on by default" asks for, and the owner look covers
it.

**Large-review rows (the 18).** At the final binaries the run passed 18/18 at
load average 6–10: `large_file_selection` 23.3 ms,
`large_source_revisit_render` 12.1, `approximate_file_selection` 16.8,
`first_tab_file` 13.9, `epoch_selection_supersession` 14.6; the largest gated
phase is 42.3 ms (`unavailable_file_selection`). Earlier runs at the same
product (`ed2ab5f7d`, `bb3308e81`) during load averages of 40–134 on 14 cores
from other worktrees' builds failed 1–3 rows each in moving phases. Two of
those were `paged_review_and_git_load` and `fixture_connect`, which run before
any comparison exists. A/B runs alternated with an `origin/main` control build
(`5efe30739`) under the same load. Responsiveness was equal in three pairs
(11–32 ms on every phase on both sides). The control failed too under load
(`large_file_selection` 151 ms), and `epoch_selection_supersession` overlapped
(control 22–86, candidate 15–153). No regression was found; the clean gate is
the low-load run above.

**Pins.** Main moved again before the push. BPR-1341 (`d9cc2aebf`) resealed
over BPR-1336's and BPR-1338's pinned-scope commits and renamed the
Conversations modality to Threads. The landed commits are this card's,
rebased onto `d9cc2aebf`, with one BPR-1334 reseal over BPR-1341's (pins match
there; mutation pin unchanged). They were not rebuilt again. Apart from
BPR-1341's renames, which share no identifier with this card's code, the
product equals the frozen candidate `f6f247e8b`.

## Owner look v001 (C5, rejected)

One new short session, `54a2166987592a13adcae6767089fe2c`, with state in
`/Users/jaeger/.mask/bpr-1334-line-details-20261008-v001/attended-review`
(repository root: the main checkout). It holds one Accept/Reject candidate,
`BPR-1334-line-details-v001`, at revision `f6f247e8b` (fingerprint
`1f420af2…`), ready and protected. Launch command:
`python3 /Users/jaeger/Documents/GitHub/BetterPullReview/artifacts/bpr-1334/line-details-frozen-v001/launch.py`.
C1–C4 rest on the direct evidence above. C5 waits on the owner and is not
self-certified.

## Follow-up on main — 2026-10-08

The windowless macOS bar (`desktop_menu_bar`) did not mirror View's new Line
Details item, which broke `qt_profile_management_window`'s comparison with a
window's View. The native empty-desktop course compares the same thing.
BPR-1338's follow-up commit adds the item after One pane, checkable and
disabled, and the row passes. Nothing else on this card changed, and its owner
look is unaffected.

## Owner verdict on v001 — 2026-10-08

Session `54a2166987592a13adcae6767089fe2c` (state
`/Users/jaeger/.mask/bpr-1334-line-details-20261008-v001/attended-review`),
handed back at event 11. Candidate `BPR-1334-line-details-v001` at
`f6f247e8b`, fingerprint `1f420af2…`: **Reject**. The owner's note, verbatim:

> Turn off word wrap, let it have a horizontal scroll if the line is too long.

The candidate was released with `resume-development` (event 12), its running
frozen window was stopped, and `artifacts/bpr-1334/line-details-frozen-v001/`
was deleted.

## v002 — no wrap, one shared horizontal scroll (2026-10-08)

Design: `frontend-design:frontend-design` was loaded first. Nothing new is
drawn; the change is behaviour.

- `line_details_pane` uses `NoWrap` with the horizontal bar as needed. Every
  row is one line of the one document, so the base and head rows share the
  pane's single horizontal bar and a column sits at one x on both rows.
- The gutter is outside the viewport, so it does not scroll. It is now placed
  beside the viewport only (`viewportEvent` re-places it when the bar comes or
  goes), so it no longer covers the start of the bar.
- `present` ends with `reveal_first_change`: the leftmost changed character
  over all rows (an emphasis start, or column 0 of an added or removed row).
  If it lies within the first screenful less twelve glyphs, the strip stays at
  column 0; otherwise it scrolls there, keeping up to twelve glyphs (at most a
  quarter of the width) of text before it. A line with no marked change opens
  at its start. `first_change_x()` exposes the measured position.
- A selection of up to 16 lines keeps each row on one line. The core's
  4,096-byte cut is unchanged.
- The default height (eight text lines) is unchanged; its comment no longer
  says it is room to wrap. At the smallest height the widget's own minimum
  already keeps both rows above the bar (checked, below), so the splitter's
  minimum is unchanged.

**Tests.** `qt_diff_workbench_scale_{100,125,200}`
(`test_line_details_shows_the_current_line_base_over_head`): no wrap and the
bar as needed; long rows one line each with a visible bar; the strip opens
with the first changed character (`omega`, near the end of a line several
screens wide) in the viewport; a column at the same x on both rows before and
after a scroll, both moving by the scroll distance; the gutter's geometry and
painting identical scrolled and unscrolled while the code moves; the gutter
above the bar; an unchanged line opens at its start; each row of a three-line
selection on one line; at the smallest height both rows stay above the bar.
`application_line_details_public_path` (composed, the line-details-v1
fixture at 1280 wide): each long SQL row one line with the bar, the first
change in view on open, the gutter beside the rows and above the bar.

**Failing-before controls** (each mutation reverted after its run; build and
run at scale 100 plus the composed row): wrap restored (`WidgetWidth`, which
is v001) fails "does not wrap" and the composed "each long row stays one
line"; reveal removed fails "opens scrolled so the first changed character is
in view" in both; head row letter-spaced by 1 px fails "a column sits at the
same x on the base row and the head row"; gutter painting offset by the
horizontal scroll fails "the gutter does not scroll with the rows"; gutter as
tall as the frame (v001's placement) fails "the gutter stops above the
horizontal bar" and the composed gutter check. The smallest-height check also
passes with the bar left out of the splitter's minimum, which is why the
splitter's minimum was not changed.

**Rows at the candidate** (Release, tests on, Apple clang `-O3`, Qt 6.11.2,
macOS 26.6.2, M4 Pro, offscreen): `qt_diff_workbench_scale_{100,125,200}`,
`application_line_details_public_path`, `architecture_boundaries` and
`application_review_round_trip_composition_contract` pass; the pins match, so
no reseal (the change is outside the pinned scope). The 18 optimized
large-review rows (`application_large_review_*`,
`application_{unavailable,approximate}_diff_public_path`,
`application_first_tab_comparison`, `large_review_replay_contract`) pass 18/18
at load average 5–10; the largest responsiveness pulse is 33.1 ms
(`review_modality`), `large_file_selection` 22.9 ms,
`large_source_revisit_render` 15.7 ms, no input-budget failures. Format check
clean on the changed files; clang-tidy on `line_details_pane.cpp` clean.
Documentation: `DOCUMENTATION_OK`.


**Merged tree.** Rebased onto `origin/main` `c6e78d9e2` (BPR-1091 and
BPR-1092, none on Line details paths) as `f2f9ce1e0`. At the merged tree
`qt_diff_workbench_scale_{100,125,200}`, `application_line_details_public_path`,
`architecture_boundaries` and `application_review_round_trip_composition_contract`
pass (pins match; no reseal).

## Owner look v002 (C5, pending)

**Frozen root.** `artifacts/bpr-1334/line-details-frozen-v002/` holds
`source/` (`git archive f09687aa0`, the pre-rebase commit; `diff -rq` clean
against the worktree; source tree digest `c68053e6…`), `build/` (Release,
tests on) and `launch.py` (sha256 `83be3038…`), which checks the fixture tree
and the executable and reports an exit within 30 s. The attended executable
`build/tests/end_to_end/line_details_public_path` (sha256 `7ec6d695…`) passes
its course and reaches `LINE_DETAILS_ATTENDED_READY` offscreen. The landed
`f2f9ce1e0` differs from it only by the BPR-1091/BPR-1092 commits above.

One new short session, `3e36d128a75c3e250a312d78e593f8c4`, state
`/Users/jaeger/.mask/bpr-1334-line-details-20261008-v002/attended-review`
(repository root: the main checkout), host detached. One Accept/Reject
candidate, `BPR-1334-line-details-v002` ("line details v002"), revision
`f2f9ce1e0`, fingerprint `07bda61b…`, ready and protected; event cursor 4.
Launch command:
`python3 /Users/jaeger/Documents/GitHub/BetterPullReview/artifacts/bpr-1334/line-details-frozen-v002/launch.py`.
C1–C4 rest on the direct evidence above. C5 waits on the owner and is not
self-certified.

## Owner verdict on v002 — 2026-10-08

Session `3e36d128a75c3e250a312d78e593f8c4` (state
`/Users/jaeger/.mask/bpr-1334-line-details-20261008-v002/attended-review`),
handed back at event 6. Candidate `BPR-1334-line-details-v002` at
`f2f9ce1e0`, fingerprint `07bda61b…`: **Reject**. The owner never saw the
strip. The launch printed

> STARTUP_FAILED: candidate exited before ready; log …/artifacts/bpr-1334/line-details-frozen-v002/native-launch.log

and the log ended with `FAILED: the gutter sits beside the rows and above
the bar, outside what scrolls`, then `libc++abi: terminating due to uncaught
exception of type bpr::tests::expectation_failed`.

Two faults. The product placed the gutter for a legacy bar only. macOS draws
overlay (transient) bars by default, and an overlay bar takes no height: it
floats over the bottom of the viewport, so the gutter, which ran the
viewport's height, reached under it, and at the smallest height the bar
would float over the head row while it is being scrolled. Every offscreen
run used a legacy bar, so none saw it. The launcher ran the course's checks
before its window, as hard expectations, so one failed check of the window
killed the owner's launch without a window, and the exception escaped
`main`.

The candidate `BPR-1334-line-details-v002` was released with
`resume-development` (event 7). Its host was already stopped, and
`artifacts/bpr-1334/line-details-frozen-v002/` was deleted.

## v003 — the bar covers no row, and the launch never aborts (2026-10-08)

Design: `frontend-design:frontend-design` was loaded first. Nothing new is
drawn. The bar's band stays free, so the strip is the same in both bar styles.

- **Product.** `line_details_pane::place_gutter` sets a bottom viewport
  margin of the horizontal bar's height when the bar's style overlaps
  (`PM_ScrollView_ScrollBarOverlap` > 0, which is Qt's transient style, the
  macOS default). It sets none for a legacy bar, which takes its own height.
  The viewport and the gutter beside it therefore end above the bar in both
  styles. An event filter on the bar re-places them when its style changes,
  as it does when the macOS scroll-bar setting changes.
- **Attended launch.** `line_details_public_path --attended` reports a failed
  check of what its window shows as `LINE_DETAILS_ATTENDED_CHECK_FAILED: …`
  and opens the window anyway. A failed precondition (startup, sign-in,
  Review, the line's comparison) still ends the run, with its `FAILED:` line
  and `LINE_DETAILS_ATTENDED_FAILED`. `main` catches the failure, so no
  exception escapes to `libc++abi`. The course run (no `--attended`) fails
  on every check as before. The frozen root's `launch.py` (not in the
  repository) checks the fixture tree and the executable. If the run exits
  before ready it prints `STARTUP_FAILED` with the course's `FAILED` lines.
  When the window opens it prints each self-check disagreement as a note. It
  gives up after 30 s.

**Tests.** `qt_diff_workbench_scale_{100,125,200}` gains
`expect_line_details_bar_band`. It checks that the gutter is beside the rows
and exactly as tall as they are, that the rows and the gutter end above the
bar, and that both rows are read above it. It runs at the default and the
smallest heights with the platform's bar, then with Qt's transient style
(`overlay_scroll_bar_style`, a `QProxyStyle` with `SH_ScrollBar_Transient`
and the bar's extent as `PM_ScrollView_ScrollBarOverlap`). That style is set
on a strip that is already shown, as the macOS setting would change it. It
then runs once more on the legacy bar again, which must keep no extra band.
`application_line_details_public_path` also checks that the viewport ends
above the bar in the platform's own style.

**Failing-before controls** (each reverted after its run):
- Reserve removed (v002's placement). Fails "overlay bar, default strip: the
  rows and the gutter end above the bar", with the viewport at 1..128 against
  a bar at 111. That is v002's native failure, reproduced offscreen.
- A window check forced false. The course run exits 1 with its `FAILED:`
  line and no abort. `--attended` prints `LINE_DETAILS_ATTENDED_CHECK_FAILED`,
  then `LINE_DETAILS_ATTENDED_READY`, and stays open.
- A precondition forced false under `--attended`. Exits 1 with `FAILED:` and
  `LINE_DETAILS_ATTENDED_FAILED`, and its course root is removed.
- `launch.py` against a stub root: an early exit, a ready window with a
  check note, and a missing executable. Each prints its message under the
  system `python3` 3.9.

**Rows.** At `e1f003d51` (Release, tests on, Apple clang `-O3`, Qt 6.11.2,
macOS 26.6.2, M4 Pro, offscreen), `qt_diff_workbench_scale_{100,125,200}`
and `application_line_details_public_path` pass. The four
`expect_line_details_bar_band` cases print matching bands (scale 100,
default: viewport 1..110, gutter 1..110, bar at 111).
`architecture_boundaries` passes too. The three round-trip pins match
(`BPR_PRINT_PINNED_DIGESTS`), because the change is outside the pinned scope.
clang-tidy on `line_details_pane.cpp` and the public-path course is clean.
The workbench test's only finding, `column_x`, predates this change. Format
is applied and `DOCUMENTATION_OK`. Under the platform's legacy style, which
offscreen uses, the product change sets no margin, so the large-review rows'
v002 evidence stands. At the merged tree `db7026a2d` (over BPR-1091,
BPR-1343), the same rows and `architecture_boundaries` pass, the pins match,
and `DOCUMENTATION_OK`. It landed as `e43c666c5`, whose only addition is
BPR-1336's zoom test follow-up. That commit touches no Line details path.

## Owner look v003 (C5, rejected)

**Frozen root.** `artifacts/bpr-1334/line-details-frozen-v003/` holds
`source/` (`git archive e1f003d51`, `diff -rq` clean) and `build/`. The
build is Release with tests on, configured from that `source/`, with its CPM
cache inside the root. `launch.py` has sha256 `ae1b1e39…`. The attended
executable `build/tests/end_to_end/line_details_public_path` has sha256
`26cbaab6…`. **Native start:** `launch.py` reached `READY` on this Mac's
Cocoa desktop in 2 s with no self-check note. The candidate was then closed
with SIGTERM, and its course root was removed. Nothing on the desktop was
captured or driven.

One new short session, `e45c5020c1c1b1dd90c41f66b8dd373a`. Its state is
`/Users/jaeger/.mask/bpr-1334-line-details-20261008-v003/attended-review`
and its repository root is the main checkout. The host is detached
(`serve.log` beside the state). It holds one Accept/Reject candidate,
`BPR-1334-line-details-v003` ("line details v003"), at revision `e43c666c5`
with fingerprint `538d0513…`, ready and protected. The event cursor is 4.
Launch command:
`python3 /Users/jaeger/Documents/GitHub/BetterPullReview/artifacts/bpr-1334/line-details-frozen-v003/launch.py`.
C1–C4 rest on the direct evidence above. C5 waits on the owner and is not
self-certified.

## Owner verdict on v003 — 2026-10-08

Session `e45c5020c1c1b1dd90c41f66b8dd373a` (state
`/Users/jaeger/.mask/bpr-1334-line-details-20261008-v003/attended-review`),
handed back at event 6. Candidate `BPR-1334-line-details-v003` at
`e43c666c5`, fingerprint `538d0513…`: **Reject**. The owner's note, verbatim:

> there's no syntax highlighting.

The strip drew every row in the plain code ink: it took no classification,
while the panes above it colour the same lines. The candidate was released
with `resume-development` (event 7). Its host was already stopped, and
`artifacts/bpr-1334/line-details-frozen-v003/` was deleted.

## v004 — rows in the panes' syntax colours (2026-10-08)

Design: `frontend-design:frontend-design` was loaded first. Nothing new is
drawn: the strip takes the panes' colours, so a row reads as the line it
repeats.

- **Product.** `line_details_pane::set_token_spans` adopts the base and the
  head blob's classification, the `shared_token_spans` the panes already
  hold; `diff_workbench::adopt_token_spans` hands both over, and the reset
  that drops a comparison's spans drops the strip's too.
  `line_details_colors` carries the panes' role table (`Syntax`, which
  replaces `Ink`). `apply_syntax` colours each row from its side's spans for
  its line through `code_syntax_format_ranges` and
  `apply_syntax_format_ranges`, the functions the panes use. It runs on a
  new projection, a late classification and a theme change. Only the shown
  bytes are coloured: a span past the core's 4,096-byte cut is dropped, and
  one across it is clipped there. The row's shown bytes are kept with the
  row, so a classification that lands later needs no diff pointer.
- **Change highlight.** The panes' intraline mark moved into one shared
  `append_intraline_emphasis_span` (`code_sample_painter`). It lays the fill
  as a background over the exact range, the plain ink readable over it, and
  each classified token's own colour over it, moved only as far as
  `core::readable_emphasis_foreground` needs. `diff_workbench` and the strip
  both call it, so the panes' behaviour is unchanged and a changed token reads
  the same in both.
- The 100 ms gates are not touched: the strip colours at most its 33 rows of
  at most 4,096 bytes each, on events it already handled.

**Tests.** `qt_diff_workbench_scale_{100,125,200}` gains
`test_line_details_colours_tokens_as_the_panes_do`. The spans arrive after
the line is shown. In light and then in dark, each row's layout formats
equal its pane's formats for the same line (five ranges each). The function
and the string read in the profile's colours. Every highlighted character of
the changed token is painted
`readable_emphasis_foreground(token colour, intraline fill)`, not the plain
ink's. On a 4,500-byte line of tokens, the pane has 1,500 ranges and the
strip 1,366: the token across the cut is clipped to one character and none
lies past it. `application_line_details_public_path` (composed, real
classifier) waits for the classification. It then checks that the SQL line's
base and head rows equal the panes' formats for line 6 (a check that
`--attended` prints rather than ends on). On the cut minified line it checks
that the strip's 1,120 ranges stop at 4,096 while the pane's 3,915 run past
it, and that each shown range equals the pane's.

**Failing-before controls** (each mutation reverted after its scale-100 run):
- Syntax not applied (v003's strip) fails "light: the base row's token
  colours are the base pane's for that line", with 0 ranges.
- The strip's old emphasis (plain ink only) fails "light: a changed character
  of a token keeps the token's colour over the change highlight".
- Rows coloured against the uncut line fail "the strip colours the tokens it
  shows, and none past the 4,096-byte cut", with 1,500 ranges.
- `adopt_token_spans` not handing the spans to the strip fails the light
  check with 0 ranges.
- A theme change not recolouring fails "dark: the base row's token colours
  are the base pane's for that line".

**Rows.** Release, tests on, Apple clang `-O3`, Qt 6.11.2, macOS 26.6.2,
M4 Pro, offscreen. At the frozen candidate `7e43eba32` (over `2762359ec`)
and again at the rebased tree over `15c86f2ed` (clang-format 22 clean on
the changed files), `qt_diff_workbench_scale_{100,125,200}`,
`application_line_details_public_path`, `architecture_boundaries` and
`application_review_round_trip_composition_contract` pass (pins match; the
change is outside the pinned scope). `DOCUMENTATION_OK`. Format applied.
`tidy.py` reports nothing on `line_details_pane.cpp`,
`code_sample_painter.cpp` or the public-path course. On `diff_workbench.cpp`
and its test, its only findings predate this change (`optional-value-conversion`
at 2717–2718, `column_x`).

**Large-review rows (the 18).** Under the same tests at the pre-rebase
candidate (load 12–23), 17 of 18 passed. `application_large_review_history`
failed once on one 100.1 ms pulse in `epoch_refresh_29`. The other rows' worst
pulses were 33–92 ms, with no input-budget failures. Earlier, at load 30–110,
`launch` (414 ms before `main`, a process-start phase),
`pathological` and `cache_bytes` failed once each and then passed. A/B against
an `origin/main` control binary built in the same tree:
- `cache_bytes`: the control failed 1 of 3 at the same 187 MB footprint.
- `history`, alternated at load 7–28: the candidate failed 3 of 7
  (`epoch_refresh_29` pulses of 100.1 and 185.7 ms, and
  `historical_base_selection` 117.8 ms). The control failed 0 of 4 there,
  and 1 more at load 145.
- All 18, alternated at load 180–310 at the merged tree: the candidate failed
  4 and 5 rows, the control 3 and 5, in the same phases.

The history course's line is `int fixture_0030 = …;`. The strip's added
work there is two rows of about 20 bytes per publication. A probe that was
never committed timed every strip entry point inside the optimized courses at
load near 300, where pulses of 100–250 ms came from other phases, and was then
reverted:
- `history`: `set_token_spans` was at worst 0.93 ms over 174 calls, `present`
  0.29 ms and `present_empty` 0.03 ms.
- `responsiveness`: `present` was at worst 0.53 ms.

So the strip, including what v004 adds, is at most about 1 ms of any pulse.
Even so, this is not a clean 18/18. A quiet run of the 18, with `history`
alternated against the control, is still owed, and C4 is not reclaimed until
that run is made. The machine stayed at load 100–425 from other worktrees'
builds from 21:30 to 00:10.

## Owner look v004 (C5, pending)

**Frozen root.** `artifacts/bpr-1334/line-details-frozen-v004/` holds
`source/`, which is `git archive 7e43eba32` (`diff -rq` clean). The
line-details-v1 fixture (`construction.json`, `objects.pack` and the contract
directory) was regenerated inside `source/` by its three repository scripts
when the build was frozen, and matches the archive byte for byte. `build/` is
a Release build with tests on, configured from that `source/` with its CPM
cache inside the root; the attended executable names no path outside the
root. `launch.py` has sha256 `73850c35…`; it checks the three fixture files
and the executable, and reports an exit within 30 s. The attended executable
`build/tests/end_to_end/line_details_public_path` has sha256 `f4f8fc0c…`.
At the frozen build, `qt_diff_workbench_scale_{100,125,200}` and
`application_line_details_public_path` pass. **Native start:** `launch.py`
reached `READY` on this Mac's Cocoa desktop in about 7 s, at load near 300,
with no self-check note; the new syntax check runs before the window opens.
The candidate was then closed with SIGTERM and its course root removed by
hand. Nothing on the desktop was captured or driven.

The product landed as `5fbc155e9`. It differs from the frozen `7e43eba32`
only by upstream commits outside the Line details paths: BPR-1094, BPR-1335,
BPR-1346 to BPR-1349 and BPR-1355. BPR-1335 touches `diff_workbench.cpp`, for
the "+" column only.

One new short session, `42f024b816d83ebe52af5d7aa579b881`. Its state is
`/Users/jaeger/.mask/bpr-1334-line-details-20261008-v004/attended-review`,
its repository root is the main checkout, and its host is detached
(`serve.log` beside the state). It holds one Accept/Reject candidate,
`BPR-1334-line-details-v004` ("line details v004"), at revision `5fbc155e9`
with fingerprint `e956fff9…`, ready and protected. The event cursor is 4.
Launch command:
`python3 /Users/jaeger/Documents/GitHub/BetterPullReview/artifacts/bpr-1334/line-details-frozen-v004/launch.py`.
C1–C3 rest on the direct evidence above. C4 owes a quiet run of the 18
(above). C5 waits on the owner and is not self-certified.

## Follow-up: `qt_appearance_profile` (2026-10-09)

`e7e5ffb3b` broke `qt_appearance_profile` ("the reprojected position keeps its
comparison and semantic anchor"). A `git bisect` from `b62420807` (pass) to
`54dfbd463` (fail), building only that test, named `e7e5ffb3b` as the first
failing commit. Line details' default height is eight lines of its own font,
and the strip takes the code font, so the course's larger code font grew the
strip from 130 to 170 px and shrank the comparison viewport from 601 to 556 px
(macOS offscreen); the synchronization rail then crossed a different segment.
The product is as this card intends, so the test is what changed:
`test_a_font_change_moves_geometry_and_not_semantics` now pins the strip at
its shown height with `set_line_details_height` before the font change, as a
reader's stored height does, so it measures only the font's effect. Test-only;
no product or card-scope change. `qt_appearance_profile` passes on macOS.

## Windows station run (bpr-main-win, 2026-10-09)

At `7672d5a8b`, Windows 11, MSVC 19.50, `build/windows-dev-tour-cli` (Debug), offscreen,
network-isolated runner, `-j4`:

- **Pass:** `qt_appearance_profile`, after `2339d0912`.

## C4: Windows quiet run of the large-review rows (bpr-main-win, 2026-10-09)

The Windows station was otherwise idle. Two Release tests-on trees (MSVC 19.50,
Qt 6.11.2):
- **candidate:** `772200c95` (main, with this card's `5fbc155e9`);
- **control:** `0f1ba22f0`, the commit just before `5fbc155e9`.

The large-review selection ran at `-j1` in three alternating pairs
(candidate, control, candidate, control, …). On Windows it holds 19 rows: the
18 plus the benchmark's contract rows.

| Run | Candidate | Control |
|---|---|---|
| 1 | 17/19 | 17/19 |
| 2 | 17/19 | 17/19 |
| 3 | 17/19 | 16/19 (also `cache_bytes`) |

- **Failures on both sides:** `application_large_review_active_work`
  (`active_supersession_final_drain` 1577–2541 ms on the candidate, 1521–2265 on
  the control, against 1250) and `application_large_review_history` (epoch
  source selection over 2000 ms). Both are [[BPR-1327]]'s preparation on
  Windows, which [[BPR-1344]] addresses. Neither is this card's.
- **Control only, once:** `cache_bytes` failed its correctness check, "stale
  comparison published while superseding file B remained selected". The
  candidate passed it 3 of 3.
- **The 100 ms input target:** no input-budget failure on either side in any
  run. The worst pulse of every row was 19–70.4 ms on the candidate and
  19–71.6 ms on the control. Each row's worst sat in the same phases on both
  sides, mostly `paged_review_and_git_load`, history's epoch refreshes and
  cache bytes' byte refreshes.
  - `large-review-responsiveness`: candidate 57.3, 64.1 and 57.2 ms; control
    57.3, 58.0 and 64.0 ms.
  - `large-review-history`: candidate 57.5, 54.4 and 57.9 ms; control 49.6,
    53.3 and 51.8 ms.

**Verdict for C4:** on a quiet machine the strip adds no failure and no
pulse near 100 ms on Windows. Every row's worst pulse stays within a few
milliseconds of the control's. The two failing rows fail the same way without
this card, so they are not C4's. A macOS quiet run remains the coordinator's
call.
