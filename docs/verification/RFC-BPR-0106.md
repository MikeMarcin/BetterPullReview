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
give itself.

An independent reviewer (read-only, ran nothing) reviewed `93b1045a8` and
returned pass-with-findings, provisional. The post-review engineering round
(2026-11-10) took each finding test first: a test that reproduces it, run
against the unfixed code, then the fix, and the test kept. A re-review of the
fixes by an independent reviewer is still owed; it is not self-certified here.

| # | Finding | Reproduced by (failed before the fix) | Disposition |
|---|---|---|---|
| 1 | A part-sent tour update saved before acceptance (tour payload version 10) fails to load: `staged_update_bodies` re-plans the frozen tour through `plan_tour_update`, which now requires every stop Accepted, so `valid_staged_update` fails, the SQLite journal reports an integrity failure and the pull request's saved publication state is unavailable | `staged_review_session` (`test_update_saved_before_acceptance_resumes`: a part-sent update of stops not accepted, its frozen tour rewritten in the journal as version 10 with its digest, reopened through `pending_staged`, reconciled and resumed). The reviewer's contradiction resolves the other way: `staged_review_session` did not pass at `93b1045a8`; it failed in `test_mixed_sessions` for the same cause and was not in the candidate's runs | Fixed. `render_tour_update` renders a frozen, already authorized update without judging acceptance again; `plan_tour_update` is that render behind the acceptance gate, and authorization stays at plan time (`assess_tour_update`). The staged record resumes as it was sent |
| 2 | The terminal walk's Ready accepts with no changed files (the owner codec carries none, the CLI sets none), so references to other files are unrecorded and a push that changes them leaves the stop Accepted | `core_tour_stop_acceptance` (`test_walk_ready_records_references`) | Fixed. An observed Ready accepts only with the changed files when the stop's text names another file (`tour_stop_names_files`); without them it is Ready, not Accepted, and `accept` without them is refused. A standalone walk passes the changed paths its read observed (`tour_runtime::source`); a desktop window showing the pull request supplies them to a forwarded Ready (`live_tour`), and the window's own dispatch does for `complete` as for `accept`. No codec change: files never travel on the owner channel |
| 3 | Accept at a head where the stop has no mapped lines (or while "not compared yet") records no own lines: Accepted with nothing to compare | `core_tour_stop_acceptance` (`test_reaccept_keeps_own_lines`) | Fixed. Accept keeps the stop's own lines to compare when they are not at the head: what was accepted of them with the comparison's verdict, or (never accepted) its lines at the tour's head. Not compared yet stays yellow until compared; compared changed or gone with nothing mapped makes Accept ask for its lines at this head (`tour_accept_need::lines_at_head`). Only a historical stop is accepted with no own lines. `core_diff_semantics` (real push) and `application_review_tour_publication_reconciliation` (Accept while not compared yet) now pin this; both had asserted the old behaviour |
| 4 | The carry of version 10 drafts ignores `CompletionOrigin` (an agent's relayed Ready becomes Accepted), accepts outline-suggested Ready stops (against AC07) and erases reference tracking | `core_tour_stop_acceptance` (`test_carry_grants_only_the_author`); `sqlite_review_tour_storage` (a relayed version 10 stop) | Fixed. Only the author's own Ready (`observed`), not outline-suggested, reaffirmed, naming no other file, carries; its lines and bare line references are recorded. Every other stop comes over Not accepted yet |
| 5 | References outside the changed-file list, ambiguous ones and those past the 33-range bound are not recorded | Not a defect by the stated rule | Recorded, unchanged: the RFC's implementation notes say a reference is recorded when it resolves to exactly one changed file; such a stop can be Accepted while a later push changes an unrecorded range |
| 6 | The owner channel accepts an observed `complete` with client-supplied attention from a command client | Not reproduced as a defect: the walk is isatty-gated and the channel's peer is the same user | Recorded, unchanged (same-user trust model). A client that forges `observed` gains what the walk itself grants; it cannot `accept` or `unaccept` |
| 7 | A comparison of accepted code that failed is never asked again at that head | Read from the code (`check_tour_material` recorded the key before the read and never forgot it). No course has a failing realignment read to drive; not reproduced by a test | Fixed. A read that fails, or whose realignment is not ready, is forgotten and asked again on a later presentation, at most three times per file, commit and head (`tour_material_failed`) |
| 8 | Codec version 11 without a tour protocol bump: a mismatched CLI and desktop fail on the payload instead of at hello | `sqlite_tour_owner_channel` (`test_version_mismatch` with a version 6 owner) | Fixed. Tour protocol version 7; either side refuses version 6 at hello |

Visible change: Accept on a stop whose lines changed or are gone at the new
head with nothing mapped there is disabled, and its reason line adds "Choose
its lines again to accept it." (frontend-design was loaded after the change,
to review it; it turned "at this head" into the card's plainer "again"). The filed look (session
`679d26ab…`, built from `93b1045a8`) does not show that state: its
`--look` fixture's stop 2 now maps its changed lines to the new head, as an
in-place edit does, so every state the owner was asked about looks as it
did. Not refiled.

