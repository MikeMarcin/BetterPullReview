---
schema: "implement-rfc.card.v1"
id: "BPR-2181"
title: "The session queue host owns and drains every session's queue; windows only present it"
status: "In Progress"
rfcs: ["RFC-BPR-0126"]
depends_on: []
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2181--BPR-2181--a919bdd13421.json", "origin": "owner, 2026-11-14 (relayed): \"Why does a queue have anything to do with a window. Queues should be managed by something like a session manager at a higher level and have nothing to do with the UI, the UI is just a presentation layer\"; first critical review of the queue at 03aae05cb (B1, B2, N1 to N7); second critical review of the queue (2026-11-14); Prompts settings review B1", "criteria": ["AC01","AC02","AC03","AC04","AC05","AC06","AC07","AC08","AC09","AC10","AC11","AC12","AC13","AC14","AC15"]}
---

# BPR-2181: The session queue host owns and drains every session's queue; windows only present it

Part of [RFC-BPR-0126](../../rfcs/RFC-BPR-0126-queue-follow-ups-and-steer-native-agent-chats.md).
A correction to [[BPR-2151]], whose AC04 evidence was overstated: the queue
there was decided and drained by windows (the host stopped when no window
showed a session), and only `queue_pause::none` was ever settled.

The owner, verbatim: "Why does a queue have anything to do with a window.
Queues should be managed by something like a session manager at a higher
level and have nothing to do with the UI, the UI is just a presentation
layer".

## Acceptance

Origin: the owner's direction above; the first critical review of the queue
at `03aae05cb` (B1, B2, N1 to N7); the second critical review of the queue
(2026-11-14); and the Prompts settings review's B1. Each criterion has its
own course.

- **AC01 (first review B1):** The application-layer session queue host owns
  every session's queue and alone decides when its next message goes. It
  starts it through a profile-reachable turn runner, only on the previous
  run's typed finish and retirement on the decision lane; no window has to be
  open, free or loaded, no timer watches for retirement, and turn output for a
  session no window shows is kept in its transcript. Windows subscribe to its
  semantic deltas and send it commands. Course: a host test with zero windows
  drains a queue; a public path with the session's window closed.
- **AC02 (first review B2):** A run's finish carries its outcome; failure,
  usage limit and Stop pause the queue with that reason (a core mapping from
  outcome to pause). Course: a failing fixture agent leaves `Paused ==
  failed` with the second entry still queued; a usage limit pauses as usage
  limit. BPR-2151's AC04 evidence cited only a helper test for these and was
  overstated.
- **AC03 (first review N1):** A release returns a typed admission with its
  run id, and that run's completion settles it; a refused material read, a
  slash command or a slow retirement cannot leave a message Sending. Course.
- **AC04 (first review N2, N5):** A delivery-unknown message after a crash can
  be discarded, and an unsent entry goes back to Queued, both by core
  transitions. Course.
- **AC05 (first review N3; second review):** A refused store write is returned
  as a refusal and the composer keeps its text; queue store writes run on the
  host's effect path, not synchronously on the GUI thread. Course.
- **AC06 (first review N7):** Sessions and queued inputs are typed in core
  (session identity, the threads an ask carries), not strings built in
  composition. Course.
- **AC07:** A run that ends while a second window is still loading the pull
  request retires, and the session's queue goes on. Course:
  `application_agent_queue_second_window_loading_public_path`.
- **AC08 (second review):** Switching pull request mid-turn does not wedge
  the old session: its Sending entry and runner state settle from that run's
  retirement, per session, not per window. Course: switch mid-turn.
- **AC09 (second review; Q10):** Closing a window neither pauses a queue nor
  hands it over. Once the agent runtime is the profile's (slice 6), a close
  does not touch a running turn at all. Until then, a turn the close stops
  returns its message to the front, queued (core `return_unsent`), and the
  queue goes on only once that run has really retired; no other window starts
  the next turn before then. Stop pauses only when the reviewer presses Stop
  or Stop all. Course: close mid-turn
  (`application_agent_queue_windows_public_path`).
- **AC10 (second review):** Queue-versus-send is a core decision over the
  queue and the run state, not the view's; a Return in the gap before a turn
  shows working is queued and never credited to a queued entry. Course.
- **AC11 (second review):** Tour feedback never waits forever: shown and
  released by the host, returned to the chat as not sent after a crash (and
  its queue resumable), and the notice says only what will happen. Course.
