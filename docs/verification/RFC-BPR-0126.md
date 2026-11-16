# RFC-BPR-0126 verification

Plan `queue-and-steer-v1` ([milestones](../rfcs/plans/queue-and-steer-v1/MILESTONES.md)),
one milestone, `queue-and-steer`. Lane `claude/v0-5-0-prerelease-targets-52265d`;
RFC base `dbdecf9a32c9fc7cdd28122721dc61011236d60e`. Cards [[BPR-2151]],
[[BPR-2152]], [[BPR-2153]], [[BPR-2154]] Done; closeout [[BPR-2155]].
Frontend-design was loaded before every Qt round. Debug timings are
nongating; nothing below is a timing claim.

## Owner decisions in force

The owner's answers of 2026-11-13 (on the RFC): Q1 fold the whole-tour
first-draft feedback into the queue; Q2 the local draft's discussion joins;
Q4/Q5 Follow-up behaviour is a setting, Queue or Steer, default Queue, with
Cmd+Return (Ctrl+Return off macOS) the other once; Q9 a message is edited in
place and held while edited; Q10 a queue belongs to its session, not a
window; Q14 Claude steers with `priority: "next"`, never `now`; Stop all in
the agent footer (AC09). Every other question took its default. On
2026-11-16 the owner kept the tour chats and the local draft discussion on
BPR-2154 rather than a new card.

## Integration pass (2026-11-16, closeout)

Dev preset, Debug, macOS, lane HEAD `76f59f17a` (product tree identical to
`4bda08895`, the last product commit; the lane was merged into `main` as
`7a472e246`). `cmake --build --preset dev` exit 0, then:

```sh
ctest --test-dir build/dev -j4 --output-on-failure -R 'agent_queue|ask_queue|tour_chat_queue|tour_feedback_queue|agent_steer|agent_steering|protocol_loop|agent_invocation_host|review_agent_runtime|qt_agent_footer|tour_agent_session|local_draft_tour|application_agent_launch$|architecture_boundaries|round_trip_composition_contract|qt_agent_steering_fixture|qt_tour_side_chat'
```

30 of 30 passed: `architecture_boundaries`, `core_tour_agent_session`,
`core_agent_steering`, `core_local_draft_tour`, `core_agent_queue`,
`application_agent_launch`, `application_agent_protocol_loop`,
`application_agent_invocation_host`, `application_agent_queue_host`,
`application_agent_queue_handoff`, `application_review_agent_runtime`,
`qt_agent_steering_fixture`, `qt_agent_footer`, `qt_tour_agent_session_view`,
`qt_tour_side_chat`, `application_review_round_trip_composition_contract`,
`application_local_draft_tour_desktop_public_path`,
`application_agent_steer_{sent,unavailable}_public_path`,
`application_ask_queue_public_path`, `application_tour_feedback_queue_public_path`,
`application_agent_queue_{windows,second_window_loading,close_runs_on,restart,tour_feedback_crash,finish_pauses}_public_path`,
`application_tour_chat_queue_{stop,whole_tour,local_draft}_public_path`.

## Criteria

