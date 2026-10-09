---
schema: "implement-rfc.card.v1"
id: "BPR-1337"
title: "Beautify the top section of Discuss"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","found_by":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1337--BPR-1337--2dee31ef43be.json","requested_skill":"frontend-design","related_cards":["BPR-1212","BPR-1089","BPR-1083","BPR-1290","BPR-1293","BPR-1294"]}
acceptance: [{"criterion":"BPR-1337:inventory","outcome":"pass","revision":"55dec39a4","evidence":{"kind":"artifact","path":"docs/design/BPR-1337-discuss-top/before.png","detail":"application_discuss_top_public_path before renders at 1440x900, light and dark, on overview-labels-v1 (34 labelled threads, 3 PR comments, 2 review summaries); the card's Inventory section lists what looked unpolished."}},{"criterion":"BPR-1337:candidates","outcome":"pass","revision":"55dec39a4","evidence":{"kind":"artifact","path":"docs/design/BPR-1337-discuss-top/candidate-a.png","detail":"Two real Qt candidates, A (one band, candidate-a.png) and B (heading over tools, candidate-b.png); A chosen with the written rationale in the card (height, one language with Conversations' bar, narrow width, counts as status, minimal words)."}},{"criterion":"BPR-1337:built","outcome":"pass","revision":"55dec39a4","evidence":{"kind":"command","detail":"application_discuss_top_public_path (composed, 1440x900: one row under 50 px from x 12, counts and tooltip, base in light and dark, drill-down pill, wraps beside the composer) and three new qt_github_conversation_panel tests (accessible names, tab order, narrow 300 px wrap, themes and the dot() ink fix); five failing-before controls fail as recorded; 19 focused rows, coding-format-check and tidy pass on macOS arm64. Linux and Windows runs owed by the stations, not blocking."}},{"criterion":"BPR-1337:owner-look","outcome":"pass","revision":"55dec39a4","evidence":{"kind":"observation","detail":"Session 6e2b595ff9c4160456b63e491b0f69c0: BPR-1337-discuss-top-choice-v001 (design-55dec39a4-v001) selected 'A: one band', no note, decision 198433b7-859d-480f-a87e-26f3fc7d6dcb (2026-10-09T01:33:21Z); BPR-1337-discuss-top-built-v001 (frozen-55dec39a4-v001, executable 4bc752801a45...f761) Accept, no note, decision 9d0e321b-a9a8-468b-bc3b-cd409d35b3d4 (2026-10-09T01:31:50Z); handed back at event 10."}}]
---

# BPR-1337: Beautify the top section of Discuss

## Owner note

Owner, chat, 2026-10-08, verbatim: "The top section of discuss could use some
beautification".

## Scope

Discuss's top section (`github_conversation_panel`, shown while the panel is
Discuss's page): the threads summary, the scope (whole pull request or one
file), the file choice, the Unread and Unresolved filters, Clear, and an
Overview count's drill-down caption with its × ([[BPR-1089]]). It sits above
the conversations Discuss lists; since [[BPR-1212]] slice 2 it also sits above
the narrowed list beside the full composer and the ⌥R summary.

Designed with `frontend-design`, inside the application's language: minimal
words, icons with tooltips, [[BPR-1083]]'s marks, palette roles only, and the
chip language Conversations' bar already uses ([[BPR-1293]]).

- Every control stays, with an accessible name and description, in the same
  tab order.
- Discuss's behaviour and keys stay: scope, file, filters, Clear, the
  drill-down and its ×, the summary's counts, and Clear's reset of the list.
- Beside the code nothing changes: the drawer's two filter icons stay on its
  tab row.

## Acceptance

- `BPR-1337:inventory`: the card records the before renders (1440x900, light
  and dark, on a fixture with conversations, pull request comments and review
  summaries) and what in the top section looks unpolished.
- `BPR-1337:candidates`: two candidates as offscreen renders, the better one
  chosen with a written rationale.
- `BPR-1337:built`: the chosen candidate is built in Qt; tests cover its
  structure, every control's accessible name and both themes, and a control
  fails against the code before the change where behaviour changes.
- `BPR-1337:owner-look`: the owner, in one short attended session on a frozen
  build, picks between the two candidates' renders and accepts or rejects the
  built one. Pending until answered.

## Inventory (before, `BPR-1337:inventory`)

Drawn with `frontend-design` (loaded first). Rendered offscreen at 1440x900 in
light and dark by `application_discuss_top_public_path` on `overview-labels-v1`
(34 labelled threads, 3 pull request comments and 2 review summaries): the
whole pull request, one file, one file with Unresolved on, an Overview
drill-down, and the narrowed list beside Compose review (⌥R). The record is
[before.png](../../design/BPR-1337-discuss-top/before.png): six top-section
crops (light, then dark) and the narrow list.

What looked unpolished:

