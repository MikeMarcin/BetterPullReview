---
schema: "implement-rfc.card.v1"
id: "BPR-1374"
title: "An unsent reply draft survives Refresh and restart, and a vanished thread reports it rather than dropping it"
status: "Parked"
rfcs: []
depends_on: []
parked_on: "human-review"
priority: "P2"
platforms: []
metadata: {"found_by":"BPR-0143's live mixed-client course on BPR-Test#47, shipped (tests-off) composition, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1374--BPR-1374--0119abb53d8c.json","related_cards":["BPR-0143","BPR-0138","BPR-1290","BPR-1212","BPR-1133"]}
---

# BPR-1374: an unsent reply draft survives Refresh and restart

## Observation

[[BPR-0143]]'s live course ran the shipped composition offscreen (Release,
tests off, `ec16c550b`), signed in through the GitHub CLI, on BPR-Test#47.
The probe source is on local branch `claude/bpr-0143-live-probe`
(`36a093af2`). It writes a reply as a reviewer does: select the thread's
line, press Comment, press the open card's **Reply** (`threadReplyField`),
then type into the in-place reply editor. The review editor then holds a
draft with `ReplyToThread` set to the thread and the typed body.

- **Refresh drops the draft.** After a public **Refresh**, the review
  editor's `shown_draft()` is empty, even when nothing changed on GitHub.
  The in-place reply field still shows the typed text.
- **Restart drops the draft.** After a quit and a start on the same profile
  root, no reply draft is presented: not on the thread's line, not on the
  line it was written from, and not as a draft card. This holds whether or
  not a Refresh ran first.
- **A thread deleted elsewhere drops the draft silently.** When another
  client deletes the thread's root, Refresh removes the thread, as server
  truth requires. The draft goes with it, with no card, notice or other
  indication.

GitHub is never overwritten and nothing is published, so server truth holds.
But [[BPR-0138]]'s rule is that a held edit the server has moved past is
reported as conflicted so a person can decide, and is never discarded
quietly. The shipped application discards an unsent reply quietly.

## Outcome

Find where the reply draft is lost. Candidates: the accepted handoff
resetting `ReviewDraft` on every Refresh; a reply draft never reaching the
shared draft store; or a stored reply draft never re-presented. A reviewer's
unsent reply should survive a Refresh and a restart. When its thread no
longer exists on GitHub, the reply should be kept and shown as unable to be
sent there, until the reviewer discards it or copies it.

## Direct acceptance

- AC1: a public-path test (fixture provider) writes a reply draft, presses
  Refresh with no server change, and finds the same draft in the review
  editor. It fails before the fix.
- AC2: the same test quits and restarts on the same root, and the reply draft
  is presented again on its thread.
- AC3: a staged server deletion of the thread's root leaves the draft
  presented as conflicted (named, not published, discardable). Nothing is
  sent to GitHub.
- AC4: BPR-0143's probe re-runs the three bullets above live on a disposable
  BPR-Test pull request in the shipped composition, and the result is
  recorded on BPR-0143.

## Cause

The reply was never lost from storage: the private review store kept it,
with its thread and words, through Refresh and restart. It was lost from
view, in three places.

- **Refresh.** Every accepted handoff runs `compose_review_mutation`, which
  drops the editor's draft and selection. A same-head Refresh then shows the
  tab again with its selection already on screen ([[BPR-1303]]), so no line
  is chosen again and the editor stays empty (`editor_draft: none`).
- **Restart.** Saved drafts are drawn as notes ([[BPR-1326]]), but a reply
  drew nothing, and choosing its line opens only a new comment. The thread's
  Reply then converted that comment into a second, empty reply.
- **Thread deleted.** A reply only ever showed inside its thread, so when
  the thread left, nothing showed it.

None of the earlier draft cards regressed. BPR-1326 drew every saved draft
except replies, BPR-1303 restored the tab but not the editor's draft, and
BPR-1345's pending drafts are never replies.

## Fix

- **Core** (`bpr/core/reply_draft_standing.hpp`): where an unsent reply
  stands against the accepted threads. It is `answerable`,
  `thread_resolved` (still sendable), `thread_deleted` (never sent; it can
  be kept as a comment) or `thread_unknown` (the threads were not read, so
  nothing is called deleted). The core also decides which saved draft a
  line or a thread's Reply opens (`reopen_draft_at`), and which reply the
  editor holds again after a Refresh (`resume_reply_after_refresh`). It
  builds the Keep as comment and Discard commands.