- **AC12 (second review):** A stale refusal note is never read as a release
  verdict; Resume cannot send a message twice. Course.
- **AC13 (Prompts review B1):** Prompt facts are assembled once, outside the
  widget; the tab and the tour side chat are handed decided offers, and a
  queued release carries its decided expansion, so the tab never decides
  again. Course: a prompt conditioned on pushes since the review, pressed,
  reaches the agent.
- **AC14:** `agent_queue_host::steer(session, id)`: holds the message, offers
  it to the running turn; sent, it leaves the queue; otherwise it goes next
  and the reason is returned. Course.
- **AC15 (first review N6):** Stop all is a host operation across the
  profile's sessions and runs, not the footer's over one window. Course.

## Plan (slices)

Approved by the coordinator 2026-11-14, with slice 6 moved first after
slice 1.

1. Done (`c65c59df0`, AC09 corrected in the next commit): AC07, AC09, part
   of AC01 and AC14.
2. AC01 rest: a profile-level agent runtime, so a session no window shows
   still runs and keeps its transcript, a window close does not touch a
   running turn, and the Agent tab stops being the send transport.
   Application layer done (`c1c80c1e6`, see Evidence). The epoch actor
   registry admits a profile-owned host (`9c7672579`), bounded by the queue
   host's session limit (`03a540b91`). The lent lease landed
   (`0aff5a7be`): `github_application_host` lends the runtime a review
   authority in its own name and ends it on identity invalidation, on a 404
   or refused permission for its pull request, on shutdown, or when the
   runtime lets go, and an ended authority pauses the queue with
   `lost_access` and starts no turn (`32fb55c83`). The private-run port is
   now free of Qt (`application::scoped_private_agent_runs`; the window's
   `private_review_workflow` delegates to it), the boundary fix the
   architecture asks for. The app composition landed (`32946c58e`): the
   Agent tab's turns run on the profile's runtimes, a close detaches, and
   turns no window saw join the session's conversation. Those turns are
   now written to the profile's own conversation store as they end, so a
   quit before a window reattaches loses none (see Evidence, 2026-11-15).
   The window's own fallback host for turns is retired: with no runtime a
   turn is refused with words and never runs on a window (see Evidence,
   "Lane red since slice 2, fixed").
3. AC02, AC03, AC12: typed finish and admission with run id; core
   outcome-to-pause mapping; failure and usage-limit courses. Started
   2026-11-15: the typed finish and the core mapping landed, with the
   failure and Stop pauses and a refused turn that never stays sending.
   The usage limit is decoded and the fixture-agent public course for
   failure and usage limit landed. The typed admission keyed by run id and
   AC12 landed in the fourth round (see Evidence, "AC03 and AC12").
4. AC04, AC05: discard uncertain (back-to-queued done as `return_unsent`);
   store refusal; store writes on the effect path. Started 2026-11-15:
   Discard is a core transition and a host command, and the Agent tab
   offers it on an uncertain message. The store refusal landed in the
   fourth round (AC05), and the crash course (AC04) with it.
5. AC06, AC08, AC10, AC15: typed session identity and inputs; per-session
   runner state; core queue-versus-send; Stop all on the host. Done in the
   fourth round (see Evidence).
6. AC13, AC11: facts assembled outside widgets, decided expansions carried by
   releases; tour feedback through the host. With it, [[BPR-2184]]'s N4
   (`quick_ask_facts` sets each thread's `Changed` and `CommitsSince` from
   `core::thread_baseline` and `core::thread_progress_since`) and N5 (report
   identity: a stable turn identity and structured byline, reports keyed by
   it, dropped when their tab closes), recorded on BPR-2184 when done.

## Evidence

