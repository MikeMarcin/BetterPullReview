---
schema: "implement-rfc.card.v1"
id: "BPR-2323"
title: "A finding's side chat and its updated suggested comment are not stored, the report shows the update as an extra block, an existing draft keeps the old text, and a suggestion outside the diff can be drafted but never published"
status: "Backlog"
rfcs: []
depends_on: ["BPR-2322"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-20, build 71 on macOS, a GitHub Enterprise pull request: from a report's finding the owner asked a follow-up, used Update suggested comment, drafted the comment and published. GitHub refused the line, the app's advice led to a second refusal, and after a restart the follow-up and the update were gone. A second follow-up the same morning showed the same: nothing of it is stored","reconciled_with":"BPR-2322 (owner, 2026-11-20: \"You're going to have to reconcile this one with what we've actually decided to do with follow-ups\")","reservation":"docs/issues/_reservations/BPR-2323--BPR-2323--bbd90ddef505.json","related_cards":["BPR-1084","BPR-1706","BPR-1735","BPR-1736","BPR-2232","BPR-2286","BPR-2322"],"related_rfcs":["RFC-BPR-0127","RFC-BPR-0128"]}
---

# BPR-2323: A finding's side chat and its updated suggested comment are not stored, the report shows the update as an extra block, an existing draft keeps the old text, and a suggestion outside the diff can be drafted but never published

[[BPR-2286]] added **Ask follow-up** on a finding and **Update suggested
comment**. [[BPR-2322]] replaced its follow-up window with the item's side
chat: a column beside the report (or the code), built from the Tour tab's
side chat, holding one conversation per item. This card is written against
that design. On the owner's first real use (before BPR-2322), every part of
the path let them down, and BPR-2322 changed only where the conversation is
shown, not any of these:

- **A.** The conversation and the update are lost on restart.
- **B.** The update is not an edit of the report. It is a flat block added
  under the unchanged finding.
- **C.** The update does not reach a draft that already exists.
- **D.** The finding's suggestion sat on lines outside the diff. It was
  drafted as a suggestion GitHub can never take, and the app's advice after
  the refusal could not work either.
- **E.** None of it is in the diagnostic log.

## Observed (build 71, before BPR-2322)

The owner, 2026-11-20, build 71 on macOS. A Claude stream-JSON agent's
review of a GitHub Enterprise pull request. Its report had one Must fix
finding. The finding was about lines 14055–14056 of a 20,432-line C++ file
that the pull request **does not change**: a second site of the same bug,
which the pull request missed. The pull request's only hunk in that file is
lines 3628–3643. The finding carried a `suggestion` block.

What the owner did, and what happened:

1. **Ask follow-up** on the finding (then a window; since BPR-2322 the
   side chat): "Can this be a cpp block instead of a
   suggestion?" The agent agreed and explained the trade-off.
2. **Update suggested comment**, then **Use this**. The window (now the
   side chat) said
   "Suggested comment updated. Draft comment drafts it from now on; nothing
   is posted." Under the finding, the report kept its "Suggested change"
   diff and added a line, "Suggested comment, updated:", followed by the new
   comment as one run of plain text, with the code fence's backticks shown
   literally.
3. The finding read **Drafted**: it had been drafted before the update. The
   owner published. GitHub answered:

   ```text
   422 Validation Failed: pull_request_review_thread.line must be part of the diff
   ```

   The app logged `off_hunk_refused` and said "GitHub refused a line comment
   here. Publish as file comment?"
4. The owner published again, as advised. The log has no further
   `publish_comment` event in that session: nothing was sent, and nothing
   said so.
5. The owner restarted the app. The follow-up and the updated comment were
   gone. The draft still held the original `suggestion` text.
6. A later publish was refused locally (`incompatible_body`): "This comment
   needs a supported inline line anchor. Your draft text is kept."
7. A second follow-up on the same finding, the same morning, ran and
   answered. After it finished, a search of every store under the profile's
   `state/v1/` found neither its question nor the updated comment. The same
   search found the report, the draft and the earlier review's turns.

## Cause

### A. Nothing of a side chat is stored

- [[BPR-2286]] kept the updated suggested comment out of the stored answer,
  and listed under **Not done**: "An updated suggested comment is not
  persisted past the window." [[BPR-2322]] kept that: the record is still
  `FollowUpRevisions`, an in-memory map in the application
  (`src/app/src/github_application/window_follow_up.cpp`).
- The rounds and proposals live only in `application::follow_up_host`, in
  memory, one `core::follow_up_state` per item. Closing the side chat keeps
  them; quitting the app does not. The review's conversation in
  `native_agent_turns` holds only the review's own turn, and no agent
  session file is written for a side chat round.

### B. The report shows the update as an extra paragraph

- `report_page.cpp` (around lines 184–189) renders "Suggested comment,
  updated:" and the new text, `toHtmlEscaped`, in one `<p>` under the
  finding's actions. The finding's body, its code and its "Suggested change"
  diff are unchanged above it.
- The new text is not rendered as Markdown, so its code block loses its
  layout and shows its fences.

### C. An existing draft is left with the old text

- `use_follow_up`'s `update_suggestion` case (`window_follow_up.cpp`, around
  lines 287–297) updates the record, the Agent tab card and the report. It
  does nothing to a draft already made from the finding.
