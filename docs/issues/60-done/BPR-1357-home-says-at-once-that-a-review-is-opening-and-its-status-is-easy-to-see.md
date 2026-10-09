---
schema: "implement-rfc.card.v1"
id: "BPR-1357"
title: "Home says at once that a review is opening, and its status is easy to see"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","found_by":"Owner chat, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1357--BPR-1357--9170a6026153.json","related_cards":["BPR-1200","BPR-1342","BPR-0775","BPR-0771","BPR-0776","BPR-0795"],"related_rfcs":["RFC-BPR-0032","RFC-BPR-0026"]}
acceptance: [{"criterion":"BPR-1357:exists","outcome":"pass","revision":"710e06796","evidence":{"kind":"artifact","path":"docs/issues/60-done/BPR-1357-home-says-at-once-that-a-review-is-opening-and-its-status-is-easy-to-see.md","detail":"Card section 'What exists (inventory at 710e06796)': homeOpenNotice 'Loading the pull request.', status bar only, no Cancel, second click silently refused."}},{"criterion":"BPR-1357:core","outcome":"pass","revision":"bf04e3918","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1357-home-says-at-once-that-a-review-is-opening-and-its-status-is-easy-to-see.md","detail":"core_home_open: each read's stage by request identity, open at once, wait on this read's elapse only, failure kept with reason and Retry, ended on load or cancel, foreign and stale observations ignored; core_github_policy and core_github_window_read pass. Windows (66c1cf4c8) passes core_home_open."}},{"criterion":"BPR-1357:clock","outcome":"pass","revision":"bf04e3918","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1357-home-says-at-once-that-a-review-is-opening-and-its-status-is-easy-to-see.md","detail":"application_home_open_host (manual clock): published in the gesture's turn, one wait per read, stale and post-teardown elapses dropped, Cancel and Retry admitted only against the core. Pass on macOS and Windows."}},{"criterion":"BPR-1357:same-turn","outcome":"pass","revision":"bf04e3918","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1357-home-says-at-once-that-a-review-is-opening-and-its-status-is-easy-to-see.md","detail":"application_home_open_feedback_public_path, 3 s fixture delay, light and dark and QT_SCALE_FACTOR=2: the clicked row reads 'Opening #17…' with no event-loop turn after the click; a second click asks for nothing. Pass on macOS and Windows."}},{"criterion":"BPR-1357:cancel","outcome":"pass","revision":"bf04e3918","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1357-home-says-at-once-that-a-review-is-opening-and-its-status-is-easy-to-see.md","detail":"Same course: after 800 ms the row names the step and carries Cancel, announced politely; Cancel stops the read on the wire and the row is itself again on Home."}},{"criterion":"BPR-1357:failure","outcome":"pass","revision":"bf04e3918","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1357-home-says-at-once-that-a-review-is-opening-and-its-status-is-easy-to-see.md","detail":"Same course: #4001 fails with a network error, the row says why in the failure ink with Retry, the line under the bar stays hidden; Retry is a new request in the same turn."}},{"criterion":"BPR-1357:bar","outcome":"pass","revision":"bf04e3918","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1357-home-says-at-once-that-a-review-is-opening-and-its-status-is-easy-to-see.md","detail":"Same course: a pasted .../pull/18 is said in Home's status band at once (busy), with the step and Cancel after the wait; application_setup_destination_public_path asserts the band."}},{"criterion":"BPR-1357:status","outcome":"pass","revision":"bf04e3918","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1357-home-says-at-once-that-a-review-is-opening-and-its-status-is-easy-to-see.md","detail":"Same course: notice and problem in the band under the bar with tone, announced politely; quiet Home hides the band."}},{"criterion":"BPR-1357:renders","outcome":"pass","revision":"bf04e3918","evidence":{"kind":"artifact","path":"docs/issues/60-done/BPR-1357-home-says-at-once-that-a-review-is-opening-and-its-status-is-easy-to-see.md","detail":"Offscreen renders inspected, light and dark at 1100 px and 200%: row opening, waiting, failed, bar waiting, status notice, status problem, idle."}},{"criterion":"BPR-1357:failing-before","outcome":"pass","revision":"bf04e3918","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1357-home-says-at-once-that-a-review-is-opening-and-its-status-is-easy-to-see.md","detail":"Deferring the read-lane observe by QTimer::singleShot(0) fails the same-turn assertion; the code at 710e06796 has no opening (workbench_window::home_open absent, baseline render empty)."}},{"criterion":"BPR-1357:owner-open","outcome":"pass","revision":"bf04e3918","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1357-home-says-at-once-that-a-review-is-opening-and-its-status-is-easy-to-see.md","detail":"Owner Accept 2026-10-09T12:51:57Z: session b64c613f41f0b571a3c226ee1fd586c8, candidate BPR-1357-open-feedback-v001 (fingerprint 69ef0f59df29..., frozen-bf04e3918-v001), decision a367f517-8d85-4738-a680-703d77872b33 at event 7, note verbatim: \"feedback: I don't like a single click on home card opening the pr.\" (taken up on BPR-1364). Handed back at 9, released at 10."}},{"criterion":"BPR-1357:owner-status","outcome":"pass","revision":"bf04e3918","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1357-home-says-at-once-that-a-review-is-opening-and-its-status-is-easy-to-see.md","detail":"Owner Accept, no note, 2026-10-09T12:52:05Z: session b64c613f41f0b571a3c226ee1fd586c8, candidate BPR-1357-status-visibility-v001 (fingerprint 9a5477fe4228..., frozen-bf04e3918-v001), decision 48ed286a-d686-4434-9e3b-9b6a1bd22975 at event 8."}}]
---

