# RFC-BPR-0118 verification

Pack `agent-assistance-v1`, milestone `comment-to-suggested-change`
([[BPR-2078]]; closeout [[BPR-2079]]). Queued on lane
`claude/v0-5-0-prerelease-targets-52265d` by the owner. RFC base
`a9663a706fd72061f60b615bd8e749ee0ddb66b5`; milestone base `1ac17ba30`;
RFC-BPR-0125 milestone 2 merged into the lane during the milestone
(`8ba69de50`, at the coordinator's request). Frontend-design was loaded
before the Qt work. Debug timings are nongating; nothing below is a timing
claim.

## Coordinator redirect (2026-11-12)

The owner is folding agent-assistance entry points into one Ask model
(recorded on the [RFC](../rfcs/RFC-BPR-0118-turn-a-draft-comment-into-an-exact-suggested-change.md#coordinator-redirect-of-2026-11-12-recorded-verbatim-intent)).
For this round: the action's core, host and prompt are built; its entry is
the draft field's right-click menu; no new visible control; no owner look of
its placement. A tool-row control built first was removed before commit of
the entry (`1f26865cf` had it; the next WIP replaced it).

## What was built

- **Core** (`bpr/core/suggest_change.hpp`): `bound_suggest_change_material`
  (the draft's lines at the head, exact, without the last one's ending; the
  draft's words within 16 KiB; at most twelve whole lines each side within
  8 KiB), `read_suggested_replacement` (one JSON object, bare or in one fence,
  exactly `first_line`, `last_line`, `current`, `replacement`) and
  `decide_suggested_change` with its ordered checks (P05). The seventh
  operation `workshop_operation::suggest_change`; `rewrites_prose` keeps the
  comment workshop's six controls and its feedback guidance to the six.
  `decide_agent_question` sends the press as `workshop_comment` with the
  operation, about lines.
- **Prompt**: `review.suggest-change` (`review-suggest-change-v1`) under
  Settings › Agents & context › Built-in prompts, Agent tab group, with
  default, Reset and versions; profile schema 32; the prompt provenance and
  import journal tables widened to hooks 0 to 14; `bpr tour hooks` names it
  `review-suggest-change`.
- **Launch**: the press is an Agent tab turn ("Suggest the change on line 6 of
  src/engine/renderer.cpp") sent on `assistance_first_answer`: fresh, one
  tool-free call with Claude, its prompt as the system prompt, no brief and no
  earlier turns, the `suggest.*` material and the operation in the request.
  Ask more on the turn promotes it, as for RFC-BPR-0116 and 0117.
- **Window**: Suggest the Change on the draft field's right-click menu, only
  where RFC-BPR-0090's range rules take a suggestion (and Stop Suggesting the
  Change while it runs). The answer is offered to the core with the head now;
  an accepted reply opens RFC-BPR-0090's edit on the draft's lines and writes
  the agent's lines as one edit; while unkept the draft keeps the lines it
  had. A panel under the field (the Enhance panel's look, Agent accent rule)
  says what came of it, with two buttons of one width that keep their places:
  Keep change and Discard; Undo and Done; Show reply and Dismiss.

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `core_suggest_change` (the seventh operation, the question rule, the material exactly the lines). `application_suggest_change_public_path`: the draft on a line the pull request adds offers Suggest the Change; writing the comment launches nothing; the press is one call and an Agent tab turn about the file | pass |
| AC02 | `application_suggest_change_public_path`: the applying reply opens in the head pane with the agent's line (`SUGGEST_ECHO`), and the draft's body is byte-identical until Keep change | pass |
| AC03 | `core_suggest_change`: a differing range, one differing byte, the last line's ending in `current`, an identical replacement, a final line ending alone, an over-bound replacement, a NUL, an unreadable answer, a line GitHub places nowhere (`not_suggestible`), lines that moved and a new head (`head_moved`, checked first). Course: a reply one byte off ("the text it quoted differs from the head") and a sentence ("the reply was not a change to these lines") are refused with that reason, never shown in the code, the draft unchanged, the reply readable as text | pass (head moved during a run: core only) |
| AC04 | Course: Keep change adds the block and the words stay as written (`split_suggestion_body` gives them back exactly); Undo restores the body exactly; no submission or target observation | pass |
| AC05 | `application_agent_launch_policy` (`test_suggest_the_change_is_one_fresh_tool_free_call`): its own prompt, purpose and version, fresh and tool-free, the prompt as the system prompt, the material in the request; an edit reaches it and no other prompt; Reset restores it. The stub reads its prompt (`system=1`). `sqlite_workbench_settings_storage` (`test_suggest_change_prompt_migrates_from_thirty_one`), `qt_settings_window` | pass |
| AC06 | The owner's look on a frozen build | pending: the control's look waits for the Ask model (coordinator redirect); no session filed |

## Decisions for the owner

- P05 (the checks) is a proposal; built as written, in the order above.
- The reply format is one JSON object; an agent that writes a sentence
  instead is refused as `unreadable` and its words can be read. Codex runs the
  same request fresh with the Agent tab's tools and no scratch folder (as for
  RFC-BPR-0116 and 0117); whether each installed agent returns the object
  reliably is established only for the stub here (IMPLEMENTATION_PROMPT asks
  this be established; open for BPR-2079's installed-agent run).
- Keeping the change composes the block into the draft; Undo is a panel
  button that restores the body as it was before Keep change (the edit's own
  undo also takes the agent's lines back while it is open).
- Not registered as an RFC-BPR-0125 macro: a macro is a template sent as a
  question, and this action needs the draft's lines and returns a structured
  reply. It is a quick-ask candidate for the Ask model.

## Runs

See the handoff for the run list at the candidate.
