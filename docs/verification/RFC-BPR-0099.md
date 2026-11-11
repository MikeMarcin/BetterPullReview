# RFC-BPR-0099 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `tour-publishing`
([[BPR-1991]], [[BPR-1992]], [[BPR-1993]]). RFC_BASE_SHA:
`dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`; milestone base `60d427a08`.
Frontend-design was loaded before the Qt surface work, and the design round's
Tour, publishing and Overview frames were the starting point. Debug timings
are nongating; nothing below is a timing claim.

## What was built

- **Head.** The core decides the head once
  (`src/core/include/bpr/core/tour_publication_head.hpp`):
  the table's six states, the stop and to-accept counts, the changes not on
  GitHub, the step count, what keeps a draft from publishing, and the actions
  in order. The Tour tab and Overview both draw it with one widget
  (`src/adapters/qt/include/bpr/qt/tour_publication_surfaces.hpp`).
- **Bands.** The Tour tab is the head, the route in its own scroll area, the
  save line and the agent bar. The drawer's scroll area around the whole tab
  is gone; the route keeps the tab's 280-pixel floor, so a narrow drawer
  narrows the route as before instead of scrolling it sideways.
- **The chosen form.** `choose_tour_plan`
  ([tour_publication_plan.hpp](../../src/app/src/tour_publication_plan.hpp))
  runs the existing per-form planner for one review, then a mixed tour, then
  compatibility comments, stopping at the first that carries every stop, and
  says the choice with the stops that forced it. No form: a refusal banner
  before any write.
- **Steps.** `staged_publication_steps` and `staged_publication_confirmed`
  ([staged_review_publication.hpp](../../src/application/include/bpr/application/staged_review_publication.hpp))
  name the plan's writes in order and count those the record holds GitHub's
  confirmation for. The host hands the projection the record it authorised
  before each write (`review_submission_projection_port::progressed`), so the
  next step reads as sending.
- **Banner.** A part-sent, stopped or restarted publication, and one refused
  before its first write, are said in an error-role banner at the top of the
  panel with the actions that move forward; the status line steps aside.
