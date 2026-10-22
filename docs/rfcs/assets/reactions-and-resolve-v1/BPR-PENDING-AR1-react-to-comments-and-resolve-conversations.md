---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AR1"
title: "React to comments and resolve conversations as GitHub's web interface does"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"kind":"feature","origin":"Owner, 2026-10-22, relayed by the coordinator","owner_note":"we're missing the ability to react to a comment with an emoji from GitHub's web interface and also missing the resolve conversation","draft_rfc":"docs/rfcs/assets/reactions-and-resolve-v1/RFC-DRAFT-0027-comment-reactions-and-resolve-conversation.md","draft_rfc_revision":"r1","frontend_design":"frontend-design:frontend-design loaded before the Qt surfaces","related_cards":["BPR-0657","BPR-0879","BPR-1368","BPR-1290","BPR-1471","BPR-1589"],"related_rfcs":["RFC-BPR-0008","RFC-BPR-0015","RFC-BPR-0028"],"note":"Provisional ID; sits beside its draft because the board refuses provisional IDs."}
---

# BPR-PENDING-AR1: React to comments and resolve conversations

## Owner request

The owner, 2026-10-22, verbatim:

> we're missing the ability to react to a comment with an emoji from
> GitHub's web interface and also missing the resolve conversation

## Phase 1 finding

Reactions are not read, shown or written anywhere. Resolve and Unresolve
were built by [[BPR-0657]] but the composition hides them on the GitHub CLI
connection and the CLI transport refuses their GraphQL documents; on a
native connection they are an unlabelled header glyph and a menu item.
GitHub's per-thread `viewerCanResolve` / `viewerCanUnresolve` are read and
unused. The design follows GitHub's web interface, so it is built without an
owner question; the choices are recorded as D1 to D6 on
[RFC-DRAFT-0027](RFC-DRAFT-0027-comment-reactions-and-resolve-conversation.md)
for the owner to amend.

## Plan

1. Core: reaction contents, summaries, the decisions (RA03), the pending
   overlay and its projection; the resolution decision takes GitHub's
   per-thread permission.
2. GitHub adapter, read: the `ListReactions` read after the snapshot
   (RA01), admitted on native HTTPS and the CLI connection, refused signed
   out; the snapshot carries the reactions.
3. GitHub adapter, write: reaction subject observation and add/remove
   documents; resolve and reactions admitted on `gh api graphql` (RA02); the
   session observes first, skips a write already in state and confirms an
   ambiguous outcome by one read-back (RA04).
4. Application: the port, coordinator and host carry a reaction under a
   one-shot gesture grant, as a resolution does.
5. Qt and composition: chips, picker, Resolve conversation, permission
   gating, immediate counts (RA05, RA06); the fixture answers reactions and
   records changes; a composed course (RA07).
6. Live course on `MikeMarcin/BPR-Test` (RA07) and the owner's look on a
   frozen build (RA08, attended).

## Acceptance

RA01 to RA08 of RFC-DRAFT-0027.
