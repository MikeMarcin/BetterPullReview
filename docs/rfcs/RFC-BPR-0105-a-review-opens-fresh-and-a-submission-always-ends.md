---
id: "RFC-BPR-0105"
title: "A review opens fresh, and a submission always ends"
status: "Auto-Verified"
metadata: {"origin":"owner notes, 2026-11-10, notes 13 and 16","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
---

# A review opens fresh, and a submission always ends

## Problem and grounded current behavior

Two reports share one shape: state from an earlier pull request survives in
a window that has moved on ([notes 13 and 16](assets/v0-5-0-prerelease-targets-v1/SOURCE_NOTES.md)).

### Approve does nothing after a long session

After reviewing several pull requests, using agents and tours, Approve does
nothing: no progress, no error. Restarting the application and trying again
works. The owner has seen it more than once and believes it predates tours.
The cause is **not confirmed**. Reading the path shows where a submission
can wait or vanish without a word:

- The sentence the owner sees, "Submitting as … (account …)", is a static
  description of the binding set by `present()` in
  [review_submission_workflow.cpp](../../src/app/src/review_submission_workflow.cpp).
  It is not a sign of progress.
- On a change of pull request the workflow retires its host and shows
  "Updating review authority…". `start_selection` returns while any retired
  host is not quiescent. A host is quiescent only when its one `Active`
  record is cleared, which happens when its effect finishes
  ([review_submission_application_host.cpp](../../src/application/src/review_submission_application_host.cpp)).
  Until then the new pull request has no host, and `decided_controls`
  returns no controls.
- Those effects run on one per-profile worker thread
  ([worker_executor.cpp](../../src/app/src/worker_executor.cpp))
  shared by review submission, conversation replies and tour publication
  ([window_construction.cpp](../../src/app/src/github_application/window_construction.cpp)).
  Tasks run one after another and have no time limit. A slow or hung earlier
  task holds everything behind it.
- Several completions are dropped quietly when they no longer match the
  current context or pending request. Each leaves "Submitting the exact
  review shown…" on screen with the controls inert, and nothing clears it.
- Queueing, the wait for a retired host, and those drops are not logged.
  The `github.review_submission` event is written only when a submission
  reaches the provider.

No card on the board describes this.

#### Found cause (BPR-2010, 2026-11-10)

A recorded course on the baseline `dc5453afa`
([verification record](../verification/RFC-BPR-0105.md#ac01-the-stall-reproduced))
confirms the account above in its shared-lane part and refines its
retired-host part. Two pull requests were opened from Home in one window
while an earlier effect held the per-profile publication lane, standing in
for a tour publication or reply that had not answered:

- Opened while the lane was held, the next pull request's review showed no
  draft, "Checking for an unfinished review…" and a disabled Approve for as
  long as the earlier effect ran: its new host existed, but its discovery was
  queued behind the earlier effect. A retired host was not needed for the
  wait; it is a second, rarer way in, when the host left behind still has
  accepted work.
- With the review ready and the lane then taken, Approve moved the page to
  "Waiting for GitHub to confirm this review…" with no bound, no progress and
  no log line, until the earlier effect finished. Nothing in the page said what
  it waited for. With a hung effect that wait outlives the window; only a new
  process has a new lane, which matches "restarting fixes it".

The owner's own session was not logged at that revision (the waits were not
recorded), so which earlier effect held the lane then is not known. The fix
does not depend on it: each review host now sends on its own lane, waits only
for its own pull request's earlier work, and every wait is stated, bounded and
logged.

### A new review shows the last one's tabs

After leaving a review whose Tour tab and side chat were open and opening
another from Home, the new review showed the Tour tab and the side chat. The
tray's tab selection and the side chat's open state are window-lifetime
widget state
([workbench_window_central_surface.cpp](../../src/adapters/qt/src/workbench_window_central_surface.cpp),
[tour_side_chat.cpp](../../src/adapters/qt/src/tour_side_chat.cpp)).
Opening from Home
([workbench_window_defects_and_home_rows.cpp](../../src/adapters/qt/src/workbench_window_defects_and_home_rows.cpp))
and clearing the previous pull request
([workbench_window_github_presentation.cpp](../../src/adapters/qt/src/workbench_window_github_presentation.cpp))
do not reset either, in the parts read. The first tray tab is Comments
(`tray_comments_tab`,
[workbench_window_support.hpp](../../src/adapters/qt/src/workbench_window_support.hpp)).

## Proposed behavior and boundaries

### A submission is always in a stated condition

From the moment the reviewer presses Approve, Comment or Request changes
until the review is on GitHub or has failed, the submission surface shows
exactly one of:

- **Waiting**, with what it waits for in plain words ("Waiting for an
  earlier reply to finish sending", "Checking the signed-in account");
- **Sending**, with the progress indicator;
- **Published**; or
- **Not published**, with the reason and Retry.

Waiting is bounded. After 20 seconds without starting to send, the
submission ends as Not published, "Still waiting for *what*. Your review is
kept.", with Retry enabled. A completion that no longer matches what the
window is doing ends the visible submission the same way; it is never
discarded while the surface still says Submitting. The draft is kept in
every case.

The static line about the acting account is labelled as what it is ("Will
submit as …") and is not shown as progress.

### Another pull request's work does not block this one

A submission for the pull request in front of the reviewer does not wait on
an effect that belongs to a pull request the window has left, nor on a tour
publication or a reply for a different pull request. How the lane is
arranged to guarantee that is an implementation choice within the
application layer's serialized-completion rules; the observable requirement
is the sentence above.

### The cause is found before the fix is called done

The first work under this RFC is a diagnosis: reproduce the stall with a
long multi-pull-request course (a slow or hung earlier effect, a change of
pull request, then Approve), and confirm or refute the account above. If the
cause is different, this section is amended with the found cause before
implementation is accepted. The bounded wait and the stated conditions hold
either way.

### The path is logged

The diagnostic log records, for a submission: that it was requested, each
wait and what it waited for and for how long, each dropped completion and
why, and the ending. A diagnostic export of a stalled session is enough to
say where it stopped.

### A review opens fresh

Opening a pull request in a window, from Home, a link, a notification or a
tab, starts its review surface in the default state: the tray on Comments,
the tour's side chat and the tour page closed, no chat thread selected, and
the Agent tab on that pull request's own session. This holds whether or not
the window last showed the same pull request.

Kept on purpose, and unchanged: the filter defaults 0.4.2 remembers, drawer
widths and other window layout, per-pull-request agent sessions and their
history, tour drafts, and a pull request's own reading position under
RFC-BPR-0025.

### Boundaries

- What is submitted, to whom and under which account is unchanged; this RFC
  adds no retry that the reviewer did not press.
- The 20-second bound is on waiting to start, not on a send in progress,
  which keeps the provider's own deadlines.

## Acceptance criteria

- **AC01:** A recorded course reproduces the stalled submission on the
  inspected revision, or records the attempts that failed to and the cause
  found by other means; the RFC's cause section agrees with the record.
- **AC02:** With an earlier effect for a previous pull request held open, a
  submission for the current pull request is sent and published.
- **AC03:** With the current pull request's own submission unable to start,
  the surface shows Waiting with its reason and, at 20 seconds, Not
  published with Retry; the draft is intact and Retry submits once the
  obstruction is gone.
- **AC04:** A completion that arrives for a superseded request leaves the
  surface in Published or Not published, never in Submitting.
- **AC05:** The diagnostic log of the blocked-lane, bounded-wait and
  superseded-completion courses contains the request, each wait with its
  subject and duration, each drop with its reason, and the ending.
- **AC06:** After a review with the Tour tab and a side chat open, opening a
  different pull request from Home shows the tray on Comments with no side
  chat, and the same holds when the same pull request is reopened.
- **AC07:** The remembered filter defaults and the window's drawer widths
  are the same before and after opening the different pull request.
- **AC08:** A pull request on MikeMarcin/BPR-Test is approved through the
  window's own Approve after a course of at least five pull requests with
  agent and tour use, and it publishes. (Amended 2026-11-10 by the owner: it
  was the owner's own run on a frozen build, which proved "way too many
  manual steps"; an agent-run live course through the same window and
  actions replaces it.)
