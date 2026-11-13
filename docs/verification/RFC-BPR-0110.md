# RFC-BPR-0110 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `local-tour-preparation`
([[BPR-2026]], [[BPR-2027]], closeout [[BPR-2028]]). RFC_BASE_SHA:
`dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`. Frontend-design was loaded before
the Qt surface work. Owner amendments in force: "If you need it you need it go
build it."; "main should be the source of truth for the tracker"; "Continue
working unattended, you can file reviews but dont block on their results".
Debug timings are nongating; nothing below is a timing claim.

The cards have not moved: [[BPR-2026]] depends on [[BPR-2022]] and
[[BPR-2025]] (the closeouts of RFC-BPR-0108 and 0109), which wait on
[[BPR-2021]]'s owner look. The engineering below needed only the built
local-draft and evidence code; the claims and finishes follow those closeouts.

## What was built

| Layer | What |
|---|---|
| Core (`core/local_draft_tour.hpp`) | The draft's `Preparation`: description (body and title revisions), tour (Problem, Solution, stops bound to a candidate's exact lines, earlier bindings, citations, acceptance fingerprint), proposals, conversations; `correspond` (exact, changed, ambiguous, missing, unavailable); `carry_local_tour`; `decide_local_stop` and the head; edits with the version read; `record_proposals`, `accept_proposal` (stale on a later edit or candidate; a claim or check enters Evidence with agent origin), `reject_proposal`; `append_turn`, `end_conversation`, `replay_turns` (12 turns, 48 KiB), `conversation_cost`; `compose_local_request` (160 KiB, the candidate's readings only) and `parse_local_answer`; `compose_local_publication`; `map_local_publication`; `update_description_by_default`; the stored form inside the draft's record. Evidence admits an agent-origin record with no import (a conversation's accepted claim or check) |
| Application | Workbench edits for the description, brief, transfer, stops, citations and proposals; refresh carries every stop onto the successor from its captured bytes; `ask` reads the latest candidate from the private store, composes the request, runs the composed runner on its own lane and records the turns and proposals (an answer that arrives after a refresh is dropped and said so); two writing hooks, `local.description` and `local.discussion` (`local-description-v1`, `local-discussion-v1`) |
| SQLite | Profile schema 28 (two prompt columns; 26 and 27 rebuilt like older versions); the agent-setup store's two hook tables widened to ten hooks, a narrow table rebuilt with its rows |
| Qt | `local_draft_preparation_area`: Description and Tour areas and the agent bar; Settings lists Built-in prompts › Local drafts; the candidate chooser holds "Candidate 32 of 32" from the start; the pull request tour's description option can be given its starting state |
| App | The areas' dispatch; tool-free runs with their own launcher (no tools, no session saved, an empty folder of their own); the window's chosen agent and the two prompts' effective text; X19 at tour composition from Home's local-draft marks; the attended look `--attended-local-draft-tour` with a scripted agent |

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `application_local_draft_tour_desktop_public_path` (real window and composition, real Git worktree, signed out, scripted agent runner): an uncommitted file included in candidate 2; the description and the tour's brief written and saved in the window; a stop added from lines selected in Changes binds the candidate's exact bytes; after a restart the description, stop, conversations and proposals resume and the page opens on Changes. `core_local_draft_tour`: the stored form names no account; a body past 65,536 bytes is refused, never cut | Pass |
| AC02 | `core_local_draft_tour`: a cited, selected result enters the preview with its check, method, outcome, "reported by author", candidate, what it speaks for and its limitations; an unselected result cannot be cited; an unselected question and a private note are not in the preview. Desktop course: the preview carries the description and tour and not the conversation or the agent's unselected planned check | Pass |
| AC03 | `core_local_draft_tour`: the same lines in place or at one other place are exact, similar nearby lines are changed, two places ambiguous, a deleted file missing, unreadable bytes unavailable; the original binding is kept and Save keeps the earlier one; a proposal on an edited field or an earlier candidate is stale and changes nothing; lines outside the candidate are recorded and refused. Desktop course: after the request, the live edit to an unincluded `src/helper.cpp` is not in it (nor the file's name) while the captured `stop_worker();` is; a proposed stop on the helper cannot be accepted; a late Solution proposal cannot be accepted after the author's edit; an accepted refresh leaves the changed stop "Changed since accepted: changed", opens a new session on candidate 3 that replays nothing from candidate 2, and the Session menu shows candidate 2's conversation read only. The candidate workspace route is not built: runs are tool-free with the captured candidate's text, and the session line's tooltip says how many changed files were sent | Pass (tool-free route; scripted agent) |
| AC04 | Desktop course: after a restart, with no agent process, the open conversation's transcript, the proposals and the description and tour are there; no account or pull request is involved. `core_local_draft_tour`: replay keeps the latest 12 turns within 48 KiB; everything else works with no agent (no runner: Ask is disabled with its reason, editing unaffected). `qt_settings_window`: Built-in prompts › Local drafts lists Description writing and Change discussion. `sqlite_workbench_settings_storage`: a version-27 profile migrates and both overrides store and survive. `sqlite_agent_configuration_storage`: an edited `local.discussion` and the shipped `local.description` export with their versions and are recognized on import | Pass |
| AC05 | `core_local_draft_tour`: saving the brief never writes the description; Add from Tour copies only on request; the mapping reports the pull request's body is the accepted description, and a replaced one is reported. Desktop course: the team-template body survives the brief's save and is byte-for-byte in the preview | Pass |
| AC06 | `core_local_draft_tour` against an exact simulated result: the matching stop gets the head's anchor and its commit-pinned permalink; the mismatched one stays local as `changed`, counted unresolved; no stop is dropped; stored artifacts are listed as having no public address. Existing pull-request tours: `qt_tour_side_chat` passes; their code changed only in X19's starting state | Pass on simulation; no publication calls the mapping yet (RFC-BPR-0111) |
| AC07 | `core_local_draft_tour` and the desktop course: nothing reaches Evidence before acceptance; the accepted planned check is in Evidence with origin agent and producer "Claude", the rejected one is not; no result is recorded | Pass |
| AC08 | Desktop course: the Tour area's head reads "1 stop, 1 to accept" then "1 stop, all accepted", and no button in it says Publish; Save accepts; Show arrives at the stop's lines selected in Changes; after the refresh only the changed stop is yellow with its reason word; the transcript, session line ("Claude · candidate 2 · $0.02 this session"), per-turn cost and Ask in the agent accent; the transcript links references to candidate files. Running: the bar shows "Asking…" and Ask becomes Stop; a run ends answered, failed with its reason, or stopped. No activity rows: a tool-free run reports none | Pass on fixture, running block simplified; owner look filed |
| AC09 | Desktop course: the tour editor's option starts on by default and off when given a local draft's pull request; `window_tours` gives it that state from Home's local-draft marks when a pull request's tour is composed; changing the option disables Publish until a new Preview (existing behaviour). `core_local_draft_tour`: a stop citing a result that needs recheck is "Changed since accepted: cited evidence needs recheck" | Pass; the composed X19 path has no course with a real associated pull request |

## Runs

At the candidate (dev, Debug): `core_local_draft_tour` (new),
`core_local_draft`, `core_local_draft_evidence`, `core_agent_configuration`,
`core_review_preparation`, `core_profile_data_reset`, `local_draft`,
`application_local_draft_desktop_public_path`,
`application_local_draft_evidence_desktop_public_path`,
`application_local_draft_tour_desktop_public_path` (new),
`application_review_skill_host`, `sqlite_workbench_settings_storage`,
`sqlite_agent_setup_storage`, `sqlite_agent_configuration_storage`,
`sqlite_profile_data_reset_files`, `qt_settings_window`,
`qt_agent_configuration_dialogs`, `qt_tour_side_chat`,
`architecture_boundaries`, clang-format 22, clang-tidy on the new and touched
units. The shipped composition (dev app, offscreen, a fresh home,
`--attended-local-draft PATH --attended-local-draft-tour`) reached
`ATTENDED_LOCAL_DRAFT_TOUR_SHOWN` on the generated worktree.

Baseline failures kept: `test_network_isolation_manifest` still reports the
raw `add_test` in `tests/end_to_end/CMakeLists.txt` (`live_agent_checkout_settings`)
and `application_review_tour_publication_push` without a Windows entry, both
from earlier milestones; this round added the four local-draft entries.

Not run: the whole suite, the core-only preset, Qt window suites, an
installed agent answering a local draft (the course's runner is scripted),
Windows and Linux.

## Owner verdicts (attended session 6c8ac4dc8b4856b0f94a4b4729af0806, handed back 2026-11-12T06:36:34Z)

- `RFC-BPR-0110-candidate-chooser`: **Reject** (decision `ad3145f7-d664-4cd5-9fe1-7cc07092bf89`, 2026-11-12T06:25:43Z, actor human), the owner: "This screen is so ugly besides maybe the stops, but not even those, that I dont even want to look at it."
- `RFC-BPR-0110-description-area`: **Reject** (decision `1d41ca91-202a-4cdb-9495-97414e9f0c5e`, 2026-11-12T06:26:20Z, actor human), the owner: "This screen is so ugly that I don't even want to look at it."
- `RFC-BPR-0110-tour-area`: **Reject** (decision `f2e3c111-e44d-4a89-a10a-2c3c354fa6ae`, 2026-11-12T06:36:33Z, actor human), the owner: "This screen is so ugly that I don't even want to look at it."

Root `/Users/jaeger/local-draft-tour-v001.3hs4zi83` removed with `freeze_attended_build.py remove` and the host
stopped (2026-11-12). The coordinator: a redesign from mockups comes first;
no new look is filed until then.

## Owner amendment: Evidence removed (2026-11-12)

The owner cut Evidence from 0.5.0 ("Mockups; cut Evidence"); RFC-BPR-0109 is
Deferred and this RFC is amended in its own text (AC02 and AC09 amended, AC07
withdrawn, X21 inactive). The rows above for AC02, AC07 and AC09's evidence
half are history. What changed on the lane (`aa00b4795`, `fcbd667b4` and the
reseal commit):

- Core: no evidence record, citations, `cite_evidence`, `evidence_recheck`,
  claim or check proposals, selected evidence in the request or the preview,
  or artifacts held only here; `decide_local_stop` takes the stop alone. A
  stored draft written before is read and the evidence, citations, claim and
  method fields and claim and check proposals (with the turns' references to
  them) are dropped. A stored publication plan that named evidence has its
  digest checked against the form it was made in and carried to the same plan
  without evidence; a mismatch is refused.
- Store: version 1 migration rewrites such drafts, rebuilds their pins and
  deletes objects nothing pins (evidence artifacts).
- Surface: the draft page is Changes, Description, Tour and Pull request; no
  Cite Evidence menu; the prompts `local.description` and `local.discussion`
  no longer mention evidence, claims or planned checks.
- App: `bpr draft open --evidence` is refused as an unknown option
  (`unsupported_option`, exit 2, nothing opened); the attended look's
  `--attended-local-draft-evidence` is gone; the app source pin is resealed
  with a removal note.

Runs on the dev preset (Debug), all passing: `core_local_draft`,
`core_local_draft_tour` (new: private preparation stays out of the preview;
a draft stored with evidence reads as the same draft without it),
`core_local_draft_publication` (new: a plan stored with evidence keeps its
checked digest; a tampered one is refused), `local_draft` (new: the store
migration on a real store file), `local_draft_publication`,
`application_local_draft_desktop_public_path`,
`application_local_draft_tour_desktop_public_path` (four areas, a proposal
accepted into the tour and one rejected),
`application_local_draft_publication_desktop_public_path`,
`architecture_boundaries`, `architecture_boundary_contract`,
`application_composition_contract`, `application_composition_source_contract`,
`application_review_round_trip_composition_contract` (after the reseal),
`sqlite_tour_owner_channel`, `test_network_isolation_manifest`,
`verify_test_network_isolation_tests.py`, `core_profile_data_reset`,
`sqlite_profile_data_reset_files`; `bpr draft open --json --evidence x` from a
terminal. clang-format 22 on every touched C++ file; clang-tidy (warnings as
errors) on every touched translation unit except `src/app/main.cpp` (only
deletions there). Not run: the whole suite, the core-only preset, Qt window
suites, Windows and Linux.

## In the pull request window (2026-11-13)

The owner's redesign ("I thought we chose to stick with the PR view and just
augment it") and the coordinator's brief: the local draft's tour in the
window's accepted Tour panel, its description in Overview, the old local
editor deleted. Frontend-design was loaded first. Implementation record: the
RFC's "In the pull request window (2026-11-13)". Lane commits `e48802f49`
(core `local_tour_view`), `191b12294` (Qt), `5337b1eda`, `736407b8f`,
`e44dba18f`, `667175975` (composition, courses, look, pin). The evidence rows
above that ran on the deleted draft page are superseded by these:

| Criterion | Evidence (dev, Debug, macOS, at `6887928bd`) | Result |
|---|---|---|
| AC01 | `application_local_draft_tour_desktop_public_path` (real window, real local-draft composition, real Git worktree, signed out, scripted agent): candidate 2 holds an uncommitted file; the description written in Overview's own editor (`description_editable`, Save); Problem and Solution typed in the Tour panel; a stop added from lines selected in Review with the panel's Add stop binds the candidate's exact bytes. `core_local_draft_tour` `test_panel_view`: the panel's owner is well formed and its candidate identity is neither the real commit nor another candidate's | Pass |
| AC02 | Same course: `compose_local_publication` carries the description byte for byte and the tour, not the conversation | Pass |
| AC03 | Same course: the request holds the captured `stop_worker();` and not the live helper edit or its name; the stop outside the candidate is recorded and never offered; the Solution proposal is stale after the author's edit and the edit stands; after an accepted refresh only the changed stop is yellow, a new session starts on candidate 3 replaying nothing, and the earlier turns stay readable in the side chat. `core_local_draft_tour`: a stale proposal shows refused | Pass (tool-free route) |
| AC04 | Same course, after a restart: description, tour, conversations and proposals resume; the panel and the open conversation show with no agent process | Pass |
| AC05 | Same course: the brief never writes the description | Pass |
| AC06 | `application_local_draft_publication_desktop_public_path`: `carry_tour` onto a pull request tour draft at the candidate's head anchors the stop there and keeps it Accepted; at another head it anchors nothing and keeps the stop with its text (`Unresolved` 1). `core_local_draft_tour`: zero-based anchors. Through the full composition the carry runs when the pull request's tour loads empty and Home marks it (`carry_local_tour`); no course drives that path end to end (the live course that would is blocked, see RFC-BPR-0111) | Pass on the application path |
| AC08 | Tour course: the head states 1 stop, 1 to accept and has no actions (no Publish); the new stop is yellow, typing saves its text without accepting it, Save accepts it (green ring); arriving at it selects lines 6-10 in Review's diff viewer; the side chat shows the turn, its answer and "$0.02 this session"; after the refresh the ring is yellow with "Changed since accepted: changed." (the draft's own word). Renders: `build/dev/tests/end_to_end/local-draft-tour-desktop/` | Pass on fixture; owner look filed |
| AC09 | Same course: the editor's option starts on by default and off for a local draft's pull request | Pass |

The full composition (dev app, offscreen, fresh home, `--github-fixture
promo-v1 --attended-open-file src/render/frame_pacer.cpp --attended-local-draft
… --attended-local-draft-tour`) reached `ATTENDED_LOCAL_DRAFT_TOUR_SHOWN`,
driving the window's Tour panel, Overview's editor and the side chat through
`github_application`'s routing.

Other runs at the candidate, all passing: `core_local_draft`,
`core_local_draft_publication`, `core_local_changes`,
`core_local_presentation`, `core_review_tour`, `core_tour_stop_acceptance`,
`core_tour_publication_head`, `local_draft`, `local_draft_publication`,
`local_changes`, `qt_workbench_window_scale_100`, `qt_tour_side_chat`,
`qt_local_draft_window`, `qt_review_tour_steps`, `qt_review_tour_stop_view`,
`qt_tour_stop_acceptance_card`, `qt_tour_stop_card`,
`qt_review_tour_publication_working`, `qt_overview_read_marks`,
`application_review_tour_host`, the four local-draft desktop courses,
`application_dependable_session_public_path`, `architecture_boundaries`,
`architecture_boundary_contract`, `test_network_isolation_manifest`,
`application_composition_source_contract`,
`application_review_round_trip_composition_contract` (after the reseal).
clang-format 22 on every touched C++ file. Not run: the whole suite,
core-only preset, clang-tidy, Windows and Linux.

Limits, for the closeout: Ask more is not offered for a local draft (its
turns replay or resume); split, duplicate and merge do nothing for a local
tour; ambiguous and unavailable correspondence show the panel's yellow ring
with the draft's word; the agent bar's readiness is the Agent tab's, so a
course sets it ready; the side chat lists earlier candidates' turns above the
open session's rather than in a Session menu.

## Owner verdicts (attended session 411e2c7497822b381ac2601afe7909f3, handed back 2026-11-13T08:47:51Z)

Session state `/Users/jaeger/.attended-review-state/local-draft-tour-panel-v005-2026-11-13/state`, frozen build from `89156b02d99e` (product as `6887928bd`). Verbatim notes, each the candidate's final decision:

| Candidate | Verdict | Owner's note |
|---|---|---|
| `local-tour-in-tour-panel` | **Reject** (08:46:23Z) | "Seems like a lot of different button styles. I see 3 buttons that say accept and they all look totally different and I'm not sure why." |
| `local-description-in-overview` | **Reject** (08:44:19Z) | "Seems like the other mode's drawers aren't being closed when going to overview." (first worded "Seems like the other drawers aren't being closed when going to overview."; superseded by the owner's edit) |
| `publish-steps-in-status` | **Accept** (08:47:49Z), with a note | "Looks pretty good, but i cant really test it, I'm not sure if those circles are meant to be misaligned vertically with the text beside them." |

Each decision carries a screenshot attachment in the session state. Not fixed in this round (coordinator, 2026-11-13: the next round fixes them). Host 6331/6340 stopped with the attended-review `stop`.

## Rework of the owner's verdicts (2026-11-13)

Fresh `rfc-implementer`, same lane. Frontend-design loaded first. The three
notes of session `411e2c74…`, fixed for the pull request and the local draft:

| Note | Change | Evidence (dev, Debug, macOS) |
|---|---|---|
| "I see 3 buttons that say accept and they all look totally different" | RFC-BPR-0106 [owner amendment: one Accept](../rfcs/RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md#owner-amendment-one-accept-2026-11-13). Accept only where a stop is accepted (its own Accept, the edited stop's, a stop's own proposed text), one button: the default push button. A proposed stop says Add to tour, a Problem or Solution Use this text; Accept all is Apply all, once per revision (the tour chat's card, or the panel's line for a Draft Tour run). Same words in the side chats. `f3fc567f3` | `qt_tour_chat_in_tab`: every visible Accept in the window is the default button at the stop Accept's size, no other button starts with Accept, a draft's parts have one Apply all (also under the native Cocoa style, dark). `qt_tour_agent_session_view`: the tour chat's revision has Apply all on its card and none on the panel's line |
| "the other mode's drawers aren't being closed when going to overview" | A local draft's modality never reached the core, so Overview's rule (every drawer closed) was not applied: the window keeps it while a local draft shows Overview (Review's Files drawer, its tray and the tour's chat leave), and Overview's own drawer offers no Comments, Notes or Agent a draft lacks. `b8dca6417` | `application_local_draft_tour_desktop_public_path` (failed before the fix): with Files, the tray and the tour's chat open, Overview shows none of them and Review brings them back. `application_overview_summary_{1,1_25,2}`: a pull request's Overview, composed with the real core, closes both drawers and the Tour tab |
| "not sure if those circles are meant to be misaligned vertically" (publish steps, accepted) | The step labels lost height-for-width when their size policy was set after word wrap, so a plain step took a two-line hint and its words sank. Height-for-width kept, label top aligned, mark centred on its first line, spare room under the rows. `b8dca6417` | `qt_review_tour_publication_working` `test_step_marks_centred` (failed before: mark centre 8, first line 15 to 16): marks centred within 1 pixel at 420 and 150 pixels wide and with room to spare; one-line rows one height |

Also: a refused carry onto the pull request's tour draft is shown in the
Tour panel (`core::local_tour_carry_refusal_words`), never dropped
(`1f153d3b5`, `core_local_draft_tour`). App source pin resealed.

Runs at `1f153d3b5`: 33 focused rows (core tour and local draft, Qt tour,
side chat, stop card, publication, workbench window at 100%, overview read
marks, local draft window, the four local-draft desktop courses, tour host,
profile tour handoff, agent invocation contract, overview summary at three
scales) pass except `qt_workbench_agent_send_refusal`, which fails the same
way at `1c7c5cd1f` without this round's changes (the Explain this file
button keeps the shipped accent after the accent changes; not this round's).
Source and round-trip composition contracts and `architecture_boundaries`
pass; clang-format 22 on every changed file; documentation gate OK.

Native renders (Cocoa off screen, `BPR_NATIVE_LOOK`), light and dark, are the
session's media. Owner look: session `a0cbb9ddda6d1ae524e5d63d4a3342d7`, state
`/Users/jaeger/.attended-review-state/local-draft-tour-panel-v006-2026-11-13/state`
(serve output beside it; the URL stays there), cursor 9, filed 2026-11-13 and
not waited on. Frozen root `/Users/jaeger/local-draft-tour-panel-v006.txmtt3kc`
from `195140d7968f` (launcher check reached `ATTENDED_LOCAL_DRAFT_TOUR_SHOWN`).
One Accept/Reject each: `tour-one-accept`, `overview-closes-drawers`,
`publish-step-marks-aligned`.

## Live: the tour carried onto a created pull request (2026-11-13)

AC06's carry ("On publication") failed on the full composition path, found
by RFC-BPR-0111's live course once its sign-in was fixed: the carried stop
was refused by `core::apply` (`invalid_source`, a destination-only stop
marked `exact_original`) and the window opened the pull request with an
empty tour. Fixed in `1237dbefd`; the fixture course now applies the carry
as the window does. Live on BPR-Test #184 (closed after): the window became
the pull request and its tour draft held the draft's stop, anchored at the
head and Accepted. Details in the
[RFC-BPR-0111 ledger](RFC-BPR-0111.md#live-on-bpr-test-sign-in-diagnosed-draft-pull-request-created-and-the-tour-carried-2026-11-13).
