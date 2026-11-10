# RFC-BPR-0106 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `tour-stops`
([[BPR-2013]], [[BPR-2014]]). RFC_BASE_SHA:
`dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`. Frontend-design was loaded before
the Qt surface work; the accepted stop mockup
([stop.html](../design/v0-5-0-targets-v1/stop.html)) is the design. Debug
timings are nongating; nothing below is a timing claim.

## Which arrival paths failed to show the code

Read from the code (the owner's extra click was not reproduced in a running
window): choosing a different row (mouse or keys), Previous and Next, a link
to another stop and Start tour all changed the outline's current row, whose
handler asks for the stop's code. Three paths changed no row and showed no
code: reopening the current stop after its card was collapsed (a click on its
row or Space, `toggle_stop_card`), Next changed stop when the changed stop was
already current (`setCurrentRow` to the same row), and choosing the stop whose
card was open, which left Show in code as the only way back. The first is the
likeliest extra click the owner met: the ring collapses the card, and the row
reopened it without its code. All three now ask for the code; the file-and-
lines line is the way back. `qt_tour_stop_acceptance_card` now pins every path.

## How acceptance is decided

The core records, on Accept, Save of an edit, Accept on the stop's own
proposal, or the terminal walk's Ready (which shows the code to the person
answering it): the stop's title and text, its publication source, the head,
and each range of code at that head, namely its own lines (as authored at the
head or as mapped there) and every range a reference in its text names that
resolves to exactly one changed file (`find_agent_references`, a bare "line
N" naming the stop's own file). The state is decided from that record and the
tour's current head (`decide_tour_stop_acceptance`):

- no record, or text the author did not accept: Not accepted yet;
- a different publication source: Changed since accepted (moved);
- a range not at the current head whose comparison found it changed or gone:
  Changed since accepted, naming its lines or the reference as written;
- a range not yet compared at a new head: Changed since accepted ("not
  compared yet") until the host's comparison arrives, so a push never leaves
  a stop green on evidence it has not read;
- otherwise Accepted.

A range at an earlier commit than the head it was accepted at (a historical
stop) is pinned bytes no push can change, and is not recorded. References
"before the change" name the base and are not recorded. On a new head the
window compares each recorded range's commit with the head, one file per
read, through the same realignment and materialization the tour's repair
preview uses (`check_tour_material`), and `map_tour_revision` stores each
verdict. Lines the author chose with Use selected lines stay chosen across a
head observation; before, a push reset them.

Publication, update publication and the head's count require every stop
Accepted, with a title, an explanation and a source; Ready, the generated
stop's completion and the explanation's reaffirmation no longer gate it.
Drafts saved before this change (codec version 10 and earlier) carry across:
a stop Ready and reaffirmed at the current head is Accepted on its own lines,
every other stop Not accepted yet. No agent can accept: the owner channel
refuses `accept` and `unaccept` from a command client (their kinds follow
`visit`), and a relayed `bpr tour ready` accepts nothing.

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `qt_tour_stop_acceptance_card`: choosing another row, the stop keys, a link to the current and to another stop (`go_to_stop`), reopening the current stop after its ring closed it, and the file-and-lines link each ask for the stop's code; no Show in code exists. Next changed stop goes through `go_to_stop`. `application_review_tour_composition_public_path` and `application_review_tour_cold_import_public_path`: the code shown after an arrival is the stop's exact admitted range | Pass |
| AC02 | `core_tour_stop_acceptance` (`test_states_and_rings`): new stop, accepted, unchecked at a new head, own lines changed, a linked range gone, unchanged at a new head, moved, and text not accepted each yield their state and reason. `qt_tour_stop_acceptance_card`: each ring and row's accessible text draws its stop's state | Pass |
| AC03 | `core_tour_stop_acceptance` (`test_what_accepts`): Accept and Save accept; opening (visit) and a relayed Ready do not; an edit never writes what was accepted. `qt_tour_stop_acceptance_card`: Accept on the current stop accepts it and stays; Save accepts; opening the stop and its ring accept nothing. `qt_tour_chat_in_tab`: Accept on the stop's own proposal accepts it once the tour holds its text; Accept all accepts no stop. `application_review_tour_writing`: a hidden Accept accepts nothing | Pass |
| AC04 | `core_diff_semantics` (`test_tour_acceptance_after_push`): three stops accepted at one head, a real comparison of a push that changes one stop's lines and the lines another links to: exactly those two are Changed since accepted (lines, and `src/a.cpp:5-6`), the third stays Accepted, and Accept at the new head turns the first green. Through the window: `application_review_tour_publication_reconciliation` accepts a published stop, moves the pull request to its second head and checks the stop is Changed since accepted, not compared yet, never green; that fixture's second head is a handoff with no readable content, so no composed course yet shows a comparison of real changed code (see below) | Pass at the core; composed comparison not shown |
| AC05 | `core_tour_stop_acceptance` (`test_publication`): publication refused while any stop is yellow, naming `not_accepted`; allowed once every stop is accepted; Ready alone publishes nothing. The publication courses publish all-accepted tours with their write assertions unchanged (below) | Pass |
| AC06 | `qt_tour_stop_acceptance_card`: at rest only the title, ring, file-and-lines link, text, Edit and Accept with its reason while yellow; Use selected lines, Enhance, Remove, Cancel and Save only while edited; no Ready, next, Head changed or Show in code; the stop's context menu holds Edit, Open side chat, Insert a stop after, Duplicate, Merge with next, Move up, Move down, Mark not accepted and Remove stop. `qt_tour_stop_card_cleanup`, `qt_workbench_window_scale_*`: related sources stay reachable while edited | Pass |
| AC07 | `qt_tour_stop_acceptance_card`, `qt_review_tour_agent_setup_call`, `qt_review_tour_agent_failure`, `application_review_tour_writing`: no Suggest an outline or Insert outline. `core_tour_stop_acceptance` (`test_carry_saved_drafts`): an older draft's outline-suggested stops open Not accepted yet and can be accepted; the SQLite codec runs that carry for every payload before version 11, and `sqlite_review_tour_storage` still reads and writes drafts. `bpr tour outline` is unchanged (no change under `src/app/src/tour_cli.cpp`'s outline paths; `application_review_tour_cli_*`) | Pass |
| AC08 | The owner's look at the stop at rest, being edited, and in each ring state on a frozen build | Pending: owner-attended (below) |

## AC08: the owner's look

Pending; filed for the owner and not waited on. Attended session `679d26abbfa93b3357729061432131ba`, state
`/Users/jaeger/.attended-review-state/tour-stops-v002-2026-11-10/state`,
cursor 15, host PID 61912 (its `tailscale serve` child 61936), five
candidates, one question each, ready and protected: `BPR-2013-arrival`,
`BPR-2013-stop-at-rest`, `BPR-2013-stop-editing`, `BPR-2014-accept` (on the
frozen app, conventional-comments-v1 renderer.cpp with `--attended-tour-look`)
and `BPR-2014-ring-states` (the frozen `tour_stop_acceptance_card --look`:
each ring state after a push). Frozen root
`/Users/jaeger/tour-stops-v001.5rt9c5gn` from `93b1045a8` (app SHA-256
`c5e2a178c2ea…`, card test `6683038724ed…`; the launcher's own check reached
`ATTENDED_TOUR_SHOWN`). The URL stays in the session state.

## What no course shows yet

The window compares the accepted code with a new head through the same
realignment and materialization as the tour's repair preview, once a
comparison of the new head is on screen (`check_tour_material`, called on
every tour presentation and every published comparison). No fixture has a
second head with real content for a tour route: `reaccept_fixture_revision`
changes the handoff's head only. So AC04 is shown on real comparisons in the
core (`map_tour_revision`, which the window feeds) and, through the window,
only as "not compared yet". A fixture with a pushed head for the tour route
is the missing piece.

## Critical review

[[BPR-2014]] decides what may be published (it amends RFC-BPR-0042, 0043
and 0096). The pack asks for a critical review of it, which this lane cannot
give itself: no separately authorized reviewer was available in this round.
It is pending, not waived.
