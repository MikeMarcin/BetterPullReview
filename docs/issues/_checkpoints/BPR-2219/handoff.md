# BPR-2219 checkpoint 1 (2026-11-16)

A review ask's report reads as a report and drafts comments from its
findings. Worker: rfc-worker-deep. `frontend-design:frontend-design` loaded.

## Done

- Card claimed: main `dc745af3b`, lane `0c1263466` (mirrors main).
- Live target made: MikeMarcin/BPR-Test#185, "Share one fetch client across
  sources", `bpr-2219/review-report/base` <- `bpr-2219/review-report/head`,
  14 files, +140/-254. A small Python feed poller written for this card (no
  BetterPullReview source). It carries real defects for a review to find:
  the request timeout dropped, the cache keyed without query string or
  credentials, 401/403 no longer `AuthError` (exit code 3 dead), one request
  fewer than `retries` promises, Retry-After uncapped and unparsed,
  `except Exception` in the scheduler, tests weakened to match. Its local
  clone is the session scratchpad's `pr-repo`; GitHub holds both branches.
- Live course written, not yet built or run:
  `tests/end_to_end/live_second_account_course.cpp`, `--course agent-review
  --root DIR --pull N --path PROGRAM [--model M] [--effort E]` (Sonnet,
  medium by default). It presses Review this pull request in the review
  bar's Ask and writes `review-answer.md`, `review-report.md`,
  `agent-review.json` and renders into DIR. Uncommitted.

## Read so far (what the cause is likely to be; not yet reproduced)

- The Claude stream decoder appends every assistant `text` block of the
  turn to the one answer (`src/application/src/agent_launch.cpp`, the
  `*blockType == "text"` branch, around line 1730): what the agent says
  between tool calls is part of the answer, so of the report.
- The report's page (`core::dress_report`, `qt::report_page_html`) offers
  Add as draft comment only under an item's `suggestion` block, and the
  choice and the comment's words are made in `qt::agent_report_view::follow`,
  not in core. An item without a suggestion block has no draft action.
- The review skill (`packaging/agent-skills/bpr-pr-review/SKILL.md`) asks for
  prose then a `bpr-findings` block; the report format
  (`src/application/resources/review-preparation/report-templates.md`) asks
  for the report template. Both ride one review request.
- The draft path to reuse: `act_on_agent_card`
  (`src/app/src/github_application/window_agent_setup.cpp`), which the
  report's draft dispatch already calls (`window_quick_ask.cpp`).

## Next

1. Build `test_live_second_account_course` (dev preset; the tree is shared,
   wait for the lock) and run it offscreen on #185 with
   `/opt/homebrew/bin/claude`; save the output under the scratchpad's
   `review-report/before/`. Check `X-RateLimit-Remaining` on a real call
   first (4989 at the last read).
2. Diagnose from the saved answer; record the cause on the card.
3. Design: two or three mockups, light and dark.
4. Implement: core report findings and the draft decision, host effect, Qt
   page; prompt; fixture agent's structured review; composed course.
5. Attended review in a fresh short session; real-agent after run.

## Rules in force

Explicit-pathspec commits of this card's paths only (three other workers
edit the lane); no stash; builds through the dev preset; own PIDs only; live
changes only on MikeMarcin/BPR-Test; no review URLs in tracked files.
