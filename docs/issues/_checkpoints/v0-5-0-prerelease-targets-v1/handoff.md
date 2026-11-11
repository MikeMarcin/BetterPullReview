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
[BPR-2059](../../40-parked/BPR-2059-suggested-comments-read-as-postable-comments.md));
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
