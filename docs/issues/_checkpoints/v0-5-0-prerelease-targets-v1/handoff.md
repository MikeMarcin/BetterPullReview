# v0-5-0-prerelease-targets-v1: handoff

Mandate `v0-5-0-prerelease-targets-r1` (owner, 2026-11-10: "Implement the
pack … Start with the dependable-session milestone and follow the plan's
order"). Strategy: coordinator-implementer, preserved in
[selection.json](selection.json). Lane `claude/v0-5-0-prerelease-targets-52265d`,
base and RFC_BASE_SHA `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`. Nothing pushed.

## Commits

| Commit | What |
|---|---|
| `5d8210443` | Pack import: RFC-BPR-0098..0112, BPR-1989..2035, plan adopted ([LIVE_IMPORT](../../../rfcs/assets/v0-5-0-prerelease-targets-v1.LIVE_IMPORT.md)) |
| `1600de86c`, `df50b29dd` | Claim BPR-2010, BPR-2011 |
| `357df825d` | Stated, bounded, logged submissions; per-host lanes; RFC cause amended; course and unit tests |
| `31f900910` | Open resets the surface at dispatch; production app source pin resealed. Product candidate |
| `f0295294d` | BPR-2011 Done (AC06, AC07) |
| `6bf259a2d` | BPR-2010 parked `human-review` (AC01–AC05 pass, AC08 pending) |

## State

`dependable-session` is engineered; its first exit (cause recorded; no
submission can wait or vanish without a visible, bounded, logged condition)
and second exit (another pull request's effects do not block; every open shows
Comments with no tour side chat; filters and layout kept) have passing
public-path evidence ([verification](../../../verification/RFC-BPR-0105.md)).
The milestone is not accepted while RFC-BPR-0105 AC08 (the owner approves on
BPR-Test from a frozen build after a five-PR session with agent and tour use)
is pending. BPR-Test #151, authored by the test account, is open for it.

AC08 has no prepared session: `scripts/freeze_attended_build.py` freezes only
tests-on builds, and a tests-on composition refuses live review submission
(`compose_review_submission` is read-only without a fixture). A live frozen
build needs either a tests-off freeze path (a `scripts/` change outside this
lane's authority) or the next prerelease build. That choice is the
coordinator's or the owner's.

## Commands and helpers

- Course: `ctest --preset dev -R application_dependable_session_public_path`.
- Baseline recording: the same course on a detached `dc5453afa` checkout with
  only the course and the shared-lane hold added; that checkout is removed.
- `milestone_checkpoint.py bind` could not run: plan discovery scans every
  live plan and `docs/rfcs/plans/release-v041/milestone-plan.json` is invalid
  under the installed helper (`unknown fields ['amendments']`). Not this
  lane's file; the binding is recorded here by hand.
- `.agents/skills/implement-rfc` in this worktree is an ignored symlink to the
  installed skill, made so the repository's board helpers and documentation
  gate run here.
- The ID allocator refuses non-`main` branches; under the coordinator's
  instruction it ran once against the main checkout, whose counter and receipt
  were moved to this lane and the main checkout restored (see LIVE_IMPORT).

## Next

Next eligible milestones in plan order: `agent-tab-asks`, `tour-publishing`
and `home-next-move` (independent of this one and of each other). BPR-2012
(RFC-BPR-0105 closeout) belongs to `accepted-scope` and waits for AC08.

## agent-tab-asks (2026-11-10)

Engineered on the same lane; nothing pushed. Frontend-design was loaded before
the Qt work. Records: [RFC-BPR-0101](../../../verification/RFC-BPR-0101.md),
[RFC-BPR-0102](../../../verification/RFC-BPR-0102.md).

| Commit | What |
|---|---|
| `29f144959`, `0a126de53`, `259a58e57`, `044bf7a8f`, `23ee47ccc` | Ready the four cards; claim BPR-2002, 1998, 1999, 2000 |
| `dd3ea5ba9` | Question subject and attachment chip, Escape clears the selection, the core reference recogniser |
| `574958186` | NEXT_ID to BPR-2038: `main` reserved BPR-2036 and 2037 after the import, so the lane's counter rewound the high water and every board helper refused |
| `2c4372be5` | Return sends, cost per turn and session, Agent accent role and the run rule, live-claude course, RFC amendments (0012, 0034, 0101), ledgers, app pin resealed |
| `073cca272` | Tour accent tests follow the rule. Product candidate |
| `f6d06a4fa`, `3a4dd269d`, `b5bb93c46` | BPR-2002, BPR-1998, BPR-1999 Done |
| `c1d445720` | BPR-2000 parked `human-review` (AC07, AC09 pass; AC08 pending) |

State: both exits have passing public-path evidence (fixture course
`application_agent_chat_subject_contract`, Qt window courses, and the opt-in
installed-Claude course `live-claude`). The milestone is not accepted while
RFC-BPR-0101 AC08 (the owner's look at the Agent tab's accent and footers on a
frozen build) is pending.

Established facts: installed Claude Code 2.1.319 reports `total_cost_usd` as
the resumed session's running total; BPR accounts a resumed turn as the
difference, recorded at launch (`ResumedNative`).

Integration pass at `073cca272` (113 focused rows): only the two baseline
failures recorded under RFC-BPR-0105 remain
(`application_agent_chat_custom_agent_contract`,
`application_agent_finding_codex_found_contract`). The documentation gate's only
finding before `574958186` was the counter rewind.

Decisions made here for the coordinator or owner to see: the "selected-lines
side chat" is the panel the Agent tab presents, so its rules arrive through the
Agent tab; opening a chat, showing a run and Set up the agent are ordinary
controls now (owner look BPR-1914 v004 had chat toggles in the accent); Ask the
agent about these lines and Review Cycles' "Send to Agent…" only open a step,
so the accent is on the step's Send; unchanged-file references are plain text
because there is no unchanged-file view.

Next: AC08 attended look (see the coordinator report for its session), then
`agent-work-visible` (depends on this milestone), `tour-publishing`,
`home-next-move`. Closeouts BPR-2001 and BPR-2003 belong to `accepted-scope`.

AC08 attended look prepared: frozen build `/Users/jaeger/agent-tab-asks-v001.qgjq6axg`
(from `c1d445720`, executable SHA-256 `e02d77336f22…`, launcher `--check` passed on
README.md of nested-folders#21), session `fd2f7fdb4d5b89ca67e9793084a68d04`, state
`/Users/jaeger/.attended-review-state/agent-tab-asks-2026-11-10/state`, served over
Tailscale, event cursor 6, two candidates ready: `BPR-2000-agent-accent`
(fingerprint `40b96d3f8f1f…`) and `BPR-1999-agent-footers` (`10e13f7c7435…`).
The URL stays in the session state. Remove the root with
`scripts/freeze_attended_build.py remove` once decided.

## tour-publishing (2026-11-10)

Engineered on the same lane; nothing pushed. Frontend-design was loaded before
the Qt work. Record: [RFC-BPR-0099](../../../verification/RFC-BPR-0099.md).

| Commit | What |
|---|---|
| `c16cb7f7f`, `2b0140564` | Ready and claim BPR-1991 |
| `3d97ea548` | Three bands and the head (core `decide_tour_head`, shared with Overview); the plan chooses the form; steps from the plan's writes with per-write progress from the host; error-role banners; Overview Markdown, stop rows and head; courses, unit tests, resealed app pin. Product candidate |
| `d72b337ae` | RFC-BPR-0099 implementation notes and verification record |
| `2a8055583` | BPR-1991 Done (AC01, AC02) |
| `6ac50280e`, `74921a104`, `0903160f4` | Ready and claim BPR-1992, BPR-1993 (their product edits were already in `3d97ea548`: both depended on BPR-1991 being Done) |
| `9097343a8` | BPR-1993 Done (AC07) |
| `dceb3c333` | BPR-1992 parked `human-review` (AC03 to AC06 pass, AC08 pending) |
| `bce0d0eba` | Tour page unscrolled; route area out of the focus order. Look candidate |

State: exit 1 (bands, head) and exit 3 (Overview) have passing public-path
evidence. Exit 2's form, steps and banners pass on fixture courses; its live
mixed tour on BPR-Test (RFC-BPR-0099 AC08) is not run: a tests-on build holds
no publication grant, `freeze_attended_build.py` freezes only tests-on builds,
and `bpr tour publish` needs a person to type `publish`. It needs a tests-off
build the owner runs, the same route choice RFC-BPR-0105 AC08 waits on.

AC09 attended look prepared: frozen build `/Users/jaeger/tour-publishing-v001._2twcw6p`
(from `bce0d0eba`, executable SHA-256 `33c8788efc01…`, launcher `--check`
passed on README.md of nested-folders-v1), session
`5127270e92b1b77ceae8f9df8356c1f6`, state
`/Users/jaeger/.attended-review-state/tour-publishing-2026-11-10/state`, served
over Tailscale, event cursor 12, four candidates ready:
`BPR-1991-tour-tab-head`, `BPR-1992-publication-running`,
`BPR-1992-publication-stopped`, `BPR-1993-overview-tour`. The URL stays in the
session state. Remove the root with `scripts/freeze_attended_build.py remove`
once decided.

Runs: 74 focused tour, overview, staged-review and window tests; all fourteen
publication courses together at `-j6`; boundary and manifest tests; app source
contracts; documentation gate. Baseline failure kept:
`application_review_tour_cold_import_public_path` (also on `60d427a08`). The
shadowed local reported in `agent_reference.cpp` did not reproduce under
the build's `-Wshadow` or clang-tidy 22; left as is.

Disk: a scratch baseline build filled the shared disk for a while (since
removed). Builds by other agents during that window may have failed with
`No space left on device`.

Next: `home-next-move` (independent). `tour-stops` follows this milestone;
BPR-1994 (RFC-BPR-0099 closeout) belongs to `accepted-scope` and waits for
AC08 and AC09.


## Corrections round (2026-11-10)

Owner amendments: "main should be the source of truth for the tracker"; "If
you need it you need it go build it" (a frozen build that submits to live
GitHub). Frontend-design was loaded before the Qt work. Nothing pushed.

| Commit | What |
|---|---|
| `78ad5391b` | Merge `main` (`1513d4a85`): BPR-1988, BPR-2036, BPR-2037 arrive; no ID collides, nothing renumbered |
| `481096d5b` | BPR-2038 (footers rework) and BPR-2039 (Colours filter box, Backlog) reserved against main; counter BPR-2040 |
| `d0cac878a`, `caa36c301` | [LIVE_IMPORT](../../../rfcs/assets/v0-5-0-prerelease-targets-v1.LIVE_IMPORT.md) reconciliation record |
| `4b39981ee` | BPR-2000 Done: the owner accepted the accent (AC08, accent half) |
| `2cebc037a` | Claim BPR-2038 |
| `cdc92d686` | RFC-BPR-0099 AC09 accepted (all four candidates of `5127270e…`); the freeze-with-the-surface rule in `AGENTS.md` |
| `b1d66829d` | Footers on one line: "Claude answered in 0:11 · < $0.01 · 1 finding", detail in the tooltip; reads note to the field's tooltip; RFC-BPR-0101 (format, AC04) and RFC-BPR-0102 amended. Product candidate |
| `42eca211e` | `freeze_attended_build.py freeze --live` and its contract tests |
| `308d53c89` | RFC-BPR-0101 AC08 split verdict recorded; closeout AC04 text |
| `f543f549f` | BPR-2038 parked `human-review` |

Main's own allocator refuses until main's counter is raised to BPR-2040
(checked from the main checkout); the main integrator's step.

Runs at `b1d66829d`: `core_agent_cost`, `qt_workbench_agent_send_refusal`
(new one-line assertion at the default width), `qt_workbench_window_scale_100`,
`qt_tour_side_chat`, `application_agent_chat_*` (only the baseline
`application_agent_chat_custom_agent_contract` fails), boundary and source
contracts, `coding-format-check`, `freeze_attended_build_contract`,
documentation gate.

Closed: the agent-tab-asks session `fd2f7fdb…` and the tour-publishing
session `5127270e…` are stopped and their frozen roots removed.

Open, awaiting the owner (URLs stay in the session state):

- Footers: session `9b8a51a3e23f4f46260bdb9d4832f5bf`, state
  `/Users/jaeger/.attended-review-state/agent-footers-v002-2026-11-10/state`,
  cursor 3, candidate `BPR-2038-agent-footers-v002`; frozen tests-on build
  `/Users/jaeger/agent-footers-v002.tcfr4gcq` from `b1d66829d` (SHA-256
  `7e814abaabf0…`, `--check` passed on nested-folders-v1 README.md).
- Live: session `243efdc0c300f68944464fc350c097ed`, state
  `/Users/jaeger/.attended-review-state/bpr-live-v001-2026-11-10/state`,
  cursor 6, candidates `RFC-BPR-0099-AC08-live-mixed-tour` (BPR-Test #152,
  next push `929b028f7` applied by the owner mid-course) and
  `RFC-BPR-0105-AC08-approve-151`; frozen tests-off build
  `/Users/jaeger/bpr-live-v001.wpvoqim9` from `42eca211e` (SHA-256
  `39983ab2229e…`, self-test and launch checks passed).

Next: read both sessions after hand-back. On accept, verify with `gh`: #151
has MikeMarcin's APPROVED review; #152 has one review carrying stop 1, one
conversation comment for stop 2, the tour comment and the description. Then
finish BPR-2038, BPR-2010 and BPR-1992, and remove the roots with
`freeze_attended_build.py remove`. Disposable BPR-Test leftovers: #152, its
`-next` branch.

## home-next-move (2026-11-10)

Engineered on the same lane; nothing pushed. Frontend-design was loaded before
the Qt work. Record: [RFC-BPR-0098](../../../verification/RFC-BPR-0098.md).
Owner amendment during the round: "Continue working unattended, you can file
reviews but dont block on their results".

| Commit | What |
|---|---|
| `013250aa4`, `477ec1713` | Ready and claim BPR-1989 |
| `5018fe6aa` | Next move in the core; decision, merge state, running checks, reviewers' verdicts and conversations carried (queue facts v4); `HomeConversations` fragment on authored aliases of Home's batch; word, sentence, tone, order, header, Hide quiet; fixture rows, composed course, unit and contract tests; app pin resealed |
| `58db13bb5` | A pull request of mine whose reviews were never read is Unknown, never Quiet. Product candidate |
| `2f772b6f0` | The composed course takes `--attended` (the look); the app's home look refresh step withdrawn; pin back to `5018fe6aa`'s |
| this commit | Counter to main's BPR-2042 (LIVE_IMPORT); BPR-1989 parked `human-review` |

State: both exits have passing public-path evidence (AC01 to AC05). The
milestone is not accepted while AC06 (the owner's look) is pending.

Runs at the candidate: `core_home_next_move`, `core_review_queue{,_attention}`,
`core_home_ignore`, `core_home_{discovery,filter,open}`, `github_read_contract`,
`github_paged_read`, `application_review_queue_{enrollment,refresh,notification}`,
`application_notification_delivery`, `application_home_open_host`,
`sqlite_queue_enrollment_storage`, `qt_review_landing_model`,
`qt_home_list_scale_{100,125,200}`, `qt_home_filter`, `qt_review_queue`,
`qt_composed_home_sign_in`, `qt_workbench_home_discovery_scale_100`,
`qt_workbench_open_review_from_home_scale_100`,
`qt_surface_capture_course_scale_100`, `qt_home_card_fit_scale_100`,
`qt_pull_request_activity_notifications`, `github_notifications_read`,
`application_home_next_move_public_path`, `application_personal_discovery{,_cli}_public_path`,
`application_home_{waiting,waiting_cli,own_activity,open_feedback,return,filter}_public_path`,
`application_attended_recent_replay`, `application_queue_composition_contract`,
`application_composition{,_source}_contract`,
`application_review_round_trip_composition_contract`, `architecture_boundaries`,
`test_network_isolation_manifest`, `coding-format-check`, clang-tidy on the
touched sources, documentation gate. The `HomeConversations` fragment was run
against github.com with `gh api graphql` on BPR-Test #151 (read only).

Filed, awaiting the owner (not waited on; the URL stays in the session state):
session `bb8c24c5f3cb7ed97777f4e3f1a70a78`, state
`/Users/jaeger/.attended-review-state/home-next-move-v001-2026-11-10/state`,
cursor 9, host PID 95022 (its `tailscale serve` child 95034), three
candidates, ready and protected:

- `BPR-1989-home-moves`: the rows on the frozen composed course of
  `2f772b6f0`, root `/Users/jaeger/home-next-move-v003.ozia7rab` (course
  binary SHA-256 `55c9296fb8f8…`; launcher `--check` passed).
- `BPR-1989-word-place`: keep the move's word after the sentence (Accept) or
  lead every Home row with it (Reject).
- `BPR-1989-own-pull-requests`: the owner's own open pull requests on the
  tests-off live build `/Users/jaeger/home-next-move-live-v001.krnex__w`
  (`58db13bb5`, product as `2f772b6f0`; SHA-256 `a56c1ba56992…`).

Observed, not changed here: signed in through the window's connect action
(the app's attended home look), Home's refresh action stays disabled, so that
look can never read reviews a search did not; the composed course signs in
from Home and is used instead.

Next: read the session after hand-back. On Accept of all three, finish
BPR-1989 (`issue_board.py finish`), stop host 95022 and remove both roots
with `freeze_attended_build.py remove`. A Reject of the word place moves every
Home row's word to the front (`home_row_delegate`), a new candidate. The
closeout card for RFC-BPR-0098 belongs to `accepted-scope`. Next eligible
milestones in plan order: `agent-work-visible`, `tour-stops`.

## Live-freeze correction (2026-11-10)

Owner amendments in force: "If you need it you need it go build it."; "main
should be the source of truth for the tracker"; "Continue working unattended,
you can file reviews but dont block on their results". Nothing pushed.

| Commit | What |
|---|---|
| `3373b398f` | BPR-2038 Done: the owner accepted the one-line footers (session `9b8a51a3…`, decision `f55208c9`, event 5); RFC-BPR-0101 AC08 now passes both halves |
| `abba53926` | `freeze_attended_build.py`: a live freeze seals the build before its check; the live launcher's check refuses a writable or sealed-over build and launches with the owner's arguments on the root's own home (offscreen), then empties it; contract tests |
| this commit | Rejection cause and refreeze recorded (RFC-BPR-0099, 0105, 0098 records, BPR-1989 card); counter to main's BPR-2043 (LIVE_IMPORT) |

Root cause of "build closes instantly" (session `243efdc0…`, both candidates
rejected): `freeze_live` ran `launch.py --check` before
`set_writable(source, False)`. The check's tests-off launch of
`BetterPullReview.app` joined the update lifetime, which made
`.bpr-update-<digest>/` (admission and running locks, a participant file)
beside the bundle; the seal then made that folder and its files read-only.
Every later start failed `admission_lock` (open for write), so
`update-lifetime-join` refused as `unavailable`, which the app reports as
`STARTUP_FAILED` and answers with the profile recovery dialog (Quit).
Established with lldb on the frozen executable (break on
`note_startup_failure`, caller `begin_update_lifetime`), and by removing the
folder: the same sealed build then passed its self-test and launch. Not a
fresh-profile product bug: a clean tests-off release build starts under a
fresh `CFFIXED_USER_HOME`, and a build sealed before its first launch takes
the read-only path. The defect hit `home-next-move-live-v001` too; tests-on
roots (`better_pull_review.app`) take no update lifetime and are unaffected.

Observations for the coordinator, not changed (no card allocated; IDs come
from main): an update-lifetime refusal is reported as `unavailable`, so the
app offers the profile recovery dialog with "A local database in this profile
could not be opened", which no reset can fix; and a control folder the
running user owns but cannot write is treated as busy rather than read-only.

Runs: `freeze_attended_build_contract` (ctest, passed), both refreezes' own
checks (self-test and launch on the root's own home), documentation gate.

Closed: session `9b8a51a3…` stopped (host 86560/86570), root
`agent-footers-v002.tcfr4gcq` removed; session `243efdc0…` stopped (host
88376/88384), root `bpr-live-v001.wpvoqim9` removed; root
`home-next-move-live-v001.krnex__w` removed after its candidate was replaced.

Open, filed and not waited on (URLs stay in the session state):

- Live: session `26d89bac1f89acd7841ee451d1e5ffd1`, state
  `/Users/jaeger/.attended-review-state/bpr-live-v002-2026-11-10/state`,
  cursor 6, host PID 36810 (its `tailscale serve` child 36892), candidates
  `RFC-BPR-0099-AC08-live-mixed-tour` and `RFC-BPR-0105-AC08-approve-151`;
  root `/Users/jaeger/bpr-live-v002.nnhzt2pb` from `abba53926` (SHA-256
  `cb02049fbf2f…`). BPR-Test #152 at `6c94a03c0`, no reviews or comments;
  its next push `929b028f7` (parent `6c94a03c0`) is applied by the owner
  mid-course. #151 open, no reviews, MikeMarcin requested; #1, #2, #3, #43
  open for the five-PR session.
- Home: session `bb8c24c5…` (host 95022/95034), cursor 13;
  `BPR-1989-own-pull-requests` replaced (released at event 10, now on
  `/Users/jaeger/home-next-move-live-v002.s9ny7rtj` from `abba53926`,
  SHA-256 `27bf0f4818e9…`); the other two candidates untouched.

Next: read both sessions after hand-back. Live accept: verify with `gh` that
#151 has MikeMarcin's APPROVED review and #152 has one review carrying stop 1,
one conversation comment for stop 2, the tour comment and the description;
then finish BPR-2010 and BPR-1992, stop host 36810 and remove
`bpr-live-v002.nnhzt2pb`. Home accept of all three: finish BPR-1989, stop
host 95022, remove `home-next-move-v003.ozia7rab` and
`home-next-move-live-v002.s9ny7rtj`. Disposable BPR-Test leftovers: #152 and
its `-next` branch.

## agent-work-visible (2026-11-10)

Engineered on the same lane; nothing pushed. Frontend-design was loaded before
the Qt work. Record: [RFC-BPR-0103](../../../verification/RFC-BPR-0103.md).
Owner amendments in force as above.

| Commit | What |
|---|---|
| `df1701992` | BPR-2043 (refused update-lifetime start shows the profile recovery dialog) and BPR-2044 (owned unwritable update control folder treated as busy) reserved against main as Backlog; not implemented |
| `cc3a11407`, `fc24c151a` | Counter to main's BPR-2047; ready and claim BPR-2004 |
| `12e9c6fb4` | Activity core, decoders (Claude helpers and later parts, Codex App Server sub-agents), loop activity and heartbeats, `helpers_unfinished`, diagnosis fields, Codex foreground instruction, Agent tab and tour side chat running block, quiet notice and folded line, recorded traces, unit/Qt/composed courses, live `agent-helpers` course, `--attended-agent-helpers` look, RFC-BPR-0096 amended, app pin resealed. Product candidate |
| `aec356af6` | Counter to main's BPR-2048 (main had reserved BPR-2047) |
| `8e531521e` | BPR-2004 Done (AC01 to AC04, AC07) |
| `274ee1144`, `807d5ae67` | Ready and claim BPR-2005 |
| `0fe6d1357` | BPR-2005 parked `human-review` (AC05, AC06, AC08 pass; AC09 pending) |

Facts established (AC01): installed Claude Code 2.1.319 on stream JSON reports
helpers (`system/task_started|task_progress|task_updated|task_notification`,
`background_tasks_changed`, helper frames by `parent_tool_use_id`), writes its
message's `result` while they run, then answers each report in a further part
of the turn (`system/init` … `result`); with input closed it holds the results
until the helpers finish. Codex 0.167.0 reports sub-agents on the App Server
(`subAgentActivity` items, helper threads) but never answers them on its own,
reports nothing on `exec --json`, and no flag of that version removes its
sub-agent tools; Codex runs are asked to keep helpers in the foreground. The
reported silent failure is reproduced on the frames (first result ended the
turn; the idle drain ended the connection under the helpers).

State: both exits have passing evidence except the owner's look (AC09). Live
AC07 passed with both installed agents on BPR-Test #40 (read only).

Runs at the candidate: `core_agent_activity`, `application_agent_helper_turn`,
`application_agent_chat_helpers{,_unfinished}_public_path` (stable over
repeated runs), `qt_agent_activity_view`, `qt_tour_side_chat`, an
integration pass of 105 agent/tour/diagnosis/composition/boundary rows (only
the baseline `application_agent_chat_custom_agent_contract` fails; one
regression found there, `application_agent_launch`'s stale App Server
terminal, fixed and rerun), `application_composition_source_contract`,
`architecture_boundaries`, `test_network_isolation_manifest`,
`coding-format-check`, clang-tidy on the touched sources (only two findings,
both pre-existing in `agent_tab_view` lines this change did not touch),
documentation gate and its tests. Live course commands are in the record.

Observed, not changed: in the live course an added agent can leave the Agent
tab on its setup, so the tab's clock (started on entering `working`) reads
0:00 although the run proceeds; a native-history redraw stores a turn's ending
as the outcome's name ("success"), which this change now overrides for turns
this window saw end; Codex App Server answers render each delta on its own
line, as before.

Filed, awaiting the owner (not waited on; the URL stays in the session state):
session `d2cc338937915480c469bdbd1ffd7f89`, state
`/Users/jaeger/.attended-review-state/agent-work-visible-v001-2026-11-10/state`,
cursor 9, host PID 29030 (its `tailscale serve` child 29040), candidates
`BPR-2005-running`, `BPR-2005-quiet`, `BPR-2005-ended`, ready and protected;
frozen build `/Users/jaeger/agent-work-visible-v001.9any75gt` from `12e9c6fb4`
(executable SHA-256 `9eca0ba356b2…`, launcher `--check` passed).

Not touched: sessions `26d89bac…` (36810/36892) and `bb8c24c5…`
(95022/95034), host 74692, BPR-Test #151/#152 and the `-next` branch.

Next: read session `d2cc3389…` after hand-back. On Accept of all three,
finish BPR-2005, stop host 29030 and remove the root with
`freeze_attended_build.py remove`. A Reject goes to a fresh implementer with
the decision. BPR-2006 (RFC-BPR-0103 closeout) belongs to `accepted-scope`.
Next eligible milestones in plan order: `agent-scratch` (depends on this
milestone's acceptance), `tour-stops`.

## Owner fix round: BPR-2048 and BPR-2049 (2026-11-10)

Owner-requested, on the same lane; nothing pushed. Frontend-design was loaded
before the Qt work. Record:
[RFC-BPR-0098, owner fix round](../../../verification/RFC-BPR-0098.md#owner-fix-round-bpr-2048-and-bpr-2049-2026-11-10).
Owner amendments in force as above.

| Commit | What |
|---|---|
| `6bc29a5b3` | BPR-2048 and BPR-2049 reserved against main (allocator in the main checkout at `243dddcbb`, counter and receipt `BPR-2048--BPR-2049--1f6f243721c8.json` moved here, main restored; lane counter BPR-2050) and readied |
| `262959cf9`, `ba951deb4` | Claim BPR-2048, BPR-2049 |
| `146bc2b35` | Oversized row fails no refresh (state before roster bound, closed alias on state, row mark, roster limit keeps the row); too-large card with Open on GitHub and Retry; Seen (core decision, facts v5, row and Pull Request menu); Approve again opens the review with Approve focused; fixture knobs, two composed courses, unit/host/adapter tests, app pin resealed. Product candidate |
| `6f12ae3f9` | The course's `--attended` look stops before any gesture |

Root cause (BPR-2048): a retained read too large for one pull request was a
failure of the whole refresh in `personal_discovery_host` (stale with an
error, and every search answer marked incomplete so nothing was withdrawn),
and the REST retained read bounded requested reviewers (over 100) before
reading the state, so a closed pull request with a large request roster never
left Needs me and failed every refresh. The owner's actual pull request is not
reachable from this machine's `gh` accounts (review requests in every state,
read only); limits were measured on public nodejs/node pull requests (Home
batch with conversations at most 170 KB for ten of the busiest; opening trips
on `changed_files` over 3,000). The `home-next-move` review-thread read cannot
cross the limit. No live mutation was made.

Runs: `core_pull_request_refusal`, `core_review_queue_attention`,
`application_review_queue_enrollment`, `github_paged_read`,
`application_home_{oversized,approve_again}_public_path`,
`application_composition{,_source}_contract`,
`application_review_round_trip_composition_contract`, and a 36-row Home
regression set (`qt_composed_home_refusal_scale_100`,
`application_setup_destination_public_path`, `qt_review_landing_model`,
`qt_home_list_scale_100`, `qt_home_filter`, `qt_review_queue`, the Home
public paths, `application_personal_discovery_public_path`,
`application_home_open_host`, `github_read_contract`,
`sqlite_queue_enrollment_storage`, `application_review_queue_*`,
`core_github*`, `core_home_*`, `core_review_queue*`, `qt_composed_github*`,
`architecture_boundaries`, `test_network_isolation_manifest`): all pass.
Full dev build; formatted with clang-format 22; documentation gate.

Counter: main reserved BPR-2050 meanwhile and reads BPR-2051; the lane
follows (LIVE_IMPORT). Both cards parked `human-review`.

Filed, awaiting the owner (not waited on; the URL stays in the session state):
session `9ed1f1a047a56e4d35f43e92d4f9d70b`, state
`/Users/jaeger/.attended-review-state/home-oversized-seen-v001-2026-11-10/state`,
cursor 12, host PID 74660 (its `tailscale serve` child 74677), four
candidates ready and protected: `BPR-2048-oversized-row`,
`BPR-2048-too-large-card`, `BPR-2049-seen`, `BPR-2049-approve-again`
(Accept keeps Approve again opening the review with Approve focused; Reject
asks for approving straight from the row). Frozen composed course of
`6f12ae3f9`: `/Users/jaeger/home-oversized-seen-v001.9rw9vr9h` (course
SHA-256 `2766916cba0e…`, launched with `--attended`, and `--attended
--approve-again` for the last candidate; app `f4309598a892…`, launcher
`--check` passed on nested-folders-v1 README.md).

Not touched: sessions `d2cc3389…` (29030/29040), `26d89bac…`
(36810/36892), `bb8c24c5…` (95022/95034), host 74692, BPR-Test #151/#152
and the `-next` branch.

Next: read session `9ed1f1a0…` after hand-back. Accept of all four: finish
BPR-2048 and BPR-2049 (`issue_board.py finish`), stop host 74660 and remove
the root with `freeze_attended_build.py remove`. A Reject of Approve again
asks for a row-level approval, which needs a Home-side review submission
(no window); a Reject of Seen's hover-only button asks for it always shown.

## Owner hand-back folded in (2026-11-10)

The owner accepted every candidate of four sessions with no notes (read with
`attended_review.py status`). Nothing pushed.

| Commit | What |
|---|---|
| `024846177` | BPR-1989 Done (RFC-BPR-0098 AC06: `23a80fed`, `cae206d9`, `0b371105`, session `bb8c24c5…`) |
| `99817aeb2`, `3245505b7` | BPR-2048, BPR-2049 Done (session `9ed1f1a0…`: `b441c298`, `4fd937ca`, `c495f9e1`, `3e464b5d`) |
| `2f5feae13` | BPR-2005 Done (RFC-BPR-0103 AC09: `eead98a1`, `25cac43a`, `d19d4d2d`, session `d2cc3389…`) |
| `4bc60a496` | Session `26d89bac…`: Accept recorded on both live candidates (`ec9e4113`, `e5e3d58e`) but GitHub holds no evidence (#151 no review; #152 no review, comment or push, head `6c94a03c0`; read with `gh`, remaining 4973). Neither AC08 passes; BPR-2010 and BPR-1992 stay parked |

Stopped: hosts 74660/74677, 29030/29040, 95022/95034. Removed with
`freeze_attended_build.py remove`: `home-oversized-seen-v001.9rw9vr9h`,
`agent-work-visible-v001.9any75gt`, `home-next-move-v003.ozia7rab`,
`home-next-move-live-v002.s9ny7rtj`. Left for the coordinator: session
`26d89bac…` host 36810/36892 and root `bpr-live-v002.nnhzt2pb`.

## tour-stops (2026-11-10)

Engineered on the same lane; nothing pushed. Frontend-design was loaded before
the Qt work; the accepted stop mockup is the design. Record:
[RFC-BPR-0106](../../../verification/RFC-BPR-0106.md).

| Commit | What |
|---|---|
| `5df66da08`, `06a24758c` | Ready and claim BPR-2013 |
| `93b1045a8` | Arrival shows the code (three paths did not), Show in code gone, file-and-lines link; rest versus edit; context menu; outline out of the app; core acceptance record and decision, accept/unaccept commands, material checks from the host's realignment reads, publication gated on Accepted, codec v11 with carry, Overview and agent status speak acceptance; RFC-BPR-0042/0043/0096 amended; tests; app pin resealed. Product candidate |
| `dee2b7b90` | BPR-2013 Done (AC01, AC06, AC07) |
| `5c086a178`, `dd766bd4f` | Ready and claim BPR-2014 (its edits are in `93b1045a8`) |
| `2443a73b8` | BPR-2014 parked `human-review` (AC02 to AC05 pass, AC08 and the critical review pending) |

Runs at the candidate: every `tour` ctest row, every `core_` and `sqlite_`
row, the Qt stop, drawer, keyboard, workbench window and overview rows (258
rows), all fourteen publication and CLI courses,
`application_composition_source_contract`,
`application_review_round_trip_composition_contract`,
`architecture_boundaries`, `test_network_isolation_manifest`,
`coding-format-check`, documentation gate. Baseline failure kept:
`qt_drawer_parts_scale_*` ("a writer sees an empty field", from `3d97ea548`,
where Overview's Solution became a Markdown view). Fixed while here, both
from sibling rounds: `application_review_tour_writing`'s diagnostics check
(BPR-2004 added fields after `plugins=`) and the `qt_tour_click_scroll` pencil
now holds the scroll.

Gap: no fixture has a readable second head for a tour route, so AC04 is shown
on real comparisons in the core and, through the window, only as "not compared
yet". Decisions for the coordinator: the terminal walk's Ready accepts (it
shows the code); a historical stop's pinned bytes are not re-checked; changed
rings carry a small mark.

Filed, awaiting the owner (not waited on; the URL stays in the session
state): session `679d26abbfa93b3357729061432131ba`, state
`/Users/jaeger/.attended-review-state/tour-stops-v002-2026-11-10/state`,
cursor 15, host PID 61912 (its `tailscale serve` child 61936), five
candidates: `BPR-2013-arrival`, `BPR-2013-stop-at-rest`,
`BPR-2013-stop-editing`, `BPR-2014-accept`, `BPR-2014-ring-states`. Frozen
root `/Users/jaeger/tour-stops-v001.5rt9c5gn` (app `c5e2a178c2ea…`). An
earlier attempt, session `5aedae7b…`, was never served and its state removed.

Owed: the critical review of BPR-2014's publication rule (not
self-certifiable). Disk: 73 stale non-tour e2e binaries removed from this
worktree's build (list in the coordinator report; relinked on demand).

Next: read session `679d26ab…` after hand-back. Accept of all five: finish
BPR-2014, stop host 61912, remove the root. BPR-2015 (RFC-BPR-0106 closeout)
belongs to `accepted-scope`. Next eligible milestones in plan order:
`agent-scratch` (unblocked), `tour-conversation` (needs `tour-stops`
accepted).

## Enhance on comments and notes: BPR-2051 (2026-11-10)

Owner request, on the same lane; nothing pushed. Owner amendments in force as
above. Frontend-design was loaded before the Qt work. Record:
[RFC-BPR-0106, Enhance on comments and notes](../../../verification/RFC-BPR-0106.md#enhance-on-comments-and-notes-bpr-2051-2026-11-10).

| Commit | What |
|---|---|
| `d3517ccac`, `b35139c5d` | BPR-2051 reserved against main at `809d0296a` (receipt `BPR-2051--BPR-2051--efa66a8a01d3.json`, main restored), readied and claimed |
| `821307015` | Enhance on a draft comment, a reply and a note (icon in the field's tool row, Agent accent; suggestion under the field with Discard and Use suggestion); three Enhance prompts as writing hooks with defaults, Reset, versions, purpose IDs and digests; a stop's Enhance sends its own prompt; profile schema 25; RFC-BPR-0106 and 0107 amended; app pin resealed; counter to main's BPR-2056. Product candidate |
| this commit | BPR-2051 parked `human-review` (scopes 1 to 6 pass; owner look pending) |

How it fits RFC-BPR-0107: the prompts are writing hooks in the same Built-in
prompts list, store and revision, so the exchange takes them as purposes
`enhance.tour-stop`, `enhance.comment`, `enhance.note` (existing hooks:
`tour.problem`, `tour.solution`, `tour.outline`, `tour.stop-comment`,
`review.feedback`), each with its own built-in version and SHA-256 digest
(`application::writing_hook_purpose`, `default_writing_hook_digest`). Not
stored for any prompt yet: which built-in version an override was written
against; that record belongs with the exchange (share-agent-prompts).

Filed, awaiting the owner (not waited on; the URL stays in the session
state): session `ac6e3a324ea3472583fe1d6e82c300f6`, state
`/Users/jaeger/.attended-review-state/enhance-v001-2026-11-10/state`, cursor
9, host PID 77843 (its `tailscale serve` child 77881), candidates
`BPR-2051-icon` (option list of three icons), `BPR-2051-placement`,
`BPR-2051-prompt-editor`; frozen root `/Users/jaeger/enhance-v001.dzk_0xf4`
from `821307015` (app SHA-256 `f91330402280…`, launcher `--check` passed;
the look is the composed course with `--enhance STUB --attended`).

Disk: 72 non-tour e2e binaries in this worktree's `build/dev` (about 13 GB)
were removed twice (a full dev build relinks them); build named targets.

Not touched: sessions `679d26ab…` (61912/61936), `26d89bac…` (36810/36892),
host 74692, BPR-Test #151/#152 and the `-next` branch.

Next: read session `ac6e3a32…` after hand-back. An icon other than option 1
sets `CurrentStyle`'s default in `enhance_control.cpp`. Accept of placement
and the prompt editor: finish BPR-2051, stop host 77843 and remove the root
with `freeze_attended_build.py remove`.

## tour-stops: critical review round (2026-11-10)

An independent reviewer (read-only) returned pass-with-findings, provisional,
on `93b1045a8`. Each finding was reproduced by a test against the unfixed code,
then fixed or recorded; dispositions are in the
[record](../../../verification/RFC-BPR-0106.md#critical-review). Nothing
pushed. Owner amendments in force as above.

| Commit | What |
|---|---|
| `a4643168d` | `qt_drawer_parts_scale_*` read Overview's Markdown Solution (test only; baseline failure gone) |
| `55551a90c` | Manifest entry for BPR-2051's `application_enhance_comment_note_public_path`; counter to main's BPR-2058 (LIVE_IMPORT) |
| `d02f13efb` | Findings 1-4, 7, 8 fixed with their tests; RFC-BPR-0106 implementation notes; app pin resealed. Product candidate |
| this commit | Accept's added sentence in plainer words ("Choose its lines again to accept it."); record, BPR-2014 card, handoff |

Tests that reproduced the findings: `staged_review_session`
(`test_update_saved_before_acceptance_resumes`; `staged_review_session` had
not passed at `93b1045a8`), `core_tour_stop_acceptance`
(`test_walk_ready_records_references`, `test_reaccept_keeps_own_lines`,
`test_carry_grants_only_the_author`), `sqlite_review_tour_storage` (relayed
version 10 stop), `sqlite_tour_owner_channel` (version 6 refused). Finding 7
was read from the code; no course drives a failing realignment read. Findings
5 and 6 are recorded, unchanged.

Runs at the candidate: every `core_` and `sqlite_` row, every `tour`,
`staged` and `publication` row, Qt workbench window and drawer rows,
`application_composition_source_contract`,
`application_review_round_trip_composition_contract`,
`architecture_boundaries`, `test_network_isolation_manifest` (248 rows): all
pass, except `application_review_tour_publication_mixed` under `-j4` load
(a 20 s wait; passes alone three times in 3.3 s; Debug timing, nongating).
Format check on changed files and the documentation gate pass.

Visible change: Accept is disabled, with "Choose its lines again to accept
it.", on a stop whose lines changed or are gone at the new head with nothing
mapped there. The filed look (`679d26ab…`, frozen from `93b1045a8`) shows no
such state; not refiled. Frontend-design was loaded after that change, to
review it, not before.

Owed: a re-review of these fixes by an independent reviewer; the owner's
looks `679d26ab…` and `ac6e3a32…` (not touched, nor `26d89bac…`, host 74692,
BPR-Test #151/#152 or the `-next` branch).

Next: as above (read `679d26ab…` and `ac6e3a32…` after hand-back), and file
the re-review.

## Owner feature round: Draft all (BPR-2061) and postable suggested comments (BPR-2059) (2026-11-10)

Owner requests, relayed by the coordinator; nothing pushed. Owner amendments
in force as above. Frontend-design was loaded before the Qt work. Evidence is
on the cards ([BPR-2061](../../60-done/BPR-2061-draft-all-suggested-comments-from-a-reply.md),
[BPR-2059](../../60-done/BPR-2059-suggested-comments-read-as-postable-comments.md));
RFC-BPR-0092 and RFC-BPR-0107 amended.

| Commit | What |
|---|---|
| `b583bb919`, `388466350`, `8863decf4` | BPR-2058/2059 reserved against main at `86653eacd`, readied and claimed |
| `227174441` | Suggestions fixture agent (test only) |
| `52e085d57` | Draft all (core plan, Agent tab row, host batch and Undo); Markdown cards, inset suggestion, chips keep width, anchor elides; replay counts off the continuation line; Suggested comments writing hook (`review.suggested-comment`, `suggested-comment-v1`), `brief.comment_format`, review skill v5, profile schema 26; pin resealed |
| `a823552f6`, `832254a71` | Tour CLI test lists the ninth hook (not run: tour-cli preset not built); settings render hook |
| `e4aeb679e` | Composed-path fixes (skipped stays skipped; Agent tab shown again after the batch); the agent look finishes setup on a fixture `codex` and `--attended-agent-draft-all-check`. Product candidate, frozen |
| `38ef8e5e0`, `f7668858f` | Collision: main reserved BPR-2058 meanwhile; Draft all renumbered BPR-2061 (LIVE_IMPORT) |
| this commit | Both cards parked `human-review`; handoff |

Composed check (`e4aeb679e`, dev app, offscreen): Draft all (3) → "Drafted 3
private drafts. Skipped 1: 1 no longer in the diff." → Undo "Removed 3
drafts." Focused set (29 rows) passed at `52e085d57`;
`qt_workbench_window_scale_100/125/200`, round-trip contract and
`architecture_boundaries` again after `e4aeb679e`/`f7668858f`. Not run (disk):
tour-cli preset, the e2e `application_agent_*` courses, core-only preset,
`coding-tidy`. Disk: five stale non-tour e2e binaries removed from
`build/dev/tests/end_to_end` (tour ones kept for the critical reviewer).

Decisions for the coordinator or owner: the review skill no longer asks for a
Conventional Comments header (BPR-1861 design point 1); the format prompt
governs it, and a label the agent does write is still read into the editor
and shown beside the card's caption. The replay counts stay as label
properties and in the tooltip when nonzero; the typed diagnostic log has no
free-text field, so no new diagnostic event. The default drawer is narrower
than a card's action row, so cards clip on the right (before and after alike).

Filed, awaiting the owner (not waited on; the URL stays in the session
state's `serve.log`): session `fbdd40b2768761881d2a420844cb4ec2`, state
`/Users/jaeger/.attended-review-state/suggested-comments-v001-2026-11-10/state`,
cursor 9, host PID 7089 (its `tailscale serve` child 7121), candidates
`BPR-2058-draft-all` (card BPR-2061), `BPR-2059-card-formatting` (before/after
renders inline), `BPR-2059-prompt-default`; frozen root
`/Users/jaeger/suggested-comments-v001.irz2s4en` from `e4aeb679e` (executable
SHA-256 `642b4a6c43d9…`, launcher `--check` passed, ready line
`ATTENDED_AGENT_SHOWN:findings=5`).

Not touched: sessions `ac6e3a32…`, `679d26ab…`, `26d89bac…`, host 74692,
PID 10695, BPR-Test #151/#152, the `-next` branch.

Next: read `fbdd40b2…` from cursor 9 after hand-back. Accept of all three:
finish BPR-2061 and BPR-2059, stop host 7089 and remove the root with
`freeze_attended_build.py remove`. A Reject goes to a fresh implementer with
the note.

## tour-stops: re-review round (2026-11-10)

An independent re-review of `d02f13efb`/`fb94545ec` returned
pass-with-findings, nothing blocking. Each of its five findings was taken test
first; dispositions are in the
[record](../../../verification/RFC-BPR-0106.md#re-review-of-the-fixes-2026-11-10).
Nothing pushed; no IDs allocated. Owner amendments in force as above.
Frontend-design was loaded before the one Qt change (item 4).

| Commit | What |
|---|---|
| `e06af4246` | Finding 1: the tour keeps earlier observed heads a stop has its lines at (`EarlierHeads`), so such a stop is compared, never Accepted with nothing to compare; tour payload version 12, owner protocol 8. Finding 5: the v10 staged-record test also with Ready stops |
| `816864eb5` | Finding 2: `tour_changed_paths` (only the tour owner's pull request's read), host wiring pinned by a pty walk and a forwarded walk and window Accept against a real desktop owner; `bpr tour status` reports `acceptance`. Finding 3: `tour_material_ledger` (dropped reads asked again uncounted; failed or verdict-less reads at most three times). Finding 4: one accept need with the changed files for core and button. Stale never-built tests brought up to date; app pin resealed. Product candidate |
| this commit | Handoff |

Tests: `core_tour_stop_acceptance` (`test_intermediate_head_stop_is_compared`,
accept need with files), `sqlite_review_tour_storage` (v12/v11),
`sqlite_tour_owner_channel` (v7 refused), `staged_review_session`
(`test_update_saved_before_acceptance_resumes`, Ready case),
`application_review_tour_host` (ledger, compared, changed paths),
`qt_tour_stop_acceptance_card`, `tour_cli_process_commands` (walk),
`application_profile_tour_handoff_public_path` (forwarded walk, window
Accept). Items 1 and 4 and every wiring test failed against the unfixed code;
item 3's ledger is new and was read from the code (no course drives a dropped
or failing realignment read).

Runs at `816864eb5`: 200 `core_`/`sqlite_`/`staged_`/tour CLI/application
rows; 69 tour, publication and workbench-window rows (Qt offscreen, both
tour e2e courses); every `tour_cli_*` row (the dev build now configures
`BPR_BUILD_TOUR_CLI=ON`); `application_composition_source_contract`,
`application_review_round_trip_composition_contract`,
`architecture_boundaries`, `test_network_isolation_manifest`; format check
on changed files; documentation gate. All pass.

Catch-up: the tour-cli rows had never run in this lane and the hooks count
there (and in the handoff course) still said five; `tour_publication_cli`
had not been updated for acceptance (its stops are now accepted, as Save
does); `agent_context_supplement` did not compile after BPR-2059. All fixed
and passing. The Draft-all round's e2e assertion
(`application_agent_invocation_composition_contract`, the brief's comment
format) passes, as do the ten other courses of that binary.

Disk: 3.9 GB free at the end (the shared compiler cache is 19 GB and grows
with any build). Removed from this worktree's `build/dev`, all relinkable:
`tests/end_to_end/profile_tour_handoff_public_path`,
`tests/end_to_end/review_tour_composition_public_path`,
`tests/tour_publication_cli`,
`tests/end_to_end/better_pull_review_agent_invocation_composition_public_path`.

Not touched: sessions `fbdd40b2…`, `ac6e3a32…`, `679d26ab…`, `26d89bac…`,
host 74692, PID 10695, BPR-Test #151/#152, the `-next` branch. The filed
looks were frozen before these changes; none of them shows the new reason
line ("Accept it once the pull request's files load."), which appears only
while the window has no read of the tour's pull request. Not refiled.

Owed: as before (the owner's looks; AC08). A further independent review of
`e06af4246`/`816864eb5` may be filed; it is not waited on.

## Owner feedback round: Enhance, finding cards, tour stops, freeze guard (2026-11-10)

Owner amendments in force as above, plus: merge main and use main's
theme-parity work; the tracker lives on main ("the cards should be on main").
Frontend-design was loaded before the Qt work. Nothing pushed.

Hand-back recorded and cleaned up first (`e183472b1`, `2679d0094`):
`679d26ab…` (tour stops: arrival, Accept, ring states accepted; stop at rest
accepted with a note; stop editing rejected), `ac6e3a32…` (Enhance: icon
rejected, "nib and spark"; placement and prompt editor accepted with
changes), `fbdd40b2…` (Draft all accepted, BPR-2061 Done; card formatting
rejected; prompt default accepted), `26d89bac…` abandoned (frozen from
`abba53926`, before the stop redesign): RFC-BPR-0099 AC08 and RFC-BPR-0105
AC08 not passed, moved to an agent-run live course. Hosts 61912, 77843,
7089, 36810 (and their tailscale children) stopped; roots
`tour-stops-v001.5rt9c5gn`, `enhance-v001.dzk_0xf4`,
`suggested-comments-v001.irz2s4en`, `bpr-live-v002.nnhzt2pb` removed (the
owner's app PID 46168 had exited). BPR-Test left as is.

| Commit | What |
|---|---|
| `5380330fb` | `freeze_attended_build.py` freezes only the checkout's clean HEAD unless `--not-head-because REASON`; marker, launcher and `SESSION_LINE:` name the frozen commit; contract tests |
| `90785ae34` | Nib glyphs (default: spark cut out), Enhance first under every field with short context tooltips, one `enhance.writing` prompt (schema 27 fold), Agents & context top in three options, stop editing reworked (Accept, equal Cancel, labelled tools), fenced-code assertion, `BPR_NATIVE_LOOK` renders |
| `9b67037b9` | Merge main `e5bb712ab` (finding-card memory, quiet buttons painting their own faces, release fixes); pin resealed; canary kept free of Qt; chat contract counts Enhance's launch |
| `4646358f6` | Merge `claude/agent-assistance-v1-import` (RFC-BPR-0116..0121, BPR-2073..2085; queued, not implemented) |
| `8ac77be96`, `02200853d` | Card records; the hint names where Link selected lines is when narrow (frozen) |
| main `634bd6fa7` | The tracker on main: all three packs, the lane's cards in their current columns, receipts, RFC amendments, records, design pages, NEXT_ID BPR-2086 |
| `2bedfe0db`, `89d6aa2d6` | Merge main back (tracker, qualification inventory fixes); the lane's workbench cases registered |

Theme parity: main's fix (BPR-2056, 2060, 2072) makes quiet buttons paint
their own faces in every style and measures them under Cocoa
(`qt_button_faces_cocoa`, `WA_DontShowOnScreen`). The lane-specific gap was
the renders: offscreen meant Fusion, always Light, another font database.
`native_look_startup.cpp` applies main's Cocoa-off-screen procedure to any Qt
test under `BPR_NATIVE_LOOK=1` (documented in `src/adapters/qt/AGENTS.md`);
this round's renders use it. Cancel/Save size was the lane's own (a chip
beside a native button), fixed.

Runs: `freeze_attended_build_contract`; `qt_native_look_{light,dark}`; 43
Qt rows (tour, enhance, settings, workbench window scales, agent, notes,
cards, button faces, drawer, side chat); `qt_agent_launch_canary`; every
`core_`, `sqlite_`, application writing/skill rows and `tour_cli_*`
(206); 26 `application_agent_chat_*`/`application_agent_finding_*` rows (the
earlier baselines now pass); `application_review_tour_composition_public_path`,
`application_enhance_comment_note_public_path`,
`application_profile_tour_handoff_public_path`; app source and round-trip
contracts, `architecture_boundaries`, `test_network_isolation_manifest`,
`command_correctness_registry_contract`, qualification inventory script;
documentation gate (lane and main).

Filed, not waited on (URL in the session state's `serve.log`): session
`5ea0ce22c14ed5083b9d2e7382717df9`, state
`/Users/jaeger/.attended-review-state/owner-look-v003-2026-11-10/state`,
cursor 18, host PID 78319 (tailscale child 78332), six candidates:
`BPR-2051-icon-v2` (options), `BPR-2051-agents-top` (options),
`BPR-2051-placement-tooltip`, `BPR-2059-code-blocks`,
`BPR-2013-stop-at-rest-v2`, `BPR-2013-stop-editing-v2`. Frozen root
`/Users/jaeger/owner-look-v003.w0k3mfr3` from `02200853d` (app SHA-256
`667c03614657…`, launcher check reached `ATTENDED_TOUR_SHOWN`).

Next: read `5ea0ce22…` after hand-back. An icon other than 1 sets
`CurrentStyle` in `enhance_control.cpp`; a top other than 1 sets
`AgentsContextTop`'s default (option 2 drops the controls, so the settings
search test for "Private draft" changes). Accept of placement, code blocks
and both stop looks: finish BPR-2051, BPR-2059 and BPR-2014 (AC08), stop host
78319, remove the root. Card moves from now on are mirrored to main.

## Live-evidence round: agent-run live course (2026-11-10)

Owner amendments in force as above, plus: "I mean the cards should be on
main" (every card move mirrored to main; rule added to `AGENTS.md`), and
RFC-BPR-0099 AC08 and RFC-BPR-0105 AC08 moved off the owner to an agent-run
live course ("way too many manual steps"). Nothing pushed; no pull request
other than disposable BPR-Test ones.

| Commit | What |
|---|---|
| `c3765a50f`, `df5216947`, `15a3709bc` | Owner look `5ea0ce22…` folded in (handed back at sequence 49): stop at rest, icon (option 1, already the default), placement and code blocks accepted; stop editing (`1d39ba1e`: the text at the bottom wastes space; no "Enhance" label beside the icon) and Agents & context top (`06aceb8f`: "all ugly", mockups by a deep worker with frontend-design) rejected, recorded on BPR-2014 and BPR-2051 and not implemented (next layout round). BPR-2059 Done, moved by hand: `issue_board.py finish` refused because the protected RFC-DRAFT-0003 asset linked the parked path; that one link repaired. Host 78319/78332 stopped, root `owner-look-v003.w0k3mfr3` removed |
| `823b3c796` | `AGENTS.md`: mirror every card move to main |
| `33c282ccd` | `DesignatedLivePublication` (tests-enabled, no fixture, BPR-Test only: shipped live provider and the gesture's one-use grant); `live_second_account_course --course live-session` and `tests/end_to_end/live_session_course.py`; fix: tour repair and acceptance comparisons between two heads read each side as a pinned source (sparse reviews could never compare a stop after a push); `application_epoch_host` test; app source digest resealed; `tests/AGENTS.md` |
| `e88e6a90d` | RFC-BPR-0105 AC08 amended; records, card notes, closeout rows (BPR-2012, BPR-1994) |
| `a87f08b10`, `8d6e6b3d8` | BPR-2010 and BPR-1992 Done |

Evidence run (2026-11-11 06:20–06:28 UTC, `live_session_course.py --reads
1,2,3,43 --approve 151 --agent /opt/homebrew/bin/claude --cleanup`): six pull
requests in one window, a Haiku question from #1's Agent tab, a two-stop tour
on disposable #177 published as a mixed tour (stop 2 the reason) in six
steps waiting/sending/sent with the head's count equal to the panel's, then
Approve on #151. `gh`: #177 has one COMMENT review (5854708476) carrying stop
1, one conversation comment (6788770868) for stop 2 at the earlier head, the
review body as the tour index, the description linking the review; #151 has
MikeMarcin's APPROVED review 5854726479. #177 closed and its branches
deleted; every debug run's pull requests (#153–#176) closed and branches
deleted (upstream and the test account's fork); #151, #152 and the `-next`
branch untouched otherwise. Rate limit 2,841 remaining at the end.

Runs: `application_epoch_host`; the 17 tour publication, tour host, tour
stop acceptance, tour composition, profile handoff and dependable-session
rows; `application_composition_source_contract`,
`application_review_round_trip_composition_contract`,
`architecture_boundaries`, `test_network_isolation_manifest`; format check
on the changed C++; documentation gate (lane and main).

Milestones: `dependable-session` (BPR-2010, BPR-2011) and `tour-publishing`
(BPR-1991, BPR-1992, BPR-1993) have every card Done; their closeouts BPR-2012
and BPR-1994 belong to `accepted-scope`.

Observed, not changed (no card allocated): during publication the tour's
status line still reads "Saved locally. Nothing published."; Home does not
show while the Tour page is open; an open from Home during "Refreshing the
pull request." after a publication is dropped silently; the tour's "Drafting
help needs the agent." band shows with an agent set up; the Agent tab never
reported idle before the course's question.

Owed: BPR-2014 (stop editing look, critical review) and BPR-2051 (Agents &
context top) wait on the next layout round. Evidence files stay outside the
repository (the session scratchpad root `evidence1`); nothing of them is
tracked.

Next: the coordinator's choice among `agent-scratch`, `tour-conversation`
(needs `tour-stops` accepted) and the layout round for the two rejections.

## Layout round: agent footer, Agents roster, stop editing, tour panel (2026-11-11)

Owner decisions: "A + footer 1" (mockups A · Roster and footer 1 · In the
status bar, accepted design in
[docs/design/BPR-2051-agents-roster-and-footer](../../../design/BPR-2051-agents-roster-and-footer/README.md));
BPR-2014's stop-editing reject (`1d39ba1e`); "Things are moving around as I
click them, buttons packed in like a clown car" on the tour panel. Owner
amendments in force as above. Frontend-design was loaded before the Qt work.
Nothing pushed.

| Commit | What |
|---|---|
| main `43203ba7c` | BPR-2088..2090 reserved and carded (Backlog): live-course bugs b, d, e |
| `f51063b5b` | Agent footer (one picker per pull request, run with place and Stop, several runs, quiet tone, ending, session cost, setup state); drawer pickers, Tour agent bar and setup band removed; Enhance's Stop stops the tour lane; roster with kept checks (`EntryChecks`, `agent_entry_checks`), Check all, row menu, drag; next-request choices off the Settings page; stop editing (no hint, icon-only Enhance); tour panel layout pass; status line no longer "Nothing published." while publishing; app pin resealed. Product candidate |
| `be50866a1` | Design record, RFC amendments (0101, 0103, 0104 with BPR-2009's checklist, 0106, 0107), `agent_footer --look`, `--agents-roster-look`, BPR-2088..2090 mirrored |
| `3285ec887` | BPR-2051 and BPR-2014 records. Frozen |
| this commit | Session recorded on the cards; counter to main's BPR-2092; handoff |

Live-course bugs: (a) fixed (routine save text no longer in the panel or
wrong while publishing); (c) gone with the setup band; (b) BPR-2088,
(d) BPR-2089, (e) BPR-2090, all Backlog on main.

Runs at `f51063b5b`/`be50866a1`: `qt_agent_footer` (new; replaces
`qt_agent_picker_wrap`), 39 Qt tour/agent/window/settings rows, 55
application and contract rows (composition source and round-trip
contracts, `architecture_boundaries`, network-isolation manifest,
`application_agent_chat_*`, `application_agent_finding_*`, tour courses,
`application_agent_setup_host`, `sqlite_agent_setup_storage`,
dependable session), `application_enhance_comment_note_public_path`,
`application_agent_invocation_composition_contract`,
`application_profile_tour_handoff_public_path`, `clang-tidy` on the new
and changed host and Qt sources, format check, documentation gate. Not run:
core-only preset (no core change), the whole dev suite.
`test_better_pull_review_qt_agent_launch_canary_tests` and
`..._claude_process_wrapper_tests` do not compile in `tests/adapters_qt/all`
(native_look_startup.cpp needs QtWidgets they do not link); not this
round's change, not built by the dev preset's default course.

Native renders (Cocoa off screen, light and dark) are in the session's
candidates; scratch copies in the session scratchpad `layout-round/`.

Filed, not waited on (URL in the session state's `serve.log`): session
`7c9206759d935f663d6a13ce0e008afc`, state
`/Users/jaeger/.attended-review-state/layout-round-v001-2026-11-11/state`,
cursor 12, host PID 73758 (tailscale child 73768), four candidates, ready
and protected: `BPR-2051-roster-top`, `BPR-2051-agent-footer`,
`BPR-2014-stop-editing-v3`, `tour-panel-layout-v1`. Frozen root
`/Users/jaeger/layout-round-v001.5q22yahv` from `3285ec887` (app SHA-256
`97d0861e19dc…`, launcher check reached `ATTENDED_TOUR_SHOWN`); its build
holds the three look helpers.

Disk: 78 relinkable e2e binaries removed from this worktree's
`build/dev/tests/end_to_end` (about 14 GB) to make room for the freeze.

Not touched: host 74692, PID 10695, BPR-Test #152 and its `-next` branch.

Next: read `7c920675…` from cursor 12 after hand-back. Accept of the roster
and footer finishes nothing yet (BPR-2051's other looks are accepted; finish
it when both are). Accept of both tour looks satisfies BPR-2014's AC08 look;
its critical review is still owed. Then stop host 73758 and remove the root
with `freeze_attended_build.py remove`. Not built in this round: the
mockup's restyled agent sheet, RFC-BPR-0107's Import/Export (placement
recorded), RFC-BPR-0104's grant item (placement recorded), a fixture with
configured agents and a running run in the frozen app (the looks stand in).

## Drafts round: find my drafts, Overview, Finish review (2026-11-11)

Owner requests: "I don't seem to have an easy way to find all my draft
comments"; Home: "I dunno if I'd put it there. I feel like discuss or threads
is probably better" (no Home change); "finish review should note draft comments
that haven't been added to the review"; Overview: "we already have a comments
column so probably something there". Owner amendments in force as above.
Frontend-design was loaded before the Qt work. Nothing pushed.

How drafts worked: a saved line comment is a private draft; Start a review /
Add review comment gives it a pending mark (path at the head) and Submit sends
it; without the mark it is "not in the review" and only its own Publish sends
it; a reply is never in a review. A draft recorded no file, so a draft in a
file not shown this session could not be listed by file.

| Commit | What |
|---|---|
| main `783dac754` | BPR-2092..2094 reserved and carded (counter BPR-2095) |
| `d61b3ee42`, `61aad5dec`, `34d96d2dc`, `93c754fd1`, `2b3b496ab`; main `e76a725a8` | Cards to the lane, Ready, claimed; claims mirrored |
| `7cd7fab2b` | Core `review_draft_place` and `draft_roster`; private-store record v4; host place stamping, roster, Add/Add all/Leave out; Threads drafts rows and My drafts; Overview drafts chip; Finish review drafts section; `--attended-drafts-look` (+ `BPR_ATTENDED_DRAFTS_CHECK`); native-look startup only for Qt Widgets tests (fixes `agent_launch_canary`, `claude_process_wrapper` compiling); app pin resealed. Product candidate |
| `ee4724b2a`; main `9b0aa6423` | Card records (BPR-2092 AC4 amended: My drafts is not remembered); mirrored. Frozen |
| this commit | Cards parked `human-review`; session recorded; handoff |

Placement: Threads (file groups, filter bar, goes to a line); not a close call
against Discuss, so no option list; a Reject note can ask for Discuss.

Runs at `7cd7fab2b`: `core_draft_roster` (new), `application_comment_range_draft_store`
(new place round trip), `qt_conversation_finder_scale_100` (new
`test_my_drafts`), `qt_pull_request_overview_page` (new drafts chip test),
`qt_pull_request_overview_chips_scale_125`,
`qt_review_submission_editor_scale_{100,125,200}`, `qt_agent_launch_canary`,
`qt_agent_launch_canary_durable_course`, `qt_claude_process_wrapper`, and the
draft/review/composition rows: `core_{draft_state,review_submission,revisioned_review_draft,private_review,reply_draft_standing}`,
`application_{review_submission_contract,review_submission_application_host,shared_draft_application_host,private_review_host,feedback_draft_store,reply_draft_survival,composition_source_contract,review_round_trip_composition_contract,draft_state_contract,private_draft_restore,reply_draft_contract,start_review_contract,draft_delete_contract,line_draft_contract,review_submission_composition_public_path}`,
`sqlite_{private_review_storage,review_submission_recovery_storage}`,
`qt_{workbench_line_draft,workbench_overview,workbench_window_scale_100,review_draft_editor,private_review_panel,feedback_label_threads}`,
`architecture_boundaries`, `test_network_isolation_manifest` (39 rows, all
pass); composed public path `--attended-drafts-look` with
`BPR_ATTENDED_DRAFTS_CHECK=1`: THREADS 5,2; OVERVIEW 3,2; OUTSIDE 2; ADDED 5;
`coding-format-check`; clang-tidy on the new and changed core, Qt and host
sources; documentation gate. Native renders (Cocoa off screen, light and
dark) are in the session's candidates.

Filed, not waited on (URL in the session state's `serve.log`): session
`585d18246da0bd78411dee4b26777a2d`, state
`/Users/jaeger/.attended-review-state/drafts-round-v001-2026-11-11/state`,
cursor 9, host PID 24810 (tailscale child 24818), three candidates, ready and
protected: `BPR-2092-threads-drafts`, `BPR-2093-overview-drafts`,
`BPR-2094-finish-drafts-line`. Frozen root
`/Users/jaeger/drafts-round-v001.yva0tan3` from `ee4724b2a984` (SHA-256
`56ba5f7f9482…`, launcher check reached `ATTENDED_FINISH_REVIEW_SHOWN`).

Not touched: session `7c920675…` (host 73758/73768, root
`layout-round-v001.5q22yahv`), host 74692, PID 10695, BPR-Test #152 and its
`-next` branch.

Known limits: a draft saved before this change has no recorded place until it
is edited or its file is shown in the window (Finish review says "Open it to
add it"); a left-side or filtered-diff draft is listed but not addable from
the sheet (open it). Pre-existing clang-tidy naming warnings in
`window_review_drafts.cpp` (`invalidate_all`, `live_factory`) left as they
were.

Next: read `585d1824…` from cursor 9 after hand-back. Accept of each finishes
its card (`issue_board.py finish`, mirrored to main); a Reject is a new
candidate. When all three are decided, stop host 24810 and remove the root
with `freeze_attended_build.py remove`.

## agent-scratch (2026-11-11)

Milestone `agent-scratch` (RFC-BPR-0104, [[BPR-2007]], [[BPR-2008]]), on the
same lane; nothing pushed. Frontend-design was loaded before the Qt work.
Ledger: [RFC-BPR-0104](../../../verification/RFC-BPR-0104.md). Owner
amendments in force as above.

| Commit | What |
|---|---|
| `5592956a4`, `b83072662`; main `26d1474eb` | Ready and claim BPR-2007 (BPR-2008 cannot be readied before BPR-2007 is Done; it was built under BPR-2007's claim). The protected `local-draft-authoring-v1/AMENDMENTS.md` links the card by lane path, so each move repaired that one link path by hand (the board helper refuses protected files) |
| `882ebed54` | Product candidate: core grant, launch arguments, scratch folders, storage, footer grant item, agent sheet settings, build question, folded Ran/Wrote lines, live `agent-scratch` course; app source pin resealed (its note says this is a grant) |
| `c19749f58` | Ledger, RFC-BPR-0104 implementation record, RFC-BPR-0011 grant amendment, card records. Frozen |
| `8a56ee009`; main `fa6adc3f0`, `ae0f44726` | BPR-2007 parked `human-review`; BPR-2008 records (stays Backlog); session recorded; handoff; mirrored with the RFC-BPR-0104 ledger the cards link |

Facts established (installed Claude Code 2.1.319, Codex 0.167.0): Claude takes
`--add-dir` and `--allowedTools` rules; `Bash(DIR/:*)` never matches a file in
the folder and no rule matches a quoted path, so the rules are globs and the
folder's path has no space (macOS: the user's cache folder, since "Application
Support" has one). Codex takes `-c permissions.NAME.filesystem={…}` with
`-c default_permissions="NAME"`, which replaces the configured sandbox mode and
is enforced by Seatbelt with network off; granted runs add
`approval_policy="never"`.

State: AC01, AC02, AC05, AC06, AC08 pass; AC09 passes except a separate New
session course; AC04 passes at unit, host and storage layers (the question's
composed path has no course); AC03 is partly met (the 256 MiB bound is checked
at turn start, not during a turn), asked of the owner; AC07 (independent
critical review of both agents' allowances) is pending and not done by the
implementer, whose self-review in the ledger is input to it.

Disk: 34 relinkable executables removed from this worktree's
`build/dev/tests/end_to_end` (about 7 GB; list in the session scratchpad
`deleted-binaries.txt`); `agent_invocation`, `agent_chat` and
`agent_finding` composition binaries were rebuilt for their contract rows.

Filed, not waited on (URL in the session state's `serve.log`): session
`e97c6a3d72ac570115fc961555b09d6d`, state
`/Users/jaeger/.attended-review-state/agent-scratch-v001-2026-11-11/state`,
cursor 12, host PID 72034 (Tailscale child 72042), four candidates, ready and
protected: `BPR-2007-grant-menu` (footer picker's grant item; `agent_footer
--look`), `BPR-2007-agent-sheet` (Scratch folder starts and Build and run
tests; `--agents-roster-look`), `BPR-2008-build-question` (the look's "Show
the build question…"), `RFC-BPR-0104-AC03-bound` (accept the turn-start
256 MiB bound, or reject to stop a turn that passes it). Frozen root
`/Users/jaeger/agent-scratch-v001.weg1sgre` from `c19749f58892` (app SHA-256
`cb283436e169…`, launcher check reached `ATTENDED_FILE_SHOWN`). The frozen app
itself opens conventional-comments-v1 with no agents added; the looks carry the
surface, as in the layout round. Native renders (Cocoa off screen, light and
dark) are the candidates' media.

Not touched: sessions `585d1824…` (24810/24818, root
`drafts-round-v001.yva0tan3`) and `7c920675…` (73758/73768, root
`layout-round-v001.5q22yahv`), host 74692, PID 10695, BPR-Test #152 and its
`-next` branch. A Tailscale serve process 44738 (parent 1) that is not this
round's was seen and left.

Pending: AC07 needs an independent critical review of both agents' launch
allowances (coordinator to file; the ledger's self-review is input). Next:
read `e97c6a3d…` from cursor 12 after hand-back; Accept of all four finishes
nothing alone (AC07 also). With the looks and AC03 accepted and AC07 passed,
finish BPR-2007, then ready, claim and finish BPR-2008, mirroring each to
main; a Reject goes to a fresh implementer. Then stop host 72034 and remove
the root with `freeze_attended_build.py remove`. BPR-2009 (closeout) belongs
to `accepted-scope`.

## agent-scratch post-review round (2026-11-11)

Fresh `rfc-implementer`, same lane; nothing pushed. Owner amendments in force
as above. Frontend-design was loaded before the Qt work.

Hand-backs read from each `session.json` and recorded; hosts stopped with
`attended_review.py stop` (73758/73768, 24810/24818, 72034/72042, all gone);
roots removed with `freeze_attended_build.py remove`
(`layout-round-v001.5q22yahv`, `drafts-round-v001.yva0tan3`,
`agent-scratch-v001.weg1sgre`).

- `7c920675…`: all Accept. BPR-2051 and BPR-2014 Done (RFC-BPR-0106 AC08
  met; its critical review and re-review are in RFC-BPR-0106's ledger). The
  footer note "Why is it red?": the frozen build had no agent checked, so the
  footer showed its setup state in the failed check's red; now quiet unless a
  check failed (asked again below).
- `585d1824…`: all Accept. BPR-2092, BPR-2093, BPR-2094 Done.
- `e97c6a3d…`: agent sheet and build question Accept; grant menu and AC03
  bound Reject (verbatim in the RFC-BPR-0104 ledger).

| Commit | What |
|---|---|
| `80b63a805` | Counter and reservations synced from main (BPR-2100) |
| `9c6e7811e`, `850bc3905`, `d98535a21`, `34122c7c0`, `54a1807df`; main `28d6a2cc7` | BPR-2092, 2093, 2094, 2051, 2014 Done; mirrored |
| main `06e187baa` | BPR-2101 reserved and carded (Backlog): Insights disk use, workspace cleanup, cleanup by pull request state |
| `cf8a9c12d` | Product candidate: picker toggles (Use scratch folder, Allow build and run tests) with tooltips; /model and /effort hints only where true; quiet setup state; Codex grant refused where it would replace the reviewer's profile or approval; Claude rules tightened (no direct-run or chmod, `Bash(*/..*)` denied); glob paths refused; picker and launch from one set of facts; build question re-checks the pull request; scratch limit a setting with a stop during a turn; unused folders removed after the kept days; Settings size and Clear; app pin resealed |
| `b2edc1fa3`; main `0958234d7` | RFC-BPR-0104 amendments, ledger, BPR-2007 record, BPR-2009 checklist; mirrored. Frozen |
| this commit | Live results in the ledger, BPR-2008 record, session, handoff |

Critical review findings 1–8: dispositions in the
[ledger](../../../verification/RFC-BPR-0104.md#dispositions). **A re-review of
the grant fixes by an independent reviewer is owed** (AC07 stays pending).

Runs: `core_agent_grant`, `application_agent_launch`,
`application_agent_scratch_folder`, `application_agent_invocation_host`,
`application_agent_setup_host`, `sqlite_agent_setup_storage`,
`qt_agent_footer`, `qt_agent_setup_view`, `qt_settings_window`,
`qt_agent_launch_panel`, `qt_agent_activity_view`,
`qt_workbench_agent_send_refusal`, `core_agent_entry`, `core_agent_activity`,
`application_agent_{chat,finding,invocation}_composition_contract`,
`application_agent_chat_custom_agent_contract`,
`application_composition_source_contract`,
`application_review_round_trip_composition_contract`,
`architecture_boundaries`, `test_network_isolation_manifest`; format check;
clang-tidy on the changed sources (two pre-existing warnings left:
`words_ink` naming in the picker, a concatenation at `agent_launch.cpp:1692`);
documentation gate (lane and main). Live: Claude rule probes by hand and the
`agent-scratch` course for Claude and Codex (all five probes refused, every
workspace clean). Not run: the whole dev suite, the core-only preset.

Rebuilt for these rows (not deleted): `live_second_account_course`,
`better_pull_review_agent_{chat,finding,invocation}_composition_public_path`.
Nothing deleted from `build/dev` this round (disk 50 GiB free).

Filed, not waited on: Session `246ef52c2815c6c16b3aeb5dacc5a765`, state
`/Users/jaeger/.attended-review-state/agent-grant-v002-2026-11-11/state`
(URL in `../serve.log` beside it), cursor 9, host PID 89403 (Tailscale child
89420). Frozen root `/Users/jaeger/agent-grant-v002.y_rhztpu` from
`b2edc1fa3b56` (launcher check reached `ATTENDED_PULL_REQUEST_OPENED`; the
looks `agent_footer --look` and `better_pull_review_qt_settings_window_tests
--agents-roster-look` are in its build). Candidates, one question each, ready
and protected: `BPR-2007-picker-toggles-v2`, `RFC-BPR-0104-scratch-settings`,
`BPR-2051-footer-setup-quiet`. Native renders (Cocoa off screen, light and
dark) are their media. Insights disk use has no candidate: it is BPR-2101,
not built.

Pending: the owner's three answers; an independent re-review of the grant
fixes (AC07); BPR-2101 (Backlog). Next: read `246ef52c…` from cursor 9 after
hand-back. Accept of the picker and the settings with a passing re-review
finishes BPR-2007 (then ready, claim and finish BPR-2008, mirrored); the
footer answer is recorded on BPR-2051 (Done; a Reject is a new card). Then
stop host 89403 and remove the root with `freeze_attended_build.py remove`.
Not touched: host 74692 (its Tailscale child 74706), PID 10695, BPR-Test #152
and its `-next` branch, Tailscale serve 44738.

Main mirror commits this round: `28d6a2cc7`, `06e187baa`, `0958234d7`, `fbf3c6be6`.

## share-agent-prompts (2026-11-11)

Milestone `share-agent-prompts` (RFC-BPR-0107, [[BPR-2016]], [[BPR-2017]],
[[BPR-2018]]), same lane; nothing pushed. Frontend-design was loaded before
the Qt work. Ledger: [RFC-BPR-0107](../../../verification/RFC-BPR-0107.md).
Owner amendments in force as above, plus the owner's decision to export the
shipped defaults too, recorded in the RFC with the nine recognized purposes and
the written-against record.

| Commit | What |
|---|---|
| `b827cd0db` | Counter and BPR-2102 reservation synced from main |
| `f7bd397eb`, `985d3bf9d` | Ready and claim BPR-2016 |
| `7ae9b389c` | Product candidate: core format and import decisions (glaze after `json_preflight`), `application::agent_configuration_host`, all-or-nothing store across `agent-setup.sqlite3` and the workbench settings (journal, recovery at profile start, `changed_since`), written-against record for hook overrides (Settings and `bpr` saves), Settings share row and both dialogs, `--attended-share-look` and its two composed checks, committed import fixture `tests/fixtures/agent-configuration/team-agents.bpr-config.json`, RFC owner decision and implementation record, app pin resealed (no grant change) |
| `9be81a9c2` | Ledger. Frozen |
| `4ef38b5c1`, `4213f98d8` | BPR-2016 record; Done (AC02, AC03, AC04, AC05, AC07) |
| `fd3f112b9`, `61bd263de`, `6ef228058`, `677d7b0a3` | Ready, claim, record BPR-2017; parked `human-review` (AC01 pass, AC06 pending) |
| `d9131f5ca` | BPR-2018 checklist filled; stays Backlog until BPR-2017 is Done |

Runs: see the ledger (`core_agent_configuration`, `sqlite_agent_configuration_storage`,
`qt_agent_configuration_dialogs` new; agent setup, review skill, workbench
preferences and settings rows; composition source contract;
`architecture_boundaries`; network-isolation manifest (new uncontained row
added); format check; clang-tidy on the new sources). Composed courses through
the real app, run by hand: sender `ATTENDED_SHARE_CHECKED`, recipient
`ATTENDED_SHARE_RECEIVED` (ledger). Not run: whole dev suite, core-only preset,
Windows and Linux.

Filed, not waited on (URL in the session state's `../serve.log`): session
`629d64c72746157219d2473892973d7f`, state
`/Users/jaeger/.attended-review-state/share-agent-prompts-v001-2026-11-11/state`,
cursor 9, host PID 76865 (Tailscale child 76875), three candidates, ready and
protected: `BPR-2017-export-selection`, `BPR-2017-import-preview-bind`,
`RFC-BPR-0107-default-prompt-round-trip`. Frozen root
`/Users/jaeger/share-agent-prompts-v001.h9i51jwq` from `9be81a9c2141` (app
SHA-256 `1088de08cc6a…`, launcher check reached `ATTENDED_SHARE_SHOWN:agents=2`).
The import question names
`/Users/jaeger/share-agent-prompts-v001.h9i51jwq/src/tests/fixtures/agent-configuration/team-agents.bpr-config.json`.

Not touched: session `246ef52c…` (89403/89420, root
`agent-grant-v002.y_rhztpu`), host 74692/74706, PID 10695, Tailscale 44738,
BPR-Test #152 and its `-next` branch.

Next: read `629d64c7…` from cursor 9 after hand-back. Accept of all three
finishes BPR-2017 (AC06 macOS; record Windows and Linux as not run or run them
on a station), then BPR-2018 closes RFC-BPR-0107 (ready, claim, finish,
mirrored). A Reject goes to a fresh implementer. Then stop host 76865 and
remove the root with `freeze_attended_build.py remove`. Known limit: another
open window of the same profile can write its older agent list over an import
(true of any agent edit today).

Main mirror commit this milestone: `031ea2cb6`.

## agent-scratch second post-review round (2026-11-11)

Fresh `rfc-implementer`, same lane; nothing pushed. Owner amendments in force
as above. An independent re-review of `cf8a9c12d` returned
pass-with-findings (N1 medium; N2 to N6 low). No Qt layout change, so
frontend-design was not loaded.

| Commit | What |
|---|---|
| `531a039cd` | Product candidate: Codex refused in the reviewer's own checkout (core and host); configuration read from the home's `.codex` (no `CODEX_HOME`), `/etc/codex/config.toml` and the managed file; argument allowlist parsed as Codex parses it; BOM stripped; inline tables under `profiles` refused; scratch state per pull request and agent, kept folder stored (`agent_scratch_folder`) and read on open; Clear keeps the open pull request's folders; per-runner parent-step denial only with a writable folder; a full folder gives Claude no run rule; new course `application_agent_chat_custom_agent_refused_contract`; app pin resealed (narrows a grant) |
| this commit | Ledger (finding 1 corrected to partly fixed; N1 to N6 dispositions), RFC-BPR-0104 amendment, BPR-2007 and BPR-2008 records, handoff; mirrored to main |

Evidence: [ledger](../../../verification/RFC-BPR-0104.md#re-review-of-the-fixes-2026-11-11).
N1a, N1b, N1c and N5 reproduced live (`codex sandbox` with an isolated
`CODEX_HOME`; `claude -p --model haiku --setting-sources project`); N5's
fix probed live. 29 focused rows pass (list in the ledger), format check,
clang-tidy on the changed sources, documentation gate. Mutation: with the
window's refusal removed, the new refused course fails. Not run: whole dev
suite, core-only preset, live `agent-scratch` course.

Picker: visible states change only where no filed look shows (Codex in the
reviewer's own checkout, a folder that could not be made, Claude's full
tooltip); no new attended session.

Not touched: sessions `629d64c7…` (76865/76875, root
`share-agent-prompts-v001.h9i51jwq`) and `246ef52c…` (89403/89420, root
`agent-grant-v002.y_rhztpu`), host 74692/74706, PIDs 10695 and 39804,
Tailscale 44738, BPR-Test #152 and `-next`. No live GitHub use.

Pending: another independent re-review of `531a039cd` (AC07 stays pending);
the owner's answers in `246ef52c…`; BPR-2101 (Backlog). Next unchanged from
the post-review round: read `246ef52c…` from cursor 9 after hand-back;
Accept of the picker and the settings with a passing re-review finishes
BPR-2007, then ready, claim and finish BPR-2008, mirrored.

Main mirror commit this round: `feacae49f`.

## Owner hand-backs 246ef52c and 629d64c7 (2026-11-11)

Both handed back, all Accept, no notes (`session.json`). `246ef52c…`:
`BPR-2007-picker-toggles-v2`, `RFC-BPR-0104-scratch-settings`,
`BPR-2051-footer-setup-quiet` recorded on RFC-BPR-0104's ledger, BPR-2007 and
BPR-2051; BPR-2007 and BPR-2008 stay unfinished until the independent
re-review of `531a039cd` passes (AC07). `629d64c7…`: BPR-2017 Done (AC06 on
macOS; Windows and Linux not run), BPR-2018 readied, claimed and Done:
RFC-BPR-0107 Auto-Verified. Hosts 89403/89420 and 76865/76875 stopped with
`attended_review.py stop`; roots `agent-grant-v002.y_rhztpu` and
`share-agent-prompts-v001.h9i51jwq` removed with `freeze_attended_build.py
remove`. Lane `24f28d60a`, `d311b8c14`, `f65e12b6c`, `3e0a57454`,
`0d882c8c2`; main `b0bf01049`.

## tour-conversation (2026-11-11)

Milestone `tour-conversation` (RFC-BPR-0100, [[BPR-1995]], [[BPR-1996]]),
same lane; nothing pushed. Frontend-design was loaded before the Qt work.
Ledger: [RFC-BPR-0100](../../../verification/RFC-BPR-0100.md).

Test-target hygiene first: all 352 Qt, application and core test executables
(160 core, 85 application, 107 Qt) were built by name in three batches; none
failed to compile, and every `.cpp` under those test folders is in a target.
The stale APIs clangd reported (`set_agent_picker`, `agent_bar`,
`enhance_glyph_style::pen`, designated-initializer order) are no longer in the
sources: the diagnostics were a stale index.

| Commit | What |
|---|---|
| `6a7ec471d`, `6eaf36a2d`, `6e42846ee`, `8613b130c` | Ready and claim BPR-1995, BPR-1996 |
| `e6e84653e` | Product candidate: chat foot (composer with passage chip, Session menu and cost, notices only when abnormal), the update link in every thread, request answered in its turn, revision card (Accept all, Review each, Discard), Enhance in the part's session with `core::confine_to_passage`, `core::rewrite_tour_references` (links, 12-line snippets, publication permalinks, unresolved said in the preview, plan record v7, published baselines keep the authored text), guide v2 and `tour-stop-comment-v2`, `flag-move-v1` fixture, opt-in courses `live-tour-requests` and `live-tour-draft-flag-move`; app pin resealed |
| `8143e373e` | Ledger, BPR-1997 checklist, fixture catalog. Frozen |
| `215e61af8`, `0e163ac1a` | BPR-1995 and BPR-1996 parked `human-review` |

Evidence: live `live-tour-requests` (installed Claude Code 2.1.319, Sonnet)
carded 5/5 fixed change requests in the first answer; BPR-Test #43 showed
review comments and conversation comments both embed a bare permalink (probe
comments deleted). Runs in the ledger; a run of all 431 `qt_`, `core_`,
`sqlite_` rows left two failures outside this change
(`qt_profile_management_window`, `qt_workbench_conversation_reveal`).

Filed, not waited on (URL in `../serve.log` beside the state): session
`c46887c9608e3cab116a025a744f2379`, state
`/Users/jaeger/.attended-review-state/tour-conversation-v001-2026-11-11/state`,
cursor 9, host PID 19967 (Tailscale child 19990), three candidates, ready and
protected: `RFC-BPR-0100-chat-lower-edge`, `RFC-BPR-0100-revision-card`
(AC09) and `RFC-BPR-0100-flag-move-draft` (AC08). Frozen root
`/Users/jaeger/tour-conversation-v001.l3mto90j` from `8143e373ec46` (app
SHA-256 `ca7a6f81fe78…`, launcher check reached
`ATTENDED_TOUR_REVISION_SHOWN`). A first freeze attempt failed at configure
(kernel.org download timeout); its unfrozen root was deleted by hand because
the remove tool refuses a root that never froze.

Pending: the owner's three answers. Known limit recorded on BPR-1996 and the
ledger: an edited published stop republished by a tour update keeps its
references as written (coordinator: a follow-up card if wanted).
`qt_tour_agent_session_view` fails under the native-look environment at an
existing focus-geometry check (offscreen it passes).

Next: read `c46887c9…` from cursor 9 after hand-back. Accept of both looks
finishes BPR-1995 (AC09); Accept of the draft finishes BPR-1996 (AC08); then
BPR-1997 closes RFC-BPR-0100 in `accepted-scope`. A Reject goes to a fresh
implementer. Then stop host 19967 and remove the root with
`freeze_attended_build.py remove`. Not touched: host 74692/74706, PIDs 10695
and 39804, Tailscale 44738, BPR-Test #152 and `-next`.

## agent-scratch third review round (2026-11-11)

Fresh `rfc-implementer`, same lane; nothing pushed. Owner amendments in force
as above. No Qt change, so frontend-design was not loaded. The independent
re-review of `531a039cd` returned FAIL (F1 blocking: a Codex build turn
trusted the pull request's checkout and ran its `.codex/config.toml` MCP
servers outside the sandbox; F2 blocking: `-c=KEY=VALUE` not read).

| Commit | What |
|---|---|
| `059e191fb` | Product candidate: every launch in a checkout BPR made names it (`PullRequestCheckout`, path and canonical path): Codex `-c projects={…={trust_level="untrusted"}}`, Claude Code `--setting-sources user`; a Codex grant or any build grant refuses without it; `-c=` parsed as Codex parses it; tests; by-hand course `live_agent_checkout_settings_course` and fixture `tests/fixtures/agent-checkout-settings` |
| this commit | Ledger third-round section, RFC-BPR-0104 amendment, BPR-2007 and BPR-2008 records, counter and BPR-2103/2104 reservations synced from main, handoff; mirrored to main |

Safe or withheld: both grants are kept, for both agents. Reproduced live
(isolated homes, every model provider at `http://127.0.0.1:9`): the exposure
was wider than the grant: Codex `exec` under this computer's own
`workspace-write` trusted the checkout with no grant, and every Claude Code
turn ran the checkout's hooks and `.mcp.json`. The App Server did not trust
it in any probe. The fix covers every launch in a made checkout, tours
included. Evidence:
[ledger](../../../verification/RFC-BPR-0104.md#third-review-round-2026-11-11).

Runs: the 29 focused rows of the second round (29/29 pass), `coding-format`,
clang-tidy on the changed sources (one pre-existing warning at
`agent_launch.cpp:1692`), documentation gate; the live course (Codex 0.167.0,
Claude Code 2.1.319): controls fire, 11 guarded runs clean. Mutation: the
parser of `531a039cd` fails `test_codex_attached_equals` at its first case.
Not run: the whole dev suite, the core-only preset, the live `agent-scratch`
course (no rule changed; a granted launch only gains the checkout argument).

Trust entries already in `~/.codex/config.toml` (read, not edited; file
modified 2026-11-10 11:14, before this round): seven
`[projects."/private/var/folders/k7/…/T/bpr-public-path-*/data-agent-workspaces-v1/betterpullreview-agent-*/workspace"]`
and `[projects."/private/tmp/claude-501/…/scratchpad/probe/ws"]`, each
`trust_level = "trusted"` (exact lines in the round's report). The owner's to
remove.

Not touched: session `c46887c9…` (19967/19990, root
`tour-conversation-v001.l3mto90j`), host 74692/74706, PIDs 10695 and 39804,
Tailscale 44738, BPR-Test #152 and `-next`. No live GitHub use. No write to
the reviewer's `~/.codex` or `~/.claude`.

Pending: another independent re-review of `059e191fb` (AC07 stays pending);
BPR-2101 (Backlog). Next unchanged: with a passing re-review, finish
BPR-2007, then ready, claim and finish BPR-2008, mirrored to main.


## Owner hand-back c46887c9 (2026-11-11)

Handed back at sequence 51, all Accept (`session.json`):
`RFC-BPR-0100-chat-lower-edge` (`8736f969`), `RFC-BPR-0100-revision-card`
(`e1ad5848`), `RFC-BPR-0100-flag-move-draft` (`1180b2e4`); no decision notes.
After accepting the lower edge the owner left one feedback note, a
suggestion, verbatim in the RFC-BPR-0100 ledger: name a session (from the
agent, or by the reviewer as VS Code allows) instead of its number
(coordinator: a follow-up card if wanted). BPR-1995 (AC09) and BPR-1996
(AC08) Done. Host 19967/19990 stopped with `attended_review.py stop`; root
`tour-conversation-v001.l3mto90j` removed with `freeze_attended_build.py
remove`. Next: BPR-1997 closes RFC-BPR-0100 in `accepted-scope`.

## Import and closeout round (2026-11-11)

Fresh `rfc-implementer`, same lane; nothing pushed; no product change. Owner
amendments in force as above, plus the relayed "/author-rfc a pack for this
and message the other session to add it to it's work".

**Import.** Branch `claude/address-review-feedback-v1` (`b1e5e6fdb`, pack
folder only) merged (`881778d5b`) and imported
([LIVE_IMPORT](../../../rfcs/assets/address-review-feedback-v1.LIVE_IMPORT.md)):
RFC-DRAFT-0001..0003 → RFC-BPR-0122..0124, DRAFT-0001..0010 →
BPR-2105..2114 (closeouts BPR-2107, 2111, 2114), all Backlog; plan adopted
under `docs/rfcs/plans/address-review-feedback-v1/`. AMENDMENTS.md not
applied. P01–P09 unconfirmed; P09 (RFC-BPR-0098's Returned rule) must be
asked before BPR-2113 is built (said on the card and the RFC index).

**Owner suggestion.** BPR-2115 (Backlog): name a tour chat session instead of
its number.

**Closeouts (`accepted-scope`).** Closed, Auto-Verified: RFC-BPR-0098
(BPR-1990), 0099 (BPR-1994; AC01 amended for the owner's removal of the agent
bar, [[BPR-2051]]), 0101 (BPR-2001), 0102 (BPR-2003), 0103 (BPR-2006), 0105
(BPR-2012). Left open: RFC-BPR-0100 (AC06 not met when a tour update
republishes an edited stop; BPR-2116, BPR-1997 depends on it), RFC-BPR-0106
(AC04 has no composed evidence; BPR-2117, BPR-2015 depends on it; AC08 now
passes), RFC-BPR-0104 (AC07 re-review of `059e191fb` pending, not touched).

| Commit | What |
|---|---|
| main `42fba4488` | BPR-2105..2114 and BPR-2115 reserved (counter BPR-2116) |
| `881778d5b`, `91cce126b` | Pack merge; counter and receipts from main |
| `5cc70e5ed` | Import |
| `54173961f` | BPR-2115 |
| main `8e22a71b0`; `d624c430b`, `a485010b9` | BPR-2116..2117 reserved (counter BPR-2118); cards; closeout dependencies |
| `7af8c1049`, `5c794d7eb`..`6c1c7b206` | Ready and claim the six closeouts |
| `45cca8934` | RFC-BPR-0100 and 0106 closeout reviews (left open) |
| `236c6f9b2`, `536b784ed`, `c2316d9f5`, `4edb28d81`, `46fc9e7df`, `ab0aadf75` | Finish BPR-1990, 1994, 2001, 2003, 2006, 2012 |

Runs: documentation gate after each step; `issue_board.py validate` (all
cards); `milestone_plan.py adopt` and `render`. Evidence for the closeouts is
the ledgers' unchanged evidence; nothing rebuilt.

Next eligible in plan order: v0.5.0 `local-draft-entry` (BPR-2019..2022, no
prerequisite, under mandate `v0-5-0-prerelease-targets-r1`); `accepted-scope`
remains for BPR-1997 (after BPR-2116), BPR-2009 (after BPR-2007's AC07
re-review and BPR-2008), BPR-2015 (after BPR-2117). Imported without an
implementation mandate: `agent-assistance-v1` (returning-pull-request,
explain-file, comment-to-suggested-change, failing-check-question,
agent-insights have no unmet card prerequisite), `local-draft-authoring-v1`
and `address-review-feedback-v1` (both wait on local-draft work).
Not touched: host 74692/74706, PIDs 10695 and 39804, Tailscale 44738,
BPR-Test #152 and `-next`.

Main mirror commit this round: `1734f1d59` (with `42fba4488`, `8e22a71b0`). Main's copies of the RFC-BPR-0099, 0101, 0102, 0103 and 0105 ledgers keep their earlier de-linked source paths; only this round's changes were applied there.

## Closeout and exposure round (2026-11-11)

Fresh `rfc-implementer`, same lane; nothing pushed. Owner amendments in force
as above. No Qt surface changed, so frontend-design was not loaded.

| Commit | What |
|---|---|
| `57747c640`, `653421192` | Ready, claim BPR-2116 |
| `f042726a6` | Product: a tour update writes an edited published stop's references as permalinks (window observes the files for a published tour's preview; plan rewrites edited stops in the frozen update tour; `core::tour_stop_text_edited`); native course extended, negative control; app pin resealed |
| `b54188c7d` | BPR-2116 Done |
| `9e1656f92`, `7a27a84fd` | BPR-1997: RFC-BPR-0100 Auto-Verified |
| `7a5bb7de1` | Product: RFC-BPR-0104 fourth review items (every checkout spelling incl. the kernel's path, unresolvable checkout refused, whitespace keys refused, merge wording, opt-in live course `live_agent_checkout_settings` with trusted-home, worktree and firmlink cases) |
| `189ee5d5d` | BPR-2119 (Backlog, reserved on main): shipped builds load the pull request's agent settings |
| `560962c3d`, `124d99297`, `38420cb03`, `c72abd4ba` | BPR-2007, BPR-2008 Done |
| `e0ed7a27a` | WIP BPR-2117: `tour-push-v1` fixture, `TourPushCourse`, course mode `push` (row `application_review_tour_publication_push`), not yet run |
| `b63c0bacab`, `ad6f7a18d` | BPR-2009: RFC-BPR-0104 Auto-Verified |

Main mirror `1232a90dc` (BPR-2116, 1997, 2007, 2008, BPR-2119). Release
exposure: [RFC-BPR-0104 ledger](../../../verification/RFC-BPR-0104.md#release-exposure-check-2026-11-11).

| `1d5558c89` | BPR-2117 course passes: the push arrives by the seeded epoch refresh (`EpochPhase` seed, `RepeatReadSessions` off); a re-accepted handoff carries no Git transport, which is why the second head was never readable before |
| `dd241bb1d`, `90f6ff55c` | BPR-2117 Done (negative control: `check_tour_material` disabled, the course fails at "every stop is compared") |
| `00356810c`, `d35d139d6` | BPR-2015: RFC-BPR-0106 Auto-Verified |

Runs: all thirteen `application_review_tour_publication_*` courses and
`application_review_round_trip_composition_contract` together (14/14);
`core_review_tour_update`; 62 `tour` rows at `f042726a6`;
`application_agent_{invocation_host,launch,scratch_folder}`, `core_agent_grant`
at `7a5bb7de1`; the live course (20 runs, real `~/.codex` and `~/.claude`
untouched); format and clang-tidy on the changed sources; documentation gate.
Not completed: the 90-row `agent` ctest pass (two session restarts and a
swapping machine cut it off; rows not run are unknown, not failed).

The reviewer's probe scripts in the session scratchpad (`r4/`) were gone
after the session restart; the ledger names them.

Next: nothing owed in this round. RFC-BPR-0100, 0104 and 0106 are closed.
BPR-2119 (main) is the hotfix for shipped builds. BPR-2101 (Backlog) remains.

## local-draft-entry (2026-11-11)

Fresh `rfc-implementer`, same lane; nothing pushed. Milestone
`local-draft-entry` (RFC-BPR-0108, BPR-2019..2022). Frontend-design was
loaded before the Qt work. Ledger:
[RFC-BPR-0108](../../../verification/RFC-BPR-0108.md); implementation record
in the RFC. Owner amendments in force as above.

| Commit | What |
|---|---|
| `24d0b845c`, `3f97d7aac` | Ready, claim BPR-2019 |
| `fe3a4a2e1` | Core `local_draft` (remote chain, target, comparison, candidates, worktree resolution, codec) |
| `c56f0b638`, `1698f5b67`, `207aeb7d9` | Installed-Git local-draft reader, discovery and capture, `local-drafts.sqlite3`, workbench host; real-worktree course `local_draft`; reset coverage |
| `7e359141b`, `1bc6cf4c1`, `d4cf5da72`, `3225243ec` | Qt page, Home author entry and In preparation, prepared mark, Review Cycles below; composition; `bpr draft open` and owner channel `local_draft`; desktop course |
| `541a6fb9b`, `c1bd85db1` | clang-format 22, clang-tidy fixes |
| `b6decde2d`, `3585dedc7` | Attended look `--attended-local-draft` and its generated worktree. Product candidate |
| `3792021fe`, `a610e1ec4` | Ledger, implementation record, `docs/development/draft-cli.md`, architecture; closeout checklist |
| `d32ad92d6`, `1e0ca5147` | NEXT_ID from main (BPR-2130 with main's reservations) |
| `779b495c9`; `1fc526c79`, `0247e9245`, `2adb3ea77`; `e06d28549`, `1c328399f`, `446b17aa6` | BPR-2019 Done; BPR-2020 ready, claimed, Done; BPR-2021 ready, claimed, parked `human-review` |

Main mirror: `7c2a6fa31` (BPR-2019, 2020 Done; BPR-2021 parked; BPR-2022
checklist; RFC-BPR-0108 and its ledger, the draft CLI guide link de-linked).

Runs at `3585dedc7` (dev, Debug): `core_local_draft`, `local_draft`,
`core_profile_data_reset`, `sqlite_profile_data_reset_files`,
`application_review_cycle_desktop_public_path`,
`application_local_draft_desktop_public_path` (6/6); negative control
(target tip as base) fails `local_draft`; `bpr draft open` from a terminal
(create, resume, `remote_chain`, `not_git`); the shipped composition reached
`ATTENDED_LOCAL_DRAFT_SHOWN` (dev app and the frozen launcher check).
Documentation gate OK on lane and main. Not run: the whole suite, core-only
preset, `application_profile_data_reset_public_path` and the Qt window suites
(their binaries were removed for disk: list in the session scratchpad
`deleted-binaries.txt`); `bpr draft open` forwarded to a running desktop has
no course.

Owner look, filed and not waited on: session
`5f049205ae4d7f19c1593afa81ea39df`, state
`/Users/jaeger/.attended-review-state/local-draft-entry-v001-2026-11-11/state`,
cursor 6, host PID 6288 (Tailscale child 6298), URL in that directory's
`serve.log`. Candidates `BPR-2021-draft-page` (`cd646094638f…`) and
`BPR-2021-home-author-entry` (`c6caca388752…`). Frozen root
`/Users/jaeger/local-draft-v001.h8pe0ibf` from `3585dedc7f50` (executable
SHA-256 `df0de0384dee…`), launch `python3 /Users/jaeger/local-draft-v001.h8pe0ibf/launch.py`.

Next: read `5f049205…` from cursor 6 after hand-back. Accept of both finishes
BPR-2021 (`issue_board.py finish`, mirrored to main), then BPR-2022 closes
RFC-BPR-0108; a Reject is a new candidate. Then stop host 6288 and remove the
root with `freeze_attended_build.py remove`. Known gaps for the closeout:
forwarding course for `bpr draft open`, a drawn queue row with the prepared
chip, and associations recorded only by the publication RFC.

Not touched: host 74692/74706, PIDs 10695 and 39804, Tailscale 44738,
BPR-Test #152 and `-next`. No live GitHub use.

## author-evidence (2026-11-11)

Fresh `rfc-implementer`, same lane; nothing pushed. Milestone
`author-evidence` (RFC-BPR-0109, BPR-2023..2025). Frontend-design was loaded
before the Qt work. Ledger: [RFC-BPR-0109](../../../verification/RFC-BPR-0109.md);
implementation record in the RFC. Owner amendments in force as above.

**Dependency.** BPR-2023 depends on BPR-2022 (RFC-BPR-0108's closeout), which
waits on BPR-2021's owner look (session `5f049205…`, not touched). The cards
were therefore not claimed or moved, and main has no mirror this round; the
engineering needed only the built local-draft code. Once BPR-2021 is accepted
and BPR-2022 closes: ready and claim BPR-2023 and BPR-2024, finish both on the
ledger's evidence (and BPR-2024's owner look), then BPR-2025 closes
RFC-BPR-0109; mirror each move to main.

| Commit | What |
|---|---|
| `8dbe497ba` | Core evidence record, import format, binding, currency, selection; `core_local_draft_evidence` |
| `a70a3e432` | Import from a chosen folder through the workspace port; workbench evidence edits; artifact pins; `local_draft` course extended |
| `be0a3d246` | Evidence area (Changes and Evidence tabs), `bpr draft open --evidence`, owner-channel field, desktop course `application_local_draft_evidence_desktop_public_path` |
| `8f5ca195f` | Attended evidence look (`--attended-local-draft-evidence`, `tests/fixtures/local-draft/make_evidence_look.sh`); composition contract inventory and pin resealed; draft page held by `QPointer` (fixes the contract probe's shutdown crash); tidy fixes; docs. Product candidate |
| (this commit) | `architecture_boundaries` fix (`QRandomGenerator::global()` in `local_draft_handoff.cpp`), ledger, this checkpoint |

Runs at `8f5ca195f` (dev, Debug): `core_local_draft`,
`core_local_draft_evidence`, `local_draft`, `core_profile_data_reset`,
`sqlite_profile_data_reset_files`, `application_local_draft_desktop_public_path`,
`application_local_draft_evidence_desktop_public_path`,
`application_review_round_trip_composition_contract` (8/8);
`sqlite_tour_owner_channel`, `architecture_boundary_contract`;
`architecture_boundaries` after the fix; negative control (material without
working files) fails `core_local_draft_evidence`; `bpr draft open --evidence`
from a terminal (import, duplicate, observed refused); clang-format 22 and
clang-tidy on the changed units; documentation gate OK. Not run: the whole
suite, core-only preset, Qt window suites. An accidental untargeted build was
stopped; the 101 binaries it linked were removed (list in the session
scratchpad `deleted-binaries.txt`), all relinkable.

Owner look, filed and not waited on: session
`7fc09e758ebf8ac0d028da38a5d374f7`, state
`/Users/jaeger/.attended-review-state/local-draft-evidence-v001-2026-11-11/state`,
cursor 6, host PID 4867 (Tailscale child 4877), URL in that directory's
`serve.log`. Candidates `BPR-2024-evidence-area` (`64c7cc55f1d2…`) and
`BPR-2023-result-wording` (`1bb95d1a6928…`). Frozen root
`/Users/jaeger/local-draft-evidence-v001.pdc9rq1b` from `8f5ca195f370`
(executable SHA-256 `055e387e9e0e…`), launch
`python3 /Users/jaeger/local-draft-evidence-v001.pdc9rq1b/launch.py`.

Next: after BPR-2021's hand-back and BPR-2022, claim and finish BPR-2023 and
BPR-2024 (BPR-2024 also on `7fc09e75…` from cursor 6), then BPR-2025; a
Reject is a new candidate. Then stop host 4867 and `freeze_attended_build.py
remove` the root. Known nit seen in renders, from local-draft-entry: the
candidate chooser clips "Candidate 2 of 2". Next milestone in plan order,
`local-tour-preparation`, depends on this one.

Not touched: session `5f049205…` (host 6288/6298, root
`/Users/jaeger/local-draft-v001.h8pe0ibf`), host 74692/74706, PIDs 10695 and
39804, Tailscale 44738, BPR-Test #152 and `-next`. No live GitHub use.

## local-tour-preparation (2026-11-12)

Fresh `rfc-implementer`, same lane; nothing pushed. Milestone
`local-tour-preparation` (RFC-BPR-0110, BPR-2026..2028). Frontend-design was
loaded before the Qt work. Ledger:
[RFC-BPR-0110](../../../verification/RFC-BPR-0110.md); implementation record
in the RFC. Owner amendments in force as above.

**Dependency.** BPR-2026 depends on BPR-2022 and BPR-2025 (closeouts of
RFC-BPR-0108 and 0109), which wait on BPR-2021's owner look (session
`5f049205…`, not touched). The cards were not readied, claimed or moved and
main has no mirror this round. Once BPR-2022 and BPR-2025 close: ready and
claim BPR-2026 and BPR-2027, finish both on the ledger's evidence and this
round's look, then BPR-2028 closes RFC-BPR-0110; mirror each move to main.

| Commit | What |
|---|---|
| `0f11cb853` | Core `local_draft_tour`: preparation in the draft's record, binding and correspondence, acceptance, proposals, conversations, preview, mapping; `core_local_draft_tour` |
| `8b5c8aea2` | Two writing hooks `local.description` / `local.discussion`, profile schema 28, Settings group, exchange; candidate-only request and answer parsing |
| `1234fa739` | Workbench host: edits, refresh carry, `ask` on its own lane |
| `79589a941` | Qt Description and Tour areas, agent bar; tool-free runner composition; X19 default; candidate chooser fix; desktop course |
| `32509525c` | Storage migrations, Settings test, transcript links without QUrl, `--attended-local-draft-tour`, isolation entries |
| `8fe16c86d` | Tidy fixes, narrow hook-table rebuild test, prompt round trip, app pin resealed, docs, ledger. Product candidate |
| `13bb65bb8` | NEXT_ID and reservations BPR-2129, 2130 from main (BPR-2131) |

Runs: see the ledger. Baseline failures kept: `test_network_isolation_manifest`
(raw `add_test` for `live_agent_checkout_settings`,
`application_review_tour_publication_push` entry), and its script test
`test_repository_raw_registrations_match_the_exception`; both from earlier
milestones. Documentation gate OK.

Not built (recorded in the RFC): the candidate-workspace route and native
resume for local-draft conversations (runs are tool-free with the captured
candidate's text; every turn replays); the running block is "Asking…" with
Stop, no activity rows; no publication calls `map_local_publication` yet
(RFC-BPR-0111). An installed agent never answered a local draft here.

Disk: no binaries were deleted this round. One accidental dependency build
did not happen; every build named its targets with -j4.

Owner look, filed and not waited on: session
`6c8ac4dc8b4856b0f94a4b4729af0806`, state
`/Users/jaeger/.attended-review-state/local-draft-tour-v001-2026-11-12/state`,
cursor 9, host PID 71192 (Tailscale child 71200), URL in that directory's
`serve.log`. Candidates `RFC-BPR-0110-tour-area` (`086ad8e2e0b1…`),
`RFC-BPR-0110-description-area` (`d71ec0288768…`) and
`RFC-BPR-0110-candidate-chooser` (`7f70240ac247…`). Frozen root
`/Users/jaeger/local-draft-tour-v001.3hs4zi83` from `13bb65bb8365`
(executable SHA-256 `91778c647a7c…`; launcher check reached
`ATTENDED_LOCAL_DRAFT_TOUR_SHOWN`), launch
`python3 /Users/jaeger/local-draft-tour-v001.3hs4zi83/launch.py`. The look's
agent is scripted.

Next: read `6c8ac4dc…` from cursor 9 after hand-back; a Reject is a new
candidate. After BPR-2022 and BPR-2025 close, ready, claim and finish
BPR-2026 and BPR-2027 (mirrored to main), then BPR-2028. Then stop host 71192
and `freeze_attended_build.py remove` the root. Open work for the closeout to
judge: the candidate-workspace route and native resume, RFC-BPR-0103's full
running block, an installed agent answering a local draft, and X19 with a real
associated pull request (RFC-BPR-0111 creates one).

Not touched: sessions `5f049205…` (host 6288/6298) and `7fc09e75…` (host
4867/4877) and their roots, host 74692/74706, PIDs 10695 and 39804,
Tailscale 44738, BPR-Test #152 and `-next`. No live GitHub use.

## publish-prepared-draft (2026-11-12)

Fresh `rfc-implementer`, same lane; nothing pushed. Milestone
`publish-prepared-draft` (RFC-BPR-0111, BPR-2029..2031). Frontend-design was
loaded before the Qt work. Ledger:
[RFC-BPR-0111](../../../verification/RFC-BPR-0111.md); implementation record
in the RFC. PUBLISH-01 built as settled (the author pushes, BPR names the
push); amendment A1 of the local-draft authoring pack belongs to
`commit-and-push` and was not applied here.

**Coordinator direction mid-round (2026-11-12).** The owner rejected the
local-draft looks (draft page accepted; Home entry, evidence area and result
wording, and the tour, description and candidate screens rejected). Verdicts
recorded verbatim on BPR-2021, BPR-2023, BPR-2024 and the RFC-BPR-0108, 0109
and 0110 ledgers (`a8bd8d684`; main mirror `344360c01` for the cards and the
0108 ledger, the 0109/0110 ledgers are not on main). Hosts 6288/6298,
4867/4877 and 71192/71200 stopped through the attended-review `stop`; roots
`local-draft-v001.h8pe0ibf`, `local-draft-evidence-v001.pdc9rq1b` and
`local-draft-tour-v001.3hs4zi83` removed with `freeze_attended_build.py
remove`. This round: engine and live evidence only, a minimal Qt area from
existing controls, no freeze and no look filed.

**Dependency.** BPR-2029 depends on BPR-2025 and BPR-2028, which wait on the
rejected looks' redesign; cards not readied, claimed or moved.

| Commit | What |
|---|---|
| `1ed42caa0` | Network isolation manifest green: opt-in live agent row through `bpr_add_live_agent_course`; push course pinned |
| `387f7298d` | Core `local_draft_publication` and `core_local_draft_publication` |
| `4675974cf` | Pull request catalog over gh; `accepted_pull_request_account` |
| `6bc5217f5` | Application check/create/recover/associate; workbench projection |
| `75c035a49` | Real-worktree course `local_draft_publication` |
| `36dcd7e21` | Pull request area (existing controls); desktop lane; BPR-Test guard |
| `a8bd8d684` | Owner verdicts recorded (see above) |
| `62f740fc6` | gh catalog under the lease's scoped operation (the fix the live run found); live-draft course and driver |
| `433068905` | Journal steps re-read on a competing write; progress to the page; desktop course |
| `96bd411cf` | clang-format 22, tidy fixes, boundary fixes, app pin resealed, ledger, RFC record, threat model, card notes, this checkpoint |

Live: `python3 tests/end_to_end/live_draft_course.py --course
build/dev/tests/end_to_end/live_second_account_course --root DIR --cleanup`
(DIR new, outside the repository). Runs 4 and 5 (#178, #179) passed; both
closed and branches deleted; three earlier failed runs pushed nothing.
Evidence files in the session scratchpad `live-draft-4`, `live-draft-5`.

Open for the closeout: the tour follow-up (not built), the review-submission
clause of AC07 (structural only), fork heads and GHES (unsupported, stated).

Not touched: host 74692/74706, PIDs 10695 and 39804, Tailscale 44738,
BPR-Test #152 and `-next`.

## Evidence cut from 0.5.0 (2026-11-12)

Fresh `rfc-implementer`, same lane; nothing pushed. Owner decision, verbatim
(2026-11-12): "Mockups; cut Evidence". No UI redesign (mockups are made
separately), no owner look this round.

Tracker and plan (lane `7640fd509`; main `59b6a8b23` + `3a52440c5`): RFC-BPR-0109
Deferred (kept as a possible workflow extension); BPR-2023, 2024, 2025 moved to
`50-blocked`, `blocked_by: owner-deferred`, with the decision noted; BPR-2026,
2029 and 2033 lose their BPR-2025 dependency. RFC-BPR-0108, 0110 (AC02, AC09
amended, AC07 withdrawn, X21 inactive), 0111 (AC03 amended) and 0112 (AC01
amended) carry the amendment; RFC-BPR-0115 and 0120 and BPR-2082 note it
(BPR-2082 still depends on BPR-2024 and waits on 0109's reactivation). The
plan drops `author-evidence` and RFC-BPR-0109 and BPR-2023..2025 from scope;
`local-tour-preparation` no longer depends on it; MILESTONES.md regenerated.
Recorded in the pack's DECISIONS.md (an owner change of requirements, not a
same-scope refinement, so not passed through `check-amendment`) and pointed to
from local-drafts/DECISIONS.md. Main's mirror took the cards, RFCs, index,
plan and decision records; 59b6a8b23 committed only the renames by mistake and
3a52440c5 completes it.

| Commit | What |
|---|---|
| `7640fd509` | Tracker, RFC and plan amendments, card moves |
| `aa00b4795` | Product removal: core, application, store migration, Qt, app, tests |
| `fcbd667b4` | clang-format 22, tidy fixes, draft-cli and architecture docs, NEXT_ID BPR-2132 from main |
| (this commit) | App source pin resealed with a removal note, ledgers, this checkpoint |

Removed: the Evidence area and tab, `bpr draft open --evidence`, the evidence
record and import (`draft_evidence`, `local_draft_evidence`,
`local_draft_evidence_import`), artifact pins and the artifact reader, agent
claim and check proposals, citations and `evidence_recheck`, selected
evidence in the request, the preview and the publication plan, the attended
evidence look and its fixture script, `core_local_draft_evidence` and
`application_local_draft_evidence_desktop_public_path`. Forward migration:
core drops legacy fields on read (a stored plan's digest is checked against
its earlier form first); the store migrates to `user_version` 1, rewriting
such drafts and deleting objects only evidence pinned. Tab indices moved:
Description 1, Tour 2, Pull request 3.

Runs and limits: [RFC-BPR-0110 ledger](../../../verification/RFC-BPR-0110.md#owner-amendment-evidence-removed-2026-11-12).
Documentation gate OK on lane and main.

Pending obligations unchanged: the rejected local-draft looks wait on the
mockup redesign; BPR-2021/2022 then BPR-2026..2028 and BPR-2029..2031 as
before (now without BPR-2025); RFC-BPR-0105 AC08, RFC-BPR-0101 AC08 and the
other parked looks as recorded above. Next milestone in plan order after the
redesign: `local-tour-preparation` (its prerequisites now exclude
`author-evidence`).

Not touched: host 74692/74706, PIDs 10695 and 39804, Tailscale 44738,
BPR-Test #152 and `-next`. No live GitHub use. Every build named its targets
with -j4; the composition contract's own sub-build ran inside its test.

## local-draft redesign: PR window, author state, Local Changes (2026-11-12)

Fresh `rfc-implementer`, same lane; nothing pushed. Owner chose mockup A +
Home 1, author state and Local Changes variant a, and the agent's commit
message ([accepted design](../../../design/local-draft-pr-window-v1/README.md),
owner words verbatim there and in the RFC amendments). Frontend-design loaded
first. RFC-BPR-0113/0114 pulled forward (amended 2026-11-12).

Plan of the round (status in brackets, updated as it goes):

1. Design record and owner amendments (0108, 0113, 0114, 0122) [done `a735c6a44`]
2. Core `local_changes`: status model, three comparisons, change hunks,
   stage/unstage-one-change texts, commit and push decisions, commit-message
   request and answer [done `0f766be13`; `core_local_changes` passes]
3. Author-Git runner (adapter) + application host; real-repo course with a
   local bare remote [done `73f3b9109`; `local_changes` passes: stage/unstage
   one change, hooks, refusing hook, amend, push to a bare remote,
   refused-when-behind, Save, agent message, declined question]
4. Qt: Local Changes tab, commit box, push line; segmented view picker with
   Edit; local draft in the PR window (A); Home 1; old UI removed [done
   `a58baed97`, `b3acafe2d`, `11b744e66`; PR window review bar has the
   picker (workbench_window_tests' layout-toggle case still to update)]
5. Agent commit message (`local.commit-message`, profile schema 29, 0107)
   [done `dda67270c`; composition wired; the three local-draft desktop
   courses, `local_changes`, settings and agent configuration storage pass]
6. App pin reseal, focused tests, native renders, live BPR-Test push,
   freeze, one attended session [pin `7550a3ee9`; window suites `a7ef7c0c5`;
   live push `e4e053c52`; records `559a345c5`; BPR-2062 and BPR-2065
   claimed `3a6adf516`, `316135d2b`; main mirror `962371113`; freeze and
   session next]

Decisions taken in the round (recorded in the design README, the RFC
amendments and the v0.5.0 DECISIONS): tab named Local Changes; opening reads
no network (Check Again or Push reads GitHub); review-bar picker two 20 px
segments in a review (its well replaces the rule), three in author state; the
author-Git runner is a named QProcess owner whose catalogue
`architecture_boundaries` holds.

Not built (in the ledgers): the accepted PR-window Tour panel bound to a
local tour (the Tour tab hosts the local tour editor); the Status tab's
three-step strip (it hosts the publication area); Address feedback opening a
real pull request into author state (RFC-BPR-0122); leaving with unsaved
edits does not ask; no course stops a running push.

Live: BPR-Test disposable branch `bpr-local-changes-live-20261112084707`
pushed by the author runner (tip `2dd171d1abad`), deleted (HTTP 204).

Tab name decided: **Local Changes** (recorded in the design README).

Owner look, filed and not waited on: session
`6d00826a4d9c2ee08fe91321c2c1d521`, state
`/Users/jaeger/.attended-review-state/local-draft-pr-window-v002-2026-11-12/state`,
cursor 15, host PID 75022 (Tailscale child 75032), URL in that directory's
`serve.log`. One question per surface: `local-draft-pr-window`, `home-1`,
`view-picker-and-edit`, `local-changes-commit-push` (its copy value opens the
same root as pull request #128 opened by its author), `agent-commit-message`.
Frozen root `/Users/jaeger/local-draft-pr-window-v002.lyqc133b` from
`be709f8e6c6a` (executable SHA-256 `efc0b3fbde21…`; launcher check reached
`ATTENDED_LOCAL_DRAFT_AUTHOR_SHOWN … 2 ahead of GitHub`), launch
`python3 /Users/jaeger/local-draft-pr-window-v002.lyqc133b/launch.py`.
After the freeze, `local-draft redesign: tidy fixes` changed no behaviour
(focused courses rerun).

Left on disk: `/Users/jaeger/local-draft-pr-window-v001.27acdz8g` (a failed
freeze's exported source and generate.log; `freeze_attended_build.py remove`
refuses it as not a frozen build, so it was not deleted by hand).

Next: read `6d00826a…` from cursor 15 after hand-back; Accepts finish
BPR-2062/2065 where their other criteria allow (see the 0113/0114 ledgers),
a Reject is a new candidate. Then stop host 75022 with the attended-review
`stop` and `freeze_attended_build.py remove` the root. Mirror card moves to
main as before.

Not touched: host 74692/74706, PIDs 10695 and 39804, Tailscale 44738,
BPR-Test #152 and `-next`. Live GitHub: one disposable BPR-Test branch,
deleted.

## Agent launch policy round (2026-11-12)

Fresh `rfc-implementer`, same lane; nothing pushed. Owner decisions of
2026-11-12 (fresh vs resumed per action; MCP per agent, Off by default; P02
settled with built-in prompts in the inline system prompt; one call per fresh
click; the corporate wrapper's sign-in failure and argument order), recorded
in RFC-BPR-0101, 0103, 0104, 0107, 0116 to 0120 and agent-assistance-v1
DECISIONS (P02 Settled). Cards BPR-2138..2142 reserved against main
(counter BPR-2143). Frontend-design loaded before the Qt work. Record:
[RFC-BPR-0101 ledger](../../../verification/RFC-BPR-0101.md#agent-launch-policy-owner-2026-11-12-bpr-2138-bpr-2141).

| Commit | What |
|---|---|
| `224170753` | Owner decisions in the RFCs and DECISIONS; BPR-2138..2142 Ready (main mirror `18b9ed121`) |
| `f54978e89`..`451f6eac7` | Claim BPR-2142, 2139, 2140, 2138, 2141 |
| `524361ba7` | Core `agent_run_policy` (paths, session modes, MCP, instructions); launch options; argument order; sign-in failures; store; app wiring |
| next two WIP commits | Fresh tour Enhance with a proposal, Settings rows, footer MCP tooltip; tests follow the new order |
| `5286b69c2` | clang-format 22, canary printer, app source pin resealed |
| `1257a42c7` | Courses follow fresh Draft Tour and tour Enhance and the MCP-off argv; Enhance tooltips; stale purpose list in `writing_delivery_tests` corrected. Product candidate |
| `8c7b285db` | Counter to main's BPR-2144 |
| `901f67d44`, `8ac34ae9b` | BPR-2142, BPR-2139 Done (main mirror `d9b0d6656`) |

Integration pass at `1257a42c7`: 115 agent, tour, writing, local-draft,
Review Cycle and composition rows; all pass but two that predate this round
(`tour_cli_composition` and `application_profile_tour_handoff_public_path`
expect eight writing hooks; the local-draft hooks made eleven before this
round). Also `architecture_boundaries`, `test_network_isolation_manifest`,
`application_composition_source_contract`, `coding-format-check`, clang-tidy
on the touched sources (two findings, both in lines this round did not
touch), documentation gate and its tests. Canary: `CANARY_PASS: 'ok\n\nPINEAPPLE'`.

Status: BPR-2139 (MCP) and BPR-2142 (sign-in, argument order) are complete
against their criteria. BPR-2138, 2140 and 2141 stay In Progress with the
remaining work in the ledger's "Not done" and "Pending" lists: the local
draft's discussion as a native resumed session; an explicit Ask more control;
the remaining built-in prompts (tour conversation, Draft Tour, local draft,
review skill) moved to the system prompt; Draft Tour, field drafts and the
outline as one tool-free call with their context up front.

No owner look filed: the Settings rows and the tooltip line follow the
existing rows, and a part's Enhance now shows its result in the tour's
existing proposal (already accepted surfaces). Behavioural confirmation of
the inline system prompt through the corporate wrapper is the owner's
pending rerun; the per-agent "Instructions: In the message" switch is the
fallback.

Not touched: attended session 6d00826a… (host 75022/75032, root
`local-draft-pr-window-v002.lyqc133b`), PIDs 74290, 74692/74706, 10695,
39804, Tailscale 44738, BPR-Test #152 and `-next`. No live GitHub use. Two
canary model calls (Claude haiku, Codex), recorded in the RFC-BPR-0107 ledger.

## Agent launch policy, second pass (2026-11-12)

Fresh `rfc-implementer`, same lane; nothing pushed. Finished BPR-2138, 2140
and 2141 against their criteria (RFC-BPR-0101 AC10 to AC13, RFC-BPR-0107
AC09). Frontend-design was loaded before the Ask more control. Record:
[RFC-BPR-0101 second pass](../../../verification/RFC-BPR-0101.md#second-pass-2026-11-12-resumed-local-discussion-ask-more-one-call-per-fresh-click).

| Commit | What |
|---|---|
| `49ef71558` | The two tests that expected eight writing hooks list eleven |
| `726078731`, `b8e2adeac`, `b8c371613` | Local discussion resumes its native session; first description fresh; instructions apart (local, commit message, review skill, tour prompts); diff up front with the 96 KiB budget; Ask more |
| `b423298bb` | Counter to main's BPR-2151 (main reserved BPR-2144..2150) |
| `6c354f8f0` | clang-format 22, app source pin resealed, ledgers |
| `f75a925ac` | Opt-in `claude_resume_canary.py`; live pass recorded |
| `76100684f` | Drafting stub answers a tool-free Draft Tour. Product candidate |
| `10e800150`, `7ecd2bd03`, `1c201f73c`, `59546899c` | Progress; BPR-2141, 2140, 2138 Done (main mirror `c6d71e58f`) |

Verification at the candidate: `application_agent_launch_policy`,
`core_local_draft_tour`, `core_local_changes`, `local_changes`,
`local_draft`, `application_github_contract`,
`application_tour_agent_conversation`, `qt_tour_agent_session_view`,
`qt_tour_side_chat`, `qt_tour_chat_in_tab`,
`application_local_draft_tour_desktop_public_path`,
`application_local_draft_author_desktop_public_path`,
`application_agent_invocation_composition_contract`,
`application_review_tour_composition_public_path`,
`application_review_tour_writing`, `application_review_tour_writing_wrapper`,
`tour_cli_composition`, `application_profile_tour_handoff_public_path`,
`application_composition_source_contract` (resealed),
`architecture_boundaries`, `coding-format-check`, clang-tidy on the touched
sources (two findings, both in lines this round did not touch), the
documentation gate and its tests. Live: `claude_resume_canary.py`
`CANARY_PASS: 'KUMQUAT' same_session=True` (two haiku calls, $0.0013).

Pending: the owner's look at Ask more (non-blocking; session below); the
owner's rerun of the inline system prompt through the corporate wrapper
(the per-agent switch is the fallback); the agent-assistance actions
(RFC-BPR-0116 to 0121) take `assistance_first_answer` when built.

Not touched: attended session 6d00826a… (host 75022/75032, root
`local-draft-pr-window-v002.lyqc133b`), PIDs 74290, 74692/74706, 10695,
39804, Tailscale 44738, BPR-Test #152 and `-next`. No live GitHub use.

Owner look (non-blocking, filed 2026-11-12): frozen root
`/Users/jaeger/bpr2138-ask-more-v001.kbc0nnpf` of `76100684f` (executable
SHA-256 `8073cae03154d670c69cbbd88b0d1360add4a4ac3d8b5e05d03e5f948d257f3e`),
`--attended-tour-drafting` with the `attended-drafting-claude` stub on
`conventional-comments-v1` / `src/engine/renderer.cpp`; the launcher's own
check reached `ATTENDED_TOUR_DRAFTING_SHOWN`. New short session
`dfc73933e0b3fc4c218f4e46d3e90998`, state
`/Users/jaeger/.codex/attended-review/bpr2138-v001-UJfuLr/state`, host PID
6341 (Tailscale child 6351) by `serve --tailscale`; cursor 3. One candidate,
`BPR-2138-ask-more`, one Accept/Reject question. Status: ready, awaiting the
owner. A Reject's note is a follow-up; BPR-2138's behavioural criteria are
met without it. Remove the root with `scripts/freeze_attended_build.py
remove` once answered.


## agent-assistance-v1: returning-pull-request (2026-11-12)

Fresh `rfc-implementer`, same lane; nothing pushed. The owner added the
agent-assistance-v1 pack to this lane's queue; P02 settled (one editable
prompt per action, system prompt). P03 (an answer is a claim, changes no
state) is unconfirmed and taken as written. Frontend-design was loaded before
the Qt work. Record: [RFC-BPR-0116](../../../verification/RFC-BPR-0116.md).

| Commit | What |
|---|---|
| `421cecc34`, `e3f4e7a39` | RFC-BPR-0116 In Progress; ready and claim BPR-2073 |
| `2567db51e`..`94854701f` | Core `returning_review` (offers, earlier revision, change text, claims, supplement); `evaluate_proposed_fix` acts on a conversation; prompts `review.latest-upload` and `review.proposed-fix` (schema 30); returning supplement; window composition (`window_returning_review.cpp`); Pull Request menu item, Agent tab suggestion and note, thread control, claim line, Ask more; stub answers; fixture `returning-review-v1`; composed courses; regressions (hook lists, settings rows, a stale argv test path in `application_agent_launch`, the settings window's prompt list); app pin resealed |
| `98dfeb91a` | Outdated thread read at the head by number (from the live check). Product candidate |
| `4d157eb36` | BPR-2073 Done (AC01, AC02, AC05, AC06) |
| `c95b68ace`, `72288736c` | Ready and claim BPR-2074 |
| `5e6cc6cd1` | BPR-2074 parked `human-review` (AC03, AC04, AC05 pass; AC07 pending) |

Runs at the candidate: `core_returning_review`, `core_agent_chat_scope`,
`core_agent_question`, `core_review_preparation`, `core_agent_configuration`,
`application_agent_launch_policy`, `application_agent_launch`,
`application_writing_delivery`, `sqlite_workbench_settings_storage`,
`sqlite_agent_setup_storage`, `sqlite_agent_configuration_storage`,
`application_profile_tour_handoff_public_path`, `tour_cli_composition`,
`application_returning_review_{latest,addressed,gone}_public_path`,
`qt_settings_window`, `qt_conversation_cards_view`,
`qt_workbench_window_scale_100`, `qt_agent_configuration_dialogs`,
`application_agent_chat_{subject,conversation,composition}_contract`,
`application_agent_invocation_composition_contract`,
`application_composition_source_contract`,
`application_review_round_trip_composition_contract`,
`architecture_boundaries`, `test_network_isolation_manifest`, clang-format 22
on the changed files, documentation gate. Live: disposable BPR-Test #180
(MikeMarcinTest's fork; review, push, GraphQL read; closed, branch deleted);
no live agent call.

Filed, awaiting the owner (not waited on; the URL stays in the session
state): session `ff82755e1b9fba2463ef8b257a6de0f5`, state
`/Users/jaeger/.attended-review-state/returning-review-v001-2026-11-12/state`,
cursor 6, host PID 48480 (its `tailscale serve` child 48488), candidates
`BPR-2073-what-changed` and `BPR-2074-was-addressed` (one Accept/Reject each);
frozen root `/Users/jaeger/returning-review-v001.34b39a8g` from `72288736c`
(app SHA-256 `7ffda3265ccf…`, launcher `--check` passed on both heads; the
look is the composed course, `returning_review_public_path latest|addressed
STUB --attended`, run from the root).

Disk: removed my own relinkable binaries in `build/dev` (agent chat and
invocation composition courses, profile tour handoff course, Qt workbench
and settings window tests); a named build relinks them.

Not touched: sessions 6d00826a… (75022/75032) and dfc73933… (6341/6351) and
their roots, PIDs 74290, 74692/74706, 10695, 39804, Tailscale 44738,
BPR-Test #152 and `-next`.

Next: read session `ff82755e…` after hand-back. Accept of both: finish
BPR-2074 (AC07), stop host 48480, remove the root with
`freeze_attended_build.py remove`. BPR-2075 (closeout, installed agent as
well as the stub) follows. Next milestone in plan order: `explain-file`.

## agent-assistance-v1: explain-file (2026-11-12)

Fresh `rfc-implementer`, same lane; nothing pushed. P02 settled (one editable
prompt per action, system prompt). P04 (the answer is a card at the file's
header) is unconfirmed and built as written. Frontend-design was loaded before
the Qt work. Record: [RFC-BPR-0117](../../../verification/RFC-BPR-0117.md).

| Commit | What |
|---|---|
| `780874846`, `2fd00e504` | RFC-BPR-0117 In Progress; ready and claim BPR-2076 |
| `fda988710`, `44e01a659` | Core `explain_file` (material bounds, note text); `analyze_current_file` acts on the open file; prompt `review.explain-file` (schema 31, hook tables 0 to 13); the card, the header control and the tab menu item; Agent tab Explain turns and their observer |
| `75ab1e0fa` | Merge `main` (`c5a0f0823`), at the owner's request ("Integrate latest from main when you get a chance"); see below |
| `d47362061`, `153e28d0f` | `window_explain_file.cpp`, Agent chat wiring, stub answer, composed course, card Qt test at three scales, launch policy, schema 31 storage tests, docs; tab strip keeps its minimum; app pin resealed. Product candidate |
| `bb8560026` | BPR-2076 parked `human-review` (AC01 to AC05 pass, AC06 pending) |

Main mirror: `62f64edf0` (BPR-2076 parked, RFC-BPR-0117 In Progress).
NEXT_ID (BPR-2151) and `_reservations` equal on lane and main.

Merge of main (`75ab1e0fa`). Brought in: account setup in the window
(RFC-BPR-0032 amendment r1), in-PR link routing, RFC-BPR-0125 milestone 1
(suggested prompts as macros, core only: `prompt_macro`), the review sheet's
pending-comments loading state, Linux release fixes, BPR-2131..2137 and the
main mirrors. Conflicts resolved: lane text kept for the tracker, handoff and
RFC implementation records (0107, 0108, 0109, 0110, 0116); main's RFC-BPR-0125
index row added; setup helpers from main's `setup_step_parts.hpp`; both
workbench strip glyphs; Home through `present_home_action`; the submission
workflow keeps the lane's keyed lock and main's `CommentsRead`; removing an
agent erases its entry check and restarts the check; the picker-wrap test stays
removed (the picker left the drawers); test lists and courses carry both sides
(`setup-list` beside `subject` and `custom-agent-refused`, `--tour-link` beside
the Enhance courses); a brace the merge dropped in the agent chat course
restored. RFC-BPR-0125's shipped macro set has no entry for What changed since
my review, Was this addressed? or Explain this file: the macros are not wired
into the Agent tab yet (its milestones 2 onward), and when they are, the
returning suggestions must become macros or stay beside them.

Runs at the candidate (dev, Debug): `core_explain_file`, `core_agent_chat_scope`,
`core_agent_question`, `core_returning_review`, `core_review_preparation`,
`core_agent_configuration`, `core_prompt_macro`, `application_agent_launch`,
`application_agent_launch_policy`, `application_agent_setup_host`,
`application_writing_delivery`, `tour_cli_composition`,
`sqlite_workbench_settings_storage`, `sqlite_agent_setup_storage`,
`sqlite_agent_configuration_storage`, `qt_explain_file_card_scale_{100,150,200}`,
`qt_settings_window`, `qt_workbench_window_scale_100`, `qt_conversation_cards_view`,
`qt_agent_launch_panel`, `qt_agent_setup_view`, `qt_agent_footer`,
`qt_skill_completion_keys`, `qt_agent_configuration_dialogs`,
`application_agent_chat_{composition,conversation,custom_agent,subject,setup_list}_contract`,
`application_agent_invocation_composition_contract`,
`application_composition_source_contract`,
`application_review_round_trip_composition_contract`,
`application_profile_tour_handoff_public_path`,
`application_returning_review_{latest,addressed,gone}_public_path`,
`application_explain_file_public_path`, `architecture_boundaries`,
`test_network_isolation_manifest`, clang-format 22 on the changed files,
documentation gate. `qt_workbench_window_scale_100` first failed ("a splitter
drag resizes the drawer") because the strip counted the new control in its
minimum; it no longer does. `application_agent_chat_custom_agent_contract`,
a baseline failure earlier, passes. No live agent or GitHub call.

Filed, awaiting the owner (not waited on; the URL stays in the session
state): session `0002b9fad401fad26fe5fa36206d539b`, state
`/Users/jaeger/.attended-review-state/explain-file-v001-2026-11-12/state`
(serve output beside it), cursor 3, host PID 29331 (its `tailscale serve`
child 29341), candidate `BPR-2076-explain-this-file` (one Accept/Reject);
frozen root `/Users/jaeger/explain-file-v001.4gr0t3jw` from `153e28d0f`
(app SHA-256 `fd4e064a52d0…`, launcher `--check` passed; the course
`explain_file_public_path explain STUB` also passed from the root; the look is
that course with `--attended`).

Known, deferred while the candidate is protected: the card sets its answer
area to a height of -1 once while hidden (Qt prints "Negative sizes" once); a
`std::max(0, …)` in `explain_file_card::fit_answer`.

Not touched: sessions 6d00826a… (75022/75032), dfc73933… (6341/6351),
ff82755e… (48480/48488) and their roots, PIDs 74290, 74692/74706, 10695,
39804, Tailscale 44738, BPR-Test #152 and `-next`. (Session ff82755e… already
holds an owner verdict on `BPR-2073-what-changed`: reject, with notes; for the
coordinator.)

Next: read session `0002b9fa…` after hand-back. Accept: finish BPR-2076
(AC06), then BPR-2077 (closeout), stop host 29331, remove the root with
`freeze_attended_build.py remove`. Next milestone in plan order:
`comment-to-suggested-change`.

## Local draft in the PR window: rework round (2026-11-12)

Fresh `rfc-implementer`, same lane; nothing pushed. Frontend-design loaded
first. Owner intent (session `6d00826a…` verdicts, verbatim in the
[RFC-BPR-0108](../../../verification/RFC-BPR-0108.md),
[0113](../../../verification/RFC-BPR-0113.md) and
[0114](../../../verification/RFC-BPR-0114.md) ledgers): augment the real pull
request window, do not rebuild it.

Step 0 (done): verdicts of sessions `6d00826a…` (Home 1 accept with a
selection fault; four rejects), `dfc73933…` (BPR-2138 Ask more accept) and
`ff82755e…` (BPR-2073/2074 reject, recorded only) written to the ledgers and
cards; hosts 75022/75032, 6341/6351, 48480/48488 stopped with the
attended-review `stop`; roots `local-draft-pr-window-v002.lyqc133b`,
`bpr2138-ask-more-v001.kbc0nnpf`, `returning-review-v001.34b39a8g` removed with
`freeze_attended_build.py remove`.

Plan (status updated as it goes):

1. The draft opens in the window's own review surface: Files drawer tree,
   Search tab, diff viewer with syntax colours, Overview, drawers, status bar.
   The separate page, its bar, its Files list and its diff view are deleted.
2. Author state adds only: Local Changes tab (always), branch with ahead and
   behind, the picker's Edit, tracked and excluded files behind a disclosure.
3. Edit view with syntax colours, following the selected file.
4. Agent commit message beside Commit, nib in the agent accent.
5. Short tooltips on the touched surfaces.
6. Home: one selection across its lists; Escape clears it.

All six done (status 2026-11-13). Main mirror `0cec1ec77` (verdicts on
BPR-2062, 2065, 2073, 2074, 2138; no card moved this round).

| Commit | What |
|---|---|
| `f6497fb90` | Verdicts recorded, hosts stopped, roots removed |
| `f8bcd3b31`..`22090f1fe` | Core `local_presentation` (a local comparison's identity for the viewer); `present_local_comparison` on both hosts; Qt `local_draft_parts` in the window's slots; `local_draft_view` and `local_diff_view` deleted; composition feeds the window and classifies the draft's texts |
| `2b1eb64f1`..`a84bcaa95` | Stage/Unstage Change on the code's menu; nib beside Commit in the accent; short tooltips (draft, Local Changes, publish steps, picker, Enhance, footer); Home's one selection with Escape; courses drive the window; Find in files over the candidate (`local_draft_search`) |
| `337b7d1ea`, `40f74bdbc` | Coordinator item 4: `prompt_kind` (RFC-BPR-0125) renamed `prompt_ask` as on `rfc0125-m2-on-lane` `ad948e5c8` (the rename is there, not in `224b97603`); `core_prompt_headers_together` |
| `9b55e58c8`..`2bb702657` | Search controller outlives its panel safely; app pin resealed; attended author look copies its worktree where Git may write (the v002 look's staging failed on the read-only root: "Unable to create … index.lock: Permission denied") |
| `af839e73d` | Ledgers |

Coordinator items (2026-11-13): (1) the agent chat course's brace is balanced
at `153e28d0f` (and the course passes); (2) `qt_workbench_window_scale_100`
passes at HEAD (its review-bar order now sets the draft's two hidden slots
aside); (3) the full `coding-format-check` passes at HEAD (2350 files); the
14 files reported at `d47362061` were already fixed by `153e28d0f`; (4) done
as above.

Runs at the candidate: see the [RFC-BPR-0108 ledger](../../../verification/RFC-BPR-0108.md#local-draft-in-the-pull-request-window-2026-11-13).
Documentation gate passes.

Known gaps (in the ledger): the Tour tab hosts the local tour editor, not the
pull request's Tour panel; a pull request opened from Home is not put in
author state by itself (author state comes with the draft associated with it,
which is how the look shows "a pull request opened by its author").

Owner look, filed and not waited on: session
`767b92339e278ea303b3836f21426fd3`, state
`/Users/jaeger/.attended-review-state/local-draft-in-pr-window-v003-2026-11-13/state`
(serve output beside it; the URL stays there), cursor 15, host PID 84895
(its `tailscale serve` child 84904). One Accept/Reject each:
`draft-in-pr-window` (its copy value is the second command, the pull request
opened by its author: `BPR_ATTENDED_LOCAL_DRAFT_PULL_REQUEST=512`),
`local-changes-commit-push`, `edit-view`, `agent-commit-message`,
`home-selection`. Frozen root `/Users/jaeger/local-draft-in-pr-window-v003.3yt_ouf8`
from `af839e73d89c` (executable SHA-256 `0cbb9ec0db39…`; launcher check
reached `ATTENDED_LOCAL_DRAFT_AUTHOR_SHOWN … 2 ahead of GitHub`), launch
`python3 /Users/jaeger/local-draft-in-pr-window-v003.3yt_ouf8/launch.py`.
The look opens fixture pull request #512 (promo-v1), then the draft from a
generated worktree it copies to `$TMPDIR/bpr-local-draft-look-<pid>` so Git
can write; those copies are the look's own scratch.

Renders compared offscreen (native looks need a cocoa window on the shared
desktop, which this lane does not open): the draft
(`build/dev/tests/end_to_end/local-draft-author/a-review.png`) against a pull
request (`build/dev/tests/adapters_qt/workbench-window-100-navigation-labels-selected-1280.png`):
the same title row, review bar, Files/Search tabs, diff viewer, tray and
status bar; the draft swaps in its candidate, branch and Create Draft PR…,
and adds Local Changes.

Next: read session `767b9233…` from cursor 15 after hand-back. Accepts finish
BPR-2062/2065 where their other criteria allow; a Reject is a new round. Then
stop host 84895 with the attended-review `stop` and remove the root with
`freeze_attended_build.py remove`. Session `0002b9fa…` (explain-file) untouched.

## Import: RFC-BPR-0126 queue and steer (2026-11-12)

Fresh `rfc-implementer`, same lane; nothing pushed; no product code. Owner:
"Add to queue /implement-rfc Implement the RFC at
docs/rfcs/assets/queue-and-steer-v1/RFC-DRAFT-0001-queue-and-steer.md".
Queued behind the lane's work, not started.

Mapping: RFC-DRAFT-0001 → RFC-BPR-0126; cards cut at import (the draft had
none) BPR-2151 (core and host queue, AC02-AC04), BPR-2152 (durable handoff,
AC06), BPR-2153 (native steering and accounting, AC05, AC07), BPR-2154 (Qt
composer, AC01, AC08), BPR-2155 (closeout), all Backlog; one milestone
`queue-and-steer` ([plan](../../../rfcs/plans/queue-and-steer-v1/MILESTONES.md),
[LIVE_IMPORT](../../../rfcs/assets/queue-and-steer-v1.LIVE_IMPORT.md)).
RFC_BASE_SHA `dbdecf9a32c9fc7cdd28122721dc61011236d60e`. The draft's bytes
are tracked here; the main checkout's untracked copy is untouched (a later
merge into that checkout must remove it first; same bytes).

Reconciliation (on the RFC): Queue and Steer only on resumed paths
(`agent_tab`, `tour_side_chat`, `tour_conversation`); Ask more stays the
only promotion; BPR-2140's system-prompt split applies to released entries,
Steer resends no instructions; BPR-2141's one call per fresh click is
untouched. The one conflict is BPR-1930's whole-tour feedback buffer
(`queue_tour_feedback`, `send_waiting_tour_feedback` timer poll): a fresh
first draft leads into the conversation without Ask more. Left as is until
owner question Q1. Q1-Q18 on the RFC list every default the owner did not
state; none is answered.

| Commit | What |
|---|---|
| main `9d899bb35` | BPR-2151..2155 reserved (counter BPR-2156); verified by a dry run offering BPR-2156 |
| `34a4f4073` | Import (counter and receipt from main) |
| main `8195599f8` | Mirror: RFC, cards, plan, LIVE_IMPORT, index section |
| this commit | LIVE_IMPORT names the retained draft without a link; handoff |

Runs: documentation gate on lane and main; `issue_board.py validate`
(lane and main, valid); `milestone_plan.py check` and `render`.

Next for this RFC: the owner's answers to Q1-Q18 (Q1 and Q2 shape BPR-2151's
scope); then BPR-2151. Implementation also needs, from the lane: the
`agent_run_path` policy (BPR-2138), the tour chat composition in
`window_tours.cpp`, and an installed Codex App Server and Claude for
BPR-2153's qualification. Not touched: sessions `767b9233…` and
`0002b9fa…` and their roots and hosts, PID 74290, host 74692/74706, PIDs
10695/39804, Tailscale 44738, BPR-Test #152 and `-next`.

## agent-assistance-v1: comment-to-suggested-change (2026-11-13)

Fresh `rfc-implementer`, same lane; nothing pushed. Frontend-design was
loaded before the Qt work. P05 (the checks before a reply is shown) is
unconfirmed and built as written. Record:
[RFC-BPR-0118](../../../verification/RFC-BPR-0118.md).

Coordinator directions this round: (1) merge `rfc0125-m2-on-lane`
(`c596320e5`) with a normal merge commit: `8ba69de50`; (2) owner redirect to
one Ask model: the action's core, host and prompt built, its entry on the
draft field's right-click menu, no new visible control, no owner look of its
placement (recorded on RFC-BPR-0118); (3) sessions `767b9233…` (all accept)
and `0002b9fa…` (BPR-2076 reject, "folded into Ask") folded in.

| Commit | What |
|---|---|
| `ad5d5d816`, `6c48db176` | RFC-BPR-0118 In Progress; ready and claim BPR-2078 |
| `5690f3540` | Core `suggest_change` (material, structured reply, P05 checks); seventh workshop operation; question rule; prompt `review.suggest-change` (schema 32, hook tables 0 to 14); `suggest.*` supplement fields |
| `1f26865cf` | Panel and window press/decision/proposal/Keep/Discard/Undo (with a tool-row control, removed next) |
| `8ba69de50` | Merge `rfc0125-m2-on-lane` (`c596320e5`) |
| `47a5ed625` | Entry on the field's right-click menu; Agent tab turns; app composition (`window_suggest_change.cpp`); stub reply; composed course |
| `c74171d0d` | Regressions for the new hook and M2 (`side_chat_panel_tests` member list), launch policy and storage tests, clang-format 22, app pin resealed. Product candidate |
| this commit | Ledgers, verdicts, card moves (BPR-2078 parked `human-review`; BPR-2021, 2022, 2062, 2145 Done; RFC-BPR-0108 Auto-Verified; BPR-2156 Backlog) |

Runs at `c74171d0d` (dev, Debug, macOS; 49 of 49 pass): `core_suggest_change`,
`core_agent_chat_scope`, `core_agent_question`, `core_agent_context_supplement`
(its member list, stale since RFC-BPR-0116, fixed), `core_explain_file`,
`core_returning_review`, `core_prompt_macro`, `core_review_preparation`,
`core_pull_request_suggestion`, `core_agent_configuration`,
`application_agent_launch_policy`, `application_agent_launch`,
`application_writing_delivery`, `application_agent_comment_workshop_host`,
`tour_cli_composition`, `sqlite_workbench_settings_storage`,
`sqlite_agent_setup_storage`, `sqlite_agent_configuration_storage`,
`architecture_boundaries`, `test_network_isolation_manifest`,
`application_composition_source_contract`,
`application_agent_chat_*_contract` (all), `application_agent_invocation_composition_contract`,
`application_review_round_trip_composition_contract`,
`application_profile_tour_handoff_public_path`, `qt_prompt_macro_chips`,
`qt_tour_side_chat`, `qt_side_chat_panel`, `qt_review_draft_editor`,
`qt_conversation_cards_view`, `qt_settings_window`,
`qt_agent_configuration_dialogs`, `qt_workbench_window_scale_100`,
`qt_explain_file_card_scale_100`, `application_suggest_change_public_path`,
`application_explain_file_public_path`,
`application_returning_review_{latest,addressed,gone}_public_path`,
`application_agent_chat_replies`. Full clang-format 22 check (2359 files) and
the documentation gate pass. Whole dev tree builds. clang-tidy not run.

RFC-BPR-0125 milestone 2 and the assistance actions: the merged Agent tab
shows the core's macro offers, and RFC-BPR-0116's returning prompts stay
beside them (first, as the host's questions, not macros); Explain this file
is a header control and Suggest the change a draft-field action, neither an
Agent tab chip. Not rewired: the Ask model will decide how they become quick
asks. BPR-2145 Done: `qt_prompt_macro_chips` (AC05 to AC10) passes at the
candidate.

Sessions folded in: `767b9233…` all five accepted (verbatim in the
RFC-BPR-0108 ledger); BPR-2021 (AC06), BPR-2062 Done; BPR-2022 Done and
RFC-BPR-0108 Auto-Verified; BPR-2065 stays In Progress (AC06 has no course);
BPR-2026..2031 not moved (their evidence ran on the deleted draft page and
their looks were rejected; RFC-BPR-0110/0111 need the PR-window rework);
follow-up BPR-2156 (Backlog, allocated on main `f0c799c71`): Changes tab
name, per-OS folder icons, Edit view underrail. `0002b9fa…` BPR-2076 Reject
recorded; parked pending Ask. Hosts 84895/84904 and 29331/29341 stopped with
the attended-review `stop`; roots `local-draft-in-pr-window-v003.3yt_ouf8`
and `explain-file-v001.4gr0t3jw` removed with `freeze_attended_build.py
remove`.

Main mirror: `f0c799c71` (BPR-2156 reserved), `ca15aa0d9` (card moves,
RFC statuses, verdict sections of the RFC-BPR-0108, 0113 and 0114 ledgers,
RFC-BPR-0118 and its record; BPR-2076's ledger link written as a lane path).
NEXT_ID (BPR-2157) and `_reservations` equal on lane and main. The lane's
renames of BPR-2021, 2022 and 2062 and the BPR-2156 card landed in
`c74171d0d` (staged with the code); their texts in `8cf1a29be`.

AC06 of RFC-BPR-0118 is pending: no attended session filed (the coordinator
withheld the look until the Ask model). BPR-2078 is parked `human-review`.

Not touched: PID 74290, host 74692/74706, PIDs 10695/39804, Tailscale
44738, BPR-Test #152 and `-next`. No live GitHub or live agent call.

Next: BPR-2079 (closeout; needs an installed-agent run and AC06) once the Ask
model settles the entry. Next milestone in plan order:
`failing-check-question`.

## Local draft in the PR window: Tour panel, Overview description, publish steps (2026-11-13)

Fresh `rfc-implementer`, same lane; nothing pushed. Frontend-design loaded
first (reuse of the accepted panels; no new visual invention). Brief: the
local draft's tour in the window's accepted Tour panel, its description in
Overview, the publish steps in the Status tab with Create Draft PR… in the
Submit slot, the window becoming the pull request, the tour carried; BPR-2065
AC06. Owner words in force as before, and "I thought we chose to stick with
the PR view and just augment it."

| Commit | What |
|---|---|
| `e48802f49` | Core `local_tour_view` (the panel's shapes from a local draft; command translation), `edit_local_stop`, `unaccept_local_stop`, Problem/Solution threads |
| `191b12294` | Qt: the Tour panel takes a local draft's words and has no Publish for it; Status tab page (`set_status_page`); publish steps rewritten (step list, What is sent); local Description and Tour areas deleted (`local_draft_preparation_area` removed); Rename… |
| `5337b1eda`, `736407b8f`, `e44dba18f` | Composition routing (window_construction, window_tours guards, `restore_tour_panel`, `carry_local_tour`), desktop bridge (queued edits), Create Draft PR… opens Status, the window becomes the pull request, the carry; courses; the look |
| `d8ea21b72` | BPR-2065 AC06: own lane for the author's Git, every push outcome reads GitHub again, courses |
| `667175975` | Opener through `open_review_destination`; app source pin resealed |
| `5cb4bda05`, `6887928bd` | RFC records; live course on the new surfaces and its sign-in diagnostic. Product candidate `6887928bd` |
| `17a012773`..`89156b02d` | Ledgers (0110, 0111, 0114); BPR-2065 Done; BPR-2026 Done; BPR-2027 parked `human-review` |

Main mirror: `489c64452` (BPR-2065, BPR-2026 Done; BPR-2027 parked; RFC link
paths; RFC-BPR-0114 ledger). NEXT_ID and `_reservations` unchanged (no
allocation). The RFC-BPR-0110/0111 ledgers stay lane-only, as before.

Runs: [RFC-BPR-0110 ledger](../../../verification/RFC-BPR-0110.md#in-the-pull-request-window-2026-11-13),
[RFC-BPR-0111 ledger](../../../verification/RFC-BPR-0111.md#in-the-pull-request-window-2026-11-13),
[RFC-BPR-0114 ledger](../../../verification/RFC-BPR-0114.md#ac06-courses-2026-11-13).
29 focused rows plus the two source contracts pass; documentation gate OK on
lane and main.

Live BPR-Test: blocked. The live course's sign-in stays at account-setup
phase `idle` (two runs; nothing pushed, no pull request, branches deleted;
rate 4,985 to 4,925). Not caused by this round's changes as far as read; it
needs its own investigation (the window's `begin_account_setup` returned
without starting discovery).

Owner look, filed and not waited on: session
`411e2c7497822b381ac2601afe7909f3`, state
`/Users/jaeger/.attended-review-state/local-draft-tour-panel-v005-2026-11-13/state`
(serve output beside it; the URL stays there), cursor 9, host PID 6331 (its
`tailscale serve` child 6340). One Accept/Reject each:
`local-tour-in-tour-panel`, `local-description-in-overview`,
`publish-steps-in-status`. Frozen root
`/Users/jaeger/local-draft-tour-panel-v005.kgbcys_i` from `89156b02d99e`
(product as `6887928bd`; executable SHA-256 `531e566d6c38…`; launcher check
reached `ATTENDED_LOCAL_DRAFT_TOUR_SHOWN`), launch
`python3 /Users/jaeger/local-draft-tour-panel-v005.kgbcys_i/launch.py`.
After hand-back: read from cursor 9; Accept of the tour candidate finishes
BPR-2027 (AC08), then BPR-2028 closes RFC-BPR-0110 and BPR-2029..2031 can be
claimed (the live run is still owed); a Reject is a new round. Then stop host
6331 with the attended-review `stop` and `freeze_attended_build.py remove` the
root.

Pending: BPR-2027 AC08 owner look (session above); BPR-2028 closes
RFC-BPR-0110 after it; BPR-2029 depends on BPR-2028, so BPR-2029..2031 are not
claimed (their evidence is on the ledger, the live run missing); RFC-BPR-0110
and 0111 stay open. Left on disk: `/Users/jaeger/local-draft-tour-panel-v004.9ao22ezm`
(a failed freeze's export; `remove` refuses it as not a frozen build).

## Live sign-in diagnosis, tour carry fix, and the comments/Ask/queue import (2026-11-13)

Fresh `rfc-implementer`, same lane; nothing pushed. Owner amendments in force:
"If you need it you need it go build it."; "main should be the source of truth
for the tracker"; "Continue working unattended, you can file reviews but dont
block on their results". Frontend-design was not loaded: no UX was designed
this round (accepted mockups copied as they are).

Part 1, the live course's "stopped at idle": **not an application defect and
not a release blocker.** The account setup driver (`tests/end_to_end/account_setup_course.hpp`)
presses Connect on the tick that shows gh's accounts and Start reviewing once
connected, so the course's poll for `choosing_account` missed it and saw the
setup back at `idle`, signed in. Reproduced with `--course reader --pull 1`,
instrumented, fixed test-only (`0a85bc2b5`). No bisect was needed: the course
was ported onto the driver by main's `f0dfd707c` (BPR-2132), merged here at
`75ab1e0fa`; `main` has the same course defect, card **BPR-2169** (Backlog,
on main) carries it there. The live run then exposed a **real product defect
in the shipped composition (lane-only code)**: a pull request created from a
local draft opened with an empty tour, because `carry_local_tour_to_pull_request`
made a destination-only stop `exact_original` without `Original` and
`core::apply` refused the restore (`invalid_source`), discarded by
`carry_local_tour`. Fixed test-first in `1237dbefd` (the fixture course now
applies the carry as the window does; it failed before). Live on BPR-Test the
draft course passes (#184; #181 to #184 closed, branches deleted; rate 4,877
→ 4,601). Ledgers: RFC-BPR-0111 and 0110.

Part 2, imports (docs only): RFC-BPR-0127 comments absorb notes (BPR-2157..2162,
plan `comments-absorb-notes-v1`, design `docs/design/comments-absorb-notes-v1`,
draft and inventory under `docs/rfcs/assets/comments-absorb-notes-v1`; all
fourteen defaults accepted, Local marked with a computer icon); RFC-BPR-0128
Ask (BPR-2163..2168, plan `ask-v1`, design `docs/design/ask-v1`; report host
= file tab; Q1 to Q14 otherwise open, built as defaults); owner amendments on
RFC-BPR-0007, 0011, 0012, 0023, 0049, 0085, 0086, 0094, 0101, 0102, 0107,
0108, 0110, 0111, 0113, 0115, 0116, 0117, 0118, 0119, 0121, 0125; RFC-BPR-0126
owner answers Q1 to Q18 (fold-in, local-draft discussion, Queue|Steer setting
default Queue with Cmd+Return, edit hold, queue tied to the session, steer
after the tool call) and Stop all (new AC09, AC04 amended; BPR-2151, 2154,
2155 and the plan follow).

Session `411e2c74…` folded in (coordinator's instruction, verified in its
`session.json`): `local-tour-in-tour-panel` Reject, `local-description-in-overview`
Reject, `publish-steps-in-status` Accept with a note; verbatim on BPR-2027,
2028, 2029 and the RFC-BPR-0110/0111 ledgers; not fixed this round. Host
6331/6340 stopped with the attended-review `stop`. Its root
`/Users/jaeger/local-draft-tour-panel-v005.kgbcys_i` is **not removed**: PID
26296 (the owner's frozen app, launched from that root) still runs from it;
remove it with `freeze_attended_build.py remove` once that app is closed.
`/Users/jaeger/local-draft-tour-panel-v004.9ao22ezm` (failed export, no
process) removed.

| Commit | What |
|---|---|
| main `eaf625a0f` | BPR-2157..2162, 2163..2168, 2169 reserved (counter BPR-2170; dry run offered BPR-2170) |
| `0a85bc2b5` | Live course sign-in waits for the outcome (test-only) |
| `f85b216ec` | Counter and receipts from main |
| `4b091837f` | RFC-BPR-0126 owner answers, Stop all |
| `fbdc1e509` | Import RFC-BPR-0127 and 0128, amendments, BPR-2169 |
| `b281fc291` | Session 411e2c74 verdicts |
| `1237dbefd` | The tour carry fix and its failing-first check; live course diagnostic. Product candidate |
| `3af4dc0bc` | Ledgers |
| main `aa26649c9` | Mirror: RFCs 0127/0128, cards, plans, designs, amendments, 0126 answers, verdicts, BPR-2169 |
| this commit | Handoff |

Runs at `1237dbefd` (dev, Debug, macOS): 19 focused rows (local draft core,
host, Qt and desktop courses, review tour, tour stop acceptance, profile tour
handoff, account setup, setup destination, device flow, remembered
connection) and the 9 other account-setup-driver courses pass;
`application_composition_source_contract`, `architecture_boundaries` pass;
clang-format 22 on the changed files; documentation gate OK on lane and main;
`issue_board.py validate` clean on both; plan checks pass. The whole dev tree
builds.

NEXT_ID BPR-2170 and `_reservations` equal on lane and main. The main
checkout's untracked `docs/rfcs/assets/queue-and-steer-v1/` is untouched.

Next: rework round for the two rejected looks (three differently styled
Accept buttons in the local Tour panel; other modes' drawers left open on
Overview) and the publish steps' circle alignment question; then a new look;
then BPR-2028, BPR-2029 (its live evidence is now on the ledger).

## Rework of session 411e2c74's notes: one Accept, Overview drawers, step marks (2026-11-13)

Fresh `rfc-implementer`, same lane; nothing pushed. Frontend-design loaded
first. Owner amendments in force as before. Records:
[RFC-BPR-0110 ledger](../../../verification/RFC-BPR-0110.md#rework-of-the-owners-verdicts-2026-11-13),
RFC-BPR-0106 [owner amendment: one Accept](../../../rfcs/RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md#owner-amendment-one-accept-2026-11-13).

| Commit | What |
|---|---|
| `f3fc567f3` | One Accept (the default button, only where a stop is accepted); Add to tour, Use this text; Apply all once per revision; `qt_tour_chat_in_tab` checks every Accept is one button at one size |
| `b8dca6417` | A local draft's Overview closes Review's drawers and the tour's chat (core never sees its modality); Overview's drawer drops Comments/Notes/Agent for a draft; step marks centred (labels kept height-for-width); courses failed first |
| `1f153d3b5` | A refused carry is shown in the Tour panel (`core::local_tour_carry_refusal_words`); app source pin resealed |
| `195140d79` | RFC-BPR-0106 amendment, RFC-BPR-0100, shared buttons, tours architecture. Product candidate and frozen commit |
| this commit | Ledgers (0110, 0111), BPR-2027 record, handoff |

Runs: 33 focused rows pass at `1f153d3b5` except `qt_workbench_agent_send_refusal`,
which fails identically at `1c7c5cd1f` (Explain this file keeps the shipped
accent after an accent change; pre-existing, not this round's; needs its own
card). Contracts, `architecture_boundaries`, clang-format 22, documentation
gate pass. Native renders (Cocoa off screen) light and dark are the media.

Owner look, filed and not waited on: session `a0cbb9ddda6d1ae524e5d63d4a3342d7`, state
`/Users/jaeger/.attended-review-state/local-draft-tour-panel-v006-2026-11-13/state`
(serve output beside it; the URL stays there), cursor 9, filed 2026-11-13 and
not waited on. Frozen root `/Users/jaeger/local-draft-tour-panel-v006.txmtt3kc`
from `195140d7968f` (launcher check reached `ATTENDED_LOCAL_DRAFT_TOUR_SHOWN`).
One Accept/Reject each: `tour-one-accept`, `overview-closes-drawers`,
`publish-step-marks-aligned`.
Host PID 45750 (its `tailscale serve` child 45758).

After hand-back: read from cursor 9. Accept of `tour-one-accept` and
`overview-closes-drawers` finishes BPR-2027 AC08 (Done, mirrored), then
BPR-2028 closes RFC-BPR-0110 and BPR-2029..2031 can be claimed; a Reject is a
new round. Then stop host 45750 with the attended-review `stop` and
`freeze_attended_build.py remove` the v006 root.

Left in place: `/Users/jaeger/local-draft-tour-panel-v005.kgbcys_i` (PID 26296,
the owner's app, still runs from it; remove with `freeze_attended_build.py
remove` once closed). Not touched: PIDs 20044, 74290, host 74692/74706,
10695/39804, Tailscale 44738, PID 39623 (another repository's review host),
BPR-Test #152 and `-next`, the main checkout's untracked
`docs/rfcs/assets/queue-and-steer-v1/`.


## RFC-BPR-0127 local-comment-model, BPR-2170, and the tour-one-accept rework (2026-11-13)

Fresh `rfc-implementer`, same lane; nothing pushed. Frontend-design was loaded first. Owner amendments in force as before.

| Commit | What |
|---|---|
| `807d8f5be`, `d6ccfb8b3`; main `6abd8a6e7` | BPR-2170 Done: Explain this file wears a changed Agent accent (the document tab strip re-wears its run controls); Resend sends a typed question again (lost since `7ff279c29`). `qt_workbench_agent_send_refusal` passes |
| `b50f64f54`, `f0639a9b2`; main `771509b86` | Ready and claim BPR-2157 |
| `e270bbfa4` | `tests/fixtures/review-ledger/v11-notes.sqlite`: a schema 11 ledger the shipped code wrote, notes before and after the path and line tables |
| `25d98b02a`, `84cde1c7f`, `72a0d55af` | BPR-2157: core `local_comment`, ledger port `store_local_comment`/`erase_local_comment`/`LocalComments`, schema 12 `private_local_comment` and the note migration, tests, AC03 course, docs, app source pin resealed |
| `3b385c04d` | Session a0cbb9dd rework: one button style for every Tour panel and card action (`make_tour_action`), test of class/flat/style/font/height. Frozen commit |
| `91a448ca7`, this commit; main `f463b4544`, mirror of this commit | Verdicts of a0cbb9dd verbatim; new session record |

State:
- `local-comment-model` is engineered; see the [BPR-2157 card](../../60-done/BPR-2157-comments-absorb-notes-model-store-migration.md#progress-2026-11-13) for the design, the identity decision and the evidence.
- AC11 passes against the shipped schema 11 file, interrupted at every statement.
- AC03 passes for Local comments on a line and on the pull request through the submission course. Threads, files and Publish response have no surface before milestones 2 to 4.
- AC13 has core evidence only.
- BPR-2157 stays In Progress until AC03 and AC13 have composed evidence with the Comments drawer (milestone 2).
- Integration pass at `72a0d55af`: 114 rows (note, ledger, private, agent launch and invocation, overview, explain, finding, feedback, local_comment, `architecture_boundaries`, source contract) pass. The 8 `qt_tour*` rows and `qt_workbench_agent_send_refusal` pass at `3b385c04d`. The documentation gate is OK.

Pre-existing failures, not this round's:
- `qt_document_tabs_scale_100` fails the same way without BPR-2170.
- `application_review_tour_link_public_path` clicks `tourReturn`, which `93b1045a8` removed.
- `application_review_tour_cli_ordinary_stop` stops at account-setup consent.

Session a0cbb9dd, handed back, verified in `session.json`:
- `overview-closes-drawers`: Accept.
- `publish-step-marks-aligned`: Accept.
- `tour-one-accept`: Reject (button style). Verbatim on BPR-2027 and the 0106/0110 ledgers.

Host 45750/45758 was stopped with the attended-review `stop`. `/Users/jaeger/local-draft-tour-panel-v006.txmtt3kc` is **not removed**: PID 51569, the owner's frozen app, runs from it. Remove it with `freeze_attended_build.py remove` once that app is closed. The v005 root is in the same state with PID 26296.

New look, filed, not waited on:
- Session `d420cccbfbe3c56bb172612cf51aa661`, state `/Users/jaeger/.attended-review-state/local-draft-tour-panel-v007-2026-11-13/state`, cursor 3, host PID 92512 (Tailscale 92524).
- Root `/Users/jaeger/local-draft-tour-panel-v007.hvczp7ad` from `3b385c04dacf`. One question, `tour-one-button-style`.
- After hand-back: an Accept finishes BPR-2027 AC08 (the drawers part is accepted), then BPR-2028 closes RFC-BPR-0110. A Reject is a new round. Stop host 92512 and remove the v007 root.

Next: RFC-BPR-0127 `comments-absorb-notes` (BPR-2158).
- The Comments drawer absorbs the Notes tab, with the state control, N, the computer icon and one gutter mark.
- It needs the move store operation. Settle how a move keeps identity: Local comments keep the note identity space, drafts the private frontier.
- With it, finish AC03 (threads, files) and AC13 composed, then close BPR-2157.
- Owner look on a frozen build whose fixture has Local, Draft and Published comments on one line and a migrated note. The fixture must be written: promo-v1 has none.

Pending decisions:
- A selected Local comment over the supplement's 16 KiB refuses the agent request (milestone 4).
- The Overview Note column now shows text (milestone 3 retires it).

### Follow-up in the same round: d420cccb accepted, RFC-BPR-0110 closed (2026-11-13)

Session `d420cccb…` was handed back at 16:14:32Z. `tour-one-button-style` was an **Accept** with no note, verified in its `session.json`. Host 92512/92524 was stopped with the attended-review `stop`. `/Users/jaeger/local-draft-tour-panel-v007.hvczp7ad` was removed with `freeze_attended_build.py remove`; nothing ran from it.

| Commit | What |
|---|---|
| `faa0761e0`, `a143adfc1` | Verdict recorded; BPR-2027 Done (AC08 pass) |
| `ffe23a183`, `966e078ec`, `3ceefb3e9` | BPR-2028 Ready, claimed, Done: **RFC-BPR-0110 Auto-Verified**. Closeout re-run at `a143adfc1`: 25 local-draft, tour-publication and Tour rows pass. `application_review_tour_publication_stop` failed once under parallel GUI load and passed alone three times |
| `a559d12e5` | BPR-2029 readiness judgment |
| main `01b881e0d` | Mirror of all of the above |

RFC-BPR-0111 is **not closed** this round:
- BPR-2029 and BPR-2031 cannot change lanes. `docs/rfcs/assets/local-draft-authoring-v1/AMENDMENTS.md` (protected) links their Backlog paths, and `issue_board.py move` refuses to edit it. A coordinator or owner decision is needed: an exception for those link lines, or a path-preserving approach (`immutable-review-link` assumes the card can at least park).
- BPR-2029's AC07 clause "a review submission for another pull request completes while a creation is in flight" has no course. It is structural only (per-host lanes).
- BPR-2030 depends on BPR-2029. Its AC03 to AC06 evidence is recorded (tour carry on the application path, live #184).

Next, RFC-BPR-0127 milestone 2 (BPR-2158), to start fresh.

Design start:
- **One move operation.** Add it to the SQLite ledger store, in one transaction:
  - Keep local deletes a `private_draft` row, after checking its revision, that no `private_publication` references it, and that the core allowed it, then inserts a `private_local_comment` row.
  - Make draft deletes the Local row and inserts a draft row with an ID from the scope frontier (`private_database::next`).
- **Identity.** A kept comment takes the draft's ID when no Local comment holds it, and the next free Local ID otherwise. The move returns the new identity, so the window and finding cards follow it.
- **Host orchestration.** Both the shared draft host (fan-out of drafts) and the review ledger host (`LocalComments`) must observe the move. Run it on the ledger lane, then refresh the shared draft host's roster from the store; do not have two hosts each delete their half.
- **UI.**
  - The Comments drawer takes Local comments. The two-segment state control (Local, and Draft or Reply) sits in the composer's foot.
  - N presets Local. The computer icon is `QStyle::SP_ComputerIcon`, with tooltip and accessible text "Only on this computer".
  - One gutter mark per line: thread over draft over Local. The Notes tab and `private_notes_panel` retire.
- **Look fixture.** Write one with Local, Draft and Published comments on one line and a migrated note; promo-v1 has none. Then freeze from HEAD and file one session.
- **Close BPR-2157** once AC03 is shown for threads and files and AC13 in the composed app.

## RFC-BPR-0111 closed; RFC-BPR-0127 milestone 2 started (2026-11-13)

Fresh `rfc-implementer`, same lane; nothing pushed. Owner correction in force: no Co-Authored-By or other attribution lines in commits.

| Commit | What |
|---|---|
| `d3e7cb8a8` | Coordinator decision: the local-draft-authoring amendment record names BPR-2029 and BPR-2031 by card reference; note in its LIVE_IMPORT |
| `0f444ac0a` | AC07 submission clause: `application_dependable_session_public_path` step 2c holds Create Draft PR's lane (`hold_local_draft_publication_lane`) and #18's Approve publishes; app source pin resealed |
| `65549c2ea` | RFC-BPR-0111 ledger closeout judgment |
| `188eb577c`..`5f5b4df23` | BPR-2029, BPR-2030, BPR-2031 readied, claimed, Done: **RFC-BPR-0111 Auto-Verified** |
| main `21f865e9f` | Mirror (ledger links rewritten as lane references, implementation records lane-only, as before) |

Runs at `0f444ac0a`: `core_local_draft_publication`, `core_local_draft_tour`, `local_draft_publication`, `application_local_draft{,_tour,_publication}_desktop_public_path`, `application_dependable_session_public_path`, `application_composition_source_contract`, `architecture_boundaries` pass; documentation gate OK on lane and main; board validate clean.

Next: RFC-BPR-0127 milestone 2 (BPR-2158), see below.

### RFC-BPR-0127 milestone 2 (BPR-2158), first round (2026-11-13)

Frontend-design was loaded first. BPR-2158 is engineered in part and **not claimed**: it depends on BPR-2157, which stays In Progress. Design, evidence and what is not built: [BPR-2158 card](../../30-in-progress/BPR-2158-comments-drawer-holds-local-comments.md#progress-2026-11-13-not-claimed).

| Commit | What |
|---|---|
| `9e408149a` | Store move: `keep_local` (the draft ends discarded, a revision on; the Local comment is stored, keeping the draft's identity where free) and `MakeDraftFrom` in one transaction; storage test |
| `e8553b931` | Local cards in Comments, the computer glyph, the composer's Local/Draft control, N (`localCommentAction`), the workflow's moves, the composition (`window_local_comments.cpp`), Notes tabs hidden, one gutter mark naming everything |
| `007d48bdc` | Qt tests; tests follow the retired Notes tab and N |
| `beede2fc3` | Composed look `--attended-local-comments-look`; Local reply marked in the open thread; app inventory and pin resealed. **Frozen commit** |
| `14261ddb7` | BPR-2158 progress record |

Runs at `beede2fc3` (dev, Debug, macOS), all passing:
- Qt: `qt_conversation_cards_view`, `qt_workbench_window_scale_{100,125,200}`, `qt_keyboard_shortcuts_scale_{100,125}`, `qt_private_notes_panel`, `qt_enhance_control`, `qt_drawer_surfaces_fit_scale_{100,125}`, `qt_button_faces`, `qt_tour_chat_in_tab`, `qt_hide_drawers_native_menu_key`.
- Storage and host: `sqlite_private_review_storage`, `sqlite_review_ledger_storage`, `application_review_ledger_host`, `application_private_review_host`, `application_review_ledger_composition_contract`.
- Composed courses: `application_review_submission_composition_public_path`, `application_review_tour_composition_public_path`, `application_explain_file_public_path`, `application_dependable_session_public_path`, `application_local_draft_publication_desktop_public_path`.
- Contracts: `application_composition_source_contract`, `architecture_boundaries`.
- The documentation gate is OK.

Owner look, filed and not waited on:
- Session `12534e154303dd1913382442a83b8c8f`, state `/Users/jaeger/.attended-review-state/comments-absorb-notes-v001-2026-11-13/state`. Its URL is in `serve.log` beside the state.
- Cursor 9. Host PID 68507, its Tailscale serve child 68517.
- Frozen root `/Users/jaeger/comments-absorb-notes-v001.1a7vqce8` from `beede2fc33b5`; the launcher check reached `ATTENDED_LOCAL_COMMENTS_SHOWN:6`.
- One Accept/Reject each: `comment-states-on-a-line`, `composer-state-and-computer-icon` (a line glyph, not `SP_ComputerIcon`, asked as part of the look), `notes-tab-retired`.
- After hand-back, read from cursor 9 and record the verdicts verbatim on BPR-2158. Stop host 68507 with the attended-review `stop`, then `freeze_attended_build.py remove` the root once nothing runs from it.
- The look holds no migrated note: one is an ordinary Local comment and looks the same. Migration evidence is AC11's storage course.

Remaining in milestone 2:
- Edge chips for Local comments on the pull request, a whole file or another file (AC06).
- The reply composer's Local/Reply control.
- Editing a Local comment in place, and asking before Delete.
- Composed AC04 and AC05 courses (Keep local in the review lowers the pending count; refusals).
- Hidden tabs out of the accessibility tree.
- Retiring the `private_notes_panel` code.
- BPR-2157's composed AC03 (a Send in a thread holding a Local reply sends none of it; a file-level Local) and AC13 (counts unchanged by Local), then close BPR-2157 and claim BPR-2158.

Not touched: the v005 and v006 roots and PIDs 26296 and 51569; PIDs 20044, 39623, 74290, 74692/74706, 10695/39804; Tailscale 44738; BPR-Test #152 and `-next`; the main checkout's untracked `docs/rfcs/assets/queue-and-steer-v1/`.

### RFC-BPR-0127 milestone 2, second round (2026-11-13)

Fresh `rfc-implementer`, same lane; nothing pushed; no attribution lines in commits. Frontend-design was loaded first.

Owner verdict on session `12534e15…` (forwarded by the coordinator, verified in `session.json`): `comment-states-on-a-line`, `composer-state-and-computer-icon` (the line-drawn computer glyph included) and `notes-tab-retired` all **Accept**, no notes. Recorded on BPR-2158 and in the new [RFC-BPR-0127 ledger](../../../verification/RFC-BPR-0127.md). Host 68507/68517 stopped with the attended-review `stop`; `/Users/jaeger/comments-absorb-notes-v001.1a7vqce8` removed with `freeze_attended_build.py remove` (nothing ran from it).

| Commit | What |
|---|---|
| `a8d2d9366` | Verdict recorded; ledger started (main `be5eb5774`) |
| `98069b5c5` | Edge chips (pull request count, This file, Other files), the reply field's Local/Reply control, edit in place, Delete asks, the Local reply's row; Save as Local moves a stored draft |
| next | Qt evidence `test_local_comments_edges_and_editing` |
| `64476cd4d` | Composed course `application_local_comments_composition_public_path`; N on a line where my draft stands opens it on Draft; Keep local frees its line (`leave_deleted_draft`); Make draft's offer recomputed on draft changes and redrawn |
| `fe32da7ee` | Formatting; app source pin resealed |
| `b79461670` | **BPR-2157 Done** (AC03, AC11, AC13) |
| `e384701c1`, `bf8303a93` | BPR-2158 Ready, claimed |
| `6ffcf4cce` | The look also seeds a whole-file and a pull request Local comment; pin resealed. **Frozen commit for look v002** |
| main `ddecc6d8f` | Mirror of the card moves, LIVE_IMPORT links and the ledger |

Runs (dev, Debug, macOS), all passing: `application_local_comments_composition_public_path` (four runs), `qt_conversation_cards_view`, `qt_workbench_window_scale_{100,125,200}`, `qt_drawer_surfaces_fit_scale_100`, `application_review_submission_composition_public_path`, `application_conversation_read_public_path`, `architecture_boundaries`, `application_composition_source_contract` (after each reseal). Format gate passes; `coding-tidy` not run. Documentation gate OK on lane and main; board validate clean.

Findings this round:
- The conversations fixture does not complete an inline thread reply's publication (Publish leaves the draft drafting, nothing reaches the server). The course records what Send hands to publication and checks every received body at the end.
- After a submitted review the course could not reopen a reply in the same thread (no reply draft is offered); not investigated.
- A draft held by a submitted review still offers the composer's Local segment; Save is then refused with its reason.

BPR-2158 remains In Progress for:
- Retiring `private_notes_panel` (hidden backend) and removing the hidden Notes tabs (now `setTabVisible(false)`). The courses and Qt tests that drive the panel move to the Comments drawer first: `review_submission_composition_public_path`, `review_tour_composition_public_path`, `explain_file_public_path`, `hide_drawers_native_menu_key_tests`, `workbench_window_tests`, `drawer_surfaces_fit_tests`, `button_faces_tests`, `enhance_control_tests`, `private_notes_panel_tests`; `dispatch_private_note`, `ask_agent_about_note` and the attended notes look in `main.cpp` read it.
- Owner look v002 (below), filed and not waited on.

Then milestone 3 (`finding-and-counting`, BPR-2159). It was not started this round.

Owner look v002, filed and not waited on:
- Session `3fefa3666ff9e2c2fc0943c4672f8b1e`, state `/Users/jaeger/.attended-review-state/comments-absorb-notes-v002-2026-11-13/state`. Its URL is in `serve.log` beside the state.
- Cursor 12. Host PID 29437, its Tailscale serve child 29448.
- Frozen root `/Users/jaeger/comments-absorb-notes-v002._jfvvf9h` from `6ffcf4cce274`; the launcher check reached `ATTENDED_LOCAL_COMMENTS_SHOWN:6`.
- One Accept/Reject each: `local-edge-chips` (AC06), `reply-local-state`, `local-reply-actions`, `local-edit-and-delete`.
- After hand-back, read from cursor 12 and record the verdicts verbatim on BPR-2158 and the ledger. Stop host 29437 with the attended-review `stop`, then `freeze_attended_build.py remove` the root once nothing runs from it.

Next action: retire `private_notes_panel` and the hidden Notes tabs (list above), then finish BPR-2158 (all its ACs have evidence; the panel retirement is the remaining milestone work), then milestone 3 (BPR-2159).