- **AC01 (partial, slice 2 application layer, `c1c80c1e6`):**
  `application::review_agent_runtime` and the `review_agent_runtimes`
  registry (`src/application/include/bpr/application/review_agent_runtime.hpp`).
  There is one runtime per review session, owned by the profile. The
  registry offers it to `agent_session_runner`, whose
  `agent_turn_runner_port::steer` is unchanged. A runtime runs its turns on
  machinery the wiring composes for it (`review_agent_turn_port`). It
  builds a released typed message's request from the review snapshot it
  holds, and it keeps every turn no window saw end (`unshown_turns`). It
  lives while its queue can drain or a turn runs, and the registry lets it
  go off its own callback once neither holds and no window shows it.
  Course: `application_review_agent_runtime`. A queue with zero windows
  drains in order, each request comes from the snapshot, and the transcript
  keeps all three turns. A window detaching neither cancels nor shuts down
  the running turn. A quick ask with no window stays queued and unpaused
  until AC13 gives releases a decided expansion. Still open for AC01: the
  app composition. Each runtime gets its own invocation stack (an epoch
  host holding the review's accepted handoff for workspaces, a ledger for
  session admission, and the process chain), windows attach and adopt
  snapshots, the Agent tab sends through `review_agent_runtime::dispatch`,
  and a close stops touching a running turn. The public path is to queue
  two messages, close the window, let the turns run, then reopen and see
  them.
- **AC01 (partial, slice 2, `9c7672579`), the composition's open risks
  checked:** (1) The epoch actor registry did refuse a context that is not
  a window. It admitted only the profile's live window observers, and every
  consumer took one of the profile's 32 window places. It now admits a
  profile-owned consumer (`attach_profile_owned`). That consumer is current
  until it retires or the registry closes, and it is bounded apart from the
  windows (`maximum_profile_owned_epoch_consumers`). `epoch_application_host`
  attaches as one through its own constructor. Course `application_epoch_host`
  (`profile_owned_host_holds_its_review_after_the_window_closes`): a
  profile-owned host accepts the review with no window, reads it after the
  window closed, and never takes a window's place. (2) Two ledger hosts on
  one review file do not clash. Admission is one `BEGIN IMMEDIATE`
  transaction over the scope's `agent_session_sequence` row, and every
  connection waits up to 2 s for a busy writer. (3) A third conflict the
  same course pins: the epoch host ingests only under a live lease, even
  for objects the profile cache already holds, and a lease is bound to the
  holder's application session. The window's lease is its read host's,
  bound to the window's session and revoked with it. The runtime therefore
  needs a lease lent in its own name by the GitHub host, and the profile
  must revoke it when the account or identity changes. That is the next
  step, at its owner (`github_window_read_host`, `github_application_host`).
  The profile-owned consumers take the queue host's session limit,
  `core::maximum_queued_sessions` (32), which `agent_queue_host::enqueue`
  now enforces (`03a540b91`, course `application_agent_queue_host`).
- **AC01 (partial, slice 2, `0aff5a7be` and `32fb55c83`), the runtime's
  own authority:** `github_application_host::lend_review_authority` lends an
  attached window's accepted pull request to a holder that is not a window.
  The handoff names the holder's application session, and its smart-Git
  lease is bound to that session. The lease is minted over the session's
  account transport, or over the credential-free one when signed out. The
  lease types moved to the internal `git_smart_authority_lease.hpp`, which
  the read host and the GitHub host share. The host ends a loan when the
  identity it was lent under is invalidated, when any attached window's read
  of that pull request under the same account answers 404 or refused
  permission, and at shutdown. It revokes every issued transport at once
  and then posts the holder's callback to the decision lane. The 404 signal
  is a new read-host observer (`observe_access_loss`), separate from
  `invalidate_epoch_authority`. Releasing the lease revokes it and tells
  nobody. The lender switching pull request or closing ends nothing. An
  account change, a generation change and a profile-revision change each
  invalidate the identity first, before the next session names its account
  or revision, and that is the moment the lease has to stop. So all three
  report one cause, `identity_changed`, and each has its own course. Course
  `application_github_host`: seven `test_a_lent_review_authority_*` cases
  (the holder's own lease and let-go, sign-out, account change, generation
  change, profile revision, pull request inaccessible, host closing).
  Runtime side: `review_agent_turn_port::authorized` gates every turn.
  `review_agent_runtimes::authority_ended` pauses the session's queue with
  the new `core::queue_pause::lost_access` ("Paused: this pull request can
  no longer be read with your account") and cancels the running turn.
  Course `application_review_agent_runtime`
  (`test_a_lost_authority_pauses_the_queue_and_starts_nothing`). Rerun on
  the dev tree at `32fb55c83` with the app built:
  `application_ask_menu_public_path`, `application_ask_queue_public_path`,
  `application_tour_feedback_queue_public_path`,
  `application_agent_queue_windows_public_path` and
  `application_agent_queue_second_window_loading_public_path` pass, along
  with `application_agent_queue_host`, `application_epoch_host`,
  `application_github_window_read_host` and `core_agent_queue`.
- **AC01 (partial, slice 2), the private-run boundary:**
  `application::scoped_private_agent_runs`
  (`private_review_application_host.hpp`) runs `admit`, `claim` and
  `finish` over a scope lookup under the ledger-file gate, with no Qt. A
  claim opens its own connection. `private_review_workflow` now delegates
  its `private_agent_run_port` to it, so the window keeps the panel and the
  run boundary is shared with the profile's runtime (coordinator,
  2026-11-14). Course `application_private_review_host`
  (`scoped_runs_without_a_panel`): a turn through it is admitted, claimed
  and finished durably, and a scope the lookup does not find is refused
  before any process starts.