- **Two counts that disagree, far apart.** "34 review threads available." sat
  at the top left. "39 conversations, 27 unresolved, 0 new" sat at the foot of
  the list beside the status bar, where nobody looks, and read "1
  conversations". In File scope a long sentence about pathless comments was
  appended to it.
- **A grid that floats.** No control could grow, so the grid centred itself
  (x 310 to 1128 in a 1440 window), out of line with the list's 12-pixel edge.
  Its three columns threw Unread, Unresolved and Clear to three distant points.
- **Plain controls.** The controls were default text buttons ("File scope",
  "All PR conversations", "Unread", "Unresolved", "Clear"). The checked scope
  was a faint grey fill, barely visible in dark. The filters were words,
  although the drawer and Conversations show them as icons.
- **The file menu always showed**, full width, naming a file (README.md) even
  while the whole pull request was in scope.
- **The drill-down caption** was a fourth row, indented to the grid's start,
  with a bare 10-pixel ×.
- **No rhythm.** Rows were 4 pixels apart, with no edge between the controls and
  the list's first heading. Everything was the same weight.

## Candidates (`BPR-1337:candidates`)

Both are real Qt renders of the same build, switched for the comparison only.

- **A, one band**
  ([candidate-a.png](../../design/BPR-1337-discuss-top/candidate-a.png)).
  Conversations' bar transplanted: scope as two segments with the file menu
  inside it, icon chips for Only new (with the new count) and Hide resolved,
  the drill-down as a pill, and the counts with a Clear icon at the right end.
  One 48-pixel row at 1440; it wraps to two rows beside the composer.
- **B, heading over tools**
  ([candidate-b.png](../../design/BPR-1337-discuss-top/candidate-b.png)). A
  heading line ("39 conversations" larger, then "27 unresolved", "5 new" and
  Clear with its word), and under it the scope and labelled chips (Only new,
  Hide resolved). Two rows (about 88 pixels) at 1440; four beside the
  composer.

**Chosen: A.**
- **Height.** Discuss is a reading page, and A gives the list 40 more pixels.
- **One language.** A speaks exactly the language of Conversations' bar and the
  drawer's filter icons, so the same filter looks the same on all three
  surfaces. B's labelled chips would be a third variant.
- **Narrow width.** A stays compact beside the composer, where B grows to four
  rows.
- **Counts.** A's counts read as status at the right. B's heading competes with
  the title row's pull-request name.
- **Minimal words.** A uses only the words the owner asked for, with icons and
  tooltips doing the rest.

## Built (`BPR-1337:built`)

- `discuss_header.{hpp,cpp}` (new, Qt only):
  - `discuss_tool`, a tool button drawn from the palette as a chip, a segment
    or a bare glyph. The drawer's own icon look stays its `native` shape. The
    focus ring shows on keyboard focus only, because a click's ring on
    "Pull request" read as a choice.
  - `discuss_counts`: semibold numbers with quieter words. Its text and
    accessible text are the whole sentence.
  - `discuss_pill`, `discuss_band`, and `discuss_flow_layout`, a wrapping row
    whose trailing item sits at the right end.
- `github_conversation_panel`:
  - The band replaces the grid and the foot summary.
  - The file menu shows only in File scope.
  - The counts read "1 conversation" in the singular.
  - The tab order is reading order: scope, file, filters, ×, Clear, notes.
  - `set_availability_ordinary` lets the workbench say that the availability
    line holds only a thread count. In Discuss that count moves into the
    counts' tooltip. Any other line (threads unavailable, public preview)
    stays shown.
- `conversation_glyphs`:
  - Adds `clear_filters` (a funnel with a cross) and `dismiss` (×).
  - Fixes `dot()`. It read the pen's colour after putting the pen down, so
    every dot (Only new's, Next new's, the target's) was black, and invisible
    in dark themes. This also fixes the drawer's and Conversations' Only new
    icon in dark.
- Kept: every control and its accessible name, the scope's radio keys (one tab
  stop, the arrows choose), Clear's reset of scope, filters and listing, the
  drill-down caption's words, and the drawer's two icons beside the code.
- Spec: "Discuss's top is one band" under the conversation drawer in
  `docs/ux/workbench-specification.md`.

## Verification (macOS arm64, Debug without debug info, offscreen)

- New `application_discuss_top_public_path` (composed, `overview-labels-v1`,
  1440x900):
  - The band is one row under 50 pixels, starting at x 12, with Clear 16
    pixels from the right.
  - The availability count is the counts' tooltip, and the counts say "39
    conversations, 27 unresolved, …".
  - In light and dark the band is drawn in the theme's base, dark in dark.
  - The drill-down is a pill after the filters, in the band's row.
  - Beside the composer the band wraps, and every control stays whole inside
    the narrow list.
  - With `BPR_SETUP_ARTIFACT_ROOT` set, it captures every state in both themes.
