---
schema: "implement-rfc.card.v1"
id: "BPR-1350"
title: "The Overview Files heading is clipped without an ellipsis at a narrow width"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"bpr-main-linux, BPR-1094's rendered checks, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1349--BPR-1351--2371e399c373.json","requested_skill":"frontend-design","owner_look":"required, on the Mac","related_rfcs":["RFC-BPR-0049","RFC-BPR-0050"],"related_cards":["BPR-1094","BPR-1089"]}
acceptance: [{"criterion":"BPR-1350:ellipsis","outcome":"pass","revision":"ed28a2a7d","evidence":{"kind":"command","detail":"qt_pull_request_overview_page and qt_pull_request_overview_chips_scale_{125,200}: at 1,000/800/600/420 px in four themes and both mark styles, the heading stays on the page and runs under no control in its row, and the narrowest page shortens it; renders show the ellipsis (1,000 px at 125%: '..., 2 not ...' before Open). Before: clipped at '2 not rea'."}},{"criterion":"BPR-1350:tooltip-and-name","outcome":"pass","revision":"ed28a2a7d","evidence":{"kind":"command","detail":"The accessible name is always the whole sentence; when shortened the tooltip begins with it (anchored by BPR-1354). Controls: the page source before this change fails the accessible-name check at 100/125/200%; dropping the tooltip fails the tooltip check."}},{"criterion":"BPR-1350:owner-look","outcome":"pass","revision":"ed28a2a7d","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1350-the-overview-files-heading-is-clipped-without-an-ellipsis-at-a-narrow-width.md","detail":"Owner Accept, no note, 2026-10-09T12:56:37Z: session 4395ac1375d6c5d1e37bb79d863fb2e8, candidate BPR-1350-files-heading-v001 (fingerprint fd88439e541d..., frozen-ed28a2a7d-v001), decision 5de1251b-2fb5-4b54-b03c-7e9bacfffd4e at event 4, handed back at 5, released at 6."}}]
---

# BPR-1350: The Overview Files heading is clipped without an ellipsis at a narrow width

## Finding

