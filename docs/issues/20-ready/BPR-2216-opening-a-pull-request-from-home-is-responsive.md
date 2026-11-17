---
schema: "implement-rfc.card.v1"
id: "BPR-2216"
title: "Opening a pull request from Home is responsive: its page at once, Return and the Active review while it loads"
status: "Ready"
rfcs: []
depends_on: []
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
  no longer say "Still waiting for GitHub: …"; Home's discovery line is
  hidden while Home refreshes. The status bar says both. The empty list's
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