- The same file already has the right operation: `update_draft` replaces a
  draft's text "as one undoable edit, only if the field still holds the text
  the proposal was made from". It is offered only from the draft's own Ask
  about this, not from a finding whose draft exists.

### D. A suggestion outside the diff

- A comment with a `suggestion` block needs a line inside a hunk. GitHub
  refuses it elsewhere, and it cannot go as a file comment.
- The app knows the file's hunks when it drafts, but drafted the finding's
  `suggestion` on lines 14055–14056 as a line comment anyway.
- After GitHub's refusal, `review_draft_editor.cpp` (around lines 1142–1147,
  [[BPR-1736]]) offers "Publish as file comment?" without looking at the
  body. The publication policy then refuses that representation for a body
  that `requires_line_subject` (`src/core/src/review_publication_policy.cpp`,
  `fallback`, around lines 30–35): the `incompatible_body` above.
- Step 4's publish reached no `publish_comment` at all. Whether the policy
  refused it before the mutation started, or the press was lost, is
  unconfirmed. Either way the reviewer was told nothing.

### E. Not in the diagnostic log

- The review run is logged (`agent.run.started` and `agent.run.succeeded`).
  Neither follow-up wrote any event at Standard detail: no run, no apply,
  no refusal.

## Change

### 1. Store an item's side chat with the review it is about

- Each finding's side chat is stored with its review, keyed by the
  finding's key (its card's delivery and place, or the report's turn and
  row, as `window_follow_up.cpp` keys it today): its rounds (question,
  answer, outcome, time) and each proposal with its state (shown, used,
  discarded).
- Reopening the report, or the app, shows the finding's **Ask follow-up**
  chip as "Follow-up (3 rounds)". It opens the stored conversation in the
  item side chat column, and a new question carries the stored rounds, as
  the side chat does within one session.

### 2. Use edits the finding in the report, with undo and redo

- **Use this** in the side chat replaces the finding's suggested comment in
  the report: its body, its code block and its suggested change,
  re-rendered as the report renders any finding. No "Suggested comment,
  updated:" block is added under it.
- The report records the edit as a revision of the stored report, so it
  survives a restart. The original stays reachable: "Edited from the
  follow-up · Show original".
- **Undo** and **Redo** work on the report like any edit: Edit › Undo and
  ⌘Z / ⇧⌘Z while the report has focus, and an Undo link in the side chat's
  status line right after Use. Undoing restores the original text and its
  rendering. Redo applies the update again.
- A finding's card on the Agent tab shows the same text, and undoes and
  redoes with it.

### 3. Offer Update the draft too

- When the finding is already drafted, **Use this** in the side chat says
  so and offers **Update the draft too**: the side chat's existing
  `update_draft` apply, as one undoable edit of the draft, refused if the
  draft's text has changed since it was made from the finding.
- Without that, the finding keeps saying "Drafted (old text)", and **Go to
  draft** opens the draft with a note that the report's comment has changed.

### 4. Draft only what GitHub can take

- When drafting a finding whose lines are not in the diff, a `suggestion`
  block becomes a fenced code block in the file's language, and the draft
  goes as a file comment that links to the lines. The draft says so before
  it is published: "Lines 14055–14056 are not in this pull request's
  changes. This will post as a file comment, without Commit suggestion."
- After an `off_hunk_refused`, the advice looks at the body. For a body
  with a `suggestion`, offer **Make it a code block and publish as a file
  comment** (one undoable edit, then the file comment), not plain "Publish
  as file comment?".
- A publish that sends nothing always says why, in the status line and the
  log.