# BPR-1357: Home says at once that a review is opening, and its status is easy to see

## Owner notes

Owner, chat, 2026-10-09, verbatim: "I think the general status text on the
home page is still a bit lacking, it's kind of hard to see. There's no obvious
immediate indication you actually tried to open a review if there is real
network latency on the action."

## What exists (inventory at `710e06796`)

- **Opening from Home.** A queue row's click or Return
  (`workbench_window::open_pull_request_item`), a Recently viewed row
  (`dispatch_recent_review`, through the setup-destination or restoration
  path) and the Open bar (`setup_destination_host::submit`) all end in one
  `core::open_pull_request_intent` on the window's read lane. The core
  transition is synchronous: `PendingPullRequest` is set and the status is
  `loading_pull_request` in the gesture's own turn.
- **Where it was said.** Since [[BPR-1200]] a first read stays on Home until
  GitHub has answered twice or shown its overview, so on a slow network the
  window does not move. The only sign was `homeOpenNotice`, a helper-text line
  under the bar reading "Loading the pull request.", far from the row that was
  clicked, and the status bar at the window's bottom edge. Nothing named the
  pull request or the step, nothing offered Cancel (the read host's `cancel`
  existed with no gesture), and a second click was silently refused
  (`pull_request_load_in_progress`).
- **Home's general status.** The session line ("Signed in as … on …"), host
  and profile failures, and every transient message ("Added …", "Opened in
  your browser.") lived only in the status bar: small type at the bottom edge,
  replaced by any transient message. The discovery sentence under the bar
  ("Found at … in 2 saved repositories.") is the queue's own and is unchanged.

## Scope

1. Immediate feedback when opening a review from Home: in the same turn as the
   click, Return or bar submission, the clicked row (or, with no row, Home's
   status band) shows a pending state — a turning mark and "Opening #42…" — and
   a second gesture on it asks for nothing. After a short wait the text names
   the step and offers Cancel; a failure says why on that row with Retry. The
   state comes from the core's read identity (`core::pull_request_read_delta`,
   `core::home_open_state`); the application owns the clock
   (`application::home_open_host`).
2. Home's general status is said in a status band directly under the Open bar,
   at body size in full ink with a tone mark, announced politely, and hidden
   while Home is quiet.
3. A composed course with a several-second fixture transport delay that fails
   before and passes after, with Cancel and failure courses; offscreen renders
   in light and dark.

## Acceptance

- `BPR-1357:exists`: the card records how Home said an opening before.
- `BPR-1357:core`: the GitHub transition names each read's stage by request
  identity; the core's Home-open policy opens at once, waits only on the
  application's elapse for the same read, keeps a failure with its reason and
  Retry, and ends on load or cancel; stale and foreign observations change
  nothing.
