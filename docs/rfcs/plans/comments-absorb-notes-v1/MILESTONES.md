# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **comments-absorb-notes-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## local-comment-model

One store answers what unsent comments I have: every private note is a Local comment with its text, anchor and order kept, and no action that sends anything can send one.

**Work:** `BPR-2157`

**Prerequisites:** None

**Completion:**

- The migration of a ledger holding notes from before and after the path and line tables gives one Local comment each, repeatable after interruption.
- No send path admits a Local comment, and counters ignore Local.
- The Notes tab reads the new store and nothing else looks different.

**Boundary:** Core, host and SQLite only; no visible change beyond the Notes tab reading the new store. Review Cycle notes, the note: label and local PR drafts' records are untouched.

## comments-absorb-notes

The Comments drawer holds Local, Draft and Published comments with one state control, N for Local, the computer icon for Local and one gutter mark a line; the Notes tab is gone.

**Work:** `BPR-2158`

**Prerequisites:** local-comment-model

**Completion:**

- AC01, AC02, AC04 to AC06 and AC14 pass on the composed application.
- The owner's look on a frozen build with all three states on one line is filed (Accept or Reject).

**Boundary:** The drawer, composer and gutter. Threads, Overview, Finish your review and agent surfaces wait for their milestones.

## finding-and-counting

Local comments are found and counted where drafts are: Threads' state filter, Overview's Local chip and My review, and one quiet line in Finish your review.

**Work:** `BPR-2159`

**Prerequisites:** comments-absorb-notes

**Completion:**

- AC07 to AC09 pass on the composed application.
- The owner's look is filed (Accept or Reject).

**Boundary:** Threads, Overview, Finish your review and Insights' tile name. No change to what a review sends.

## one-keep-action

Every agent surface that offered a note and a draft offers one keep action, and the private-context opt-ins name local comments and drafts.

**Work:** `BPR-2160`

**Prerequisites:** comments-absorb-notes

**Completion:**

- AC10 and AC12 pass on the composed application.
- The owner's look is filed (Accept or Reject).

**Boundary:** What the agent surfaces keep, and wording. Where the agent surfaces are reached is RFC-BPR-0128's.

## local-pr-draft-vocabulary

A local PR draft's notes read as Local comments, with Keep local, and its storage is unchanged; the RFC closes.

**Work:** `BPR-2161`, `BPR-2162`

**Prerequisites:** comments-absorb-notes, finding-and-counting, one-keep-action

**Completion:**

- A rendered local draft shows the new words with storage unchanged.
- Every criterion of RFC-BPR-0127 has direct evidence on the closeout and the amended RFCs are reconciled.

**Boundary:** Vocabulary and card only (owner, Q8); nothing follows a local draft to its pull request (Q12).
