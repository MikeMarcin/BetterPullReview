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
