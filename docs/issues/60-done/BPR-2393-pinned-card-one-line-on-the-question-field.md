---
schema: "implement-rfc.card.v1"
id: "BPR-2393"
title: "The card of a session that read an earlier head stands on the question field with Continue at its head and New session on one line at every drawer width, the same card before a question and after a refused one"
status: "Done"
rfcs: ["RFC-BPR-0130", "RFC-BPR-0096"]
depends_on: ["BPR-2389", "BPR-2392"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/BPR-2393--BPR-2393--ef873bffd344.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"be63eab00","origin":"owner, attended review d3ddb6cf3847c406cf0604509af27568, 2026-11-25, question 05 (the replayed session's card, BPR-2389 and BPR-2392); verbatim below","related_cards":["BPR-2389","BPR-2392","BPR-2379","BPR-2384"],"related_rfcs":["RFC-BPR-0130","RFC-BPR-0096","RFC-BPR-0085"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7"],"owner_step":"attended look at the card on the question field in a frozen build of the integrated revision (see Owner judgement)"}
acceptance: []
---

# BPR-2393: The pinned-revision card stands on the question field, its two choices on one line

## Why

Owner, attended review `d3ddb6cf3847c406cf0604509af27568`, 2026-11-25, question
05 (the card of a replayed session after a push), accepting that it no longer
pans:

> Yes but I think we could make it look better, surely we can find a way to
> fit both options on one line at the narrowest drawer. Also it being at the
> top of the transcript is weird since we auto scroll to the bottom.

### What existed (main ffc8dc891)

- The card was the first item of the scrolled thread. The thread keeps
  itself at its end, so a session opened after a push showed its last turn and
  the question field, and the card that says the next question waits on a
  choice was a scroll away. The send refusal beside the field ("Start a new
  session or continue on the old head, then send") named choices that were
  not in view.
- Its two choices were a platform push button and a bare chip on a
  `wrapping_row`. "Continue at *rev*" and "Start a new session" need 280 px
  side by side even as two chips in the macOS typeface (141 and 133 with 6
  between, measured); the card's column in a 280 px drawer was 232, so below
  about 350 px they stood one under the other.
- The card of a refused question was another card: its own title ("Fixture
  agent stopped before answering", though no agent ran), the refusal's reason
  again in full a few lines from the refused turn that says it, and the two
  choices in the other order (Continue first there, Start a new session first
  before a question), one a platform button, one bare.
- With work in the way of a move, the card held three kinds of button: a
  platform one, two bare ones, and the two above.

## Change

1. **Where it stands** (`agent_tab_view`): the state card has two places. A
   report of what happened (working, finished, a run that stopped, with its
   log) heads the thread as before. The card that asks which revision to go
   on with stands on the question field, in the field's own column under the
   thread and outside it (`dock_banner`, `banner_docked()`): whole, in view,
   wherever the thread is scrolled, directly over the field and its send
   refusal. `reveal_run_failure` scrolls only for a card in the thread.
2. **One line**: the card's two choices are two `quiet_push_button` chips in
   a row that does not wrap, in RFC-BPR-0130's words: **Continue at *rev***,
   then **New session** (History's own name for it; each keeps its tooltip
   saying what it does). In the macOS and Windows typefaces both fit the
   narrowest drawer whole. For a typeface too wide for both,
   `quiet_push_button::set_short_text` (new) lets the first draw "Continue"
   where the row gives it less than its words: its text, accessible name and
   size hint stay "Continue at *rev*", its minimum is the shorter words, and
   the two stay on the line.
3. **One card**: before a question and after a refused one it is the same:
   "This session read *rev*", one sentence, the two choices in the same
   order. Before a question the sentence is "The pull request has moved on.
   Its findings stay at that head." After a refused question it is "Your
   message is kept and is sent when you choose."; the refused turn, the
   thread's last and just above the card, says why in the core's words, and
   the card repeats the reason only when no turn says it. The refused card
   keeps the danger look and its close, and its choices the agent accent
   (each starts a run).
4. **Work in the way of a move**: Rebase onto *rev*, Keep my changes on a
   branch and move and Ask *agent* to reconcile are the same chips, above the
   two (the sentence and the agent's name still take a second line where the
   drawer is narrower than their words).
5. **Docs**: `docs/ux/workbench-specification.md` (the Agent tab's states and
   where the card stands), `docs/ux/shared-action-buttons.md`
   (`set_short_text`; the state card's buttons are chips).

Not in this card:

- A run that stopped for another reason keeps its report (Resend, Show the
  log) at the head of the thread: it holds a log as long as the agent wrote,
  which cannot stand on the field, and the stopped turn at the thread's end
  already carries Resend.
- The tour side chat's own "Head changed" notice, which RFC-BPR-0130 stage 6
  replaces with this card.
- The composed drawer course's two assertions about this card
  (`tests/end_to_end/composed_drawer_fit_public_path.cpp`, another lane's
  file while this was made) are given as a patch to apply at integration:
  the card's first button is Continue at *rev* in both states, its second
  says New session.
- Comments in `src/core/include/bpr/core/chat_history.hpp` and
  `src/app/src/github_application/window_agent_chat.cpp` and
  `window_agent_setup.cpp` still call the choice "Start a new session"; the
  app files are under the sealed manifest and nothing in them changed.

## Acceptance

- AC1. On the card of a session that read an earlier head, Continue at *rev*
  and New session share one line at 280, 300, 340 and 760 px, in the four
  themes and at 100%, 125% and 200%: the first then the second, each one
  line tall, inside the card, neither over the other, and (macOS, Windows)
  each drawing its whole words.
- AC2. The card stands on the question field: under the thread and outside
  any scroll area, above the field, whole in view with the thread scrolled to
  its start and to its end, leaving the thread room to be read.
- AC3. Before a question, after a refused question and with work in the way
  of a move it is one card: the same title, the two choices in the same
  order and words; after a refusal the turn says why and the card says only
  that the message is kept.
- AC4. A chip given shorter words keeps its own as its text and size hint,
  draws the shorter ones whole where a row gives it less, and stays one line
  beside the other.
- AC5. A report of what happened (working, finished, an ordinary stop) still
  heads the thread with its own title, Resend and the log.
- AC6. Public path: the application's own window at its own size
  (1280 by 820), drawer dragged to 280.
- AC7. Format, tidy on touched units.

## Evidence

Revision `be63eab00` (on main ffc8dc891), macOS arm64 Debug, `ctest --test-dir
build/dev -R '^<row>$'`.

- AC1, AC2, AC3: `qt_drawer_surfaces_fit_scale_100`, `_125` and `_200`
  passed. The course opens the card in its three states at 280, 300, 340 and
  760 px in the four themes, with the head the session read named 9696969
  (seven of the widest digits a revision holds), and requires: the card is
  `banner_docked()`, inside no scroll area, under the thread and over the
  question field; its place does not change and it is whole in view with the
  thread scrolled to its start and to its end; the thread keeps 120 px or
  more; the two choices have one top and one height, the first ends before
  the second begins, each is one line tall, and on macOS and Windows neither
  draws its shorter words; the title is "This session read 9696969" and the
  choices "Continue at 9696969" and "New session" in all three states, each
  with a tooltip; after a refused question the card's sentence is exactly
  "Your message is kept and is sent when you choose." and the turn holds "No
  question was sent."; `reveal_run_failure` moves a thread scrolled to its
  start to its end. Run as a look under the native style
  (`BPR_PINNED_CARD_RENDER_ROOT`, Cocoa, QMacStyle, nothing on screen) it
  passed in Light and Dark at 100% and 200% (`look-qt-v2c-0.5.log`,
  `look-qt-v2c-1.log`).
- Negative control: the same course with the card left at the head of the
  thread and the choice's words left "Start a new session" (a temporary
  edit, not committed) reported 42 faults in the native look
  (`look-qt-negative-1.log`): "the card is not on the question field ... is
  inside QScrollArea 'agentConversationScroll', which scrolls it away" in
  all 18 state, width and theme combinations, and "a choice draws its
  shorter words: "Continue at 3333333" is 97 wide and needs 141" in the 12
  at 280 and 300 px. No combination reported the two off one line: with the
  longer words the first drew "Continue" and the pair stayed on its line.
- AC1, AC2, AC3, AC6: `application_agent_invocation_native_pinned_contract`
  passed. In the application's own window at its own size, with the drawer's
  handle dragged to 280 px, the course requires of the card of a replay
  opened after the head moved, and again of the card of a question refused
  with work in the way: on the question field, under the thread and outside
  it, whole in view with the thread at its start and at its end, the two
  choices on one line inside the card, one line tall, and (macOS, Windows)
  in their whole words; "Continue at <earlier head>" first and "New session"
  second in both; New session opening the empty session at once and
  Continue at the earlier head answering there, as before; the refusal's
  reason ("uncommitted edits", the conflicting file's name) in the refused
  turn and the card's sentence only that the message is kept. It saves the
  window as `native-pinned-up-front-280.png` and
  `native-pinned-reconcile-280.png` beside its data root. The other eight
  `application_agent_invocation_*_contract` rows passed.
- AC3, AC5: `qt_workbench_window_scale_100`, `_125` and `_200` passed: the
  refused card on the question field with its title and sentence and the
  reason in the turn; Continue and New session each going to the host and
  resending; an ordinary failure's card back at the head of the thread
  ("... stopped before answering", Resend, not docked); the reconcile
  choices and their order. `qt_review_tour_agent_failure` passed (the tour's
  notice still reveals an ordinary failure's card in the thread).
- AC4: `qt_quiet_buttons` passed with the new course: a chip has no shorter
  words unless given some; given them its size hint is its own words, its
  minimum the shorter ones, its text unchanged, one line tall; in a row
  narrower than both chips it draws the shorter words whole inside its
  padding (less ink than its own words, none at an edge) while the other
  keeps its width and both stay on the line; word wrap or an icon turns it
  off. `qt_button_faces` and `qt_button_faces_cocoa` passed.
- The composed drawer course is another lane's file and is not changed
  here. With the two assertions of
  `/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/composed-course-pinned-v2.patch`
  applied locally (the card's first button is Continue at *rev* in both
  states; its second says New session),
  `application_composed_drawer_fit_scale_100`, `_scale_200` and `_cocoa`
  passed, fifteen states and no fault. Unpatched, those two assertions hold
  the card to the old order and words (not run so).
  One hand run of the course for the renders (Dark, 100%), made while
  another lane ran the same course, stopped at "the question is sent: What
  changed in this push?" with the session that follows the head still shown
  as having read the earlier one; run again alone it passed, as did the
  other five hand runs and the three rows above, twice.
- Around it: `qt_agent_finding_cards_retained`, `qt_agent_tone_colours`,
  `qt_agent_restored_byline`, `qt_side_chat_tab_and_send_scale_100` and
  `_200`, `qt_prompt_macro_chips`, `qt_stick_to_bottom`,
  `qt_drawer_parts_scale_100`, `_125` and `_200`, the eleven
  `application_agent_chat_*_contract` rows, the three
  `application_agent_chat_*_public_path` rows and
  `application_follow_up_public_path` passed. `architecture_boundaries`,
  `test_network_isolation_manifest` and
  `test_network_isolation_structure_contract` passed.
  `scripts/verify_documentation.py --root .` printed `DOCUMENTATION_OK` (with
  the main checkout's `implement-rfc/scripts` on `PYTHONPATH`, which a
  worktree without the MASK-deployed skills needs).
- AC7: `format_check.py` (clang-format 22.1.8) passed on the ten touched C++
  files. `tidy.py --warnings-as-errors` over the seven touched translation
  units reports nothing on a changed line; its findings are in lines this
  card does not change (`agent_tab_view.hpp:477`, `agent_tab_view.cpp:2710`,
  `local_draft_workflow_area.hpp:72`,
  `agent_invocation_composition_public_path.cpp:2330`).

Renders, all under the native look (Cocoa, QMacStyle, windows kept off
screen), in
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/pinned-card-v2-renders/`:

- `side-by-side-dark-200-offered-280-window.png`: the owner's screenshot
  (a 1280 by 820 window at 200%, drawer 280) beside the application's own
  window at that size in the same state, drawn by the composed drawer course
  with the Fixture agent on the same fixture pull request.
- `side-by-side-dark-200-offered-280-drawer.png`: the drawer from the
  owner's screenshot beside the drawer now before a question, after a
  refused question (both the composed course) and with work in the way of
  the move (the native-pinned course's window, `Native fixture 1`).
  `side-by-side-light-200-280-drawer.png` is the three in Light,
  `side-by-side-dark-200-widths.png` the card at 280, 300, 340 and 420 px,
  and `side-by-side-dark-200-refused-280-before-after.png` the refused card
  on main (a 1440 by 960 window) beside it now.
- `cocoa-macos/v2-<light|dark>-<100|200>-agent-pinned-<offered|refused>-<width>.png`
  and `...-280-window.png`: the composed course's own renders, the window at
  1280 by 820 (the course's 1440 by 960 changed for these runs only, with
  the patch above; logs `composed-v2-*.log`).
  `cocoa-macos/v2-<light|dark>-200-contract-<up-front|up-front-280|new-session|reconcile-280>-window.png`:
  the native-pinned course's window (logs `pinned-v2-*.log`).
- `qt-course-cocoa-macos/v2c-<theme>-<scale>-<state>-<width>.png`: the Qt
  course's looks at the widest digits, the refused turn above the card.

## Owner judgement

- The look of the card on the question field is the owner's to accept, in a
  frozen tests-on build of the integrated revision with the Fixture agent,
  on the replay path question 05 of `d3ddb6cf3847c406cf0604509af27568`
  walked. The question is prepared and passes the helper's `check`:
  `/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/pinned-card-v2-renders/attended-candidate-manifest.json`
  (kind build, the owner's screenshot as its reference; its launch command,
  session line and revision are placeholders until the freeze, and its Now
  images are to be taken again from the frozen build). No session was opened
  for it here.
- Choices made here that a Reject's note may change:
  - The card says **New session**, History's name for it, where it said
    "Start a new session": with the longer words the pair needs 280 px and the
    field's whole column in a 280 px drawer is 260.
  - The order is Continue at *rev*, then New session, before a question as
    after a refused one. Before a question the card used to put Start a new
    session first, as the stronger of two unlike buttons; the two are now
    equal chips.
  - After a refused question the card is titled "This session read *rev*",
    not "*agent* stopped before answering", and leaves the reason to the
    refused turn above it.
  - Before a question its sentence is "The pull request has moved on. Its
    findings stay at that head.", two lines at 280 px where the old one took
    three.
  - The three reconcile choices are chips like the other two.
- With work in the way of a move the card on the field is about 230 px tall
  at 280 px (five choices, one of them on two lines). In the application's
  1280 by 820 window the thread above it keeps about 240 px; in a window at
  its least height it keeps little.
- A thread the reader has scrolled up does not follow a new turn (the
  transcript rule of 2026-11-19), so a refused turn can be out of view while
  its card is in view. The card names the revision and both ways on without
  it.