- **Host**: `review_composition_application_host::select_draft` opens
  drafts by that rule over the private store, for both the line and Reply.
  `application::observe_reply_threads` maps the snapshot.
- **Composition**: the reply the editor held is held again on its line
  after a Refresh. Reply opens a saved reply. The standings reach the window
  with the saved drafts and again with each conversation publication.
  Publish refuses a reply whose thread is gone.
- **Qt** (frontend-design loaded):
  - A thread holding my unsent reply says "Your reply" in the writing blue.
    A resolved thread also says "The thread this replies to was resolved on
    GitHub."
  - A reply whose thread is gone is a dashed "Your reply · Not sent" note at
    its line, saying "The thread this replies to was deleted on GitHub."
  - Opened, that note offers Keep as comment, Copy text and Discard. Discard
    asks first.

GitHub is only read; nothing is sent, and no thread is recreated.

## Evidence

macOS arm64 Debug, `build/dev`, in a worktree off `origin/main`
`7b75156f7`. Every run was offscreen and network-isolated. Each "control"
row is a temporary switch, since removed, that restores the old rule.

| Level | Check | Candidate (first at `150256f35`) | Old rule restored (control) |
|---|---|---|---|
| Core | `core_reply_draft_standing` | passed | all 5 scenarios fail (17 expectations) |
| Host | `application_reply_draft_survival` (real SQLite store; Refresh = new composition, restart = store reopened) | passed | all 5 scenarios fail (16 expectations) |
| End to end | `application_reply_draft_contract`: `refresh`, `edited`, `write`+`restart`, `deleted`, `kept`, `resolved` | `REPLY_DRAFT_EVIDENCE:1111111` for each | each fails, `1110000` (`restart`: `1100000`), `editor=none` as the live course saw; `write` alone passes |
| Stability | `kept` ×10 | 10 of 10 | — |

**Neighbouring rows.** 15 of 15 passed serially:

- `architecture_boundaries`
- `core_revisioned_review_draft`
- `application_shared_draft_application_host`
- `application_comment_range_draft_store`
- `qt_conversation_cards_view`
- `qt_review_draft_editor`
- `application_review_round_trip_composition_contract` (resealed; the
  mutation pin and the app manifest are unchanged)
- `application_review_behaviour_contract`
- `application_threaded_reply_contract`
- `application_label_authoring_contract`
- `application_published_file_switch_contract`
- `application_start_review_contract`
- `core_reply_draft_standing`, `application_reply_draft_survival` and
  `application_reply_draft_contract`

`coding-format-check` passed (clang-format 22.1.8).

Rebased onto `origin/main` `6064fd94c`, the code commit is `071fd2d66`, with
the pins resealed over BPR-1345's. On it, 9 of 9 rows passed serially:
`architecture_boundaries`, `core_reply_draft_standing`,
`application_reply_draft_survival`,
`application_review_round_trip_composition_contract`,
`application_reply_draft_contract`, `application_threaded_reply_contract`,
`application_label_authoring_contract`,
`application_published_file_switch_contract` and
`application_start_review_contract`.

`test_network_isolation_manifest` stops at `gh_cli_git_smart_transport`.
This partial build has not built that test's executable, so it has no
command. Neither new row is involved.

## Owner look (parked on human-review)

Renders from the course, light and dark, are in
[bpr-1374-reply-flag-v001](../../verification/reviews/bpr-1374-reply-flag-v001/):

- `refreshed-deleted`: the kept note.
- `kept-open`: the kept note opened, with its actions.
- `refreshed-resolved`: the resolved thread with the reply in place.

**Question (Accept or Reject; put any change in the note):** a reply whose
thread was deleted on GitHub is kept as a dashed "Your reply · Not sent"
note at its line. The note says "The thread this replies to was deleted on
GitHub." and, opened, offers Keep as comment, Copy text and Discard. Keep
this presentation?

The code is on `main`; this look does not block it. No frozen build was
made: it was not cheap, with 8 GiB free.

## Left open

- **AC4, the live re-run on BPR-Test.** Skipped on the coordinator's
  wrap-up instruction. BPR-0143's two draft rows still await it.
- **A narrow race.** A Keep as comment pressed during the brief ledger
  restore that follows a Refresh can be dropped without a word. The press
  works when tried again. The course waits for the window to settle; the
  product does not yet say so.
- **Drafts on an older head.** Drafts from a head that has since moved
  (any draft, not only replies) are restored by neither the old code nor
  this change.
- **Platforms.** Windows and Linux did not run these rows. The two new rows
  are registered Windows-uncontained by shape, not measured.
