---
schema: "implement-rfc.card.v1"
id: "BPR-2216"
title: "Opening a pull request from Home is responsive: its page at once, Return and the Active review while it loads"
status: "Parked"
rfcs: []
depends_on: []
parked_on: "human-review"
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"kind":"ux","found_by":"Owner chat, 2026-11-16","reservation":"docs/issues/_reservations/BPR-2216--BPR-2216--0ff5959e3c1a.json","related_cards":["BPR-1200","BPR-1357","BPR-1365","BPR-1599","BPR-1842","BPR-1843","BPR-2180"],"related_rfcs":["RFC-BPR-0032","RFC-BPR-0026"]}
---

# BPR-2216: Opening a pull request from Home is responsive: its page at once, Return and the Active review while it loads

## Owner notes

Owner, chat, 2026-11-16, verbatim: "Right now a lot of things are slow to
react when I open a pull request from home. The pull request window doesn't
open very quickly. If I click home from a pull request that's not finished
loading it doesn't change it to the return button and it doesn't have the
active pull request near the top of home. It would be nice to make this more
responsive."

Owner, chat, 2026-11-16, verbatim, added to this card: "Seems like overview
has a loading status text that looks almost 1:1 copy of the status bar's
loading status text just below it." Then: "Seems home has a similar thing."
The owner decided: remove the in-page loading status text and keep only the
status bar's.

## Diagnosis

Read on the lane at `00f42d5d3` (code paths, then the composed course below):

- **The page waits on Home.** Since [[BPR-1200]] a first read stays on Home
  until GitHub has answered twice or shown its overview
  (`workbench_window::update_home_landing`, `unconfirmed`). The gesture's own
  turn only marks the row "Opening #17…" ([[BPR-1357]]); the window changes
  page when the first answers arrive, so on a slow network the click looks
  ignored for as long as GitHub takes. Nothing is computed synchronously that
  blocks: the read runs on the host's lane, off the UI thread.
- **Return and the Active review wait for the whole read.** The Home
  button's Return (`present_home_action`), the Active review card
  (`landing_state::back_to_review`) and the queue's reviewing mark
  (`open_queue_ref`) all required `HasGithubPullRequest`, which is set only
  when the accepted review is adopted at the read's end. The core already
  knew the opening from the request on (`core::home_open_state`), but nothing
  asked it.
- **The loading sentence twice.** The Overview's sections said "Still waiting
  for GitHub: the changed files, 3 pages so far." while the status bar said
  "Loading files… 3 pages received." (`set_read_waiting` from
  `read_stage_words`), and the review's drawers said the same in their
  plates.

## Acceptance

Timing is measured in an optimized Release build only. Debug timings are
permanently nongating (AGENTS.md, "Debug timings").

- **BPR-2216:page-at-once.** Opening a pull request from a row Home lists
  (the queue's double-click or Return, or a Recently viewed row) shows that
  pull request's page in the gesture's own event-loop turn, before GitHub has
  answered anything: the window leaves Home for the pull request's Overview,
  its title row names the pull request, and the page says it is loading,
  filling in progressively as the read's parts arrive ([[BPR-1842]],
  [[BPR-2180]]), never a frozen or blank wait. Measurably, in Release on the
  fixture GitHub server with the read's first answers held for seconds: the
  first frame of that page is painted within 100 ms of the double-click, and
  no single event-loop turn between the click and that frame takes longer
  than 100 ms.
