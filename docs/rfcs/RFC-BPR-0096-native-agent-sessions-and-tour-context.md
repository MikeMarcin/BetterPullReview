---
id: "RFC-BPR-0096"
title: "Resume native agent sessions and give tour chats their working context"
status: "Proposed"
metadata: {"origin":"owner discussion, 2026-11-08","inspected_revision":"7cda6945a95f3fa2c82b34076fb8d1cf26577174","milestone_plan":{"id":"native-agent-sessions-v1","path":"docs/rfcs/plans/native-agent-sessions-v1/milestone-plan.json"},"mandate_revision":"native-agent-sessions-r1","baseline":"bebea27edf44fd63a7308307227035bf33391c5c"}
---

# Resume native agent sessions and give tour chats their working context

## Problem and grounded current behavior

The owner wants the session behavior of the official Claude Code VS Code
extension in both BetterPullReview's Agent tab and Review Tour side chats:
continue the actual agent conversation, retain its code investigation, return
to earlier sessions, and start fresh deliberately. The owner also reports that
after a tour update every stop displayed “Source needs attention · Lines not
confirmed”, and the tour chat could not explain those messages.

The inspected implementation establishes these differences:

- The [Agent conversation host](../../src/application/include/bpr/application/agent_conversation_host.hpp)
  stores questions and replies per PR and agent, then adds them to the next
  request's brief. Its comment explicitly chooses replay over a backend's
  resumable session. This gives chat-history persistence but omits the
  backend's intervening tool observations.
- [Tour conversation replay](../../src/core/src/tour_agent_session.cpp)
  selects answered turns by tour thread, without an agent-entry identity.
  History lives in memory for the review. The
  [launch restrictions](../../src/application/include/bpr/application/agent_launch.hpp)
  turn off Claude session persistence; the
  [protocol loop](../../src/adapters/agent/src/agent_protocol_loop.cpp)
  closes Claude's input after one request. The read-only invocation host also
  rejects a `ContinuedSession` request.
- [The tour prompt](../../src/application/src/tour_agent_conversation.cpp),
  `build_tour_ask_prompt`, already serializes tour text and stop locations as
  JSON. It omits source-confirmation, preparation, head-staleness and
  publication-blocker facts, and supplies no guide to resolving those states.
- `tour_stop_of` creates an agent-proposed stop with a destination but no
  confirmed source range; split pieces use that same constructor. Text-only
  acceptance copies the existing stop and changes its body; ordering moves
  whole stop values. Those operations do not inherently erase confirmation.
  [The source label](../../src/adapters/qt/src/review_tour_editor.cpp)
  also emits “Source needs attention” for every mapping except exact-original
  and unchanged-material, including an author-selected replacement. These
  source findings explain possible warning paths; the owner's exact
  all-stops transition has not been independently reproduced.