Also in this round: `qt_drawer_parts_scale_{100,125,200}` pass (test-only:
Overview's Solution is a Markdown view since `3d97ea548`, so the test reads
its empty-body label); `test_network_isolation_manifest` passes with the
missing entry for BPR-2051's `application_enhance_comment_note_public_path`.

### Re-review of the fixes (2026-11-10)

An independent reviewer re-reviewed `d02f13efb` and `fb94545ec` and returned
pass-with-findings, nothing blocking. Each finding was taken test first; where
a test could run against the unfixed code it failed there before the fix.

| # | Finding | Test | Disposition |
|---|---|---|---|
| 1 | Accepted with nothing to compare still reachable: never-accepted lines were kept to compare only at the tour's authored `Head`, which never advances, so a stop authored at a later observed head and first accepted after a push (or Marked not accepted and accepted again) recorded no own lines | `core_tour_stop_acceptance` (`test_intermediate_head_stop_is_compared`; failed before the fix); `sqlite_review_tour_storage` (version 12 keeps the heads, version 11 reads with none); `sqlite_tour_owner_channel` (`test_version_mismatch` with a version 7 owner) | Fixed. The tour keeps the heads it observed after `Head` and left behind while a stop still has its lines there (`EarlierHeads`, at most one per stop); lines at any of them are compared against later heads. Only a stop pinned at a commit the tour never had as its head is historical. Tour payload version 12, owner protocol version 8 |
| 2 | The walk's, the forwarded and the window's Ready or Accept file lists had no test; `live_tour` took the window's read without checking it was the tour owner's pull request | `tour_cli_process_commands` (pty walk: without changed files Ready, not Accepted; with them Accepted; failed with the walk's list dropped); `application_profile_tour_handoff_public_path` (a forwarded walk Ready and the window's Accept on a stop naming `review.cpp:1-2` against a real desktop owner; each failed with its list dropped); `application_review_tour_host` (`tour_changed_paths`: another pull request's read supplies none) | Fixed and pinned. One helper, `tour_changed_paths`, gives the changed paths only when the window's read is the tour owner's pull request; the window's Accept, the forwarded Ready and the editor's button use it. `bpr tour status --json` reports each stop's `acceptance` |
| 3 | A comparison dropped by a pull request switch was never asked again (its key stayed "asked"), nor one read with no verdict | `application_review_tour_host` (`tour_material_ledger`: one read at a time, a dropped read asked again and uncounted, a failed or verdict-less read asked again at most three times; `tour_material_compared`) | Fixed. The window keeps only reads in flight as asked; leaving the pull request drops them uncounted; a read that leaves the code uncompared counts as a failed attempt. Read from the code; no course drives a failing or dropped realignment read through the window |
| 4 | The editor computed Accept's need without the file list, while the core refuses Accept with none on a stop naming a file | `core_tour_stop_acceptance` (`tour_stop_accept_need` with files); `qt_tour_stop_acceptance_card` (failed before the fix: Accept was offered) | Fixed. One decision: `tour_stop_accept_need(tour, stop, files)` (`changed_files` when the text names another file and no files are known) is what `accept` and an observed Ready apply and what the button shows; the presentation carries the window's changed files. Disabled, the reason line adds "Accept it once the pull request's files load." Frontend-design was loaded before this Qt change |
| 5 | The version 10 resume test used only `editing` stops | `staged_review_session` (`test_update_saved_before_acceptance_resumes`, now also with the old rule's Ready stops) | Covered; no defect. The carry accepts those stops as the journal's record is read, the record is otherwise exactly the one sent, and the plan the desktop discovers reconciles and resumes |

Also found: two tests no earlier run had built or run still expected the five
hooks before BPR-2051 and BPR-2059 (`tour_cli_composition` for a running
owner, `application_profile_tour_handoff_public_path`); both now list nine.
`agent_context_supplement` did not compile after BPR-2059 added the brief's
comment format; its structural check names it. The CLI's capabilities named
owner channel v6; it says v8.

## Enhance on comments and notes ([[BPR-2051]], 2026-11-10)

Owner request, on the same lane. Frontend-design was loaded before the Qt
work. The design is in the RFC's amendment; how it fits
[RFC-BPR-0107](../rfcs/RFC-BPR-0107-agent-and-prompt-sharing.md) is that
RFC's "Purposes recognized by this version".

What was reused, not rebuilt: the writing contract, the rewrite result
schema and `decode_writing_result`, `build_writing_prompt`, the skill run for
a hook, the drafting agent choice and its tool-free check, and the Agent tab
host's writing-only launch that the Settings prompt test already used. What is
new: three hooks (`enhance_stop`, `enhance_comment`, `enhance_note`) with
shipped defaults, versions, stable purpose IDs and digests; the core's
`writing_instructions_for`; profile schema 25 (three override columns; the
draft comment's is `writing_enhance_draft`, because the store's column names
may not name review content); the Qt Enhance control, suggestion panel and
binding; and the composition's request, answer and stop.

Found while here: a stop's Enhance borrowed the Generate comment prompt; it
now sends its own. `bpr tour hooks` lists the three new prompts and each
hook's purpose.

| Criterion (card scope) | Evidence |
|---|---|
| 1. Reuse | `application_writing_delivery` (each surface's prompt is built by the same builder and decodes the same result); `core_review_preparation` |
| 2. Only when asked, a suggestion, nothing to GitHub | `application_enhance_comment_note_public_path` (stub claude through the real composition: comment and note unchanged until Use, Discard keeps the text, Use is one Undo, a selection is rewritten alone, mutation counters unchanged); `qt_enhance_control` (no run until pressed, an answer to another press is ignored, a changed text refuses Use, Stop cancels its own run) |
| 3. Agent accent | `qt_enhance_control`, the course, and the window inventory (`qt_workbench_window`, in the 127-row regression set) |
| 4. Own prompt per surface, default and restore | `qt_settings_window` (13 rows; each Enhance prompt's text, version, override and Reset), `sqlite_workbench_settings_storage` (migration from 24, each column, oversize refused), the course |
| 5. Prompt saved at the press | the course: an override naming a canary is saved, Enhance is pressed, a different prompt is saved while it runs, and the canary's answer arrives |
| 6. RFC-BPR-0107 fit | RFC-BPR-0107 amended; purposes and digests in `application_writing_delivery` |
| 7. Owner look | filed (see the handoff); not waited on |

Runs: the above, `application_review_tour_composition_public_path`,
`application_review_tour_writing{,_wrapper}`,
`application_review_tour_cli_ordinary_{native,stop}`,
`application_composition_source_contract` and
`application_review_round_trip_composition_contract` (app pin resealed), a
127-row regression set (tour, writing, settings, notes, conversation cards,
workbench window, submission composition, agent launch, every `sqlite_` row,
skills): only the baseline `qt_drawer_parts_scale_{100,125,200}` fail;
`coding-format-check`; documentation gate. `tour_cli_composition` (the
listing's count and purposes) is not configured in the dev preset
(`BPR_BUILD_TOUR_CLI`) and did not run.

Not shown by a course: the owner's own Claude on a real pull request (the
stub answers fixed text); a card rebuilt while a run is going drops its
answer unused (the text is never changed by it).
