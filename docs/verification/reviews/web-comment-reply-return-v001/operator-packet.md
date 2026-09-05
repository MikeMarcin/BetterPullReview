# Existing-comment web reply and return check

Status: **prepared; new reply not yet authorized or posted**.

BPR-0256 v011 already published the source-line comment successfully. This
checkpoint uses that existing thread; it does not repeat publication or sign-in
diagnostics. The owner's standing instruction permits autonomous diagnosis and
preserving remembered sign-in. The earlier one-comment course explicitly
excluded replies, so this additional message needs its own instruction.

## Exact proposed action

Post one reply through the ordinary GitHub web UI as MikeMarcin to
[comment 3940608913](https://github.com/MikeMarcin/BPR-Test/pull/1#discussion_r3940608913)
on MikeMarcin/BPR-Test#1. The parent comment is on the RIGHT side, line 1 of
`bpr-live-review-fixture.txt`, at head
`17990ba0c2fb50338141f35c0717f0f99639c7ba`, and reads:

> BetterPullReview attended round-trip verification.

Proposed reply, in full:

> BetterPullReview web reply return verification.

Then refresh the ordinary BetterPullReview workbench and compare the new reply's
server identity, parent thread, author and complete text with GitHub. Use the
remembered MikeMarcin app session and an existing authenticated browser session
if available. Do not extract a token or move app credentials into the browser.
If browser sign-in requires the owner, leave that ordinary sign-in interaction
to them; do not request a replacement device grant for the healthy app session.

## Procedure after authorization

1. Reopen the ordinary Release app by its full path
   `build/app-release/src/app/BetterPullReview.app`, identify its sole process,
   and observe remembered account restoration using safe UI or appended
   sanitized log records. No new publication capsule is needed for app reads.
2. Open only the designated PR and verify the existing parent thread in the
   workbench. Use GitHub's web UI to confirm MikeMarcin, the exact parent comment
   and unchanged head before entering the reply. Stop on identity drift.
3. Enter precisely the proposed reply and submit once. On uncertain submission,
   inspect the existing thread before taking any further action; never click
   again merely because the response was delayed.
4. Record the resulting reply link/ID, complete text and author. Invoke the
   workbench's public Refresh action and verify the same reply appears once
   under the same thread. If it fails, diagnose and repair locally under the
   standing instruction, then re-read this same reply instead of posting another.
5. Quit without Sign out, preserving remembered sign-in. Record direct results
   and limitations, and remove only task-created temporary outputs. No additional
   comment, edit, deletion, resolution or review disposition is included.

The public Refresh action is `refreshPullRequestAction` in
`src/adapters/qt/src/workbench_window.cpp`, with the application Refresh
shortcut. This is an existing public read path, not an internal test trigger.
The preceding [v011 result](../designated-live-review-publication-v011/attended-result.md)
already supplies successful source-line publication and GitHub web rendering.

## Decision and evidence boundary

A plain instruction authorizing this exact reply is sufficient. No pasted hash
block or verbatim statement is requested: the owner's instruction to avoid
repeated digest ceremonies supersedes that local packet convention. Save the
actual response; do not manufacture an accepted verdict in advance.

Authorization of the reply does not itself prove that it returns to the app or
that the owner has accepted the comment's appearance. Record those facts only
from their respective direct observation and human decision. BPR-0143's separate
edit/delete/resolve cases and BPR-0144's review dispositions remain outside this
one-reply checkpoint. The original v011 approval stays consumed and immutable.
