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
  the changed characters are highlighted. Long lines wrap; the strip never
  scrolls sideways.
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
  head rows appear under the comparison, wrapped, with line numbers and
  intraline highlights from core. They follow a click, a change step and a
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
  and answers Accept or Reject for "line details strip".

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

## Owner look (C5, pending)

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