- **AC05 (store, found under this card):** the agent queue store never
  saved a queue. Its entries cascaded from their queue, a cascade is a
  trigger program, and the hardened connection sets the trigger depth to 0,
  so every save failed with "triggers nested too deep". The store now
  deletes the entries explicitly, and the reference has no cascade. A file
  with the old tables can only hold them empty, so it is dropped and made
  again on open. Course `sqlite_agent_setup_storage`: every
  `core::queue_pause` value, `lost-access` included, saves and reloads as
  itself, and a file with the old cascading tables saves twice and reads
  back. Restored queues (AC04, AC11) were therefore never read from disk
  before this fix. Courses that claimed otherwise did not reach the store.
- **Rerun at this round's head (dev tree, app built):**
  `application_ask_menu_public_path`, `application_ask_addressed_public_path`,
  `application_ask_report_public_path`, `application_ask_queue_public_path`,
  `application_tour_feedback_queue_public_path`,
  `application_agent_queue_windows_public_path`,
  `application_agent_queue_second_window_loading_public_path`,
  `application_agent_queue_host`, `application_review_agent_runtime`,
  `application_private_review_host`, `sqlite_agent_setup_storage` and
  `agent_workspace_ownership` pass.
- **AC01 (slice 2 composition, `32946c58e`):** the profile owns the
  runtimes (`github_profile_runtime::AgentRuntimes`, made with the queue
  host by `compose_agent_queue`). Each session's runtime runs on a
  `review_agent_stack` of its own (`src/app/src/github_application/review_agent_stack.cpp`):
  an epoch host attached as the registry's profile-owned consumer and fed
  the handoff `github_application_host::lend_review_authority` lends in the
  stack's own application session (above every window's), a
  `review_ledger_application_host` over the review's ledger file for
  session admission, a `scoped_private_agent_runs` over its own private
  store, the process chain (launcher, Claude wrapper, located programs,
  added agents, diagnosis) and the agent invocation host. A turn is
  authorized only while the loan is live and the epoch host has accepted
  the review. Windows: `refresh_agent_authority` opens the runtime, attaches
  as presenter (`Release` is `release_queued`, gated by the tab's
  readiness) and lends the review when the runtime holds no live loan of
  it. `launch_agent_invocation` sends through `send_agent_turn`, which
  adopts the request as the runtime's snapshot and dispatches through the
  runtime. The window offers the runner only tour feedback now. A close
  detaches and no longer returns the ask session's message. A turn no
  window saw end is kept by the runtime (unless a native history keeps it),
  which stays while it holds one. When a window attaches and its
  conversation has loaded, they join the conversation, so each turn has one
  transcript and shows once. Courses:
  `application_agent_queue_close_runs_on_public_path` (a message running
  and two queued, the session's only window closed with another on Home:
  the runtime starts all three with no window, nothing pauses, and the
  reopened pull request shows each once, in order) and
  `application_review_agent_runtime` (a runtime with unseen turns stays
  until a window takes them). Still open: those turns live in memory until
  a window attaches, so a quit before then loses them. A native session's
  turns are durable already.
- **AC09 (close, `32946c58e`):** a close does not touch a running turn.
  `application_agent_queue_windows_public_path`'s close case now asserts
  that the turn running at the close and every message left run once each,
  in the list's order, with no pause.
- **AC05 (effect path, `32946c58e`):** `agent_queue_dependencies::Effects`
  carries the queue store's writes, in commit order, on the profile's
  serial queue lane, off the decision lane. Course
  `application_agent_queue_host` (`test_writes_run_on_the_effect_lane`).
  The refusal half of AC05 is still open.
