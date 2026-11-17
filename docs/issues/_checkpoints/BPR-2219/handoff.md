# BPR-2219 checkpoint 7 (2026-11-16): round 2 built, look v002 filed

Supersedes the checkpoints below.

- Lane: `fae6dac55` round 2 (AC-g report in the app's own hand, Cards; AC-h
  a prompt's turn by its title, Agent tab only; AC-i the prompt editor);
  `84bfffc6b` Backlog cards BPR-2231 to BPR-2235 (**the frozen commit**);
  then the card's look record and this checkpoint.
- Main: `2e0a354ae` (the Backlog cards), `772c90d7d` (card and design round
  2), then the card's last mirror. Both gates print `DOCUMENTATION_OK`.
- Look v002: session `391c751c296232358f316776c40960fc`, state
  `~/.attended-review-state/review-report-v002-2026-11-16/` (URL in its
  `serve.log` only, host pid in `serve.pid`), frozen root
  `~/review-report-v002.qcu5faom`. Three questions pending.
- Look v001: verdicts on the card; host stopped; root removed.
- Not done: AC-h in the tour chats ([[BPR-2235]]); Settings › Appearance ›
  Reports still lists paper, ink and code wash, now unread; the whole suite
  was not run.
- Left for the integrator: park the card on human-review; on the verdicts,
  remove the v002 root with `scripts/freeze_attended_build.py remove`; close
  MikeMarcin/BPR-Test#185 and delete its two branches when the card is done;
  the private worktree `.claude/worktrees/bpr-2219-review-report` and its
  `build/dev` can be removed (nothing uncommitted).

# BPR-2219 checkpoint 6 (2026-11-16): look v001 handed back, round 2 begins

Verdicts are on the card (two Accept, two Reject with the owner's words).
Round 2 is AC-g, AC-h and AC-i on the card. Nothing of round 2 is built at
this checkpoint. Order planned: AC-i (prompt editor), AC-h (prompt turn
shows its title), AC-g (report restyle, with two or three offscreen Qt
renders light and dark), then four Backlog cards for the known gaps, then
freeze v002 and a fresh short session with three questions.

Pointers for whoever continues:

- Prompt editor: `src/adapters/qt/src/prompts_page.cpp` (the `Text` editor,
  `InsertRow`, and `ResultBox` added by this card; a chevron disclosure is a
  `disclosure_button`).
- Prompt turn: `src/adapters/qt/src/agent_tab_view.cpp` (a turn's `Name`,
  shown above its question, and the question's fold past six lines);
  `tour_side_chat.cpp` for the tour chats.
- Report styling: `src/adapters/qt/src/report_page.cpp` (the page's HTML,
  `report_serif_families`, the palette from `report_colours`) and
  `agent_report_view.cpp` (`apply_colours`, the title's font). The report
  view test draws a saved report with `BPR_REPORT_RENDER_MARKDOWN` and
  `BPR_REPORT_RENDER_OUT`; the real after report is in the session
  scratchpad's `review-report/after/run/review-answer.md`.
- The owner's screenshots of look v001: the session scratchpad's
  `looks/r-*.png`.
- Build in `.claude/worktrees/bpr-2219-review-report` (level with the lane),
  one build or test at a time, then squash onto the lane with explicit
  paths, as rounds one's commits were made.

# BPR-2219 checkpoint 5 (2026-11-16): built, tested, look filed

Supersedes the checkpoints below.

## Lane commits

- `b3569ae3b` the change (core, host, Qt, prompts, fixture agent, course,
  tests, design, docs).
- `6053ee021` a report says a named turn by its name, not its body.
- `cf4702b54` the course checks Settings › Prompts; **the frozen commit**.
- `3438cb2f5` verification, the look and known gaps on the card.
- Checkpoints: `eba9fbf31`, `ec2db966e`, `f9eff000f`, `310cae9cc`, this one.

Main: `dc745af3b` (claim), `c1670e986` (card), `e0260d9c5` (the design
folder the card links), then the card's last mirror. Both documentation
gates print `DOCUMENTATION_OK`.

## Real agent

Claude Code 2.1.319, Sonnet, medium effort, MikeMarcin/BPR-Test#185. Before:
one turn, 35 s, $0.16. After: one turn, 31 s, $0.17. Two turns in all; the
setup's Check ran no model turn. Artifacts in the session scratchpad's
`review-report/before/` and `review-report/after/` (`run/review-answer.md`,
`run/review-report.md`, `run/agent-review.json`, renders,
`raw-agent-output.jsonl`); the look's images in its `look/`.

## The look

Session `61844d36f1e7580e88fb7552e98de6b6`, state directory
`~/.attended-review-state/review-report-v001-2026-11-16/`, served on the
tailnet (host pid in its `serve.pid`); the URL is in its `serve.log` only.
Frozen build `review-report-v001`, root `~/review-report-v001.d9f1mwjz`,
from `cf4702b541f6`, `returning-review-v1` at its next push, Fixture agent
only. Four questions, all pending: `review-reads-as-a-report`,
`draft-comment-from-a-finding`, `review-report-layout`,
`review-prompt-visible-body`.

## Left for the integrator

- The card is still In Progress; parking it on human-review is the
  integrator's move.
- On the verdicts: reconcile from the session's cursor; remove the frozen
  root with `scripts/freeze_attended_build.py remove`; close
  MikeMarcin/BPR-Test#185 and delete its two branches when the card is done.
- The private worktree `.claude/worktrees/bpr-2219-review-report` (branch
  `claude/bpr-2219-review-report`, level with the lane at `3438cb2f5`) and
  its `build/dev` can be removed; nothing in it is uncommitted.
- Known gaps are on the card.

# BPR-2219 checkpoint 4 (2026-11-16)

Supersedes the checkpoints below where they differ.

## Done and committed on the lane

`b3569ae3b` holds the whole change (36 files): core report model, host
wiring, the report page and view, the review prompts' visible bodies and the
read-only Result: Findings report section, the fixture agent's structured
review, the composed course, the tests that follow, the design round, the
card's cause and the UX guide. It was built from the private worktree
(`.claude/worktrees/bpr-2219-review-report`, level with the lane) and these
pass on its content: `core_agent_report`, `core_prompt_macro`,
`qt_agent_report_view`, `qt_prompts_page`, `qt_prompt_macro_chips`,
`fixture_agent_answers`, `application_review_report_public_path`,
`application_fixture_agent_every_ask`. Before the squash, on the same code:
`qt_workbench_window_scale_100`, `architecture_boundaries`,
`architecture_boundary_contract`, the ask and prompt courses,
`core_quick_ask`, `core_agent_question`, `core_review_composition`,
`application_prompt_macro_host`, `sqlite_agent_setup_storage`,
`application_agent_chat_composition_contract`, `_subject_contract`,
`_conversation_contract`. Not run: the whole suite, scale 125 and 200 of the
workbench window, `application_agent_chat_replies` (its binary was not
built). The documentation gate's one error is another card's
(`BPR-2226`).

## The real-agent runs

Before (the old code, one turn, 35 s, 4,554 output tokens, $0.16 as
reported, Claude Code 2.1.319, Sonnet, medium): the scratchpad's
`review-report/before/run/` (answer, report, facts, renders) and
`review-report/before/raw-agent-output.jsonl`. The cause it showed is on the
card. After: in progress, into `review-report/after/run/`.

## Remaining

1. After run: read it, keep the renders.
2. Freeze the lane's HEAD (frozen builds join the jobserver; no other build
   of mine meanwhile); fixture agent only.
3. Attended review in a fresh short session: four Accept/Reject questions
   (reads as a report; Draft comment from a finding; layout; the Prompts
   page), the mockups and the before/after renders as images, SESSION_LINE
   in each; serve on the tailnet; record session ID and state directory
   here, never the URL.
4. Mirror the card to main; close MikeMarcin/BPR-Test#185 when the card is
   done.

Known gaps to say in the report: a report's keep states are kept by the view
for the session, not stored, and are not shared with the Agent tab's finding
cards for the same findings; the report has no Draft all; the first code
block of a real report rendered blank in the offscreen before-render (not
investigated).

# BPR-2219 checkpoint 3 (2026-11-16)

Supersedes checkpoint 2 below where they differ.

## Where the work is

Private worktree `.claude/worktrees/bpr-2219-review-report`, branch
`claude/bpr-2219-review-report` (the lane merged in, so it holds the
jobserver commit `51807e1cc`). Its WIP commits, newest last:

- `05f51c9fb` core report model and tests, fixture agent, design round.
- `629271e6f` the composed course, Qt and script tests, docs; **the Qt, app
  and prompt changes as patches** under
  `docs/issues/_checkpoints/BPR-2219/wip/` (`qt-2.patch`,
  `report-templates.new.md`), held out of the tree so the "before" binary is
  built without the fix. Apply with `git apply` once the before run is done;
  delete the `wip/` folder before the final commits.
- merge of the lane, then WIP (3): the live course walks the agent setup;
  the composed course is registered in `tests/end_to_end/CMakeLists.txt`;
  AC-f on the card.

Nothing of this card is uncommitted on the lane. Final commits are to be
cherry-picked (or merged) onto the lane with explicit paths once they build.

## Built and run so far

- `test_live_second_account_course` builds (output
  `build/dev/tests/end_to_end/live_second_account_course`). Copy it to a
  **new file name** before each run: `cp` over a binary that has run is
  killed by macOS (exit 137).
- The before run has **not yet reached the agent**: no agent turn has run and
  nothing has been spent. Attempts stopped in the agent setup, which on a
  live pull request is a wizard (agent, repository, check): the course now
  chooses Claude Code's row (its radio has no text; its accessible name is
  "Claude Code"), waits for Continue with Claude Code, takes Keep a clone
  for me or This pull request only, then Check. Attempt 7 is running that.
- A stale run must be stopped by its own pid before the next (two runs once
  wrote one log). The wrapper `wip/claude-tee.sh` tees the agent's
  stream-JSON to the scratchpad's `review-report/current.raw.jsonl`; so far
  it holds only `claude --help` (the app's capability probe).
- Nothing else is built: not the core tests, the Qt tests, the app or the
  composed course.

## New scope (owner decision, 2026-11-16), AC-f on the card

The shipped Review this pull request prompt carries its real instructions as
its visible, editable body in Settings > Prompts (what to review, sections,
focus, severity scheme, tone); Restore built-ins restores it. The format the
app parses stays app-owned, always added, and shows read-only under the
editor, labelled with the owner's wording from the RFC being written:
**"Result: Findings report"** ("Always added: the report format
BetterPullReview reads"). The reader tolerates a missing optional section
(no verdict guidance: the report shows without a verdict). Same for Review
{file}. The general Result and Feature model is the new RFC's, not this
card's. One more Accept/Reject question in the look: the Prompts page with
the review prompt's body and the read-only contract. The after real-agent
run uses the new visible prompt.

Plan for it (not started):
1. `prompt_macro_shipped.cpp`: the two `prompt_ask::review` built-ins become
   `named(plain(id, place, BODY, ...), "Review this pull request")`, the body
   opening with "Review this pull request" (the fixture agent keys on it);
   the Agent tab already shows a turn's name over a folded question.
2. `report-templates.md`: its review part reduced to the format contract
   (title, the optional `**Verdict:**` line, a finding as a level-three item
   with its place, suggestion blocks, the findings block).
3. `core::report_findings`: in a review (title "Review of ..." or a verdict)
   any item with a place is a finding, so a changed severity scheme still
   drafts.
4. `prompts_page`: a read-only box under the editor for a review prompt,
   fed the contract text by the app composition.
5. Tests: `core_prompt_macro`, the prompts page test, the fixture agent.

## Next, in order

1. Finish the before run; save answer, report, raw output, renders; diagnose.
2. `git apply` the held patches; build `test_better_pull_review_agent_report_core_tests`,
   `test_better_pull_review_agent_report_view_tests`, `bpr_fixture_agent`,
   `test_review_report_public_path`; run `core_agent_report`,
   `qt_agent_report_view`, `fixture_agent_answers`,
   `application_review_report_public_path`, then the ask courses and
   `application_fixture_agent_every_ask`. One build or test at a time, ctest
   no wider than `-j4`.
3. AC-f as planned above.
4. Format (`coding-format`), documentation gate, final commits on the lane,
   card and checkpoint mirrored to main.
5. After run with the real agent (one Sonnet, medium run), using the new
   visible prompt.
6. Freeze (no other build of mine running), attended review in a fresh short
   session: four Accept/Reject questions (reads as a report; Draft comment
   from a finding; layout; the Prompts page), mockups and before/after as
   images, SESSION_LINE in each; serve on the tailnet; report the URL.

# BPR-2219 checkpoint 2 (2026-11-16)

A review ask's report reads as a report and drafts comments from its
findings. Worker: rfc-worker-deep. `frontend-design:frontend-design` loaded.

## State in one paragraph

The card is claimed, the live pull request exists, the live course is
written and committed, the core half of the fix and its tests are written,
the fixture agent's review is rewritten, and the design round is drawn with
A chosen. **Nothing has been built or run yet**: the shared `build/dev` lock
was held by other workers' builds for over twenty minutes, so the real-agent
reproduction has not run and the cause below is read from the code, not
observed. Work moved to a private worktree to build without that lock.

## Committed

| Where | Commit | What |
|---|---|---|
| main | `dc745af3b` | Claim BPR-2219 (card, counter, reservation) |
| lane | `0c1263466` | Claim BPR-2219 (mirrors main) |
| lane | `eba9fbf31` | Checkpoint 1; `tests/end_to_end/live_second_account_course.cpp` gains `--course agent-review` (written, **not yet built**) |
| `claude/bpr-2219-review-report` | `05f51c9fb` | WIP, **not yet built**: core, core tests, fixture agent, design round |

The private worktree is
`.claude/worktrees/bpr-2219-review-report`, branch
`claude/bpr-2219-review-report`, cut from lane `3855972bd`. Its commits are
to be cherry-picked onto the lane with explicit paths once they build. The
lane's working tree holds none of this card's uncommitted work.

## Live target

MikeMarcin/BPR-Test#185, "Share one fetch client across sources",
`bpr-2219/review-report/base` <- `bpr-2219/review-report/head`, 14 files,
+140/-254. A small Python feed poller written for this card (no
BetterPullReview source). It carries real defects for a review to find: the
request timeout dropped; the cache keyed without query string or
credentials; 401 and 403 no longer `AuthError` (exit code 3 dead); one
request fewer than `retries` promises; Retry-After uncapped and unparsed;
`except Exception` in the scheduler; tests weakened to match. Open, to be
closed when the card is done. `X-RateLimit-Remaining` was 4989 at the last
real call.

## Cause (read from the code; the reproduction is still to confirm it)

1. **Transcript.** The Claude stream decoder appends every assistant `text`
   block of a turn to the one answer (`src/application/src/agent_launch.cpp`,
   the `*blockType == "text"` branch). What the agent says between tool
   calls therefore stands before the report in the answer.
   `core::report_card_of` and `core::dress_report` take the title only from
   a heading the answer opens with, so with notes in front the title is
   missed, the notes become the summary, the level-one title is read as a
   section and the findings fall below the level the page dresses.
2. **Draft comment.** The report page offers Add as draft comment only under
   an item's `suggestion` block, and which item offers it and the comment's
   words are decided in `qt::agent_report_view::follow`, not in core. A
   finding without a suggestion block has no action; the summary has none.
3. **Fixture blind spot.** The fixture agent answered a review with one text
   block and no notes, and its findings block listed findings its report did
   not have, so no course saw either defect.

## Design choice

`docs/design/review-report-v1/` (on the private branch): three candidates,
light and dark, HTML rendered headless. **A, at the foot of each finding**,
is chosen: a verdict line over the summary, Keep summary under it, and one
keep action at the foot of every finding (Draft comment at lines, Keep local
otherwise; then Drafted and Go to draft). B (a rail of actions and a
draft-the-rest band) and C (the comment shown as it would be drafted) are
the alternatives. The report's accepted hand (B, Dressed, 2026-11-14) is
kept.

## Written, on the private branch

- `core::read_report_answer` (notes cut at the report's level-one title),
  `dressed_report::Verdict` and `VerdictTone` (the `**Verdict:**` line),
  `core::report_findings` (each finding's keep action by `decide_keep`, and
  the comment it starts from; the agent's own suggested comment for an
  overlapping place is preferred), `core::report_summary_comment`.
- `tests/core/agent_report_tests.cpp`: `test_review_answer_verdict_and_findings`.
- `tests/support/fixture_agent_main.cpp`: working notes as text blocks beside
  tool calls, then a review with a verdict and a findings block that lists
  the report's own findings.

## Next, in order

1. Configure and build in the private worktree (`cmake --preset dev` with
   `-DCPM_SOURCE_CACHE=<lane>/build/.cpm-cache`; ccache hits across
   worktrees). Disk is 94% full: build named targets only.
2. Before run: `test_live_second_account_course --course agent-review --root
   DIR --pull 185 --path /opt/homebrew/bin/claude`, offscreen, **before** the
   Qt, caller and prompt changes go in. Save under the scratchpad's
   `review-report/before/`.
3. Qt: `report_page` draws the verdict, Keep summary and a keep chip at each
   finding's foot; `agent_report_view` takes the findings from core, keeps
   per-turn keep states, exposes them for courses; callers read
   `read_report_answer(...).Report` (`agent_tab_view::show_answer`,
   `open_report`, `agent_report_view::present`); `open_report` passes the
   turn's cards' suggested comments.
4. App: `window_quick_ask.cpp` draft dispatch takes Keep local and the
   summary (`act_on_agent_card`, `save_agent_note`), and Go to draft.
5. Prompt: `src/application/resources/review-preparation/report-templates.md`
   (verdict line; the report is the whole final message).
6. Course: new `tests/end_to_end/review_report_public_path.cpp` (fixture
   agent, `nested_folders_v1`, #21) and its CMake registration; update
   `tests/adapters_qt/agent_report_view_tests.cpp` and
   `tests/scripts/fixture_agent_tests.py`.
7. Format, focused tests, cherry-pick to the lane, docs
   (`docs/ux/ai-experience.md`, the card), documentation gate.
8. After run with the real agent; freeze; attended review in a fresh short
   session, three Accept/Reject questions; serve on the tailnet.

## Rules in force

Explicit-pathspec commits of this card's paths only; no stash; builds
through the dev preset; own PIDs only; live changes only on
MikeMarcin/BPR-Test; no review URLs in tracked files.
