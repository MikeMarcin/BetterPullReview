---
schema: "implement-rfc.card.v1"
id: "BPR-1892"
title: "A Tour tab agent request shows its failure on the Tour tab"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-06, coordinator chat: 'It's also weird that I have to switch to the agent tab to see my tour agent errors'","related_cards":["BPR-1878","BPR-1881","BPR-1891"]}
parked_on: "human-review"
acceptance: [{"criterion": "AC1 A failed Tour tab agent request shows a notice next to its control on the Tour tab, without a tab switch: one line saying it failed, in the words the Agent tab uses for that failure", "outcome": "pass", "revision": "4bee6df0f", "evidence": {"kind": "command", "detail": "qt_review_tour_agent_failure passes (notice hidden until a failure; 'Draft from PR context failed. ' + agent_tab_view::run_failure_words(nonzero_exit); sits in the helper's panel right under its row; Suggest an outline's under it). The frozen build's own offscreen look printed ATTENDED_TOUR_AGENT_FAILURE:Enhance problem statement failed. Claude Code exited with an error before answering.", "path": "docs/issues/40-parked/BPR-1892-tour-agent-errors-on-the-tour-tab.md#implementation-2026-11-06"}}, {"criterion": "AC2 The notice's button opens the Agent tab at the failed run's card", "outcome": "pass", "revision": "4bee6df0f", "evidence": {"kind": "observation", "detail": "Show on the Agent tab calls the window's dispatch (qt test: once per press); in the frozen build offscreen, pressing it showed the Agent tab with 'Claude Code stopped before answering / Claude Code exited with an error before answering.' at the top (app-tour-agent-failure-agent-tab-light.png).", "path": "docs/issues/40-parked/BPR-1892-tour-agent-errors-on-the-tour-tab.md#implementation-2026-11-06"}}, {"criterion": "AC3 The notice clears when its request is made again or succeeds, and Dismiss hides it. Successful requests show nothing new", "outcome": "pass", "revision": "4bee6df0f", "evidence": {"kind": "command", "detail": "qt_review_tour_agent_failure: Dismiss hides; pressing the helper or Suggest an outline again hides; clear_agent_failure (called by finish_writing on success or a stop the person asked for) hides.", "path": "docs/issues/40-parked/BPR-1892-tour-agent-errors-on-the-tour-tab.md#implementation-2026-11-06"}}, {"criterion": "AC4 Owner look: attended Accept/Reject on a frozen build that opens on a Tour tab showing a failure", "outcome": "pending", "revision": "4bee6df0f", "evidence": {"kind": "observation", "detail": "Session 98d6a84cfc7b4047df21682ea387eb07, candidate BPR-1892-tour-agent-failure (revision 4bee6df0f-v001), ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1892-tour-agent-errors-on-the-tour-tab.md#attended-review-ac4"}}, {"criterion": "AC5 python3 scripts/verify_documentation.py --root . passes", "outcome": "pass", "revision": "4bee6df0f", "evidence": {"kind": "command", "detail": "DOCUMENTATION_OK", "path": "docs/issues/40-parked/BPR-1892-tour-agent-errors-on-the-tour-tab.md#implementation-2026-11-06"}}]
---

# BPR-1892: A Tour tab agent request shows its failure on the Tour tab

## Owner report (2026-11-06)

"It's also weird that I have to switch to the agent tab to see my tour agent
errors."

When the Tour tab's agent drafting fails (Draft from PR context, Enhance,
Generate comment, Suggest an outline, or tour message writing), the failure
card appears only on the Agent tab. The author has to leave the tour to learn
that anything went wrong, and to find out what to do about it.

## Owner direction (2026-11-06, relayed by the coordinator)

"It's probably fine for the errors to stay in the agent tab, but we should at
least make the tour error a) stand out more and b) send you there."

This replaces the first draft of this card, which put the Agent tab's failure
card on the Tour tab with an in-place Retry.

## Change

- The failure card stays on the Agent tab. It is not reproduced on the Tour
  tab.