- **BPR-2216:return-while-loading.** Home pressed while that pull request is
  still loading shows Home with the toolbar's Home button as the way back to
  it: "Return to #N …", the return glyph, unchecked, and pressing it returns
  to the loading page, exactly as it does for a loaded review
  ([[BPR-1843]]; the owner's request of 2026-11-11).
- **BPR-2216:active-while-loading.** Home pressed while that pull request is
  still loading lists it near the top as the active pull request: the Active
  review card names it (owner/name#N and its title) with Back to the review,
  and its queue row carries the reviewing mark ([[BPR-1365]]), exactly as
  for a loaded review.
- **BPR-2216:said-once.** While a pull request loads, and while Home finds
  my pull requests, what is loading is said once, by the status bar: no
  Overview section, review pane or Home surface repeats the status bar's
  loading sentence or the read's step. A composed course reads every visible
  sentence of the window and fails if one outside the status bar says the
  status bar's sentence again.
- **BPR-2216:decided-in-core.** Whether a pull request is the window's active,
  returnable one is decided from the open request the core accepted (its
  request identity), not from the read's completion; the host does the read
  and its effects off the UI thread; Qt only presents the decision.
- **BPR-2216:courses.** A composed public-path course on the fixture GitHub
  server with a slow first read checks each criterion above through the real
  window offscreen, and fails on the code before the change.
- **BPR-2216:owner-look.** Owner Accept of a frozen fixture-agent build that
  opens a slow-loading pull request from Home, one question per criterion,
  including the single loading sentence on the Overview and on Home.

## Change

- **Core** (`bpr/core/home_open.hpp`): `decide_home_open_presence` decides,
  from the read's request on, whether an opening shows its page and is the
  window's active pull request. A read in flight that a Home row named
  (`home_open_listed`: the queue or Recently viewed) is both at once; a name
  typed into Home's bar keeps [[BPR-1200]]'s wait for GitHub's first answers;
  a failed read, or none, is neither.
- **Qt** presents it: the page in the gesture's turn, named from the row
  (`name_listed_open`); Home's Return, the Active review card and the queue
  row's reviewing mark (`reviewing_queue_ref`) while it loads; the review,
  once loaded, arrives behind Home when Home was chosen meanwhile. An opening
  that fails or is cancelled gives its card up without holding its place.
- **Host** (`window_navigation.cpp`): the Home gesture no longer cancels a
  read opened from a Home row (`listed_opening`); a startup restoration is
  cancelled by Home as before.
- **One loading sentence.** The Overview's sections and the review's panes
  no longer say "Still waiting for GitHub: …". While Home refreshes, its
  discovery line is hidden, its Refresh button shows no "Finding my pull
  requests…" note, and its listed rows carry no "Refreshing my pull
  requests…" plate at the list's foot. The status bar says both
  ("Loading changed files… N pages received.", "Refreshing…"). The Updates
  line's readiness while a pull request loads is unchanged and is asked about
  in the look. The empty list's
  own placeholder ("Finding my pull requests in …" in place of rows that do
  not exist yet) stays: it stands where content will be, not under the bar.
- Courses changed to the new behaviour: BPR-1357's (`home_open_feedback`;
  the row's opening is read on Home after Home is chosen) and the restoration
  course's `auth-home` (Home over a resumed reading keeps its read and offers
  Return), `pending_veil` and `pending_states` (no section repeats the read's
  step), `qt_workbench_home_discovery` (no line under Home while it refreshes).

## Verification (Debug, lane, 2026-11-16)

- `core_home_open`, `qt_pending_veil`, `qt_workbench_home_discovery_scale_100`,
  `application_home_open_responsive_public_path` (new),
  `application_home_open_feedback_public_path` and its `_cli_` variant,
  `application_pending_states_public_path`,
  `application_profile_window_review_restoration_public_path`,
  `application_document_tabs_restoration_public_path`,
  `application_close_review`, `application_home_return_public_path`,
  `application_personal_discovery_public_path`,
  `application_profile_multi_window_public_path`,
  `application_remembered_connection_public_path`,
  `application_returning_review_latest_public_path`: pass.
- `application_setup_destination_public_path` fails at "composed source
  layout uses the selected code-font tab width, not Qt's default", a source
  view font assertion this change does not touch; not investigated here.
- The new course, Debug (nongating): the window leaves Home in the
  double-click's own turn (18 ms), the page's first frame is painted 21 ms
  after it, longest turn 18 ms, while GitHub's first answers are held 4 s.
  Its Release assertions (100 ms) run only under `NDEBUG` and await a
  Release run.
- Before the change, by the code at `00f42d5d3`: Return and the Active
  review card required `HasGithubPullRequest`, and the page waited on
  `unconfirmed`; each criterion of the new course asserts the opposite.
  A separate failing-before run was not taken (the machine's build load).

## Owner look — prepared 2026-11-16

Frozen build `/Users/jaeger/bpr2216-home-open-v002.s_29sqy1` from `648aa4f30`
(fixture `github-com`, `--attended-home-look --fixture-read-delay-ms 4000
--fixture-page-delay-ms 700 --fixture-discovery-delay-ms 3000`; Fixture agent
only; the launcher's `--check` reached `ATTENDED_HOME_SHOWN:rows=3:delay=4000ms`;
executable SHA-256 `edf58a8ab8282fcd08f00df14920d473d78fd6a051e26c0371b64853d1d7122c`).
The look starts on Home with #17 open behind it: its steps close #17 first.
Native Cocoa dark renders from the course in `/Users/jaeger/bpr2216-look-renders-v002`.

Attended session `b18827b241a9a4112cfdc8e81bdaa3b2`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261116-bpr2216-home-open-v002/state`,
served on the tailnet; ready at cursor 15 with five protected Accept/Reject
questions: `BPR-2216-page-at-once`, `BPR-2216-return-while-loading`,
`BPR-2216-active-while-loading`, `BPR-2216-overview-said-once`,
`BPR-2216-home-said-once`. The URL stays in the session state.

Next: read `status --after 15`. Accept on all five closes `BPR-2216:owner-look`;
a Reject's note is the next round's change. The Release timing assertions
(`NDEBUG`) still await a Release run of the course.

## Owner verdicts, look v002 (2026-11-17T02:46Z)

Session `b18827b241a9a4112cfdc8e81bdaa3b2`, read at cursor 34, handed back:

- `BPR-2216-page-at-once`: Accept, no note (decision `157edf61`).
- `BPR-2216-return-while-loading`: Accept, no note (decision `2c62e99e`).
- `BPR-2216-active-while-loading`: Accept, no note (decision `534621a9`).
- `BPR-2216-overview-said-once`: Accept, no note (decision `1bd603fa`).
- `BPR-2216-home-said-once`: **Reject** (decision `320c2dd9`), verbatim:
  "No, there are still 2 differnet places status messages appear. They just
  toggle back and forth now which is terrible." Two attachments: Home with a
  framed box under Review Cycles reading "Find my pull requests in saved
  repositories again; this never changes GitHub." and no status bar; Home
  with no box and the status bar reading "Signed in as octo-reviewer on
  github.com.".

The owner also asked, before answering, to remove the Updates line's naming
of the read and an empty Home's "Finding my pull requests…" sentence.

## Round 3 (look v003)

- **Why the box toggled.** [[BPR-1357]]'s status band under Home's lists
  repeated the status bar's current message (a hovered action's status tip,
  what was just done, a problem) and, while it did, the status bar stopped
  painting (`home_status_echoes_bar` in `eventFilter`). Moving the pointer on
  and off the Refresh button flipped the sentence between the two places.
- **Change.** Home has one status place, the status bar, always shown. The
  band no longer carries general status (`update_home_status`); everything it
  said is already in the status bar's own line (session, connection,
  problems) or message. The band keeps only an opening typed into Home's bar,
  with its Cancel and Retry.
- The Updates line says nothing while the pull request itself is read; the
  status bar's line keeps the read's words (`ReadWording`).
- An empty Home waits in the shape of its rows with no sentence while it
  finds my pull requests.
- Courses: the new course checks the Updates line, a hovered action's help
  on Home and the band; BPR-1357's course, `pending_states` and the
  workbench epoch-row case follow.
- `application_setup_destination_public_path`'s tab-width failure is not
  this lane's: it passed alone with BPR-2216's product changes reversed and
  four times out of four with them in place. Carded as [[BPR-2228]].

## Release timing (2026-11-17)

`application_home_open_responsive_public_path` in an optimized Release tree
(`cmake --preset app-release -B build/app-release-tests -DBUILD_TESTING=ON`,
jobserver), three runs at `5d809b68d`, all passing with the `NDEBUG` 100 ms
assertions active: the double-click's turn took 14, 16 and 16 ms; the page's
first frame was painted 20, 19 and 19 ms after the double-click; the longest
turn on the way was 14, 16 and 16 ms. `BPR-2216:page-at-once`'s measure holds.

## Owner look v003 — prepared 2026-11-17

Look v002's host stopped and its root removed with the freeze tool. Frozen
build `/Users/jaeger/bpr2216-home-open-v003.ytamgwof` from `85111070d` (the
lane's HEAD, containing round 3 at `5d809b68d`; same fixture and delays as
v002; `--check` reached `ATTENDED_HOME_SHOWN:rows=3:delay=4000ms`; executable
SHA-256 `56e17aaf794efef4b50563cb676a7df5e0771cd1cc12926c999561473440c0e3`).
Native renders in `/Users/jaeger/bpr2216-look-renders-v003`.

Attended session `147392b2f24dd26b6a3d12da049825d3`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261117-bpr2216-home-open-v003/state`,
served on the tailnet, ready at cursor 9 with three protected Accept/Reject
questions: `BPR-2216-regression` (the four criteria accepted in v002, one
check), `BPR-2216-updates-line` (the owner's added ask) and
`BPR-2216-home-one-status` (the v002 Reject, re-asked). The URL stays in the
session state.

Next: read `status --after 9`. Accept on all three closes `BPR-2216:owner-look`.

## Owner verdicts, look v003 (2026-11-17T03:39Z)

Session `147392b2f24dd26b6a3d12da049825d3`, read at cursor 22, handed back:

- `BPR-2216-home-one-status`: Accept, no note (decision `397234e6`).
- `BPR-2216-updates-line`: Accept, no note (decision `ba157ef6`).
- `BPR-2216-regression`: Accept (decision `157870ae`) with the note,
  verbatim: "BUG: The heck now this double status came back on the review
  screen." Attachment: on the review page a floating plate "Loading comments,
  reviews and checks…" with a turning mark, above the status bar's "Loading
  pull request details… 10 pages received".

## Round 4 (look v004)

- **The plate.** It was the Overview file list's partial plate
  (`Veils.Files`, [[BPR-1842]]), shown once the file rows arrive and the
  rest of the read continues; the large fixture pull request (ten pages) held
  it long enough to see. The same kind of sentence stood in the Overview's
  status column plate ("Loading reviewers, checks, merge rules and review
  progress…"), each section's first-load words ("Loading checks…"), the
  headings ("Files — 2 reported; catalogue loading…", "Review state —
  loading…"), the summary ("Provisional metadata; review data is still
  loading."), the title before GitHub names it ("Loading pull request…"), and
  the review's panes and drawers ("Loading comments…", "Loading threads…",
  "Files open once the review has loaded…", "Refreshing remaining details…").
- **Change.** While the pull request is read, every waiting pane of the
  review screen waits in the shape of its content without a sentence; a
  partial or slow-refresh plate with nothing to say is not drawn
  (`pending_veil::plate_rect`). The status bar alone says what loads. Kept:
  a file the reviewer chose while it loads ("Opening review.cpp once the
  review has loaded…", the gesture's answer) and every failure.
- **Course.** `application_home_open_responsive_public_path` sweeps Overview,
  Review, Discuss and Threads during the slow read: no busy pane in the
  window (drawers included, shown or not) has words, and no visible label or
  button outside the status bar says loading, refreshing, still waiting,
  pages received or catalogue. `qt_pending_veil`,
  `application_progressive_open_public_path` and
  `application_refresh_current_file_public_path` follow.
- Verification (Debug): `application_home_open_responsive_public_path`,
  `qt_pending_veil`, `qt_pull_request_overview_page`,
  `qt_refresh_feedback_window`, `application_home_open_feedback_public_path`,
  `application_refresh_feedback_public_path`,
  `application_refresh_current_file_public_path`,
  `application_progressive_open_public_path` and
  `application_pending_states_public_path` pass. The last failed once at
  "the clicked file arrives" (its 90 s wait for the comparison) in a run
  beside eight others, then passed alone twice; it checks no wording.
- Not changed: the merge-rules detail text ("still loading") is the merge
  assessment's own content, not a status line; after the read, the Updates
  line's comparison-preparation words are outside this round.

## Owner look v004 — prepared 2026-11-17

Look v003's host stopped and its root removed with the freeze tool. Frozen
build `/Users/jaeger/bpr2216-home-open-v004.jbzyttil` from `e5e50701f`
(fixture `github-com`, first reads 4 s, each later part 1.5 s, Home's
searches 3 s; `--check` reached `ATTENDED_HOME_SHOWN:rows=3:delay=4000ms`;
executable SHA-256 `7e22632e9c27ae8d613688fef171c653abfd7e51ebbbe2050e419004b993414e`).
Native renders of every mode while loading in
`/Users/jaeger/bpr2216-look-renders-v004`.

Attended session `976501d361fd7566b5d41bc53ea3e260`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261117-bpr2216-review-loading-v004/state`,
served on the tailnet, ready at cursor 3 with one protected Accept/Reject
question, `BPR-2216-review-loading-once`. The URL stays in the session state.

Next: read `status --after 3`. Accept closes `BPR-2216:owner-look` and the card.