- `qt_github_conversation_panel`, three new tests:
  - Named band: every accessible name and tooltip; one row in reading order;
    the file menu only in File, right after it; the availability line moves
    and stays; Tab order with and without a drill-down, and after leaving and
    re-entering Discuss; Right and Left choose the scope.
  - Narrow at 300 pixels: the band wraps, no control is clipped or overlapped,
    and the notes start below the band.
  - Theme: the band follows Base in light and dark, the chosen segment is
    filled, and the drawer's Only new dot is drawn in the glyph's ink.
- Failing-before controls (each applied, built, run and restored):

  | Control | Fails with |
  |---|---|
  | Availability always visible in Discuss | "in Discuss an ordinary availability line moves into the counts' tooltip" |
  | `dot()` reading the pen after `NoPen` | "the Only new dot is drawn in the glyph's ink" |
  | No file-menu visibility on the scope toggle | "the whole pull request is in scope and the file choice is hidden" |
  | Counts added as an ordinary item rather than trailing | "the band starts at the list's edge and ends at its right margin" |
  | No explicit tab chain | "Tab walks scope, file, filters and Clear in reading order, then the notes" |

- Passing on the candidate, 19 focused rows: `architecture_boundaries`;
  `qt_composed_github_scale_{100,125,200}`; `qt_github_conversation_panel`;
  `qt_conversation_cards_view`; `qt_conversation_finder_scale_{100,125,200}`;
  `qt_workbench_focus_order_scale_{100,125,200}`;
  `qt_workbench_conversation_reveal`; `qt_workbench_window_scale_{100,125,200}`;
  `application_setup_destination_public_path`;
  `application_overview_labels_public_path` (the drill-down caption's mark and
  ×); and the new row.
- `coding-format-check` passed (clang-format 23.1.1); `tidy.py` on the three
  changed translation units passed.
- Linux and Windows runs of these rows are owed by the stations.

## Owner look, session v001

One new short session, not the long default one. Two questions, each asking
about a single criterion; neither is self-certified.

- Session `6e2b595ff9c4160456b63e491b0f69c0`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1337-discuss-top-v001`
  (repository root `/Users/jaeger/Documents/GitHub/BetterPullReview`). Both
  candidates were ready at event cursor 7. The review host runs detached.
  Offline page: `review.html` in the state directory.
- `BPR-1337-discuss-top-choice-v001` (revision `design-55dec39a4-v001`): an
  option list, "A: one band" or "B: heading over tools", with the before, A
  and B renders (`docs/design/BPR-1337-discuss-top/`).
- `BPR-1337-discuss-top-built-v001` (revision `frozen-55dec39a4-v001`): Accept
  or Reject on the built band (A) in the frozen build, light and dark (the
  `BPR-1337:owner-look` criterion).
- Frozen root `/Users/jaeger/bpr1337-discuss-top-look-v001.bQqqmd`: the
  `git archive` of `55dec39a4` built there (tests on, app target only, no
  debug info); executable SHA-256 `4bc752801a45…f761`. Offscreen on it,
  `--github-fixture overview-labels-v1 --attended-launch
  --attended-open-pull-request` printed `ATTENDED_PULL_REQUEST_OPENED`.
- Launch: `python3 /Users/jaeger/bpr1337-discuss-top-look-v001.bQqqmd/launch.py`
  opens bpr-fixtures/nested-folders#21 on its Overview; Command-3 shows
  Discuss. It checks the build, its fingerprint and the fixture tree first,
  and reports an early exit.

## Owner decision, session 6e2b595f (2026-10-09)

Session `6e2b595ff9c4160456b63e491b0f69c0`, read from cursor 0 to event 10
(handed back).

| Candidate | Verdict | Note |
|---|---|---|
| `BPR-1337-discuss-top-choice-v001` | Selected "A: one band" (`198433b7`, 01:33:21Z) | (none) |
| `BPR-1337-discuss-top-built-v001` | Accept (`9d0e321b`, 01:31:50Z) | (none) |

The choice confirms candidate A, the one built. The Accept was given on the
frozen build of `55dec39a4` (executable `4bc752801a45…f761`). Together they
satisfy `BPR-1337:owner-look`. The other criteria stand on their recorded
evidence at `55dec39a4`. Linux and Windows runs of the rows remain owed and do
not block; a failure there opens a new card. The card is Done.

Both candidates were released (`resume-development --all`, event 11). The
host was already stopped. The frozen root
`/Users/jaeger/bpr1337-discuss-top-look-v001.bQqqmd` was deleted.

## Next

Done (owner picked A and accepted the built band). The plan, kept as written:
- If the owner picks A and accepts the built band, finish the card.
- If the owner picks B, build B from the recorded candidate description. The
  chips take words, and the counts become a heading with Clear beside it.
  Ask it as `built-v002`.
- A Reject on the built band comes back as a v002 candidate with the note's
  change.
