---
schema: "implement-rfc.card.v1"
id: "BPR-2248"
title: "Agent actions on selected commits in the Commits tab"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2248--BPR-2248--f74f9df334a2.json","origin":"owner request, 2026-11-17","mandate_revision":"v0-5-0-prerelease-targets-r1","related":["BPR-2247","BPR-2179","BPR-2250"],"acceptance":["BPR-2248:offered","BPR-2248:base-merges","BPR-2248:shallow","BPR-2248:look"]}
---

# Agent actions on selected commits in the Commits tab

## Request

The owner, 2026-11-17: "It'd be cool to have the ability to ask what changed
in a selection of commits from the commit tab". Then: "Or review selected
commits or something. Decide what would be actually useful to get agentic
feedback on in the commits tab".

## Decision (coordinator, 2026-11-17)

The Commits tab's selection menu gets two agent actions. They have the same
scope, so it does not grow into a menu of near-duplicates.

1. **Explain these commits** (Answer). A walk-through of what the selected
   commits change and why, read against their messages. It flags commits whose
   diff does not match their message. This is what a reviewer wants when
   commits arrive on a pull request they have already reviewed.
2. **Review these commits** (Findings report). The review prompt, limited to
   the selected commits' diff, so a reviewer can review new work without
   re-reading the whole pull request.

Both are ordinary shipped prompts that can be placed on a new "Commits
selection" surface, so a user can add their own prompts there too.

Rejected options:
- One action per commit. Explain covers a single selected commit.
- "Split or squash advice". That concerns the author's history, not the
  reviewer.
- A separate "what changed" action. It is Explain.

## Shape

- Select one or more commits in the Commits tab (contiguous or not) and
  choose Explain these commits or Review these commits.
- The agent receives the diff of exactly the selected commits. Merge commits
  that bring in the base branch are left out or flagged, the same rule the
  "What changed since my review" fix uses (worked on in parallel, 2026-11-17).
- Explain answers in the Agent tab. Review produces a findings report with
  Draft comment.
- Commits missing from the local clone are fetched by the host, as in
  BPR-2246.

## Acceptance

- With commits selected, both actions are offered. Each runs on exactly those
  commits, and the request names them.
- Base-merge commits in the selection are handled as described above.
- The action works on a shallow clone.
- The UI work loads frontend-design and gets an attended look with the
  Fixture agent.

## Implementation (lane commit `21294f741`, 2026-11-17)

- Core: a new prompt surface, `commits_menu` (Settings › Prompts: Commits,
  Selection menu), a subject `selected_commits` and a `{commits}` placeholder
  (the short IDs). The two built-ins, `commits.explain` (Answer) and
  `commits.review` (Findings report), ship there; any ordinary prompt may be
  placed there, and a prompt about commits goes nowhere else.
  `plan_commit_selection` (`returning_review.hpp`) orders a selection by the
  pull request, takes each commit's own change through `own_change_steps`,
  names merges of the base, and refuses a selection with no own change.
  `decide_commit_asks` (`quick_ask.hpp`) lists the menu's rows.
  `supplement_commits` carries the commits, their message bodies and their
  changes (`bound_returning_commits`) beside the question only.
- Host: the press is queued on the Agent tab's session (the offer carries its
  commits; queued prompt encoding `p2;`). Its release reads the steps through
  `resolve_returning_change`, which fetches what a sparse review lacks, then
  sends the offer with the material (`window_commit_asks.cpp`). GitHub
  commit records now keep the message body (`github_commit_body`, 4 KiB).
- Qt: the Commits tab's context menu lists the rows after the filter actions,
  with the agent glyph; a selection that only merges the base disables them
  and says why under them. The empty-state hint names the new use. The
  frontend-design skill was loaded for the menu and wording.
- Fixture: `commit-selection-v1` (`build_commit_filter_fixture.py --merges`),
  four commits with a merge of main.

## Evidence

| Criterion | Check | Result |
|---|---|---|
| `BPR-2248:offered` | `application_commit_asks_public_path` (both rows offered, each run names exactly its commits, Explain answers in the Agent tab, Review is a report with Draft comment); `core_prompt_surfaces` (`test_commits_menu`) | Pass |
| `BPR-2248:base-merges` | `core_returning_review` (`a_selection_of_commits_is_planned_around_base_merges`); course: the merge is named, its change absent, a merge alone is disabled with a note | Pass |
| `BPR-2248:shallow` | the release reads through `resolve_returning_change`, whose sparse fetch is BPR-2247's `application_epoch_host` evidence | Pass (reused) |
| `BPR-2248:look` | attended look with the Fixture agent | Pending |

Also passed: `core_agent_context_supplement`, `core_agent_queue`,
`core_prompt_macro`, `core_quick_ask`, `application_agent_launch_policy`,
`github_paged_read`, `qt_prompts_page`, `commit_filter_fixture_contract`,
`fixture_agent_answers` and 229 focused tests in one pass; format check on
every changed file; clang-tidy on the changed translation units (no finding on
a line this card added that remains); `verify_documentation.py`.

## Remaining risks

- A Review finding's lines are read at the head; a finding on a line a later
  unselected commit moved drafts where the head has it.
- A rebased branch whose selected commits are no longer listed refuses the
  press (the queued question says so).