- `BPR-1357:clock`: the application arms one wait per read, publishes in the
  gesture's turn, drops an elapse for a read it no longer shows and any after
  the host is gone, and admits Cancel and Retry only against the core's state.
- `BPR-1357:same-turn`: through the composed application with a 3-second
  fixture delay, the clicked row says "Opening #17…" with no event-loop turn
  after the click; a second click asks for nothing.
- `BPR-1357:cancel`: after the wait the row names the step and offers Cancel
  (announced politely); Cancel stops the read on the wire and the row is
  itself again, on Home.
- `BPR-1357:failure`: a failing read says why on its row in the failure ink
  with Retry; Retry asks again in the same turn; the line under the bar does
  not repeat it.
- `BPR-1357:bar`: the Open bar's opening is said in Home's status band at once,
  with the step and Cancel after the wait.
- `BPR-1357:status`: Home's general status (a window problem, a connection
  under way, what was just done) is said in the band under the bar with its
  tone and announced politely; a quiet Home shows no band.
- `BPR-1357:renders`: offscreen renders in light and dark of the opening,
  waiting and failed rows, the bar's waiting band, a notice, a problem and
  idle Home.
- `BPR-1357:failing-before`: the same-turn course fails against a mutation
  that withholds the application's same-turn publication, and against the
  code before the change.
- `BPR-1357:owner-open`: the owner accepts the open feedback on a frozen build
  with a several-second simulated delay (one question).
- `BPR-1357:owner-status`: the owner accepts the status band's visibility on
  that build (one question).

## Implementation

The `frontend-design:frontend-design` skill was loaded before the design.

Design (Qt surface, theme tokens only, no QSS): feedback lands where the hand
was. The open row's second line gives way to a turning mark and "Opening #42…"
at the first line's size in full ink; the action place holds Cancel after the
wait or Retry on a failure; a failure is set in the theme's defect colour
(`state_defect`, held at text strength by every variant) behind a ringed
exclamation. Only the mark moves. The mark is the line's own ink on a faint
ring, not the accent: on Home colour means state and blue is Waiting on
others (a first pass used `Highlight`, the selection fill, and its arc all but
vanished in light). The status band under the bar is a slim rounded band at
body size in full ink; only a problem is tinted (10% of the failure ink, edged
at 45%); busy and notice are neutral. Idle, it is hidden.

- **Core.** `core::pull_request_read_delta` rides every GitHub transition that
  starts or ends a read (pending, loaded, failed with its reason, cancelled;
  activation and sign-out end an abandoned read). `bpr/core/home_open.hpp`
  decides what Home shows: `apply_home_open`, `reconcile_home_open`,
  `home_open_cancellable`, `admit_home_open_cancel`, `home_open_retryable`,
  `home_open_in_flight`, and `home_open_wait_milliseconds` (800).
- **Application.** `application::home_open_host` observes each read-lane
  transition in its own turn, arms one wait per read on an injected clock,
  delivers its elapse by request identity, and publishes
  `application::home_open_projection` when what Home shows changes.
  `cancel_home_open` and `retry_home_open` are admitted against the core.
- **Composition.** The window's read-lane publication feeds the host; the
  clock is a `QTimer::singleShot` on the window's thread, which is the decision
  lane's. Cancel calls the read host's existing `cancel`; Retry is an ordinary
  open through `dispatch_intent`. The profile's GitHub observations reconcile.
- **Qt.** `home_row_delegate` draws the open row (`home_row_open`,
  `draw_home_open_mark`, `draw_home_failure_mark`); `home_list` places the
  Cancel/Retry button on the row that asked, turns the mark, refuses a second
  open of that row and maps Esc to Cancel; `home_status_line` is the band;
  `workbench_window` presents the projection, words the step from the read's
  own progress, guards the queue and Recently viewed gestures, mirrors Home's
  general status into the band and announces each change once.
- **Fixture.** `github_read_delay_fixture` holds a first read's repository and
  pull request answers on the fixture transport (cancellable), and fails a
  named pull request's first reads with a network error after the wait.
  `--fixture-read-delay-ms`, `--fixture-read-fail-number` and
  `--attended-home-look` drive it from the tests-on application.
- Specification: Home's "While a row opens" and "Home's status band" in
  `docs/ux/workbench-specification.md`.