- **AC04 (restart, `32946c58e`):** `application_agent_queue_restart_public_path`:
  two messages wait, the app quits, and on relaunch over the same profile
  data the queue is read back from disk paused as `restarted`. The window
  shows it as it was, offers Resume, and nothing runs. The discard of a
  delivery-unknown message is still open.
- **SQLite hardening audit (2026-11-15):** the hardened connection sets
  the trigger depth to 0, turns triggers and views off, and allows no
  ATTACH. Every store under `src/adapters/sqlite` was checked. No schema
  creates a trigger or a view. The tables with cascading parents are
  `review_scope` (review ledger), `recovery_scope`, `recovery_attempt`,
  `logical_publication` and `submission_attempt` (mutation recovery). Only
  `submission_attempt` is ever deleted, and its one delete already turns
  foreign keys off around an explicit child-first delete ([[BPR-1345]]).
  The others are only inserted, updated or upserted, which runs no
  cascade. The sqlite3 shell confirms this: with `.limit trigger_depth 0`,
  an upsert of a cascading parent succeeds and a delete fails.
  Every other foreign key has no action, and its parents are deleted
  child-first. No store's writes fail silently the way the queue store's
  did, so nothing needed fixing and no card was filed.
- **AC07:** `application_agent_queue_second_window_loading_public_path`
  passes (slice 1, `c65c59df0`). The runtime registry does not change it,
  because windows still run their own agent hosts until the composition
  lands. Rechecked 2026-11-14 at `9c7672579`: nothing composed changed, so
  that evidence stands.
- **Regression, `application_native_agent_sessions` (2026-11-15):** it
  failed for two reasons. (1) From `b8c371613` (BPR-2140 and BPR-2141; its
  parent `b8e2adeac` passes, both built in a scratch worktree), the agent
  canary missed the tour schema, which now travels in
  `--append-system-prompt`, and a tool-free Draft Tour run refused the
  canary's tool list and tool result. The fixture was wrong, not the
  product: the canary reads the appended system prompt and answers a
  tool-free run with no tools (recorded on [[BPR-2140]]). (2) Slice 2
  (`32946c58e`) broke `native-current-generation`: the runtime's host ran
  the native turn, so the window's host kept the history it had last read,
  and the history menu never showed the new turn. A turn's finish now tells
  the window's host (`agent_invocation_host::native_history_changed`), and
  the next look reads the store again. The course passes.
- **AC01 (durable transcript, 2026-11-15):** the profile has its own
  conversation store connection (`github_profile_runtime::AgentConversationStore`).
  A runtime writes each turn no window saw end, and no native history
  keeps, to the end of its pull request's stored conversation, on the
  workbench lane where windows read and write theirs
  (`review_agent_runtime::on_unshown`; the snapshot a window adopts names
  the conversation). A window that attaches skips a turn it already holds,
  so each shows once. Courses: `application_review_agent_runtime`
  (`test_unshown_turns_are_written_to_the_conversation`: a turn a window
  saw is not written, the turn running at the close and the next are, in
  order, and a native one is not) and
  `application_agent_queue_restart_public_path`, whose first part queues
  two messages behind a running one, closes the session's only window,
  lets the turns run, quits from the other window, relaunches, opens the
  pull request and sees each turn once, in order. The queue courses now
  run the fixture agent (`bpr_fixture_agent`): the writing stub they ran
  before answered every Agent-tab turn with exit 3, which went unnoticed
  until a failure paused the queue.
