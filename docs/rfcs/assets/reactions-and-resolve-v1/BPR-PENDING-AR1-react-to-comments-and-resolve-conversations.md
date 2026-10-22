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

## Progress (2026-10-22)

Built on branch `worktree-agent-aab81f7a773163494` over main `3ddfbc5f8`.
Steps 1 to 6 are done except the owner's look (RA08), which waits in a fresh
attended-review session on a frozen build of the branch head.

- **RA01.** `github_paged_read` (`test_reaction_read`: canonical body, binding,
  unknown type and content, foreign identity, five-reactor bound, schema
  without the fields, GHES root, signed out sends nothing) and
  `github_provider` (`test_reactions_are_read_after_the_snapshot`, a refused
  read leaves the snapshot whole).
- **RA02.** `github_mutation_contract` (exact documents, revalidation,
  tampers, GHES root, the recorded BPR-Test #132 answers) and
  `gh_cli_transport` (the six fixed documents go by `gh api graphql` with the
  body on stdin; a changed body and a reply never launch).
- **RA03.** `core_conversation_reactions`, `core_review_round_trip`,
  `core_anonymous_public_read`, `application_review_mutation_application_host`.
- **RA04.** `live_review_mutation_provider`
  (`test_a_reaction_is_observed_then_written_once`,
  `test_an_ambiguous_change_is_confirmed_by_reading_it_back`): one read back,
  never a second write; a state already as asked is not written.
- **RA05, RA06.** `qt_conversation_cards_view` (chips, picker by mouse and
  keyboard, Resolve conversation beside Reply, a denied permission, the accent
  in light and dark); `qt_workbench_window_scale_{100,125,200}` and
  `qt_composed_github_scale_{100,125,200}` pass.
- **RA07.** `application_threaded_reply_contract` now runs `--reaction-probe`
  on github-com and ghes-3.18 (`REACTION_EVIDENCE:11111111`). Live:
  `tests/scripts/reaction_live_course.py` on BPR-Test #132 (closed, branch
  deleted), owner and `MikeMarcinTest`, 28 owner requests, REST
  `X-RateLimit-Remaining` 4980 to 4955.
- **Gates.** clang-format 22 check, clang-tidy on every changed translation
  unit (no finding on a changed line), the documentation gate, the
  round-trip composition and command-registry reseals.