## Verification (macOS, `build/dev` Debug, offscreen, 2026-10-09)

- `core_home_open` (new): the transition names each read's stage and request
  (pending, loaded, failed with `network_unavailable`, cancelled, and the read
  sign-out abandons); opening at once, waiting only on this read's elapse,
  failure kept with its reason and Retry (none for a refused name), cleared on
  load or cancel; foreign reads, refreshes and late waits change nothing;
  reconciliation ends an opening the lane dropped; Cancel admitted only for
  the lane's pending read. `core_github_policy` and `core_github_window_read`
  pass with the new delta.
- `application_home_open_host` (new, manual clock): the opening is published
  in the gesture's turn without Cancel; one wait of the core's length per read;
  waiting with Cancel on its elapse; a cancelled read leaves Home quiet; a wait
  for a replaced or ended read, or after the host is gone, does nothing; Retry
  names the same pull request and refuses another read's request.
- `application_home_open_feedback_public_path` (new), composed application,
  `profile_window_restoration_v1` with personal discovery, 3 s fixture delay,
  light then dark:
  - **same turn**: a pointer click on `#17` with no event-loop turn after it:
    the core's opening has reached the window, the clicked row is the open
    row, its lines read "Opening #17…", its own list carries it, the band adds
    nothing; a second click asks for nothing (same request);
  - **wait and Cancel**: after the 800 ms wait the row reads "Opening #17:
    asking GitHub for the pull request…" and carries Cancel ("Cancel opening
    octo-org/review-repository#17"); the step and "Cancel is available." are
    announced politely; Cancel stops the read on the wire (the fixture saw the
    stop) and the window stays on Home with the row itself again;
  - **failure and Retry**: `#4001`'s reads fail with a network error after the
    wait; the row reads "Couldn't open #4001. The GitHub host could not be
    reached. Check the network connection and try again." with Retry; the
    line under the bar stays hidden; Retry is a new request in the same turn
    and fails again;
  - **bar**: a pasted `…/pull/18` is said in Home's status band at once
    ("Opening octo-org/review-repository#18…", busy), with the step and Cancel
    after the wait; Cancel leaves the band quiet;
  - **status**: "Added octo-org/review-repository." in the band while the
    status bar says it (notice); agent workspaces unavailable in the band
    (problem) and announced; quiet Home hides the band;
  - the slow read then opens `#17` and leaves nothing for Home to say.
  The same course passes at `QT_SCALE_FACTOR=2`.
- Renders inspected (light and dark, 1100 px; and 200%): row opening, row
  waiting, row failed, bar waiting, status notice, status problem, idle. The
  first pass drew the turning arc in `Highlight` (the selection fill): it all
  but vanished in light and, being blue, read as Waiting on others; the arc
  is now the line's own ink on a faint ring and only a problem is tinted.
- `application_setup_destination_public_path`: its real pending pasted-link
  opening now asserts the band's "Opening octo-org/review-repository#17" with
  the busy tone (was "Loading the pull request.").
- Also passing: `qt_home_list_scale_{100,125,200}`,
  `qt_workbench_window_scale_{100,125,200}`, `qt_composed_github_scale_*`,
  `qt_composed_home_refusal_*`, `qt_workbench_open_review_from_home_scale_*`,
  `qt_workbench_open_on_github_scale_*`, `qt_anchored_tooltips`,
  `application_personal_discovery_public_path`,
  `application_refused_first_read_public_path`,
  `application_profile_window_review_restoration_public_path`,
  `application_profile_multi_window_public_path`,
  `application_github_window_read_host`, `application_github_application_host`,
  `application_composition_contract`,
  `application_review_round_trip_composition_contract` (resealed: full
  application and production manifest digests), `architecture_boundaries`,
  `core_dependency_boundary`, `core_pull_request_refusal`, the seven
  `*network_isolation*` rows (the new row is registered `WINDOWS_UNCONTAINED`
  and pinned `dos-volume-name` by the personal-discovery shape, not yet
  measured contained on Windows);
  `coding-format-check` (clang-format 22.1.8), `git diff --check`,
  `verify_documentation.py` (`DOCUMENTATION_OK`). clang-tidy 22 on the new and
  changed small sources is clean apart from findings on untouched lines
  (`home_list.hpp` template, `workbench_window.cpp:12915`,
  `github_public_path_fixture.cpp:776`).

