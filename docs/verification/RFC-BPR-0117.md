# RFC-BPR-0117 verification

Pack `agent-assistance-v1`, milestone `explain-file` ([[BPR-2076]]; closeout
[[BPR-2077]]). Queued on lane `claude/v0-5-0-prerelease-targets-52265d` by the
owner. RFC base `a9663a706fd72061f60b615bd8e749ee0ddb66b5`; milestone base
`f205fa0d3`; main merged in at `75ab1e0fa` (main `c5a0f0823`) during the
milestone, at the owner's request. Frontend-design was loaded before the Qt
work. Debug timings are nongating; nothing below is a timing claim.

## What was built

- **Core** (`bpr/core/explain_file.hpp`). `bound_explain_material` bounds the
  material the one call carries: the file's change in the pull request as
  unified text (at most 96 KiB, cut at a line end) and its text at the head (at
  most 64 KiB, cut at a line end), with the text's state said as `whole`,
  `cut`, `absent` (the pull request deletes it), `binary` or `too_large`; both
  commits are named in full. `explain_note_text` makes Keep as note's text: a
  first line "Agent: Claude explained this file.", then the answer, within the
  note's 16 KiB with an ellipsis line where it was cut.
- **Subject rule.** `acts_on_selection(analyze_current_file)` is now false: it
  acts on the file open in Review (`docs/ux/ai-experience.md` says so).
  `decide_agent_question` sends Explain this file as a file question through
  `analyze_current_file`, whatever is attached or selected.
- **Prompt.** `review.explain-file` (`review-explain-file-v1`), in Settings ›
  Agents & context › Built-in prompts under Agent tab, with the editor,
  default, version and Reset the others have; profile schema 31 (one column),
  the prompt provenance and import journal tables widened to hooks 0 to 13;
  `bpr tour hooks` names it `review-explain-file`.
- **Material** (`window_explain_file.cpp`). A press reads the file at the pull
  request's base (its earlier path for a rename) and head from the review's own
  objects, compares them with the review's diff engine and asks the Agent tab's
  question once both arrive. Nothing here runs without the press, and nothing
  reaches GitHub.
- **Launch.** The first answer runs on `assistance_first_answer` (RFC-BPR-0101
  as amended 2026-11-12), as RFC-BPR-0116's do: fresh, no grant, no scratch
  folder, no MCP server, `review.explain-file` as the system prompt, neither the
  review skill nor earlier Agent tab turns. With Claude it is one tool-free
  call; an agent with no tool-free mode runs the same request fresh with the
  Agent tab's tools and no scratch folder.
- **Surfaces.** **Explain this file**, a text control in the Agent accent at
  the right of the comparison's header (the document tab strip), before the
  plus; enabled once the agent is set up and a file is open. **Explain This
  File** in each tab's menu (it activates that tab first). The answer's card
  stands under the header while its file is the active tab's: the run's line
  and RFC-BPR-0103 activity while it runs, then the answer (references as
  links, at most fourteen lines before it scrolls), and one row of three
  actions of one width that stay in place from the press to the end: **Keep
  as note** (then **Go to note**), **Continue in Agent tab** (Agent accent),
  **Dismiss**. A refusal (agent busy, file not read) is said in the card's line.
  The question is an ordinary turn in the Agent tab ("Explain
  src/engine/renderer.cpp") with Ask more; Continue in Agent tab opens the tab
  at that turn and seeds the next question with it.

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `core_explain_file` (`analyze_current_file` not on the selection; the press decides a file question about that file whatever is attached), `core_agent_chat_scope` (one of eight entry points acts on the selection). `application_explain_file_public_path`: the header control and the tab menu's Explain This File each send one call about `src/engine/renderer.cpp` (`subject=1`), with nothing selected, and each is a turn in the Agent tab's history (`turn_explain`). | pass |
| AC02 | `application_explain_file_public_path`: the answer shows in the card at the file's header, its references links; the card showed the running turn (`saw-running=1`) and its line says how it ended. `qt_explain_file_card_scale_{100,150,200}`: the live line and activity, the answer with links, the bounded scrolling height. | pass |
| AC03 | `application_explain_file_public_path`: Keep as note makes a private note on the file, not a line, whose text begins "Agent: Claude explained this file." and the control reads Go to note; Continue in Agent tab opens the Agent tab; Dismiss hides the card, makes no note, and the turn stays in the history. `qt_explain_file_card_scale_*`: the three actions, one size, in place. `core_explain_file`: the note's text and bound. | pass |
| AC04 | `application_agent_launch_policy` (`test_explain_this_file_is_one_fresh_tool_free_call`): its own prompt, purpose and version, fresh and tool-free, the prompt as the system prompt, the change and text in the request; an edit reaches it and no other prompt; Reset restores it. The course's stub reads its prompt in the system prompt (`system=1`). Storage: `sqlite_workbench_settings_storage` (`test_explain_prompt_migrates_from_thirty`), `qt_settings_window` (listed under Agent tab). | pass |
| AC05 | `application_explain_file_public_path`: opening the pull request, opening the file and scrolling it launch no agent (`AgentProcessLauncherInvocations` 0); one launch per press; review mutation evidence (submissions, target observations, stored comments) unchanged. | pass |
| AC06 | The owner's look on a frozen build with a fixture agent. | pending |

## Decisions for the owner

- P04 (the answer is a card at the file's header) is a proposal this
  milestone depends on; built as written.
- "A file's header" is read as the comparison's header, the document tab
  strip, which names the open file; "its context menu" as that tab's menu.
- Agent origin is in the note's text (its first line names the agent), not a
  stored field: no note schema change. Conservative; a stored origin is a later
  choice.
- Continue in Agent tab opens the tab at the turn and promotes the answer as
  Ask more does (the next question there carries it). The turn's own Ask more
  does the same.
- The control stays enabled while another agent turn runs; a press then says
  "Not sent. The agent is answering another question." in the card, rather
  than the control greying out without a word.
- The card appears under the header, so the file moves down by the card's
  height (at most fourteen lines of answer) when it opens; the control itself
  does not move.

## Owner verdict (attended session 0002b9fad401fad26fe5fa36206d539b, handed back 2026-11-13T06:31:42Z)

- `BPR-2076-explain-this-file`: **Reject** (decision `3deb7d9f`, 2026-11-13T06:31:41Z)
> I think this gets folded into Ask.

The owner is redesigning the agent-assistance entry points around one Ask
model (an Ask menu at pull request scope, Ask on a file, Ask on a thread;
quick asks from suggested-prompt macros). [[BPR-2076]] stays parked pending
that redesign. The frozen root `/Users/jaeger/explain-file-v001.4gr0t3jw` was
removed and host 29331 (child 29341) stopped after hand-back.

## Closeout (2026-11-16)

Owner, 2026-11-16, on the hygiene audit: close; delivered through
RFC-BPR-0128 or superseded, the implementer's call. Closed by [[BPR-2077]] as
**delivered through RFC-BPR-0128, with AC02 and AC03 superseded**. The verdict
above, "I think this gets folded into Ask", was carried out: Explain this file
is a row of the file's Ask and the file tab's menu, sending the same
`analyze_current_file` request under its own built-in prompt, and its answer
is a report card and a report in a file tab. The answer card at the file's
header, with Keep as note, Dismiss and Continue in Agent tab, was removed
after the owner's ask-v001 Reject; those criteria are recorded as superseded,
not passed. The owner's accepts of the file's Ask and of the reports are in
the [RFC-BPR-0128 ledger](RFC-BPR-0128.md). [[BPR-2076]] closes as
superseded.
