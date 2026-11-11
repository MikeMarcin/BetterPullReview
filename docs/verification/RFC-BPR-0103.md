# RFC-BPR-0103 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `agent-work-visible`
([[BPR-2004]], [[BPR-2005]]). RFC_BASE_SHA:
`dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`. Frontend-design was loaded before
the Qt surface work. Debug timings are nongating; nothing below is a timing
claim.

## How the installed agents report background work (AC01)

Established on 2026-11-10 by running each installed runtime on BPR's own
transport and arguments, in a scratch folder of three small files, with the
request "start three sub-agents in the background, one per file, say you
started three reviewers and end your message without waiting; combine their
results when they arrive". The frames, trimmed of account and machine details,
are kept as `tests/fixtures/agent/traces` and replayed by the
tests below.

| Agent and transport | How helpers are reported | BPR runs it |
|---|---|---|
| Claude Code 2.1.319, `-p --input-format stream-json --output-format stream-json --verbose` | `system/task_started` (task id, the `tool_use` that started it, its description, `is_backgrounded`), `system/task_progress` (what it is doing, its last tool), `system/task_updated` and `system/task_notification` (its end and status), and `system/background_tasks_changed` (the full list still out). A helper's own `assistant`/`user` frames carry `parent_tool_use_id`. The message's `result` comes while helpers run; Claude then answers each helper's report in a further part of the same turn (`system/init` … `result`). With input held open the results come as each part ends; with input closed they are held back until the helpers are done, and the process exits after the last. `total_cost_usd` is the process's running total. | Helpers visible: the turn stays open until no reported helper is out and every report has been answered |
| Codex 0.167.0, `app-server` | On the agent's thread, a `subAgentActivity` item with `kind` `started`, then `completed` (or `interrupted`), naming the helper's own thread and `agentPath`. Each helper runs on its own thread with its own `turn/started`, items and `turn/completed`. The agent's `turn/completed` comes while helpers run, and the agent does not take another turn when they finish (held open 240 s after the last). | Helpers visible: the turn stays open until every reported helper has completed; and the run is asked to keep its helpers in the foreground (below) |
| Codex 0.167.0, `exec --json` | Nothing: no item for a sub-agent, and the process exits at `turn.completed` while they run. | Foreground only: asked to wait for every helper before it ends its turn |

No flag of Codex 0.167.0 takes the sub-agent tools away: with
`--disable multi_agent`, `--disable multi_agent_v2` or
`-c features.multi_agent_v2.enabled=false` (each confirmed off by `codex
features list`) the tool list still offered `spawn_agent`, `wait_agent`,
`send_message` and `followup_task`, and the App Server still spawned three
sub-agents. So a Codex launch (either transport) carries the supplement field
`helpers`: "Run any sub-agent or background task you start in the foreground:
wait until every one has finished before you end your turn, and put what they
found in your answer. Work still running when your turn ends is lost." It is an
instruction, not an enforcement, and it is stated as one. Claude needs none.

Installed: `/opt/homebrew/bin/claude` 2.1.319, `~/.local/bin/codex`
0.167.0 (`claude --version`, `codex --version`).

## The cause of the reported silent failure

Reproduced on the frames: before this change, Claude's first `result` (with
helpers out) ended the turn. On a held-open native session the loop returned
success there; the next frame (`system/init` of the part answering a helper)
was taken by the idle drain as a frame for no turn, and the drain ended the
connection, so the helpers were stopped and nothing more was shown. The
answer on screen was the message that promised more. On a one-shot run the
first later frame was refused as malformed. On the Codex App Server a helper
thread's message was refused as malformed.

## What was built