**Failing-before controls.**

- Withholding the application's same-turn publication (the read-lane observe
  deferred by one `QTimer::singleShot(0)`): the composed course fails at "in
  the same turn as the click, the core's opening reaches the window";
  restored byte for byte.
- The code before the change has no opening at all: the baseline render of
  Home at `710e06796` shows nothing on the row, and the course's first
  same-turn assertion has nothing to read (`workbench_window::home_open`
  does not exist there).

## Owner look — prepared 2026-10-09

- Landed on main as `bf04e3918`. Frozen build of `bf04e3918` (source export
  and tests-on build inside
  `/Users/jaeger/.mask/attended-review/bpr1357-home-open-v001.7f34thz4`;
  `better_pull_review` SHA-256 `d6f52871beb478fcb7bef70b44883f06fe102a33f7e904cea43b635dd369cd6e`).
  Started with
  `python3 /Users/jaeger/.mask/attended-review/bpr1357-home-open-v001.7f34thz4/launch.py`:
  the fixture account signs in, opens `#17` once, saves its repository and
  returns to Home with two rows (`#17` under Needs me, `#4001` under Waiting on
  others) and `#17` under Recently viewed; then every first read waits 4 s
  per answer and `#4001` always fails (`--attended-home-look
  --fixture-read-delay-ms 4000 --fixture-read-fail-number 4001`). The
  launcher's own offscreen check reached
  `ATTENDED_HOME_SHOWN:rows=2:delay=4000ms`.
- New short session `b64c613f41f0b571a3c226ee1fd586c8`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1357-home-open-v001/state`,
  host detached. Two Accept/Reject candidates, ready at event cursor 6:
  `BPR-1357-open-feedback-v001` (`BPR-1357:owner-open`) and
  `BPR-1357-status-visibility-v001` (`BPR-1357:owner-status`).
- Next: on hand-back, read `status --after 6`. Each Accept closes its
  criterion; a Reject's note is the next round's change.

## Owner verdict, session v001

The owner handed session `b64c613f41f0b571a3c226ee1fd586c8` back at event 9,
read from cursor 0. Both candidates are on the frozen build of `bf04e3918`
(`frozen-bf04e3918-v001`, `better_pull_review` SHA-256 `d6f52871beb478fc…`):

- `BPR-1357-open-feedback-v001` (`BPR-1357:owner-open`, fingerprint
  `69ef0f59df29…`): **Accept**, 2026-10-09T12:51:57Z, decision
  `a367f517-8d85-4738-a680-703d77872b33` at event 7. The note, verbatim:

  > feedback: I don't like a single click on home card opening the pr.

- `BPR-1357-status-visibility-v001` (`BPR-1357:owner-status`, fingerprint
  `9a5477fe4228…`): **Accept**, 2026-10-09T12:52:05Z, decision
  `48ed286a-d686-4434-9e3b-9b6a1bd22975` at event 8, no note.

Both owner criteria pass; the others stand on the verification above and the
Windows station run below. The card is Done. The note does not reject this
card's feedback: it asks to change the gesture that opens a review from a
Home row, and [[BPR-1364]] takes it up (a single click selects a row;
double-click or Return opens it). This card changes no code for it.

The host (pid 27910) was stopped and both candidates released with
`resume-development --all` at event 10. The frozen root
`/Users/jaeger/.mask/attended-review/bpr1357-home-open-v001.7f34thz4` was
removed with `scripts/freeze_attended_build.py remove`. The session's state
folder keeps the record.

## Windows station run (bpr-main-win, 2026-10-09)

At `66c1cf4c8`, Windows 11, MSVC 19.50, `build/windows-dev-tour-cli` (Debug), offscreen,
network-isolated runner, `-j4`:

- **Pass:** `core_home_open`, `application_home_open_host`,
  `application_home_open_feedback_public_path`, `test_network_isolation_manifest`
  and `architecture_boundaries`.
- **Pin measured:** `application_home_open_feedback_public_path` passes as
  registered. Contained through the sandbox launcher it exits 3 with "private
  desktop root under the canonical storage policy" before any course step,
  as its `dos-volume-name` pin predicted. The pin's comment now says measured.