- On the Tour tab, a failed agent request shows a clearly visible notice next
  to the control that started it (the Problem or Solution helper, the stop's
  comment helper, or Suggest an outline): one short line saying the request
  failed, in the Agent tab's own words for that failure
  (`agent_tab_view::run_failure_words`, no second copy), and a button that
  opens the Agent tab at that failed run's card.
- The notice clears when its request is made again or succeeds, and it can
  be dismissed.
- Nothing switches tabs by itself.

## Acceptance

- AC1. A failed Tour tab agent request shows a notice next to its control on
  the Tour tab, without a tab switch: one line saying it failed, in the words
  the Agent tab uses for that failure.
- AC2. The notice's button opens the Agent tab at the failed run's card.
- AC3. The notice clears when its request is made again or succeeds, and
  Dismiss hides it. Successful requests show nothing new.
- AC4. Owner look: attended Accept/Reject on a frozen build that opens on a
  Tour tab showing a failure.
- AC5. `python3 scripts/verify_documentation.py --root .` passes.

## Implementation (2026-11-06)

frontend-design skill loaded before the UI work.

- `review_tour_editor` has one failure notice per agent request
  (`show_agent_failure`, `clear_agent_failure`, `set_agent_failure_dispatch`,
  `agent_failure_notice`, `agent_failure_text`), placed right under the
  Problem and Solution helpers' row, under the stop's action row, and under
  Suggest an outline. It is painted in the Agent tab's stopped-card red with
  a solid edge and a warning mark; its line is the control's own words when
  pressed plus "failed.", then `agent_tab_view::run_failure_words` (the one
  copy). **Show on the Agent tab** wears the agent accent and spark.
  Pressing the control again clears it; the close button dismisses it.
- `github_application::finish_writing` shows it for a failed run and clears
  it for a success or a cancelled run. The tab's existing writing status
  line is unchanged.
- `workbench_window` sends **Show on the Agent tab** to `show_agent(false)`
  and the new `agent_tab_view::reveal_run_failure`, which scrolls to the
  stopped card and focuses Resend. Nothing switches tabs by itself.
- `--attended-tour-agent-failure` with `--attended-tour-authoring
  {build}/tests/end_to_end/attended-failing-claude/claude`: the setup finds
  only that `claude` (a post-build copy of `test_scoped_writing_agent`, which
  fails every writing request when run from that folder), the look writes a
  problem, waits for it to save, presses Enhance problem statement and ends
  on `ATTENDED_TOUR_AGENT_FAILURE_SHOWN`.
- Verification: `qt_review_tour_agent_failure` (new),
  `qt_review_tour_agent_setup_call` and `qt_review_tour_agent_help` pass;
  `coding-format-check` passes; documentation gate `DOCUMENTATION_OK`.
  `docs/architecture/review-tours.md` describes it.

## Attended review (AC4)

- v001: frozen build of `4bee6df0f` at
  `/Users/jaeger/bpr1892-tour-agent-failure-v001.fm_45dud` (executable
  SHA-256 `5d492c93721309018b6dc065884e6b5ff7f2f1b861d755a63f44c3c366f3614b`),
  fixture `conventional-comments-v1`, ready line
  `ATTENDED_TOUR_AGENT_FAILURE_SHOWN`; the launcher's own offscreen check
  reached it. Launch: `python3
  /Users/jaeger/bpr1892-tour-agent-failure-v001.fm_45dud/launch.py`.
  Pictures from that executable, offscreen: the Tour tab close up, light and
  dark windows, and the Agent tab after Show on the Agent tab.
- New short session `98d6a84cfc7b4047df21682ea387eb07`, state
  `/Users/jaeger/.codex/attended-review/bpr1892-v001-6P7yLq/state`, repo
  root the implementing worktree, host started with `serve --tailscale`. One
  candidate, `BPR-1892-tour-agent-failure` (revision `4bee6df0f-v001`), one
  Accept/Reject question. Status: ready, awaiting the owner.