Upstream supports the intended model. The
[Claude extension shares native history with the CLI](https://code.claude.com/docs/en/vs-code#switch-between-extension-and-cli).
Anthropic recommends a
[persistent streaming input session](https://code.claude.com/docs/en/agent-sdk/streaming-vs-single-mode)
for interactive applications and documents
[resumption by a specific session ID](https://code.claude.com/docs/en/agent-sdk/sessions).
Its [CLI reference](https://code.claude.com/docs/en/cli-reference)
documents stream-JSON input/output and exact-session resume.
Codex documents persistent
[App Server threads and turns](https://learn.chatgpt.com/docs/app-server)
and [resumption of non-interactive sessions](https://learn.chatgpt.com/docs/non-interactive-mode#resume-a-non-interactive-session).
These establish available integration mechanisms, not qualification of BPR's
installed agents or an assertion about every internal VS Code extension call.

## Proposed behavior and boundaries

### Settled owner decisions

The session is the agent's native conversation, separated per agent. Apply
this to the Agent tab and each tour side chat. The owner chose to keep old
sessions in history when starting fresh, explicitly allow a labelled
transcript-replay fallback for agents that cannot resume, and use the label
**New session** rather than Clear session. This document proposes the
remaining behavior below; it is not a claim that the owner has reviewed the
completed draft. [The decision record](assets/native-agent-sessions-v1/DECISIONS.md) preserves that distinction.

### Native sessions and persistent JSON connections

Use the selected installed agent or configured wrapper on its existing
sign-in. For conversational Claude Code, keep a bidirectional `stream-json`
stdio connection open across turns. A completed answer ends a turn, not the
process or input stream. For Codex App Server, keep the JSON-RPC connection
and send later turns to the same recorded native thread. Where a qualified
Codex JSONL entry requires a process per turn, use its native resume-by-ID
facility; show it as native continuation, without claiming process reuse.

An idle process may be released, and closing BPR stops its owned processes.
The next user message resumes the exact saved backend conversation. Merely
opening a review, viewing history, refreshing metadata or restoring a panel
must not submit an inference request or repeat an interrupted prompt.
Disable runtime auto-continuation on reconnect. If a runtime can only resume
interrupted work by continuing it, require an explicit action naming that
continuation; do not combine it with a new question. A hidden chat may finish an
already authorized turn; closing its review or application uses the existing
bounded cancellation path. Keep streamed progress, permitted approvals,
Stop and runtime compaction in the native protocol. This adds no parallel-run
or background-inference authority beyond the existing invocation limits.
There is no new direct model-API client, API-key requirement, Python/Node
runtime dependency or requirement to install VS Code.

Persist a BPR conversation identity for each generation, the backend's opaque
resume handle and runtime/protocol identity, agent/configuration identity,
review/revision binding and workspace ownership. This durable identity is
separate from a window's application-session ID and its transient activation
and run fences. Reopening history changes the selected conversation, not its
generation. Provider handles remain host-owned;
the deterministic core decides session legality from explicit facts and Qt
receives a presentation projection. Never choose “the most recent session”
in a directory: concurrent chats and other agent clients make that ambiguous.
Recheck authority, agent configuration and permissions before every send and
resume. A replaced/reset agent entry cannot acquire a conversation by reusing
its numeric slot. Model/effort changes may use a supported native control or
exact-ID reconnect; they do not justify relabelling a different session.

### Session scope, workspace and history

| Surface | Independent conversation scope |
|---|---|
| Agent tab | Profile/account authority, PR identity and agent entry |
| Tour side chat | The same authority and PR, tour draft identity, tour thread and agent entry |

A tour thread is the whole tour, Problem, Solution, or one stable stop ID.
Reordering does not move a chat to a different stop. A removed stop's history
remains readable under that identity; it is not an active stop chat while the
stop is absent. Replacement stops and reset drafts cannot inherit an old
scope through reused display numbers or IDs. Agent-tab and tour histories
stay separate. Switching agents or chat surfaces restores the selected
scope's own current session.
Draft-the-tour and its whole-tour follow-ups share the whole-tour session for
that agent. Small tool-free writing hooks remain separate one-shot requests.

Reuse the session's revision-pinned private workspace across turns and normal
close/reopen cycles, without writing into the user's linked clone. Preserve
owned workspace contents needed for genuine resumption, including changes
an ordinary Agent-tab session was permitted to make. Do not recreate a clean
tree and describe it as the same working state. Session ownership replaces
per-turn teardown and startup reclamation for retained conversational
workspaces. Each generation has separate mutable workspace state. Retention
must account for borrowed Git objects and validate the required pinned state
on resume; an external clone's continued existence is not durable ownership.
Native transcript persistence alone does not preserve a filesystem.

Keep history, bindings and native handles durable across BPR restarts. A
failed save is visible; do not present a conversation as safely retained
until its required record is stored. Idle sessions need no live process.
The 500-record conversation bound covers both surfaces and all generations;
the separate 128-entry immutable invocation-metadata catalog must not erase
resume ownership. Count retained and crash-recovery workspaces in a bounded
storage policy as well as counting records. Existing acquisition bounds are
not a claim that agent-created files can never exceed them.

Catalog pruning that drops resume ownership and automatic workspace
reclamation may take only inactive generations whose cleanup is verified
safe, with omissions reported; active work, pending proposal
provenance, the prior session during New session admission, and modified or
unverifiable contents are protected. If limits cannot be met safely, refuse
new admission with a storage reason until space is explicitly released;
do not silently lose work or retain unbounded orphan workspaces. Record the
chosen storage thresholds during implementation. Workspace loss leaves any
retained transcript readable but native resumption unavailable. Cleanup uses
exact owned paths; agent-owned transcript retention and deletion remain
separate from BPR's catalog and profile cleanup.

### New session and returning to history

**New session** replaces Clear session in the Agent tab and is available for
the selected tour side chat and agent. It starts an empty conversational
generation after retaining the previous one in that scope's history. It
needs no destructive-clear confirmation, submits no prompt and need not
start a process or allocate a workspace until the first message. Repeated
New session on an unused empty generation does not fill history with empty
entries. During active work the user stops the turn first; replacement waits
for the host's bounded stop/recovery path to settle. Save/admission failure
leaves the prior session selected and explains the failure.

Selecting an earlier history entry restores its transcript and permits
continuing that exact session after binding checks. Accepted tour text,
confirmed sources, focus, review notes and comment drafts are BPR state;
starting or reopening a conversation neither resets nor reverts them.
Unaccepted proposals retain their originating session/run identity and remain
subject to current-tour validation. Starting a new session does not apply
them or revive superseded proposals. Existing pending tour state may be
included as current BPR context, explicitly distinguished from accepted work.
Reset tour remains a separate operation with its existing undo contract.
It clears active tour chats and proposals, not the Agent tab. Retained native
sessions become detached history, never the reset draft's implicit session;
Undo reset restores the original associations with the restored draft.
Backend transcript deletion is not implied by either reset action.

Each turn and proposal completion is bound to its originating session,
activation, run and captured review/tour state. Late output can be recorded
only in its originating history; it cannot attach to the newly selected
conversation or apply itself to the tour. BPR permits one writer per native
conversation across its windows, under the existing profile owner. Detect
and report a backend's known external-use conflict; this does not promise to
lock arbitrary third-party clients. Stop interrupts the turn, retaining the
conversation when recovery is supported. Persist enough submission state to
distinguish unsent, completed and uncertain work after a crash. Reconcile
using native status/history where available, otherwise show the uncertainty;
never automatically resend an uncertain or interrupted prompt.

### Updated BPR state and revision changes

Bootstrap a native session with the applicable BPR guide and structured
context. Thereafter append new messages and current-state updates through
the continuing connection; do not reconstruct the conversation from BPR's
shortened visible transcript on every turn. On an explicit send, capture one
coherent snapshot of all relevant BPR facts. Its freshness identity includes
observed source/head state, preparation, proposal decisions, focus and
context selections, not just the accepted tour revision. Send the full
snapshot when that identity changes; otherwise the new question may refer
to the already supplied snapshot and carry its current selection.
Compaction, reconnect or a guide/schema change that cannot establish the
current bootstrap still present requires resending it. When the runtime
cannot expose that fact, send the current bootstrap with each explicit turn.
Context updates accompany the authorized question, never trigger their own
inference. A fresh or replay execution always receives a full bootstrap.

The latest BPR snapshot is authoritative for tour contents and decisions;
older conversation statements cannot override it. Send accepted/discarded
proposal outcomes, supersessions and refusals, so the agent distinguishes
what it suggested from what actually happened. Current-context access remains explicit under
the existing note/draft inclusion rules. Previously sent context can remain
in a native conversation; removing a context chip does not erase that
history or make a previously broad conversation narrow. Show that retained
context boundary; New session starts without the old conversation but still
receives the currently selected context. Do not import other agents' or
scopes' transcripts to supply missing memory.

Preserve the established stale-session choice after head movement: start a
new session at the observed revision, or explicitly continue against the old
revision. Never move the old workspace underneath a native conversation or
silently relabel its tool observations as current. An old-revision tour
answer names both the revision it read and the currently observed head;
source proposals retain their analyzed revision and go through the existing
stale-result checks. Resolve returned display numbers through the submitted
snapshot's stable stop identities, never through a later ordering. A tour
text edit at the same code revision updates the snapshot without requiring
a new agent session. A confirmed binding can survive that edit even when
the existing preparation contract requires the author to mark revised prose
Ready again.

### Native capability, fallback and existing histories

Qualify native session creation, multi-turn continuation, interruption and
resume for the supported installed Claude Code and Codex protocols, including
the tour's read-only restrictions. A custom wrapper can use the same route
when it exposes those capabilities. An unsupported backend is different from
a native-capable backend whose saved session has become unavailable.
Codex App Server's currently refused tour route needs an explicitly qualified
read-only configuration before admission. Neither native support nor replay
can bypass the existing tool, network or host/provider restrictions.

As chosen by the owner, offer **Transcript replay** explicitly when native
continuation is unsupported or unavailable; identify that it carries BPR's
saved questions and replies and does not restore prior tool observations.
Do not silently downgrade, label replay as native resumption or automatically
retry a failed native turn through replay. The alternative is New session.
Choosing replay creates a separately identified replay generation with a
link to its source history; the original native entry and handle are kept.
It carries only the permitted, retained prose into a new workspace at the
shown revision, not prior tool results or workspace modifications. The choice
remains visible on later turns and in history. Eligibility is checked before
either choice can send anything; an unsafe tour backend remains refused.

Existing Agent-tab histories have no native resume handle. Preserve them as
legacy replay histories and offer labelled replay or New session. Do not
claim to recover missing tool context. A legacy numeric agent slot without
trustworthy identity evidence cannot be silently assigned to today's entry;
keep it labelled legacy and require an explicit replay destination. Tour
histories retained by the new implementation become durable per agent;
already-lost in-memory histories
cannot be recovered. Display/storage limits remain distinct from native
context: retain the existing 50 saved turns, 64 KiB per saved text field and
500 conversation records. Agent-tab replay keeps its 8 KiB per-field and
newest-50-within-48-KiB bounds; tour replay keeps the newest 12 answered turns
within 48 KiB. Display truncation and replay omissions are counted. Native
context follows the runtime's compaction and retention; BPR's bounded view
is not a complete native-transcript viewer or a promise of unlimited memory.

### The tour agent's guide and structured status

Give every tour conversation a short, versioned BPR guide explaining the
Problem, Solution and code stops; proposed versus accepted edits; source
confirmation; Ready; observed-head changes; and the separate publication
step. Include the real action names, including Show in code, edit the stop,
Use selected lines, and Ready where applicable. The guide explains that
reading code or accepting prose is not the author's confirmation of lines.
The runtime is looking at the repository under review; it must not need
BetterPullReview's own source tree to understand its host application.

The versioned snapshot extends the existing JSON with these semantic facts:

| Context | Required facts |
|---|---|
| Tour and source revision | Stable draft ID and revision, snapshot freshness identity, owner-scoped PR identity, observed base/head and the session's pinned base/head; current Problem, Solution and focus |
| Stop identity and content | Stable stop ID and current one-based display number, title, explanation, primary and related source destinations with commit, side, path and one-based lines |
| Source status | Confirmed original/current bindings and confirmation head, suggested destinations and mappings, unavailable/ambiguous/missing evidence, and actual source-attention reasons for primary and related ranges |
| Preparation and eligibility | Current preparation and generated-stop visitation state; source confirmation and explanation reaffirmation separately; Ready eligibility and stop-level plus tour-level publication blockers (including ownership/conflict/read-only state), with reason codes and available actions |
| Proposal state | Pending additions and structural revisions separately from accepted stops; originating identity, current applicability and accepted/discarded/superseded/refused outcomes |
| Turn context | Current tour thread, selected passage or code lines, user question, update intent and any explicit context omissions |

Derive these facts and available actions from shared core decisions. The
host serializes them and Qt projects the same reasons; neither a prompt
builder nor a widget infers confirmation from the mere presence of a path.
Missing/unknown evidence stays explicit. Instructions are trusted framing;
repository text, tour prose and user-authored fields remain data. Preserve
the existing 160 KiB composed tour-prompt carrier, including guide, snapshot,
question and any replay, and the 16 KiB typed-question bound. Refuse an
oversized required snapshot before sending rather than silently dropping
stops or blocker facts. The refusal names the context limit and sends no
partial turn. Wire field names and schema-version
selection belong to implementation, with compatibility checked explicitly.

When asked about “Source needs attention” or “Lines not confirmed”, the
agent can identify the affected stops, explain their actual reasons and
available steps, and say what it cannot establish. The status projection
must distinguish an unconfirmed proposed destination, a changed head, an
ambiguous mapping and a confirmed author-selected replacement. That last
case alone must not trigger an attention warning. Source confirmation is
not the same as preparation Ready or eligibility to publish: missing prose,
an unvisited generated stop or a changed explanation does not by itself mean
that its source lines are unconfirmed. Labels and eligibility
agree with the supplied reasons; this requires a bounded correction to the
current warning logic, not a new tour layout.

Retain confirmed source bindings through text-only edits and reordering.
New stops and split pieces have the confirmation state established by their
actual source evidence; an agent cannot set a human-confirmation flag or
mark its own prose Ready. If an update legitimately changes the selected
source, its effect on confirmation is explicit. Proposals still use the
existing structured answer validation, author acceptance, undo and separate
publication path. This RFC adds no direct agent mutation or publication tool.

### Ownership, compatibility and qualification

Preserve the [core/host/Qt allocation](../architecture/system-architecture.md).
The host owns transports, opaque backend state, persistence, workspace
lifetimes and cancellation; core owns authority, generations, source status,
proposal applicability and staleness; Qt owns the controls and rendering.
Tour conversational sessions remain read-only with existing tool/network
restrictions and their documented enforcement limits; native resume does not
silently restore a broader tool configuration. Ordinary Agent-tab permissions
retain the selected mode's contract. Tool-free writing hooks keep their
one-shot, no-session behavior.
No provider credentials cross inward or enter the session catalog.

Update the existing operator disclosure that conversational transcripts and
owned workspaces now persist, naming BPR-owned metadata and agent-owned
transcripts separately. New session is not deletion or profile purge.
Cleanup follows existing owned-path checks and process supervision; process
idle lifetime does not create background inference authority. Existing
protocol/frame and per-turn output limits still apply to a long-lived stream.
Keep turn deadlines separate from idle-connection lifetime, correlate every
frame/approval with its connection and turn, and bound idle output and
buffering as well as active output. A second result or late frame cannot be
accepted as the next turn. Diagnostics expose local conversation/run identity,
mode and lifecycle outcomes without exporting prompts, tool results,
credentials or opaque native handles as ordinary diagnostic metadata.

Use functional public-path evidence for session identity and continuity,
and installed-agent courses for actual native behavior. Compare equivalent
Release conversations to replay, recording repeated reads, workspace starts,
input/cached/output tokens, cost and elapsed time where reported. Label
unavailable metrics and cache misses honestly; no fixed savings percentage
is promised and Debug timing remains nongating. Graphical placement follows
the existing Agent tab and tour controls; future UI work must load the
repository-required frontend-design skill before changing those surfaces.

This proposal changes adopted session/persistence and warning contracts.
[The proposed amendment](assets/native-agent-sessions-v1/proposed-session-amendment.md) identifies the exact
live sources and the scoped adoption recorded in [LIVE_IMPORT](assets/native-agent-sessions-v1.LIVE_IMPORT.md). The
[portable plan](plans/native-agent-sessions-v1/milestone-plan.json), [proposed cards](../issues/20-ready/BPR-1951-native-agent-tab-sessions.md) and
[implementation entrypoint](assets/native-agent-sessions-v1/IMPLEMENTATION_PROMPT.md) are draft artifacts.
The owner authorized implementation on 2026-11-08 under mandate `native-agent-sessions-r1`. The retained draft remains immutable provenance; the live plan and cards own execution state.

## Acceptance criteria

- **AC01:** Both conversational surfaces demonstrate native multi-turn
  continuity with the same backend handle and workspace. A follow-up uses a
  prior tool observation absent from BPR's prose replay. Claude's input stays
  open after the first result; qualified Codex protocols follow the defined
  persistent-connection or native-resume behavior.
- **AC02:** Switching agents, PRs, tour parts and stop order returns each
  scope's own session. Replacement/reset agent entries and replacement stops
  cannot acquire another identity's conversation; removed-stop history stays
  identifiable without becoming a new stop's history. Reopening a generation
  preserves its identity, and changed authority/configuration is rechecked.
- **AC03:** Closing and reopening a chat, review and application restores
  history and permits resuming the exact native session and its preserved
  workspace, including retained allowed file changes. Startup recovery does
  not reap it. Viewing history performs no inference; failed saves, missing
  native state and missing required workspace/object state are reported
  without an invented successful resume.
- **AC04:** New session starts a distinct generation only for the selected
  scope, preserves the previous session in history, and leaves accepted tour
  state, notes and drafts intact, without inference or a clear-confirmation
  dialog. Failed admission preserves the selected session. An earlier entry
  can be continued after binding checks; Reset tour/Undo reset and pending
  proposals retain the defined associations without reviving superseded work.
- **AC05:** Stop, uncertain sends, process failure and restart produce visible
  outcomes with no automatic retransmission. A late completion/frame cannot
  enter another turn/generation or apply a stale proposal. Two BPR windows
  cannot concurrently write one native session; a completed answer does not
  tear down a live conversation or make its idle interval a turn timeout.
- **AC06:** Unsupported backends, unavailable native sessions and legacy
  histories present the defined labelled replay and New session choices.
  Replay creates its own generation, preserving native provenance and
  exposing its prose/workspace limits and omissions. Ambiguous legacy agent
  identity is not silently reassigned, and replay cannot bypass read-only or
  authority restrictions. No failed turn is silently resent.
- **AC07:** Head movement exercises both established stale continuations
  with the correct workspace and analyzed OIDs. Same-head edits, state changes
  without an accepted-tour revision change, proposal decisions and runtime
  compaction/reconnect receive current bootstrap facts on the next explicit
  question, with no observation-triggered inference or stop-number rebinding.
- **AC08:** Captured tour requests distinguish all source/preparation states
  in the status table, including related lines, whole-tour blockers and unknown
  evidence. Confirmed lines with unfinished prose remain distinguishable from
  unconfirmed lines. Oversized composed prompts send no partial request. The
  guide and structured reasons let an installed agent explain a fixture's
  unconfirmed stops and actual corrective actions without BPR source access;
  an identical path/range with a different confirmation state is distinguishable.
- **AC09:** A composed update course preserves confirmation for text-only
  edits and ordering while applying the existing preparation/Ready rules,
  and exposes the actual confirmation consequences of
  new, split and retargeted stops. UI status, the agent snapshot and core
  eligibility agree; confirmed author-selected replacements do not receive a
  warning solely because of their mapping kind. Agent output cannot confer
  human confirmation, Ready or publication authority.
- **AC10:** Focused production-composition checks cover persistent streams,
  proposal acceptance and the New session/history controls with rendered
  offscreen UI evidence. Installed Claude Code and Codex courses identify
  their exact version, protocol and platform and exercise native resume under
  the requested permissions; fixture success is not claimed as installed-agent
  or all-platform qualification.
- **AC11:** Equivalent Release native/replay courses report actual reuse and
  available usage/cost metrics, without a fixed savings claim. Long sessions
  exercise bounded active/idle output, catalog and workspace admission, safe
  reclamation and full-capacity refusal without losing protected state.
  Removed context selections, diagnostics and resumed permissions preserve
  the defined privacy, read-only and explicit-invocation boundaries.
- **AC12:** Operator and architecture documentation reconcile the proposed
  amendment, explain transcript/workspace ownership and retention, and
  distinguish native continuation, replay, New session, Reset tour and purge.
  Focused acceptance and the documentation gate pass for the eventual change.