### 5. Log it

Owner, 2026-11-20: "Make sure that we can see things like these follow-up
questions being asked in the logs and being applied", and "make sure that
the actual content of messages and agent responses don't show up in any
anonymized exports". Content is logged at Verbose only, because it can be
large.

- **Already in [[BPR-2322]]** (`agent.side_chat`, Standard): a question
  asked (started and ended), a proposal asked for and shown, and a Use
  applied or refused, each with the item's kind, the round, the apply
  action (which says where it goes: report finding, draft or reply
  composer), the outcome or the core's refusal, the duration, and the
  lengths in characters (question, answer or proposed text, and before and
  after for a proposal or Use). No words, title, path or key.
- **Standard, added here**, still with no content: Discard, Update the
  draft too applied or refused, Undo and Redo of an applied update, and
  every remaining refusal with its reason; the finding's key and the agent
  on every line, in a form the anonymizer already treats as an identifier.
- **Verbose, added here**: the content itself, the question, the answer,
  the proposed text, the finding's title and its path and lines, on the
  same events, in fields the anonymized export recognises as content.
- **Anonymized export, at any detail**: the content fields are stripped
  (lengths stay). The stored side chat rounds of Change 1 are never part of
  an anonymized export or report bundle.
- A publish refused by the policy before any request writes a
  `review_mutation.failed` with its reason.

### 6. Tests

- Application: a finding's side chat rounds and a used update survive a
  restart of the host. Reopening the report shows the updated comment, the
  chip says how many rounds there are, and the side chat opens with them.
- Qt report: Use replaces the finding's rendered comment (no extra
  paragraph). Undo restores the original rendering and Redo the update, by
  ⌘Z and ⇧⌘Z and by Edit › Undo and Redo.
- Qt side chat: Use on a drafted finding offers Update the draft too.
  Accepting it changes the draft in one undoable edit, and it is refused
  over a draft changed since.
- Core policy: drafting a `suggestion` on lines outside every hunk yields a
  file comment with a fenced code block. A refused off-hunk `suggestion`
  draft is offered the conversion, never the plain file comment.
- Log: a side chat round, a Discard, Update the draft too, Undo and Redo
  each write one Standard event with lengths and no content, and a
  policy-refused publish writes one with its reason. With Verbose on, the
  same events carry their content.
- Export: with Verbose on, a side chat round whose question and answer hold
  distinctive marker strings (and a proposal, title and path that do too)
  is logged, the log is exported anonymized, and no marker appears in the
  export, nor in any stored round an anonymized bundle carries.

## Acceptance

- AC1. A finding's side chat and its used update are still there after the
  app restarts: the chip counts the rounds and the column shows them.
- AC2. Use updates the report's finding in place, rendered like any finding,
  and Undo and Redo take it back and forth.
- AC3. Updating a finding that is already drafted offers to update the
  draft, as one undoable edit. Otherwise the reviewer is told the draft has
  the old text.
- AC4. A finding on lines outside the diff is never drafted as a `suggestion`
  GitHub must refuse. After a refusal, the advice given can succeed.
- AC5. Every side chat question, proposal shown, Use, Discard, Update the
  draft too, Undo, Redo and refusal is a Standard diagnostic event with its
  kind, finding key, round, agent, duration, outcome and lengths, and no
  content; every refused publish is one too. At Verbose the events carry
  the question, answer, proposed text, finding title and path and lines.
- AC7. No anonymized export, at any detail, holds a side chat's content or
  its stored rounds: the marker test above passes with Verbose on.
- AC6. The tests above, BPR-2322's `qt_item_side_chat` and
  `application_follow_up_public_path`, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Out of scope

- Whether the agent should report findings outside the diff at all. It was
  right to: the pull request missed a site.
- Seen in the same session, to be confirmed with a Verbose log and carded
  separately:
  - A comparison's historical acquisition timed out after 61 s
    (`mode=complete_closure source=host linked_clone=0`), although the
    linked clone held both commits.
  - Next and previous comment misbehave once they reach the draft.
    Double-clicking the draft in Threads opens the file but not the comment.
    Vertical scroll gestures over the drawer reach the agent pane.
- Reconciliation (2026-11-20): the owner's draft named the follow-up
  window, which [[BPR-2322]] deleted. Changes 1 to 3 now land in the item
  side chat; Changes 4 and 5 and these out-of-scope items are as the owner
  wrote them.
