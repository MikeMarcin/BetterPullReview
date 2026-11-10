# Review Tours

A Review Tour walks reviewers through a pull request: a **Problem**, a
**Solution**, and an ordered route of stops, each a comment on selected lines.
Reviewers need nothing installed; they see ordinary GitHub comments and a
**Start here** comment with **Previous** and **Next** links. AI is optional.

Start one with **Prepare review tour** (`Alt+T`) and add stops with **Add tour
stop** (`Alt+Shift+T`).

## Pointing a stop at other lines

Edit the stop, then choose **Use selected lines** to point it at the lines
selected in the comparison. With the tour outline focused, **Alt+Shift+U**
(Option+Shift+U on macOS) does the same while the stop is being edited.
Reading or collapsing the stop withholds that action; no code is copied.

## Before you publish

- Nothing reaches GitHub until you confirm the publication plan. **Ready** on
  a stop is a private checkmark for you.
- The Markdown you see is exactly what is published.
- GitHub accepts at most 100 line comments and 64 KiB per review.
  BetterPullReview never truncates a tour to fit, so a large tour may not
  publish.
- Editing the pull-request description changes only its Problem and Solution
  sections.
- Not yet tested against live GitHub: file-level stops, stops outside the
  diff, edits to the **Start here** comment, and GitHub Enterprise Server.

If publication stops, its status says which preparation or server read failed
and preserves your draft. A status saying this attempt sent nothing applies to
that attempt; earlier staged or published work is retained. **Check status**
reconciles a possibly sent operation, and **Resume** requires a fresh preview.
Use **Help > Export anonymized diagnostics…** to inspect the publication stages
and classified failure without including your Problem, Solution or stop text.

## Repairing a destination before publication

**Preview** lists each stop that cannot join the review, with its number,
title and reason. Select its linked title to open and scroll to that stop.

- **Re-pin to current head** is available for an earlier revision only when
  the exact Git mapping proves the selected bytes unchanged at the current
  head and the whole range fits a current diff hunk. It keeps the explanation.
  Changed, ambiguous, missing or unavailable source needs your own inspection
  and **Use selected lines**; BPR does not choose a replacement for it.
- **Move to nearest change…** previews the new line range and actual pinned
  source text. **Apply destination** makes one ordinary edit; **Cancel** keeps
  the original. The range can include the hunk's context: GitHub.com and the
  observed GHES 3.20.8 course accept up to three context lines from a change.
  A line four away is outside that hunk. BPR uses the provider's hunk bounds,
  rather than guessing them from the painted comparison.
- **Undo** and **Redo** restore the destination and update the blocker list.
  Explanations with explicit references to line numbers that moved need
  reaffirmation; other prose stays ready. The source preview expires if you
  edit the draft before applying it.

Repairing every blocker keeps **Keep single-review publication**: preview again
and publish every stop in one COMMENT review. **Publish with compatibility
comments (non-atomic)** remains an explicit last choice; it publishes the whole
route as visible conversation comments. Previewing or repairing sends nothing.

File-comment support is a separate operation capability. A standalone file
comment cannot be represented as a private pending item in the current publisher.
An unknown or denied operation never enables it, and a painted file or line does
not prove support. The mixed/file publication extension is still under development.

## Updating a published tour

Reopening a tour reads it back from GitHub. If someone edited it on the
website, you choose between your version and theirs. Removing a stop leaves
its old comment and replies on GitHub.

> [!WARNING]
> If a publish ends with an unknown result, choose **Resume** rather than
> publishing again, or comments may be posted twice.

## Writing assistance

The assist buttons rewrite a field or selection as one undoable edit; their
instructions live in [Settings](settings.md#review-preparation-writing-hooks).
AI text is never published without you, so check its claims.

The whole tour, Problem, Solution and each stop have separate conversations for
the selected agent. **Draft Tour** and that agent's whole-tour chat continue the
same native session. Small writing hooks remain separate requests.

**New session** starts fresh in the selected chat and retains its earlier
conversation in **History**. It asks no clear-confirmation question and sends
nothing until you send a message. Choose an earlier session to read it or
continue it after its revision and permissions are checked. A storage refusal
keeps the previous session selected. **Other tour chats and detached history**
opens retained chats for reading, including removed stops and drafts left behind
by Reset tour. It does not make them the current draft's conversation.

When the head changes, the chat shows its saved revision and the current head.
Choose **Continue old revision** to keep its exact native conversation and
workspace, or **New session** to work at the current revision. Choosing sends
no question. Changing the selected chat/history or the observed revision requires
choosing again. A continued answer and any source proposal retain the revision
the agent actually read; current tour facts still accompany the next question.

Starting or reopening a session preserves accepted tour text, sources, focus,
notes and comment drafts. **Reset tour** is separate: its existing confirmation
clears the active tour and proposal associations; **Undo reset** restores them.
Neither action deletes the agent's own transcript. Native conversations retain
previously sent context, even after its selection is removed; New session begins
without that older memory and receives the current selected context.

Each explicit tour message carries BPR's versioned guide and a complete current
snapshot. The snapshot separates confirmed lines, preparation **Ready**, and
publication eligibility, and includes pending proposals and recorded decisions.
**Source needs attention** describes changed, ambiguous, missing or unavailable
source evidence. **Lines not confirmed** means there is no confirmed source
binding; accepting an explanation does not supply one. To confirm a proposed
place, choose **Show in code**, select its exact lines, edit the stop, and choose
**Use selected lines**. A confirmed author-selected replacement alone needs no
attention warning. Editing text or reordering preserves its confirmed binding.

After a head change, an answer must distinguish the revision its workspace read
from the observed head. Missing evidence stays unknown. Refreshing the tour sends
no question. Context updates accompany an explicit send, and the complete request
is refused if the required snapshot exceeds the 160 KiB carrier. Typed questions
have a separate 16 KiB limit.

Drafts stay on this computer; see
[Storage and diagnostics](storage-and-diagnostics.md). To work with tours from
a terminal, see the [tour CLI](tour-cli.md).