- **Overview.** Problem and Solution are `markdown_body_view`s; each stop row
  has its file, lines and first sentence and opens its stop in Review; a
  writer's state line and actions are the head's.

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `qt_review_tour_publication_working` (`test_bands`): 24 stops in a 380 × 620 tab; the route scrolls; the head is the top band and the agent bar the bottom one, outside the route, and both stay put and visible at the route's end; no bottom Preview and publish. `qt_tour_click_scroll` holds the route's place through clicks in the new scroll area; `application_review_tour_composition_public_path` (and its 125 % and 200 % runs) show no sideways clipping in wide, narrow and large-font windows | Pass |
| AC02 | `core_tour_publication_head`: every row of the table, the counts and the order of actions. `qt_review_tour_publication_working` (`test_head_states`): each state's text and actions in the tab; Publish… disabled with its tooltip naming what is missing; Publish… opens the tour page and computes the preview; Resume and On GitHub dispatch. `application_review_tour_publication_stop`: the head reads Publishing at step 1 while the first write is held, and after a restart reads stopped at step 2 of 4 with Resume first | Pass |
| AC03 | No mode list (`qt_review_tour_publication_working`, `application_review_tour_publication_mixed`). Fixture courses choose each form: `native` one review; `mixed` "Two stops go in one review. Stop 2 points at an earlier version of the code, so it is sent as a conversation comment."; `file` first compatibility comments while no file comment is offered ("Stop 2 sits outside the lines GitHub shows in this diff"), then a mixed tour with a file comment once one is; `compat` (both stops at an earlier head) "Every stop goes as its own conversation comment. Stops 1 and 2 point at an earlier version of the code, so no single review can carry the tour." Keep it to one review instead lists the stop and its moves and changes nothing (`mixed`, `repairs`) | Pass |
| AC04 | `review_submission_application_host` (`test_publication_stages_and_link`): steps and confirmation counts for one-review, mixed and compatibility records. `qt_review_tour_publication_working`: waiting, sending, sent in order with the head's count equal to the panel's at every step. `application_review_tour_publication_stop`: with the first write held, step 1 is sending and nothing reads as sent. `native` and `compat`: the pressed checklist names the plan's own steps and the head's M equals the panel's | Pass (fixture); live in AC08 |
| AC05 | `qt_review_tour_publication_working`: the banner names the step, the reason, what is and is not on GitHub, with Resume primary and Check status after it; the failed row carries "Refused by GitHub (422)"; no failure text in the placeholder colour; the head is stopped too. `application_review_tour_publication_conflict` and `_description`: banners with Check status; `_stop`: the restarted banner "Publishing stopped at step 2 of 4" with Resume | Pass |
| AC06 | `application_review_tour_publication_unsupported`: on an unqualified connection Preview shows "This connection cannot publish this tour", "Stops 1 and 2 need review comments on their lines, and this connection cannot create them.", "Nothing was sent.", Move those stops and Use another connection, with zero writes and no status line | Pass |
| AC07 | `qt_workbench_window_scale_{100,125,200}` (Overview's Tour tab): Problem renders as Markdown, the row has "item.cpp, lines 4–6" and its first sentence, the row opens its stop in Review's Tour tab, and the state line and actions equal the head's. `core_tour_publication_head`: rows carry one-based lines and the first sentence | Pass |
| AC08 | Live mixed tour on MikeMarcin/BPR-Test | Pending: owner pressed Accept but GitHub holds no published tour; see [AC08](#ac08-the-live-course) |
| AC09 | The owner's look at the Tour tab, the running and stopped panel and Overview on a frozen build (the running and stopped panel as renders of the candidate's own test, since a fixture build cannot publish) | Pass: the owner accepted all four candidates in attended session `5127270e92b1b77ceae8f9df8356c1f6` (frozen build of `bce0d0eba`, handed back 2026-11-10 16:43 UTC): `BPR-1991-tour-tab-head` (decision `2aee010d`), `BPR-1992-publication-running` (`a2474ee8`), `BPR-1992-publication-stopped` (`d5357557`), `BPR-1993-overview-tour` (`918d6411`, note: "It's weird that you didn't set it up to load a fixture that included a tour to look at.", recorded as a rule in `AGENTS.md`) |

## AC08: the live course

Owner's answer, 2026-11-10 (session `26d89bac1f89acd7841ee451d1e5ffd1`): the
owner pressed Accept on `RFC-BPR-0099-AC08-live-mixed-tour` (decision
`ec9e4113`, 19:47:01Z) and on `RFC-BPR-0105-AC08-approve-151` (decision
`e5e3d58e`, 19:47:09Z), both with no note. The public-path evidence those
candidates ask for does not exist on GitHub. Read with `gh` (read only) by the
coordinator and again here: BPR-Test #151 has no reviews (its timeline holds
only MikeMarcin's review request); #152 has no reviews, no review comments
and no conversation comments, and its head is still `6c94a03c0` (the prepared
push was not applied). Neither AC08 is marked passed and neither BPR-2010 nor
BPR-1992 is finished; the coordinator is asking the owner. Host 36810/36892
and root `bpr-live-v002.nnhzt2pb` are left in place.

Prepared 2026-11-10, awaiting the owner. `scripts/freeze_attended_build.py
freeze --live` now freezes a tests-off build (no fixture, its own profile
home in the root, a `--check` that refuses a tests-on image, runs the
self-test and starts it offscreen): `/Users/jaeger/bpr-live-v001.wpvoqim9`
from `42eca211e` (product as `b1d66829d`), executable SHA-256
`39983ab2229e…`, both checks passed. The test pull request is
[BPR-Test #152](https://github.com/MikeMarcin/BPR-Test/pull/152) (head
`6c94a03c0`); its next push is prepared at `929b028f7` on
`bpr-1992/mixed-tour-20261110-next`, applied by the owner after the two stops
are written so stop 2 points at an earlier version. Attended session
`243efdc0c300f68944464fc350c097ed` (state
`/Users/jaeger/.attended-review-state/bpr-live-v001-2026-11-10/state`, event
cursor 6), candidate `RFC-BPR-0099-AC08-live-mixed-tour`. Writing the tour
and pressing Publish are the owner's: `bpr tour publish` accepts no agent or
piped authorization. After the decision, the GitHub result is read with `gh`.

The owner rejected that candidate ("build closes instantly", decision
`723ba6f7`): every start of `bpr-live-v001` printed `STARTUP_FAILED`. Its
freeze had run the launch check before sealing the source; that launch left
the application's update control folder (`.bpr-update-*`) beside the bundle,
the seal made it unwritable, and every later start failed the update
admission lock (`update-lifetime-join`, reported as `unavailable`). Found with
lldb on the frozen executable; removing the folder made the same sealed build
start. `abba53926` seals before the check, and the live launcher's check
refuses a writable or sealed-over build and launches on the owner's own home.
Refrozen as `/Users/jaeger/bpr-live-v002.nnhzt2pb` from `abba53926`
(SHA-256 `cb02049fbf2f…`, checks passed on the root's own home), in a new
session `26d89bac1f89acd7841ee451d1e5ffd1` (state
`/Users/jaeger/.attended-review-state/bpr-live-v002-2026-11-10/state`, cursor
6), same candidate ID and course; #152 still at `6c94a03c0` with no reviews.

Before this round: A tests-on build holds no publication grant (the composition's
`BPR_APPLICATION_TESTING` grant issuer returns none, so no tour write can
leave it), `scripts/freeze_attended_build.py` freezes only tests-on builds,
and the `bpr tour` CLI publishes only after a person types `publish` for the
exact plan. A live publication therefore needs a tests-off build the owner
runs, the same choice RFC-BPR-0105 AC08 is waiting on.

What the fixture courses establish meanwhile: the mixed and file courses
publish with the form the plan chose and still meet the exact write
assertions written for the hand-chosen mixed mode before this change (one
pending review, the standalone comment, cross-kind navigation from returned
identities, one submission, six writes), and the provider path that writes to
GitHub is unchanged.

## Runs

All on the candidate in `build/dev` (macOS, Debug, tests on):

- `core_tour_publication_head`, `core_overview_tour`, `staged_review_session`,
  `review_submission_application_host`;
- `qt_review_tour_publication_working`, `qt_review_tour_steps`,
  `qt_review_tour_agent_help`, `qt_review_tour_agent_setup_call`,
  `qt_tour_click_scroll`, `qt_workbench_window_scale_{100,125,200}`;
- `application_review_tour_publication_{native,compat,stop,description,conflict,unsupported,maintenance,reconciliation,manual,repairs,mixed,file}`
  and `application_review_tour_cli_ordinary_{native,stop}`, together at
  `-j6`;
- `application_review_tour_composition_public_path` and its 125 % and 200 %
  runs; the 74 tests matching tour, overview, staged review and the window;
- `application_composition_source_contract` and
  `application_review_round_trip_composition_contract` after resealing the
  production app source pin.

`application_review_tour_cold_import_public_path` fails at "real preview
never treats loaded code as pending-operation support" on the candidate and on
`60d427a08` alike (the text it looks for is not in the editor's preview on
either). It is a baseline failure, not this milestone's.

`bce0d0eba` then kept the tour page from scrolling as a whole and took the
route's scroll area out of the keyboard focus order (a render showed both);
the Qt tour tests, the click-scroll test, the composition runs, the
`stop`, `mixed`, `repairs` and `native` publication courses and
`qt_workbench_window_scale_100` passed again on it (16 tests).

Earlier runs found and fixed: the head's first test draft that was already
publishable; a route that, in its own scroll area, scrolled sideways at a large
font where the tab used to narrow; `Keep it to one review instead` hiding the
repairs the `repairs` course reaches; and courses that asserted a preview
before the anchored preview had answered.

### Live look v002 abandoned (2026-11-10)

Session `26d89bac1f89acd7841ee451d1e5ffd1` (root `bpr-live-v002.nnhzt2pb`,
frozen from `abba53926`) is abandoned. The build predated the tour-stops
redesign, so it showed the old stop card: its stops could not be accepted and
Publish stayed disabled. The owner: "this is way too many manual steps ... a
freaking mess I cant even accept these comments or try to publish ... Things
are moving around as I click them, buttons packed in like a clown car." The
Accept records `ec9e4113` and `e5e3d58e` from that session are not evidence:
RFC-BPR-0099 AC08 and RFC-BPR-0105 AC08 are **not passed**. Both move to an
agent-run live course on BPR-Test in a later round. BPR-Test is left as is
(the owner pushed `929b028f7` to #152's branch). Host 36810/36892 stopped and
the root removed with `freeze_attended_build.py remove` after the owner's app
(PID 46168) had exited.
