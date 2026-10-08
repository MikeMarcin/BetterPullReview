---
schema: "implement-rfc.card.v1"
id: "BPR-1329"
title: "Publishing an inline draft after the GitHub CLI confirmation shows nothing"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"owner report on the private macOS 0.1.5 prerelease, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1329--BPR-1329--82d67ecdafa9.json","related_cards":["BPR-1326","BPR-1212","BPR-1290","BPR-0797"]}
---

# Publishing an inline draft after the GitHub CLI confirmation shows nothing

Owner, 2026-10-08, in chat:

> Or perhaps it did try to publish the comment and just failed to do so. It
> popped up a warning about publishing with my gh account and I allowed it but
> nothing seemed to happen

Context: on the private macOS 0.1.5 prerelease the owner published an inline
draft comment from the Comments drawer over a GitHub CLI session. Before that,
the draft had not shown in the drawer until they clicked its line; that is
[[BPR-1326]], fixed separately and not touched here. After the per-publication
"Confirm GitHub CLI publication" question, answered Yes, the owner could not
tell whether the comment was published.

## What the owner's case shows

- **Local diagnostics.** No 0.1.5 installation and no diagnostic log from
  2026-10-08 exist on the machine this card was worked on. The default data
  root's newest session log is from 2026-10-04 and the profiles' newest are
  from 2026-09-27, so the run happened on another machine and its log was not
  read. A publication attempt writes one `create_review_comment` diagnostic
  event (operation and outcome, no body or token); a Settings > Diagnostics
  export from that machine would show whether the POST was made and how it
  ended.
- **BPR-Test.** Read-only `gh api` on `MikeMarcin/BPR-Test` shows one review
  comment on 2026-10-08, on #41 at 18:20:01Z, and that is the BPR-1212 agent
  probe. No pull request there received a comment from the owner that day. If
  the owner published to BPR-Test, it did not post; no other repository was
  read.

So whether the owner's comment posted is not known from here. What is known is
that, from the drawer, nothing the publication came to could have been seen.

## Cause

The drawer's Publish runs the draft editor's own Publish. Everything that
Publish came to was written only to the draft editor's status line, which sits
on the inline composition page the drawer does not show:

- while the request was in flight the drawer said nothing of it; since
  [[BPR-1212]] (compose in the drawer) it said "No editable draft is
  selected. Return to an open pull request and select a current-head line.",
  the disabled Publish's tooltip, because a publishing draft is not editable;
- a failure or an uncertain result ("Publication result is unavailable; retry
  is safe.", a server refusal, a moved head, an incompatible body) stayed on
  the hidden page, and the editor's Retry was not offered in the drawer;
- some refusals said nothing anywhere: Cancel on the confirmation, the
  session or target changing while it was open, a draft still being saved,
  and a draft that changed before it was sent.

On success the drawer already replaces the draft with the posted thread, open
at the new post (`select_published_conversation`). Over gh that takes the
provider's observations, the POST, its reconciliation and a full pull request
refresh, each a separate `gh` process; with no word in the drawer meanwhile,
an in-flight success also looks like nothing.

## Scope

- `review_draft_editor` keeps what the last Publish of its draft came to
  apart from its other status messages (`publication_status`,
  `publication_retry_offered`, `present_publication_started`,
  `present_publication_refusal`) and tells its presentation observer.
- The workbench window gives that to the draft written in the drawer as its
  notice (`threadDraftStatus`, under Publish), ahead of the disabled
  Publish's reason, and tells it when the editor offers a retry: Publish then
  reads Retry and stays enabled although the draft is no longer editable.
- The composition says Publishing once the mutation host has taken the
  request, and names a reason for every Publish it stops before sending,
  including the confirmation's Cancel and a session that changed while it was
  asked. The confirmation's gate and question are unchanged.
- Fixture-only accessors hold and release the fixture's review-mutation
  decision gate, so a course can see the drawer while a publication is in
  flight.
- The round-trip composition pins are resealed for `github_application.cpp`
  and the fixture-access header.

Out of scope: [[BPR-1326]] (saved drafts in the drawer); replies written in
place, whose publication status has the same gap; the live gh mutation
provider, which no fixture course reaches.

Note for the owner of [[BPR-1326]]'s saved-draft notes: a note for another
line's draft (`draft:unpublished:<id>`) opens through `edit_draft`, and its
Publish, where one is shown, runs the editor's Publish, which is the selected
line's draft. This card does not change that.

## Acceptance

- Over a GitHub CLI session, publishing a new comment from the drawer asks
  the confirmation once; while the request is in flight the drawer says the
  comment is publishing and does not offer Publish again; on success the
  posted thread replaces the draft and is the one open in the drawer.
- A lost result leaves the draft saying why in words, with Retry, also after
  it is closed and opened again; Retry asks the confirmation again and the
  drawer then shows exactly one posted thread.
- Cancel on the confirmation sends nothing; the draft says so and still
  offers Publish.
- Each of the three courses fails on the source before the fix.
- Focused regression selectors, the composition contracts and the
  documentation gate pass.
