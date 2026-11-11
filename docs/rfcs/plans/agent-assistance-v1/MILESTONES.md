# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **agent-assistance-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## returning-pull-request

A reviewer returning to a pull request with new commits asks what changed since their review and whether a thread was addressed.

**Work:** `BPR-2073`, `BPR-2074`, `BPR-2075`

**Prerequisites:** None

**Completion:**

- What changed since my review names both revisions and is unavailable, with the reason, when the earlier one cannot be read.
- Was this addressed? takes a conversation as its subject and its answer is shown as a claim.
- Neither operation resolves, posts or changes attention state, and nothing runs without a press.
- Each operation has its own built-in prompt.

**Boundary:** The two unused entry points analyze_latest_upload and evaluate_proposed_fix get their surfaces. Read-only runs.

## explain-file

A reviewer has the open file explained in Review, without going to the Agent tab.

**Work:** `BPR-2076`, `BPR-2077`

**Prerequisites:** None

**Completion:**

- Explain this file takes the open file as its subject with no selection.
- The answer shows at the file header and can be kept as a note, continued or dismissed.
- Nothing runs without a press.

**Boundary:** The unused entry point analyze_current_file gets its surface. Files only, not selections.

## comment-to-suggested-change

A reviewer turns a draft comment into an exact suggested change that core has checked against the lines.

**Work:** `BPR-2078`, `BPR-2079`

**Prerequisites:** None

**Completion:**

- A reply is taken only as a replacement for the draft's exact lines and accepted only when its stated current text equals the head's lines.
- An accepted replacement opens as RFC-BPR-0090's in-pane suggestion edit; a refused one is never shown as a suggestion.
- Nothing is sent to GitHub, and one Undo restores the draft.

**Boundary:** One draft comment at a time, on lines GitHub can take a suggestion on.

## failing-check-question

A reviewer asks the agent why a check failed, from the check's row.

**Work:** `BPR-2080`, `BPR-2081`

**Prerequisites:** None

**Completion:**

- A check is a question subject, and the brief carries only what BPR already holds, with provider text as quoted data.
- BPR downloads no log, passes no credential, reruns nothing and writes nothing.
- An agent that cannot read the log answers so, as a claim.

**Boundary:** RFC-BPR-0068's boundary is unchanged for BPR itself; the agent uses its own GitHub access.

## agent-run-checks

An author has the agent run a planned check on a local draft and accepts the reported result into evidence.

**Work:** `BPR-2082`, `BPR-2083`

**Prerequisites:** None

**Completion:**

- An agent-run result is recorded as reported, never observed, with its commands and limitations.
- The result is bound to the candidate only when the worktree equals it before and after the run.
- A failed result is kept, and nothing enters evidence or is published without the author.

**Boundary:** Needs RFC-BPR-0109's evidence record and RFC-BPR-0115's change runs first.

## agent-insights

Insights shows, per agent feature, the runs, the reported spend and how many suggestions were kept.

**Work:** `BPR-2084`, `BPR-2085`

**Prerequisites:** None

**Completion:**

- Runs, endings and reported spend per feature match the sessions that produced them; a missing cost is Not reported.
- Kept, dismissed and neither are counted once and never decrease.
- The export holds counts and sums only, and an agent feature with no row fails a test.

**Boundary:** A record of what was kept, not of what was right. No budgets or alerts.