[[BPR-1094]] rendered the Overview offscreen at `269f7ccff`, with the page at
1,000 px (its files column about 680 px) and the "?" chip showing. The Files
heading is cut mid-word, reading "…2 not rea" under the Open button. It has no
ellipsis, and its accessible name has no full text. The composed 1,400 px
window shows its shorter heading ("Files: 17 in this pull request, 17 not
read") whole at 100%, 125% and 200%. The chips themselves fit whole.

## Outcome (coordinator, 2026-10-08)

When the heading does not fit, it is shortened with an ellipsis. The full
sentence is its tooltip and its accessible name.

## Acceptance

- At a narrow Overview, with "?" and Open/All showing, the heading never cuts
  mid-word without an ellipsis. Check 100%, 125% and 200% and every theme.
- The tooltip and accessible name carry the full sentence. A Qt test asserts
  both, with a failing-before control.
- **Owner look on the Mac.** The design pass uses the `frontend-design` skill
  (AGENTS.md), and the owner reviews the result in a short attended session.

## Boundaries

Presentation only: no count, drill-down or settings behaviour changes.

## Built on bpr-main-linux (2026-10-09)

**Design skill.** The `frontend-design:frontend-design` plugin is not
enabled on this station. Enabling it would change Claude Code's
configuration, which a peer request cannot authorize. Its guidance was read
from the official marketplace copy
(`~/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`)
and applied where it bears on a one-line Qt heading: restraint (no new
control or styling), plain words (the sentence is unchanged), accessibility,
and reviewing renders.

**Change** (`src/adapters/qt/src/pull_request_overview_page.cpp`, presentation
only). The Files heading is now an eliding heading, a `QLabel` subclass
private to the page:
- Its text stays the whole sentence, so every reader of `files_heading()->text()`
  is unchanged. It paints the sentence shortened at the right with an
  ellipsis (`QFontMetrics::elidedText`) to the width its row gives it.
- Its minimum width is one letter and the ellipsis, so the row narrows it
  rather than clipping it.
- Its accessible name is always the whole sentence. It was the fixed "Files
  in the pull request".
- Its tooltip is the whole sentence whenever it is shortened. The cached
  catalogue's historical note, the only other tooltip it carried, follows the
  sentence after a blank line. Since [[BPR-1354]] that tooltip is anchored
  next to the heading like every other.
- Every place that set the heading's text now goes through
  `present_heading`, which recomputes the shortening. A resize, font change
  or style change recomputes it too.

**Test** (in `tests/adapters_qt/pull_request_overview_page_tests.cpp`, inside
the chips course). It runs in all four themes and both mark styles, through
`qt_pull_request_overview_page` (100%) and
`qt_pull_request_overview_chips_scale_{125,200}`, with the label chips and the
"?", Open/All and label-key controls showing. At page widths of 1,000, 800,
600 and 420 px it checks:
- the heading keeps the whole sentence as its text and accessible name;
- it stays on the page and runs under no visible control whose centre is in
  its row;
- when the sentence does not fit, the tooltip begins with it; when it fits,
  the tooltip does not repeat it;
- the narrowest page does shorten it.

With `BPR_OVERVIEW_CHIPS_RENDER_ROOT` set, it writes
`files-heading-<theme>-<scale>-<width>.png`.

**Renders** (offscreen, not committed): at 1,000 px and 125% the heading reads
"Files: 3 in this pull request, 2 changed since my last review, 2 not …",
ending before the Open button. Before, it was cut at "…2 not rea" under it.
At 800 px and 200% in the dark theme it reads "Files: 3 in this pull
request, 2 cha…". At 420 px only "F…" or the ellipsis fits beside the
controls.

**Failing-before controls** (each built, run and reverted):
- The page source before this change fails at "the heading keeps the whole
  sentence as its text and accessible name", at 100%, 125% and 200%. The
  label measured 432 px against a 440 px sentence at 1,000 px and 125%, and
  clipped its paint.
- Dropping the tooltip when shortened fails at "a shortened heading's tooltip
  is the whole sentence".
- Restoring the full-width minimum did not fail. The Overview's pane sits in
  a splitter whose minimum is 0, so the old label was squeezed and clipped,
  never widening the page. The layout checks guard against overlap and
  running off the page, not this mutation.

**Rows.** At this card's commit, in `build/dev`, 24 of 24 pass: the overview
page and chips scale rows, `qt_files_tree`, the overview public paths,
`application_accepted_file_in_overview` (its exact heading text), the window
scales, the round-trip contract, `architecture_boundaries` and
`test_network_isolation_manifest`. `application_offline_inventory` (the
catalogue-unavailable heading) also passes. `coding-format-check` (clang-format
22) and `DOCUMENTATION_OK` pass. Not run: the optimized `-O3` tree.

**Owner look: accepted** (see "Owner verdict, session v001"). The Mac freezes the build and opens the session. Look
at the ellipsis at narrow widths, the tooltip beside the heading, and the
420 px case, where only the ellipsis fits.

## Owner look (accepted, below)

One new short session, one question, Accept or Reject: "the Files heading
shortens with an ellipsis and shows the whole sentence in its tooltip". The
owner is asked to narrow the window to about 1,000 px and below, and to rest
the pointer on the shortened heading.

- Session `4395ac1375d6c5d1e37bb79d863fb2e8`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1350-files-heading-v001`,
  repo root the main checkout, candidate `BPR-1350-files-heading-v001`
  (revision `frozen-ed28a2a7d-v001`, fingerprint `fd88439e541d…`), criterion
  `BPR-1350:owner-look`, ready at event cursor 3. The host runs detached; the
  offline page is `review.html` in the state directory.
- Frozen root `/Users/jaeger/bpr1350-files-heading-v001.ijmh105y`, made by
  `scripts/freeze_attended_build.py freeze --open-pull-request` from
  `ed28a2a7d` (fixture `overview-labels-v1`, bpr-fixtures/nested-folders#21,
  opening on its Overview with the "?" count beside the heading), executable
  SHA-256 `40a28bca6404…`. Its own `--check` reached
  `ATTENDED_PULL_REQUEST_OPENED`. The freeze script gained
  `--open-pull-request` for this look, because its launcher always passed
  `--attended-open-file`, which opens Review, and the application refuses
  that flag together with `--attended-open-pull-request`.
  `tests/scripts/freeze_attended_build_tests.py` covers it.
- Launch: `python3 /Users/jaeger/bpr1350-files-heading-v001.ijmh105y/launch.py`.
- Attached: offscreen renders from the chips course
  (`BPR_OVERVIEW_CHIPS_RENDER_ROOT`, `--chips` at 125% and 200%, both
  passing on macOS) at 1,000 px and 125% (light), 800 px and 125% (light) and
  800 px and 200% (dark). On macOS the 1,000 px and 125% heading fits whole,
  "Files: 3 in this pull request, 2 changed since my last review, 2 not read",
  so it is not shortened there. Linux's wider fonts did shorten it. At
  800 px it ends "…2 changed …" before Open in both. The fixture's own
  heading ("Files: 17 in this pull request, 17 not read") is shorter, so in
  the frozen build it starts to shorten only below about 1,000 px.

## Next

Read session `4395ac13…` from cursor 3. Accept satisfies
`BPR-1350:owner-look`; then finish the card, release the session and remove
the frozen root with `freeze_attended_build.py remove`. A Reject names what
the owner saw (a cut word, the heading under a button, a missing or
misplaced tooltip).

## Owner verdict, session v001

Accept, 2026-10-09T12:56:37Z, decision `5de1251b-2fb5-4b54-b03c-7e9bacfffd4e`
at session event 4, no note (candidate `BPR-1350-files-heading-v001`,
fingerprint `fd88439e541d…`, revision `frozen-ed28a2a7d-v001`). The owner
handed session `4395ac1375d6c5d1e37bb79d863fb2e8` back at event 5, read from
cursor 0.

This satisfies `BPR-1350:owner-look`. `BPR-1350:ellipsis` and
`BPR-1350:tooltip-and-name` stand on their recorded evidence, and the Windows
station run below passes their rows. The card is Done.

The host (pid 65613) was stopped and the candidate released with
`resume-development --all` at event 6. The frozen root
`/Users/jaeger/bpr1350-files-heading-v001.ijmh105y` was removed with
`scripts/freeze_attended_build.py remove`. The session's state folder keeps
the record.

## Windows station run (bpr-main-win, 2026-10-09)

At `66c1cf4c8`, Windows 11, MSVC 19.50, `build/windows-dev-tour-cli` (Debug), offscreen,
network-isolated runner, `-j4`:

- **Pass:** `qt_pull_request_overview_page` and
  `qt_pull_request_overview_chips_scale_{125,200}`.
- **Not registered on Windows:** `qt_pull_request_overview_chips_scale_100`,
  so it was not run.
