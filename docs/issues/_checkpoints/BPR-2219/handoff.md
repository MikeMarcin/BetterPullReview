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