| Criterion | Result | Revision | Evidence |
|---|---|---|---|
| AC01 | Pass | `4bda08895` | Agent tab: `application_ask_queue_public_path`, `application_agent_queue_windows_public_path`, looks prompts-v001 to v004. Tour parts, whole tour after its first draft, local draft discussion: `application_tour_chat_queue_{stop,whole_tour,local_draft}_public_path` (Queue label, Stop beside it, several follow-ups, in-place edit, order, Cmd+Return steer into the running turn, the steered message in the transcript while the turn runs). Owner Accepts: `queue-stop-chat` (`fe28ba85…`) and `queue-local-draft` (`98472c89…`) in queue-tour-v001; `steered-in-transcript` (`a669056b…`, session `0f0bf2f5…`, read from the session state) in queue-tour-v003, after the Rejects of v001 (whole-tour Steer) and v002 (steered message missing until the turn ended) were fixed in `b0bbe187d` and `4bda08895`. Parity table on BPR-2154. |
| AC02 | Pass | `d8f7fbf4b` | `application_agent_queue_host` (restored queue shown paused, starts nothing; a refused write keeps every message as typed; nothing past 16), `core_agent_queue` (256 KiB aggregate, 16 KiB a message), `application_ask_queue_public_path` (nothing runs while paused). Queue maintenance has no path to the launcher or the workspace (BPR-2151). |
| AC03 | Pass | `d8f7fbf4b` | `application_tour_feedback_queue_public_path`: first-draft feedback is the whole-tour conversation's first queued message, released only on the draft's real completion and host retirement (no timer poll), as one turn with the Ask more seed revising the proposal; the fresh first draft is never queued or promoted. Fresh paths have no queue: `core::queued_session_kind` names only the resumed chats (Agent tab, tour feedback, tour part, local draft discussion), and Steer on a one-shot run is `no_connection` (`core_agent_steering`). |
| AC04 | Pass | `d8f7fbf4b`, `64365d031` | `application_agent_queue_host`, `core_agent_queue` (pause causes, stale edit refused, held entry never sent or skipped, hold lifts on Save or Cancel), `application_agent_queue_{windows,second_window_loading,close_runs_on,restart,finish_pauses}_public_path`; owner Accept `BPR-2151-queue-across-windows` (queue-windows-v001); edit hold accepted in prompts-v001. |
| AC05 | Pass | `98c2e9f30` | Installed-agent probes 2026-11-14: Claude Code 2.1.319 took a mid-turn `priority: "next"` correction and named it in `user_message_uuids` ($0.0033); Codex CLI 0.167.0 App Server applied `turn/steer` within the running turn. No replay, no restart. `application_agent_steer_unavailable_public_path`: unsupported runs keep Queue and say why, no probe. Owner Accepts `BPR-2153-steer` and `BPR-2153-transcript` (steer-v002). |
| AC06 | Pass | `64365d031`, `dd3b2664a` | `application_agent_queue_handoff` (claim stored before any write; crash after the claim restores Delivery unknown, never resent; corrections settle by receipt; late or stray answers change nothing; intermediate and repeated results never complete a steered turn; a dropped correction is abandoned and its connection retired), `application_agent_protocol_loop`, `application_agent_launch` (delivery-unknown finish for a failure with no sign of delivery), `application_agent_queue_tour_feedback_crash_public_path`. |
| AC07 | Pass | `64365d031` | `core_agent_steering`, `application_agent_protocol_loop`: a successor native turn stays in the run and counts once; a figure that goes down leaves the cost unknown; Codex tokens are not turned into dollars. Only an explicit Steer or a released entry writes to an agent. |
| AC08 | Pass | `4bda08895` | `application_agent_steer_sent_public_path` (Sent to agent until the run settles it), `qt_agent_steering_fixture`, `application_agent_queue_handoff` (Delivery unknown with Discard), the tour chat courses (Pause queue holding past the turn's end, Resume sending once, Stop beside Queue); a queued tour message revises the proposal and accepts or publishes nothing (`application_tour_feedback_queue_public_path`). Owner Accept `BPR-2154-sent-to-agent` (`e795a561…`, queue-sent-v001). |
| AC09 | Pass | `4bda08895` | `qt_agent_footer` (fixed slot beside the picker, named, focusable), `application_review_agent_runtime` (`test_stop_all_is_the_hosts`: every turn stopped, every queue paused); a local draft's question stands in the footer's Tour chat slot so Stop all reaches it; Stop all accepted in look prompts-v004. |

## Live documents reconciled

- [RFC-BPR-0096](../rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md#queue-and-steer-amendment-rfc-bpr-0126-2026-11-16):
  busy-send legality for the resumed chats.
- [RFC-BPR-0103](../rfcs/RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md):
  accepted steering in the turn boundary.
- [RFC-BPR-0101](../rfcs/RFC-BPR-0101-agent-chat-conventions.md):
  composer actions and a steered run's cost.
- [RFC-BPR-0092](../rfcs/RFC-BPR-0092-agent-first-run-setup.md):
  the whole-tour first-draft feedback folded into the queue (Q1).
- [docs/ux/ai-experience.md](../ux/ai-experience.md#follow-ups-while-an-agent-works-rfc-bpr-0126)
  and the [system architecture](../architecture/system-architecture.md#feature-allocation)
  (session queue host, window runtimes, delivery rule, AI feature row).

## Known limits

Recorded, not defects against a criterion:

- A steered message keeps **Sent to agent** in the pending list until its
  turn ends, as in the Agent tab; the transcript shows it at once.
- Whole-tour feedback during the first draft and a local draft's first
  description draft are fresh runs: they queue and cannot be steered.
- A local draft saved with a steered message (the turn's `steered` field)
  cannot be opened by a build older than `4bda08895`, whose reader does not
  know the field.
- `core::queue_tour_feedback` survives only in its unit tests; production
  uses the queue.