- Core (`agent_activity.hpp`): what a turn is doing (`agent_doing`), its
  helpers and their states, counts of files read and commands, the last frame
  and the longest silence; `agent_run_is_quiet` decides the 120-second notice
  (and Keep waiting's pause).
- Decoders (`agent_launch.cpp`): Claude's task frames and helper frames,
  Codex App Server's `subAgentActivity` and helper threads, and each runtime's
  own items, become activity events; Claude phase 3 and App Server phase 7 are
  "the message ended with helpers out". A Claude `result` ends the turn only
  with no helper out and no report since the last part began; an App Server
  turn ends when its last reported helper completes. A runtime that ends with
  helpers out ends as `helpers_unfinished`, naming them, with what arrived
  kept. A frame for no open turn (a late answer, a helper started after the
  turn) is refused.
- The loop sends activity and a heartbeat for frames that show nothing to the
  sink when the request asks (`ReportActivity`, set for every run that shows a
  turn), measures the longest silence, and keeps a `helpers_unfinished` turn's
  text.
- Diagnosis: `helpers_started/finished/failed/abandoned`,
  `longest_silence_ms` and `read_past` (frame types only).
- Qt: the turn's line says what the agent is doing from the runtime's own
  events ("Claude is reading frame_pool.cpp, 0:42 · last activity 4 s ago",
  "is waiting on 2 helpers"); under it `agent_activity_view` shows a row per
  helper (its description, what it last did or how it ended, its time), the
  quiet notice in the attention amber with **Stop and keep what arrived** and
  **Keep waiting**, and after the turn the folded line ("Worked for 7 min: 3
  helpers, 41 files read, 12 commands") that opens to the rows. The Agent tab
  and the tour side chat use the same line and block. A turn stopped, or left
  under its helpers, keeps the text that arrived and says how it ended under
  it.

[RFC-BPR-0096](../rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md#open-turn-amendment-rfc-bpr-0103-2026-11-10)
records the amendment to its late-frame rule.

## Evidence

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | The table above, on the installed versions' real transports; `application_agent_helper_turn` replays the three recorded traces through the shared loop | Pass |
| AC02 | `application_agent_helper_turn` (Claude with input open and closed: one success, three `helper_started` and three `helper_ended`, the closing "a.txt: 25 lines \| b.txt: 23 lines \| c.txt: 31 lines" in the same turn's answer, a helper's own words never in it, one cost `0.016330895`); `application_agent_chat_helpers_public_path`, the composed application's Agent tab on a scripted runtime replaying the Claude trace: one turn, three rows seen while it ran, "is waiting on 2 helpers", one "answered in", the folded "3 helpers" | Pass |
| AC03 | `application_agent_helper_turn` (the trace cut after the first result: `helpers_unfinished`, names "Count lines in b.txt" and "Count lines in c.txt", keeps "I started three reviewers", two abandoned); `application_agent_chat_helpers_unfinished_public_path`: the Agent tab's turn keeps the text and ends "*Claude stopped before its 2 remaining helpers finished: “Count lines in b.txt”, “Count lines in c.txt”. What arrived is kept.*" | Pass |
| AC04 | `application_agent_helper_turn`: after the turn, a late answer and a helper start are refused; before the result's helpers end, the result does not end the turn | Pass |
| AC05 | `qt_agent_activity_view` on a held clock: no notice at 119 s, the notice at 120 s with both choices, Keep waiting leaves the run and returns the notice after another 120 s, a frame clears it, Stop and keep what arrived stops; `core_agent_activity` | Pass |
| AC06 | `qt_agent_activity_view`: "is reading frame_pool.cpp", "is running git diff --stat", "last activity 4 s ago" then "0 s ago" after a frame; the composed course saw "last activity" on the live line | Pass |
| AC07 | Live, below | See below |
| AC08 | `qt_tour_side_chat`: the side chat's running turn shows the helper's row, "is reading item.cpp · last activity 1 s ago", the quiet notice and Keep waiting, and folds to "Worked for 2 min: 1 helper, 1 file read" after; the tour session's projection now relays every stream event to it | Pass |
| AC09 | The owner's look at the running, quiet and ended states on a frozen build | Pass: owner accepted all three candidates (below) |

## Live course (AC07)

`live_second_account_course --course agent-helpers`
([source](../../tests/end_to_end/live_second_account_course.cpp)): the shipped
composition, offscreen, signed in through `gh`'s active account, on
[BPR-Test #40](https://github.com/MikeMarcin/BPR-Test/pull/40) (read only),
the installed agent added from the agent setup as a reviewer adds it, and
from the Agent tab: "Review this pull request. Start three sub-agents in the
background, one for each of three different areas of the change, and do not
wait for them: tell me you started three reviewers and end your message. When
their results arrive, combine them into a short review."

Claude Code 2.1.319 (Haiku), 2026-11-10:

```text
agent-helpers:{"agent":"claude","turns":1,"outcome":"answered","footer":"Claude answered in 0:44 · $0.02 · 1 finding","most_helper_rows":3,"summary":"Worked for 44 s: 3 helper(s), 4 file(s) read, 20 command(s)","rows":["Review removed file src/old.cpp Running List workspace files and try base history 0:07","Review modified file src/parse.cpp Running Read validate.cpp and list workspace files 0:03","Review added file src/validate.cpp running 0:00"],"heads":["● Claude Code is starting","● Claude Code is thinking","● Claude Code is writing","● Claude Code is waiting on 3 helper(s)","● Claude Code is waiting on 1 helper(s)"],"answer_head":"I started three reviewers in the background, one each for src/old.cpp (removed), src/parse.cpp (modified) and src/validate.cpp (added). I'll combine their results into a short review when they arrive. One of the three reviewers has finished: the `src/old.cpp` review, which reported no findings. The `src/parse.cpp` and `src/validate.cpp` reviewers are still running, so I'll combine all three into one review when they finish. Two of the three reviews are in. The `src/validate.cpp` review reports o","reported_cost":"0.022017855"}
live-second-account-course:agent-helpers:pass
```

(This run's build said plurals as "helper(s)"; the words are now written out,
"3 helpers", as the composed course shows.) One turn: the three helpers shown
while they ran, the agent's message and each part answering a helper in the
same answer, one duration and one cost. Claude: Pass.

Run again on the final candidate (after the paragraph and unreported-thread
fixes below), with the course now showing Review so the Agent tab renders:

```text
agent-helpers:{"agent":"claude","turns":1,"outcome":"answered","footer":"Claude answered in 0:00 · $0.02 · 4 findings","most_helper_rows":3,"summary":"Worked for 0 s: 3 helpers, 4 files read, 22 commands","rows":["Review src/old.cpp removal Running Read parse.cpp and validate.cpp with line numbers, check git history 0:05","Review src/parse.cpp changes Running List workspace files and line counts 0:02","Review src/validate.cpp additions running 0:00"],"heads":["● Claude Code is starting","● Claude Code is thinking","● Claude Code is writing","● Claude Code is waiting on 3 helpers","● Claude Code is waiting on 2 helpers"],"answer_head":"I started three reviewers in the background, one for each changed file: `src/old.cpp` (removed), `src/parse.cpp` (modified), and `src/validate.cpp` (added). I'll combine their results into a short review when they arrive.   The `src/parse.cpp` reviewer has finished. The other two reviewers (`src/old.cpp` and `src/validate.cpp`) are still running, so I'll combine all three once they report.  Interim finding from the `src/parse.cpp` reviewer: `validate()` in `src/validate.cpp` is hard-coded to `re","reported_cost":"0.021673159999999997"}
live-second-account-course:agent-helpers:pass
```

The later parts of the turn are now their own paragraphs. In this run the tab
stayed on the added agent's setup (see the Codex note below), so its clock read
0:00; the first run, on Overview, shows the clock as a person sees it.

Codex 0.167.0 (App Server, its configured model), 2026-11-10, the fourth run:

```text
agent-helpers:{"agent":"codex","turns":1,"outcome":"answered","footer":"Codex answered in 0:00.","most_helper_rows":3,"summary":"Worked for 0 s: 3 helpers, 10 commands","rows":["parser /bin/zsh -lc 'nl -ba src/parse.cpp; nl -ba src/validate.cpp' 0:06","validation /bin/zsh -lc \"rg --files --hidden -g '\"'!.git'\"'\" 0:03","integration running 0:00"],"heads":["● Codex is starting","● Codex is thinking","● Codex is writing","● Codex is running /bin/zsh -lc \"rg --files --hidden -g '\"'!.git/**'\"' -g '\"'!…","● Codex is running /bin/zsh -lc 'nl -ba src/parse.cpp; nl -ba src/validate.cpp'","● Codex is running /bin/zsh -lc 'git status --short'"],"answer_head":"I ’m  using  the  supplied  review  skill  and  starting  three  reviewers .  This  snapshot  contains  only  the  pull  request ’s  files ;  a  full  clone  would  let  us  check  callers ,  tests ,  and  history .  The  harness  requires  all  reviewers  to  finish  before  I  end  the  turn ,  so  I ’ll  combine  their  results  here .  Three  reviewers  completed  parsing ,  validation ,  and  integration  reviews .  No  verified  defects  were  found  in  the  available  files :  ` parse ()","reported_cost":""}
live-second-account-course:agent-helpers:pass
```

One turn: three helpers shown while they ran, and the agent, asked to keep them
in the foreground, said so ("The harness requires all reviewers to finish
before I end the turn") and answered with what they found. Codex: Pass.

Found on the way and fixed: in the third run a sub-agent's thread spoke
(`thread/status/changed`) before the agent's thread reported it, and the
decoder refused that frame, ending the turn as unreadable. Such frames are now
read past (`test_codex_helper_thread_before_its_report`).

Not this milestone's, observed and left: in this course the added Codex
stays on its setup (the tab's state reads `setting_up` before and through the
send, which the course makes through the tab's own `send_prompt`), so the
tab's clock, which starts when the tab enters `working`, never starts: the
line reads 0:00 and the folded line "0 s". A person cannot send from the setup.
The Codex answer's spacing ("I ’m  using") comes from the App Server's deltas,
each appended as its own line, as before this change.

AC07: Pass for both installed agents.

## AC09: the owner's look

Passed. The owner accepted `BPR-2005-running` (decision `eead98a1`),
`BPR-2005-quiet` (`25cac43a`) and `BPR-2005-ended` (`d19d4d2d`) with no note.
Host 29030/29040 stopped; root `agent-work-visible-v001.9any75gt` removed.

As filed (for the owner and not waited on (owner, 2026-11-10: "Continue
working unattended, you can file reviews but dont block on their results").
Session `d2cc338937915480c469bdbd1ffd7f89`, state
`/Users/jaeger/.attended-review-state/agent-work-visible-v001-2026-11-10/state`,
three candidates, one question each: `BPR-2005-running`, `BPR-2005-quiet`,
`BPR-2005-ended`. Frozen tests-on build `/Users/jaeger/agent-work-visible-v001.9any75gt`
from `12e9c6fb4` (executable SHA-256 `9eca0ba356b2…`; its launcher `--check`
passed: `ATTENDED_AGENT_HELPERS_SHOWN`). It opens nested-folders#21 on
README.md and the Agent tab asks Review this pull request of the scripted
Claude (`--attended-agent-helpers`), which replays the recorded Claude trace
slowly and goes silent for 130 seconds before its last part.
