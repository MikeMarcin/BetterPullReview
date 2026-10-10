---
schema: "implement-rfc.card.v1"
id: "BPR-1374"
title: "An unsent reply draft survives Refresh and restart, and a vanished thread reports it rather than dropping it"
status: "Ready"
rfcs: []
depends_on: []
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
