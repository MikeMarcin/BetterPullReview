---
id: "RFC-BPR-0126"
title: "Queue follow-ups in BPR and steer native agent chats"
status: "Proposed"
metadata: {"origin":"imported 2026-11-12 from docs/rfcs/assets/queue-and-steer-v1, where it was RFC-DRAFT-0001; owner direction 2026-11-12 (BPR owns the queue, Claude and Codex steer natively, the feature stays cheap)","inspected_revision":"dbdecf9a32c9fc7cdd28122721dc61011236d60e","inspected_lane":"claude/v0-5-0-prerelease-targets-52265d","milestone_plan":{"id":"queue-and-steer-v1","path":"docs/rfcs/plans/queue-and-steer-v1/milestone-plan.json"},"baseline":"dbdecf9a32c9fc7cdd28122721dc61011236d60e","mandate":"owner, 2026-11-12: \"Add to queue /implement-rfc Implement the RFC\"; queued, not started; owner questions Q1 to Q18 below are open"}
depends_on: ["RFC-BPR-0092", "RFC-BPR-0096", "RFC-BPR-0101", "RFC-BPR-0102", "RFC-BPR-0103"]
---

# Queue follow-ups in BPR and steer native agent chats

## Problem and grounded current behavior

A reviewer should be able to prepare the next request while an agent works, or
correct its direction without rebuilding its conversation. The owner directed
this proposal on 2026-11-12: BPR owns the queue, Claude and Codex use native
steering, and the feature stays cheap. The remaining defaults below are
proposals, not additional recorded owner answers.

BPR already has persistent native conversations and exact-ID continuation under
[RFC-BPR-0096](RFC-BPR-0096-native-agent-sessions-and-tour-context.md).
The [native connection](../../src/adapters/agent/src/native_agent_connection.hpp)
keeps one process across turns, but `submit` refuses when a current or pending
turn exists. The [invocation host](../../src/application/src/agent_invocation_host.cpp)
also refuses concurrent admission. Neither exposes an active-turn input channel.