- **Slice 2 regressions found and fixed (2026-11-15):** the last round's
  wider pass was wrong: at `121c32f58`, built from nothing in a scratch
  worktree, 14 agent contract courses failed. Causes and fixes:
  (1) A question sent right after opening a review was refused ("The agent
  isn't ready for this pull request yet"), because the tab says ready once
  the window's host observed the review, before the runtime's stack has
  accepted the review lent to it. Now a turn sent while the lent review is
  being accepted waits for it and starts when the stack is ready
  (`review_agent_turn_port::accepting`, `review_agent_runtime::ready`); with
  no authority lent at all it is still refused. Courses:
  `application_review_agent_runtime`
  (`test_a_turn_sent_before_ready_waits_for_it`), and on the public path
  `application_agent_queue_close_runs_on_public_path` and the windows
  course, which type at once after opening and were refused before. The
  chat, finding, crash-recovery, suggest-change and agent-guard courses
  pass again. (2) `application_agent_queue_restart_public_path` failed at
  "the running ask finishes" on the lane tip: its quick ask was sent before
  the relaunched runtime was ready, and the writing stub it ran failed
  every Agent-tab turn. It runs the fixture agent, waits for the first ask
  to be sending, and passes. (3) `open_file_in_review` dispatched a file
  the review had only reported so far straight to the comparison, which
  cannot be asked for it, so the comparison stayed on BPR-2180's code
  skeleton. It now goes through the Files drawer's own selection, which
  holds the file and opens it once the review lists it. Only courses call
  it while a review loads (the product calls it for findings and quick
  asks, after the review is accepted), so no reviewer saw it; course
  `application_fixture_agent_guard_public_path`. The writing stub now
  answers an Agent-tab chat turn (stream-JSON, not a writing launch), so
  `application_ask_queue_public_path` goes on past its typed follow-ups
  now that a failed run pauses the queue.
  Still failing, and not from this round: `application_agent_invocation_composition_contract`,
  `_staleness_contract` and `_cleanup_retry_contract` (the fixture holds
  and a kept older revision on the runtime's stack),
  `_workspace_diagnostic_contract` and
  `application_agent_finding_dif_context_contract`; and
  `application_pending_states_public_path` ("the clicked file arrives"),
  which fails the same way with the 121c32f58 binary and with the runtime
  attach compiled out (A/B), so it is not the runtime. The window's own
  `AgentHost` fallback stays until the holds and the kept revision reach
  the runtime's stack.
- **AC02, AC03 (partial, slice 3, 2026-11-15):** `core::queued_finish`
  (went, failed, usage limit, stopped, not taken), `core::pause_for` and
  `core::finish`, which settles or returns the sending entry and keeps a
  pause already in force (a lost access is not overwritten by the Stop that
  follows it). The runtime reads its run's result
  (`application::queued_finish_of`) and the registry hands that to the
  runner, so `agent_queue_host::retired` takes the typed finish; a turn its
  machinery refused before it started retires as failed and never stays
  sending. Courses: `core_agent_queue` (`test_finish_maps_to_pause`) and
  `application_review_agent_runtime`
  (`test_a_run_finish_pauses_the_queue_with_its_reason`: a failing run, an
  unreadable answer and a Stop each pause with their reason, the second
  message still queued; a refused turn settles). The usage-limit decode and
  the fixture-agent public courses are still open.
- **AC04 (partial, slice 4, 2026-11-15):** `core::discard_uncertain` and
  `agent_queue_host::discard`. Courses: `core_agent_queue`
  (`test_discard_uncertain`) and `application_agent_queue_host`
  (`test_discard_an_uncertain_message_after_a_crash`: a message sending at
  the crash reads back uncertain, Resume is refused, Discard removes it and
  the store keeps that, and Resume sends what waited). The Agent tab's
  control is still open.
- **Lane red since slice 2, fixed (2026-11-15, second round):** the five
  contracts the last handoff named failed for four reasons, none of them the
  fixture holds. (1) `application_agent_invocation_composition_contract`:
  the turn ran under the runtime stack's application session and the
  course expected the window's. The context evidence now carries the
  session the turn runs under (`agent_invocation_context_evidence::RunSession`)
  and the expected supplement names it. (2) `_staleness_contract`: the
  course moves the window's accepted revision itself
  (`reaccept_fixture_revision`), past what the read host lends, so the
  stack kept the older revision and the run never went stale. The loan
  now follows the revision the window accepted, in every build and on one
  path: `github_application_host::lend_review_authority` takes the
  window's accepted handoff (`aAccepted`), checks that it names the
  lender's pull request under the same identity, and lends its revision
  (course `application_github_host`,
  `test_a_lent_review_authority_follows_the_windows_accepted_revision`).
  The tests-only branch that first did this is gone. The course still
  moves the window's revision with `reaccept_fixture_revision` rather than
  through the read host: the public-path fixture serves one pull request
  head per run (its epoch phase is chosen at start), so a live head move
  through the read host has no fixture to serve it.
  (3) `_workspace_diagnostic_contract` and
  `application_agent_finding_dif_context_contract`: the Dif's cues matched
  turns against the window's session and authority, and every turn now
  runs under the stack's. The window tells the Dif the identity its turns
  run under (`observe_agent_dif_context`): the stack's review once it
  accepted it, told through a new presenter hook
  (`review_agent_presenter::Accepted`, called from
  `review_agent_runtime::ready`). (4) `_cleanup_retry_contract`: the
  stack's epoch host is its agent host's workspace port, and the stack
  shut it down with the agent host, so a refused workspace removal could
  never be retried and the profile never finished shutting down. The stack
  now closes its epoch and ledger hosts only once its agent host is
  quiescent (`review_agent_stack::close_review_hosts`), the profile closes
  its epoch actors (whose consumer that epoch host is) only once every
  runtime is quiescent (`github_profile_runtime::close_epoch_actors`), the
  profile's
  shutdown poll retries every runtime's removal
  (`review_agent_turn_port::retry_cleanup`, `review_agent_runtimes::retry_cleanup`)
  and settles a turn whose run retired since. The fixture evidence reads the
  window's last runtime after a close detached it
  (`last_agent_stack`, `last_agent_runtime`). With those, the window's
  fallback host for turns is gone: `send_agent_turn` with no runtime
  refuses ("The agent isn't ready for this pull request yet. Nothing was
  sent.") and never dispatches on the window's `AgentHost`.
- **AC02 (usage limit, 2026-11-15):** `agent_protocol_failure::usage_limit`.
  Claude's synthetic assistant frame naming `"error":"rate_limit"`, Codex
  exec's `turn.failed` (or `error`) whose message names a usage limit, and
  the app server's `error` notification whose `codexErrorInfo` is
  `usageLimitExceeded` decode to it; the run says so
  (`agent_usage_limit_words`), and `queued_finish_of` maps it to
  `core::queued_finish::usage_limit`, which pauses the queue as usage
  limit. Course `application_agent_queue_finish_pauses_public_path` on the
  fixture agent: a message sent at once, then three queued; the queued
  `FIXTURE_CRASH` run fails and the queue pauses as failed with both later
  messages still queued; Resume, the `FIXTURE_USAGE_LIMIT` run pauses it as
  usage limit with the last still queued; Resume, the last is answered.
- **AC04 (Discard control, 2026-11-15):** a message whose delivery is
  unknown shows "May not have been delivered" and Discard
  (`agentQueueDiscard`) in the Agent tab's pending list; the press only
  calls `queue_dispatch::Discard`, which the window forwards to
  `agent_queue_host::discard`. Course `qt_drawer_surfaces_fit_scale_*`
  (the pending list with an uncertain row: only it offers Discard, the
  press asks for that message, and the drawer stays whole).
- **Verification, second round (2026-11-15):** a scratch worktree of
  `d9962f2ff`, configured and built from nothing, ran the agent and queue
  course set serially (ctest `-R "agent|queue|ask_|tour_feedback|prompt_macros|explain_file|suggest_change|review_tour|enhance_comment|returning_review|application_logging|qt_drawer_surfaces_fit"`):
  168 of 176 passed. Of the 8 that failed, four chat contracts
  (`application_agent_chat_{composition,stale,anchored_continuation,conversation}_contract`)
  pinned the window's `AgentHost->dispatch(` as the one launch path; the
  pin now names the runtime's (`0d70ff618`) and they pass. Four fail on
  this round's parent `4bb44f28f` too, built in the same scratch tree:
  `application_agent_chat_continued_contract` (it passed at `0d70ff618`,
  so it is intermittent), `application_enhance_comment_note_public_path`
  ("Use puts it in the Local comment to save"),
  `application_review_tour_link_public_path` (exit -11) and
  `application_review_tour_cli_ordinary_stop` ("consented to the chosen
  account ... no read question opened"). The writing-stub courses
  (tour writing, writing wrapper, ask courses, explain, suggest, returning
  review) and `application_logging*` pass. At `0d70ff618`, the scratch tree
  rebuilt and ran the 34 courses this round touched (`application_agent_chat_*`,
  `application_agent_invocation_*`, `application_agent_finding_dif*`,
  `application_agent_queue_*`, `qt_drawer_surfaces_fit*`,
  `application_agent_launch`, `application_agent_finding_protocol`): 34 of 34.
- **AC03 and AC12 (fourth round, 2026-11-15):** a release returns a typed
  admission keyed by the runtime's run id (`application::queued_admission`:
  `started`, `awaiting`, `answered` or `refused`, with `Run`). The runtime
  numbers its runs; a release reserves one, and a turn dispatched while it
  is reserved takes it, so only that run's retirement settles the message
  (`agent_session_runner` keeps the run and ignores another's retirement).
  A release whose turn a later pass sends (Explain this file reads its
  material first; a continuation realigns first) is `awaiting` and the
  session is busy; the later pass either sends the turn, which takes the
  run, or ends without one, and the window refuses that run
  (`review_agent_runtime::refuse_release`), which settles as
  `core::queued_finish::refused`: back first, the queue paused as failed.
  A window that closes while a release awaits returns it (`not_taken`). A
  slash command is `answered` with no run and leaves the queue as sent.
  Nothing reads `send_refusal_text()` any more: the Agent tab says what it
  did with a released message from its own control flow
  (`agent_tab_view::release_outcome`), and whether a turn started is the
  runtime's to say. So a refusal note an earlier send left is never a
  verdict, and Resume cannot send twice. Courses: `core_agent_queue`
  (`refused` goes back first, paused), `application_agent_queue_host`
  (`test_admissions_settle_every_release`: a slash command never stays
  sending and the next goes; a refused release is queued again, paused; an
  awaiting one stays sending until its run's refusal; a second Resume sends
  nothing) and `application_review_agent_runtime`
  (`test_release_admission_keyed_by_run`: a refused material read leaves
  the message queued and paused, never sending; a refusal of another run
  settles nothing; the awaited turn takes the release's run and its
  retirement settles it; a slash command never stays sending; Resume after
  a window that answered "refused" though it sent sends once, and a second
  Resume sends nothing; a window closing while a release awaits returns
  it).
- **AC05 (refusal, fourth round):** a store write the profile's store
  refuses is returned. Run where the queue changed, `enqueue` answers
  `core::queue_refusal::not_saved` and nothing was queued; run on the queue
  lane, the refusal comes back on the decision lane, the message that write
  queued leaves the queue and `agent_queue_subscriber::Unsaved` hands it
  back as asked. The window puts a typed message back in the composer, ahead
  of anything typed since, with a note saying it was not queued
  (`agent_tab_view::return_unqueued`); a refused follow-up never clears the
  composer. Course `application_agent_queue_host`
  (`test_a_refused_write_on_the_effect_lane_hands_the_message_back`, and
  the refused write in `test_restored_refused_and_limits`).
- **AC06 (fourth round):** `core::queued_session` (kind, host, owner,
  repository, number) with `session_key` and `parse_session_key`; the
  window, the tour feedback's session and the profile's runtime registry
  build and read keys only through it. `core::queued_input::Threads` holds
  the threads an ask carries as a list; the store keeps them one per line
  in its existing column. Courses: `core_agent_queue`
  (`test_typed_session`) and `sqlite_agent_setup_storage` (threads reload
  as queued).
- **AC08 (fourth round):** course `application_review_agent_runtime`
  (`test_a_pull_request_switch_mid_turn_settles_per_session`): a window
  switches pull request mid-turn; the old turn is untouched, its retirement
  settles that session's message and starts its next with no window, the
  new session runs beside it, and the old session keeps both turns no
  window saw end.
- **AC10 (fourth round):** `core::decide_follow_up` over the turn the tab
  shows, whether the session's queue holds a message (any entry, a
  released one whose turn does not show yet included), the setting and the
  modifier. The Agent tab asks it for Return and for its Send label. A
  Return in the gap after a release is queued, never sent as or credited to
  that message's turn. Course `core_agent_queue`
  (`test_follow_up_decision`).
- **AC15 (fourth round):** `review_agent_runtimes::stop_all` pauses every
  session's queue as stopped, then stops every runtime's turn, shown in a
  window or not. The footer's Stop all calls it first, then stops the runs
  its window runs itself (a tour's). Course `application_review_agent_runtime`
  (`test_stop_all_is_the_hosts`).
- **AC04 (crash course, fourth round, `10e168b99`):**
  `application_agent_queue_restart_public_path` goes on past its restart:
  the store is left holding a message as sending (as a crash leaves it; a
  quit stops and settles the turn) with one waiting behind it. On the next
  launch the queue opens paused, the message that was sending shows "May not
  have been delivered" with Discard, Resume is refused, Discard removes it,
  and Resume sends the waiting message once; the discarded one is never
  sent.
- **BPR-2193 (fourth round, `c0bdc7297`, `1af5aba43`):** the runtime's stack
  runs a turn only once the epoch host has settled the ingestion of the
  loan last lent, and holds a same-revision loan while a turn runs; the
  continued chat contract passes 20 of 20 (card in Done).