There is one specialized waiting buffer: `queue_tour_feedback` in
[tour_agent_session.cpp](../../src/core/src/tour_agent_session.cpp)
concatenates whole-tour feedback while Draft the tour runs. Its
[composition](../../src/app/src/github_application/window_tours.cpp)
sends that feedback after a successful proposal, using a bounded timer retry to
wait for host retirement. This is not a general editable message queue. The same
composition assigns Draft the tour and tour questions a native scope. However,
the newer owner amendment in
[RFC-BPR-0101](RFC-BPR-0101-agent-chat-conventions.md#amendment-which-agent-runs-are-fresh-and-which-resume-owner-2026-11-12-bpr-2138-bpr-2141)
requires a disposable first draft and explicit Ask more promotion. That
code/contract drift is not authority to make disposable runs conversational.

VS Code provides a useful ownership precedent: its ordinary chat service drains
pending requests, while newer sessions delegate that work to its agent-host
server. Its Claude pipeline tracks input identities across intermediate results.
These are application scheduling and delivery responsibilities; they need no
model to manage them. See the [source observations](#source-observations).

## Proposed behavior and boundaries

### Scope and ownership

Apply this to retained Agent-tab chats, their selected-lines/conversation entry
points, and retained tour chats, including their first conversational turn and
the whole-tour conversation. Entry points sharing a generation share one queue.
Separate agents, generations and tour parts never share inputs. A disposable first Draft Tour or
agent-assistance answer gains these actions only after explicit Ask more
promotion under RFC-BPR-0101; this proposal adds no deferred promotion or steering
of that one-off. Enhance, comment workshops, Review Cycle Send to Agent and
external terminal interaction remain outside scope.

The core owns bounded pending-input state, order, legal transitions and typed
identity checks. The application host persists it, schedules admitted input,
owns observed driver capabilities and marshals completion onto the decision
lane. The existing agent transport/protocol boundary frames writes and normalizes
receipts. Qt projects state and sends intents. Extend the current ports and
lifecycle; add no model API client, SDK runtime dependency or second agent loop.

The profile owns the shared queue and its single dispatch owner. Mutations and
handoffs compare the conversation generation and queue revision; a stale window
cannot overwrite an edit or submit an entry twice. All windows project this
state. The activation owning the current run owns automatic dispatch; enqueueing
from another window does not transfer it. Closing the owning review/window pauses
the queue; closing a viewer does not. Another window must explicitly Resume the
idle queue after revalidation to take over.

The one-writer rules for native conversations and shared PR workspaces remain.
This proposal permits explicitly queued follow-ups to the conversation already
running; it does not introduce a scheduler between competing conversations.
An idle chat competing with another chat's workspace use retains the existing
busy refusal. A queued dispatch encountering such contention pauses, with its
input retained; it does not poll or retry automatically.

### Composer and pending messages

At idle with no pending entries, Send starts a normal turn. While this
conversation runs or its queue is nonempty, Send is labelled **Queue**, including
when paused. **Steer** is a separate named action for a supported active native
turn; waiting only on helpers does not itself make a run steerable.
Stop remains independently available; typing a follow-up must not replace its
only route. Return and Ctrl/Cmd+Return perform the displayed Send/Queue action;
Shift+Return inserts a line, retaining the input-method protections of
[RFC-BPR-0101](RFC-BPR-0101-agent-chat-conventions.md).
Steer also has a keyboard-accessible command; no new global shortcut is required.

A compact pending list identifies queued text and captured subject/attachments,
with Edit, Remove and reorder actions for known-unsent entries. **Pause queue**
stops further dispatch without stopping the current run. Beginning an edit first
pauses and holds that entry against handoff; if dispatch won, preserve the editor's
text without changing the sent input. Save or Cancel releases the hold and leaves
the queue paused; other windows cannot Resume through an edit hold. Closing the
editor keeps the last saved entry paused. Keyboard commands provide reordering
too. Draft edits, focus, scroll
and selection survive projection updates. Queue operations follow the existing
palette, accessibility and agent-accent rules; queued status is conveyed in
words, with one polite announcement per completed user action.

Show **Queued**, **Sending**, **Sent to agent**, **Paused** or **Delivery
unknown**, as applicable. Sent means the runtime acknowledged ownership of the
input, not that the model understood or applied it. Sending, sent and uncertain
inputs cannot be edited, reordered or withdrawn. Stop ends execution; it cannot
unsend text or undo work already done.

Retain the existing 16 KiB UTF-8 question limit. The proposed queue limits are
16 pending inputs and 256 KiB of aggregate serialized payload per conversation,
including attachments and steering until its BPR run settles. Existing context
and history limits also apply; unsettled records cannot be evicted to make room.
A capacity, validation or save failure preserves user text and prior entries and
names the refusal. Show Queued and clear the composer only after durable save.
Settled entries use the existing bounded conversation history.

### Queue dispatch and context

Queue stores an input in BPR; it sends no input to the native driver until
dispatch. Each input retains its stable ID, user text, agent/configuration,
conversation generation, authority/revision, subject, selected lines or passage,
and explicit private-context identities and revisions. Navigation changes none
of those selections, following
[RFC-BPR-0102](RFC-BPR-0102-agent-questions-say-what-they-point-at.md).
Private material follows existing admission, storage and disclosure rules; queue
capture is not private-run admission and never copies unchecked prose. It does
not enter ordinary logs or diagnostic metadata.

For an unpaused queue, successful BPR completion releases one next entry in
displayed order. Each entry is its own turn. Completion includes durable result
handling, processing any tour proposal, settling helpers and accepted steering,
and host retirement so the workspace lease can be reacquired. A raw `result` or
silence is insufficient. This extends the existing turn boundary of
[RFC-BPR-0103](RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md).
The next dispatch waits on the real lifecycle completion, not a timer retry.

Before dispatch, recheck the bound conversation/configuration, authority,
permissions, revision and workspace admission, including ordinary private-run
admission. Revalidate selected private identities/revisions against current
observations; a queue record or cached text cannot substitute for admission.
Construct one coherent current BPR snapshot within the original binding, keeping
the captured subject and attachments. Include the preceding tour proposal and
its decisions where relevant, without implicitly accepting it or adding newly
selected private material. A changed/deleted target, invalid private selection,
changed configuration or unresolved old-revision choice pauses the queue. It
never retargets to the currently selected chat, lines, account or revision.

Successful enqueue authorizes those follow-ups after the current work. A
transient disconnect, native resume or restoration of history supplies no new
authority. Closing the owning review/window or app, Stop, failed/unreadable/stale
results, usage limits and uncertain delivery pause the remaining queue. A
restored queue opens paused. **Resume queue** explicitly releases its known
unsent entries through the same checks, immediately if idle and otherwise after
successful completion; it never bypasses unresolved delivery or resumes an
interrupted run. Enqueueing while paused does not resume the queue.
New session and Reset tour retain the original generation's pending text in
history, paused, without moving it into the new generation; existing history
and purge semantics govern its eventual removal.

The general queue replaces ad hoc waiting only in these resumed chats; the
disposable first-draft path is outside scope. Queue never publishes feedback,
answers an approval or grants additional tools.

### Durable handoff and recovery

Queue dispatch and Steer use the same delivery rule. Atomically claim the exact
entry/version and durably mark it Sending, bound to its BPR run and known native
session/thread (also the expected turn for Steer), before an input write. No
entry becomes both dispatched and still eligible for dequeue. A failure proven before any write leaves it
unsent; a possible write without a definitive outcome is Delivery unknown.
Persist receipts and terminal disposition before advancing the queue. A failed
receipt save is uncertainty on recovery, even if execution completed.

Reconcile uncertainty only with qualified native status/history, never a guessed
timeout or automatic resend. Until resolved, retain the text/identity and pause
later entries. Explain the existing inspect-history/New session recovery; an
explicitly composed replacement is a new request and may repeat earlier work.
Late receipts update only the originating record. Terminal delivery uncertainty
uses bounded transport shutdown before releasing execution ownership, never an
indefinite workspace lease or an automatic reconnect-and-resend.

### Native steering and completion races

Steer submits a correction to the exact active BPR run, leaving its conversation,
workspace, model, effort, tool permissions and result contract in force. It may
carry source attachments allowed by that run's admission. Adding private sources,
widening permissions or changing runtime settings requires a subsequent admitted
turn. Steering corrects the main agent at its next supported boundary; it does
not directly address helper threads, undo work or provide emergency cancellation.

Serialize steering admission and terminal decisions under the originating run
identity. If completion wins before handoff, or the runtime definitively refuses
an ended/wrong turn without consuming the input, retain it as unsent and offer
Queue; do not silently start another turn. Corrections are handed off in admitted
order, each with its own identity. BPR neither merges nor delays them for batching.

Claude may accept a correction just as its native turn ends and consume it in a
successor native turn. That accepted continuation remains part of the original
BPR run, under its lease and accounting. Successful completion requires every
accepted input to have a terminal disposition and reported work to have ended.
A result can leave a positively identified continuation or helper outstanding;
it does not license the next queued request. If a native terminal cannot be
reconciled to settled inputs or identified outstanding work, end with visible
delivery uncertainty and bounded retirement. This also covers hooks that drop
an echoed input without a result. A failure pauses the queue even if some inputs
were consumed. Acceptance never promises that the agent obeyed the words.

### Driver mapping and qualification

| Driver | Queue release | Steer |
|---|---|---|
| Codex App Server | `turn/start` on the saved thread | `turn/steer` with the active `threadId`, `expectedTurnId` and new input; match the RPC receipt to that run |
| Claude bidirectional stream JSON | One user input on the continuing session after dispatch admission | Proposed qualified mapping: UUID-bearing input with `priority: "next"` and human origin |
| Codex exec / another harness | Existing supported continuation or explicitly labelled replay mode | Unavailable unless the actual transport is separately qualified |

Claude's SDK documents `next` (also the default) as delivery after current tool
calls, or in a successor turn if the current one ends first. Although `later`
exists, BPR keeps unsent messages locally; do not prefill the runtime's queue.
`now` can interrupt or background work and is excluded; immediate intervention
uses Stop. SDK documentation alone does not qualify BPR's direct CLI transport.

Qualify native steering against the actual supported CLI and wrapper, including
input identity, acknowledgment, terminal correlation and cancellation. Claude's
result envelope `uuid`, which BPR currently fences, is distinct from the input
UUID and cannot establish which correction was consumed. Use qualified input
acknowledgments and `user_message_uuid`/`user_message_uuids` result correlation
where reported. Result identity and input identity are separate fences. Neither
an echoed input nor a zero queued-input count proves all work finished; helper
notifications and dropped-input handling remain necessary. Missing/conflicting
correlation cannot manufacture success. Reject further input before identity or
run bounds are exhausted; do not discard fences or reset them per correction.
An older driver may retain Queue while explaining why Steer is unavailable.
Capability checks use local protocol/version observations or explicit checks;
opening a chat or enqueueing text must never launch a paid probe. A failed steer
does not fall back to kill/restart, transcript replay or another agent.

### Cost and existing contracts

Enqueue, edit, removal, reordering, restoration and display perform zero inference
and no workspace materialization. No model classifies intentions, summarizes
pending text, generates acknowledgments, polls readiness or keeps a cache warm.
Only a released queued entry or explicit Steer can authorize further inference.

Reuse the existing native connection and conversation. Append the new text and
necessary context updates using RFC-BPR-0096's bootstrap/freshness contract;
do not replay BPR's visible transcript, reread unchanged source deliberately or
force compaction because an input was queued or steered. Keep stable guide and
tool definitions stable where the driver permits. The feature adds no default
model/effort change, background helper or extra summarization request.

A continuation still consumes history/cache-read and output tokens. Attribute
observed usage to the BPR run once across all native parts, including interrupted
generation and continuations. Preserve RFC-BPR-0101's cumulative-session handling:
deduplicate result identities and derive deltas from the established baseline,
never sum cumulative totals. Missing or reset totals remain explicitly unknown
until a valid baseline is established; do not invent Codex dollar figures.
Configured deadlines, output/resource limits and Stop cover the whole BPR run,
without resetting for a correction or native continuation. Each released queued
entry starts a new admitted run under its captured limits; it does not extend
the preceding run's budget. The existing no-time-limit default is unchanged.

This proposal changes busy-send legality for resumed chats in
RFC-BPR-0096, adds accepted steering to RFC-BPR-0103's turn boundary, and extends
RFC-BPR-0101's cost attribution and composer actions. Other disclosure,
publication, source-staleness, workspace, reset and native-history contracts
remain authoritative. Adoption must reconcile those live documents; this draft
does not change their current status or allocate live work.

### Reconciliation with the fresh and resumed rule (import, 2026-11-12)

Amended at import against the lane at the baseline revision. The owner's
classification of 2026-11-12
([RFC-BPR-0101 amendment](RFC-BPR-0101-agent-chat-conventions.md#amendment-which-agent-runs-are-fresh-and-which-resume-owner-2026-11-12-bpr-2138-bpr-2141))
is built and Done: [[BPR-2138]] (every launch path names its session mode in
`core::agent_run_path`, and Ask more), [[BPR-2140]] (built-in instructions
as the system prompt) and [[BPR-2141]] (a fresh one-off is one tool-free
call). This RFC fits that rule as follows; where the text above reads
otherwise, this section governs.

- **Queue and Steer exist only on resumed paths.** The chats in scope are the
  resumed paths `agent_tab`, `tour_side_chat` and `tour_conversation`. The
  core refuses an enqueue or a Steer on a path whose `session_mode_of` is
  fresh, as it already refuses a native turn on one. No fresh run gains a
  queue, a session or a steering channel.
- **Ask more stays the only promotion.** A fresh answer becomes a
  conversation only through Ask more, whose first resumed turn carries the
  seed of the fresh question and answer (accepted by the owner in attended
  session `dfc73933…`). Once promoted, the thread is an ordinary resumed
  path with Queue and Steer. Enqueueing while a fresh run works is not a
  promotion (but see Q1 for the one existing exception).
- **One call per fresh click.** Nothing here adds a call to a fresh one-off:
  it has no native turn to steer and nothing waits behind it.
- **System prompt.** A released queued entry builds its turn like any other
  turn of its path: built-in instructions as the system prompt, the run's
  material in the message ([[BPR-2140]]). A Steer carries the reviewer's
  text and permitted attachments only and never resends instructions; this
  is what "keep stable guide and tool definitions stable" means here.
- **The whole-tour feedback buffer.** [[BPR-1930]] AC3 (owner, 2026-11-07:
  feedback "particularly during the initial agent draft") holds whole-tour
  feedback typed while Draft the tour runs (`queue_tour_feedback`) and,
  after a successful proposal, sends it as the whole-tour conversation's next
  turn (`send_waiting_tour_feedback`, polling `tour_session_idle` every
  100 ms up to 50 times). Since [[BPR-2138]] the first draft is a fresh run
  (`draft_tour_first_draft`), so this buffer is the one place a fresh run
  leads into a conversation without Ask more; its turn carries the proposal
  in its prompt but not the Ask more seed. The draft leaves the buffer out
  of scope and unchanged. Q1 asks the owner whether to keep it or to fold it
  into this queue as an explicit promotion; until answered, implementation
  leaves it as it is.
- **Other resumed paths.** The local draft's discussion
  (`local_draft_discussion`), Review Cycle rounds, self-review and fix
  loops, and address-feedback runs are resumed but not in this scope. Q2
  asks whether the local draft's discussion joins.
- **Cost.** The accounting extends the existing cumulative-session handling
  (turn and session cost per run, deltas from an established baseline).

### Source observations

Inspected 2026-11-12. These establish protocol/design inputs, not installed-agent
acceptance evidence:

- [Codex App Server](https://developers.openai.com/codex/app-server/#steer-an-active-turn)
  defines active-turn steering, `expectedTurnId`, acceptance and forbidden
  turn-level overrides.
- [Claude SDK user messages](https://code.claude.com/docs/en/agent-sdk/typescript#sdkusermessage)
  document priority, input/result identity, helper caveats and hook-dropped
  messages. These are candidate semantics to qualify on the actual CLI/wrapper,
  not an SDK dependency or installed-agent evidence.
- [Claude CLI flags](https://code.claude.com/docs/en/cli-reference)
  document bidirectional stream JSON and optional user-message replay receipts.
- [VS Code chat scheduling](https://github.com/microsoft/vscode/blob/2c4376d5a2e5b6278c3c6b685bc9d0488857ae19/src/vs/workbench/contrib/chat/common/chatService/chatServiceImpl.ts#L2431-L2454)
  shows ordinary dequeue and the agent-host ownership distinction.
- [VS Code Claude queue](https://github.com/microsoft/vscode/blob/2c4376d5a2e5b6278c3c6b685bc9d0488857ae19/src/vs/platform/agentHost/node/claude/claudePromptQueue.ts)
  preserves input identities and postpones completion until input has drained.
  Its separate [steering implementation](https://github.com/microsoft/vscode/blob/2c4376d5a2e5b6278c3c6b685bc9d0488857ae19/src/vs/platform/agentHost/node/claude/claudeAgentSession.ts#L1278-L1306)
  uses `now`; this proposal deliberately chooses the documented tool-boundary
  `next` behavior instead.
- [OpenAI prompt caching](https://developers.openai.com/api/docs/guides/prompt-caching)
  and [Claude costs](https://code.claude.com/docs/en/costs) support stable context
  reuse and distinguish cached input from free inference.

## Acceptance criteria

- **AC01:** Every in-scope chat accepts and displays multiple busy follow-ups
  with the defined Queue/Steer distinction, keyboard behavior and unsent editing
  controls; projections preserve the composer and captured subject.
- **AC02:** Queue maintenance and restored paused queues produce no inference
  or workspace creation. Refused persistence/capacity leaves prior entries and
  user text intact, under the defined limits.
- **AC03:** A successful originating run dispatches known-unsent entries in
  displayed order only after durable application completion and host retirement,
  with the preceding tour proposal represented correctly. Disposable first
  drafts gain neither implicit promotion nor these actions.
- **AC04:** Stop, failure, usage limits, closure and restart produce the defined
  retained paused state. Changed authority, revision, configuration and context
  cannot retarget queued input or bypass admission; competing chats retain busy
  refusal and there is no automatic contention retry. Multiple windows agree on
  one queue; stale edits, closure and takeover preserve the defined ownership.
- **AC05:** Qualified installed Claude and Codex App Server each accept a native
  correction on their existing conversation without transcript replay or a
  deliberate process restart. Unsupported configurations keep Queue and report
  Steer as unavailable without a paid discovery probe.
- **AC06:** Queue and steering handoff survive crash/write/save races without
  losing or automatically duplicating input. Intermediate/duplicate results and
  dropped corrections cannot report premature success, retarget output, or hold
  a terminal run's lease indefinitely. Unknown input stays identifiable and
  never auto-resends.
- **AC07:** All native parts of a steered run contribute once to the existing
  turn/session accounting and limits. Unreported costs remain unknown, and
  pending-input management adds no model request, summarizer or keep-alive call.
- **AC08:** The composed Qt flow exposes pending, paused, sent and uncertain
  states and recovery with reachable Stop and keyboard/accessibility actions,
  while retaining the existing proposal-acceptance and publication boundaries.

Installed-agent qualification should use the smallest bounded conversations
that distinguish Queue from Steer, reusing functional fixtures for failure and
race coverage. Runtime observations must name exact versions and actual usage;
this authored draft and its independent review supply no implementation or
installed-agent acceptance evidence. No material owner question remains open in
this proposed version; installed transport qualification is implementation work.

At import the owner's standing direction was applied: every default above that
the owner did not state is a proposal, and each is put to the owner as one
question in [Open owner questions](#open-owner-questions-import-2026-11-12).
The sentence above is kept as drafted and is superseded by that list.

## Open owner questions (import, 2026-11-12)

The owner stated three things on 2026-11-12: BPR owns the queue, Claude and
Codex steer natively, and the feature stays cheap. Everything else above is
the draft's proposal. Each line is one question with the draft's default;
the default is built unless the owner answers otherwise.

| # | Question | Default |
|---|---|---|
| Q1 | Whole-tour feedback typed while the fresh first draft runs: keep today's separate buffer, or make it the whole-tour conversation's first queued entry, released on real completion with the Ask more seed (an explicit promotion by typing)? | Draft: keep the buffer, out of scope. Implementer's suggestion: fold it in, which removes the timer poll |
| Q2 | Does the local draft's discussion get Queue and Steer too? | No; Agent tab and tour chats only |
| Q3 | Queue lives in BPR; the runtime's own queue (Claude `later`) is never prefilled? | Yes |
| Q4 | While busy (or with entries waiting), Send is relabelled Queue, and Steer is a separate named action? | Yes |
| Q5 | Return and Ctrl/Cmd+Return do Send/Queue; Steer has a keyboard command but no new global shortcut? | Yes |
| Q6 | Limits: 16 pending entries and 256 KiB total per conversation, 16 KiB per message as today? | Yes |
| Q7 | Each queued entry is its own turn, never merged or batched with the next? | Yes |
| Q8 | Stop, a failure, a usage limit, closing the owning window, a restart or delivery uncertainty pause the rest of the queue, and a restored queue opens paused until Resume queue? | Yes |
| Q9 | Editing a queued entry pauses the queue, and it stays paused after Save or Cancel? | Yes |
| Q10 | With several windows, the window running the chat dispatches; closing it pauses; another window takes over only by Resume queue? | Yes |
| Q11 | A queued entry keeps the subject, lines and attachments captured when it was queued; a changed or deleted target pauses the queue rather than retargeting? | Yes |
| Q12 | When another chat holds the workspace, a queued entry pauses instead of retrying? | Yes |
| Q13 | New session and Reset tour leave the old conversation's waiting entries in its history, paused, not carried over? | Yes |
| Q14 | Claude steers with priority `next` (after its current tool call); `now` is not used and immediate intervention stays Stop? | Yes |
| Q15 | Steer cannot add private sources, widen permissions or change model or effort; those need a new turn? | Yes |
| Q16 | Delivery unknown is never resent automatically; recovery is inspecting history or New session? | Yes |
| Q17 | A driver that cannot steer keeps Queue and says Steer is unavailable, with no paid probe and no kill-and-replay fallback? | Yes |
| Q18 | Deadlines and limits cover a whole steered run without resetting per correction, and each released queued entry gets its own new budget? | Yes |
