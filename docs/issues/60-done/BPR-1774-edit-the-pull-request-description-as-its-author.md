---
schema: "implement-rfc.card.v1"
id: "BPR-1774"
title: "Edit the pull request's description (summary) as its author"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1774--BPR-1775--77f10df92a48.json","kind":"feature","found_by":"owner chat, 2026-10-26","related_cards":["BPR-1775","BPR-1741"],"frontend_design":"frontend-design:frontend-design loaded before the Qt surface"}
acceptance: [{"criterion": "DE01 who sees Edit", "outcome": "pass", "revision": "15dd21d45", "evidence": {"kind": "command", "command": "ctest -R '^(core_description_edit|qt_pull_request_overview_page)$'", "detail": "Owner decision (coordinator, 2026-10-26) replaces the draft's viewerCanUpdate offer: Edit goes to the pull request's author. core_description_edit: description_edit_offered for the author, case-insensitive logins, refused for another account, an anonymous read and an unknown author. qt_pull_request_overview_page test_the_description_is_edited_in_place: no Edit until offered, Edit in the reader's corner beside Full view, withdrawn Edit gone, Add a description for an empty body, an invalidated page withdraws it. Live: MikeMarcinTest is not offered Edit."}}, {"criterion": "DE02 core decisions", "outcome": "pass", "revision": "15dd21d45", "evidence": {"kind": "command", "command": "ctest -R '^core_description_edit$'", "detail": "decide_description_edit in order not_author, unchanged (bytes, so a CRLF to LF change is a change), too_long (65,536 code points kept, one more refused, three-byte and four-byte characters counted as one); decide_description_save: already_saved, not_permitted before conflict, conflict, write. No Qt."}}, {"criterion": "DE03 fixed GraphQL documents, both connections", "outcome": "pass", "revision": "15dd21d45", "evidence": {"kind": "command", "command": "ctest -R '^(github_mutation_contract|gh_cli_transport|github_read_contract)$'", "detail": "test_the_description_goes_by_graphql_exactly: ObserveDescription and UpdateDescription compile to exact bodies naming the accepted read's pull request node, revalidate canonically, refuse a read passed off as the write, a body changed in flight and another pull request node; unbound, foreign, nodeless, other-actor and non-UTF-8 requests refused; 65,536 characters fit one write, a body past the 255 KiB request ceiling is refused; GHES 3.18 at /api/graphql; answers held to the designated pull request, the write's answer must carry the body asked, a GraphQL refusal is not a success, an answer with the longest body is read. Every operation row and request-shape pin covered on both servers. gh_cli_transport: both documents (and a 65,536-character write) launch gh api graphql with the exact body on stdin and the write's own stdin ceiling; a body naming another pull request never launches; a lost write stays possibly sent. UpdateDescription is reserved as a mutation name in the read contract."}}, {"criterion": "DE04 exactly one write, conflict never overwritten", "outcome": "pass", "revision": "15dd21d45", "evidence": {"kind": "command", "command": "ctest -R '^(live_review_mutation_provider|application_review_mutation_application_host)$'", "detail": "test_a_description_is_observed_then_written_once: observed, then written once with GitHub's answer; a changed body is a conflict with no write and the newer body back; a body GitHub already has is not written again; viewerCanUpdate false is refused_by_server with no write; another pull request in the read or the write's answer is undesignated_target, never a second write; a too-long body is refused after the read; a lost answer is settled by one read back (confirmed or not_confirmed) with exactly one write; a cancelled write is not read back; the coordinator confirms the target first. application host: refused before any effect without the capability, for an unchanged or too-long body, another session's grant or a publication's one-shot grant; an admitted edit is queued, reaches the provider once bound to target and actor, and its terminal carries GitHub's answer; shutdown stops a queued edit."}}, {"criterion": "DE05 shipped composition course", "outcome": "pass", "revision": "15dd21d45", "evidence": {"kind": "command", "command": "ctest -R '^(application_description_edit_contract|application_threaded_reply_contract|application_review_round_trip_composition_contract)$'", "detail": "application_description_edit_contract runs --threaded-reply-probe --description-edit-probe on github-com and ghes-3.18 fixtures: DESCRIPTION_EDIT_EVIDENCE:11111 (Edit offered in Overview, Save performed exactly one write (provider edits=1 writes=1), the next read shows GitHub's body, a body another client changed meanwhile is refused with nothing written (edits=2 writes=1) and the typed text kept, Edit GitHub's newer text loads it). Threaded-reply and round-trip composition contracts pass; the latter's application digests are resealed."}}, {"criterion": "DE06 live course", "outcome": "pass", "revision": "15dd21d45", "evidence": {"kind": "command", "command": "python3 tests/scripts/description_live_course.py --tool BUILD/tests/github_contract/reaction_live_course --work DIR", "detail": "MikeMarcin/BPR-Test #137 (opened by the owner with a CRLF body, closed, branch deleted), 2026-10-27: the shipped review_mutation_session::edit_description over gh api: description-saved writes=1, description-saved-again writes=0 (GitHub already had it byte for byte), description-conflict (stale original) conflict=1 writes=0, description-restored writes=1, body read back equal to the original byte for byte. MikeMarcinTest (GH_TOKEN in the child only): offered=0, the session asked anyway was refused_by_server (viewerCanUpdate false) with writes=0, body unchanged. 11 tool requests; REST X-RateLimit-Remaining 4999 to 4989. The live carrier is gh api with the product's compiled bytes; the product's native HTTPS connection was not run live (contract and fixture evidence only)."}}, {"criterion": "DE07 owner look", "outcome": "pending", "revision": "15dd21d45", "evidence": {"kind": "observation", "detail": "Deferred by the owner; not a blocker. Offscreen light renders of the page (Edit offered, editor open, conflict) are in the session scratchpad of 2026-10-27; no attended-review session was opened."}}, {"criterion": "DE07", "outcome": "pass", "revision": "6f9f302c17f814bce3c77588586504fb1ddd18a1", "evidence": {"kind": "observation", "detail": "Owner in chat, 2026-10-27, after the attended-review rejection, verbatim: \"Edit doesn't need the label the pencil and tooltip are sufficient I just didn't see it and the cursor through me off\". The reader's caret is removed (cursorWidth 0) and Edit stays the pencil with its tooltip; qt_pull_request_overview_page asserts both."}}]
---

# BPR-1774: Edit the pull request's description (summary) as its author

## Request

The owner, 2026-10-26, verbatim:

> We seem to be missing author/uploader controls for editing the summary and
> we seem to be missing controls for editing comments we have left. Add
> cards for both of those and add them to your work queue

This card is the first half: the summary. [[BPR-1775]] is the comments.

## Grounded current behavior (to confirm when the card starts)

- The description is read and rendered (Overview), but no surface offers to
  change it. A search of `src/` finds no `updatePullRequest` mutation and no
  description editor; the mutation catalog's update operations are for
  pending review comments ([[BPR-1368]]) and tour staging only.
- GitHub lets the author and anyone with write access edit the title and
  body (`updatePullRequest`, GraphQL, or `PATCH /repos/{o}/{r}/pulls/{n}`).
  The read model would need the viewer's permission to show the control only
  to them (`viewerCanUpdate`).

## Outcome

- Whoever GitHub lets edit it sees **Edit** on the description in Overview;
  others see nothing new.
- Editing opens the existing Markdown composer on the description's raw
  body, with Save and Cancel. Nothing is sent until Save.
- Save sends one fixed mutation through the connection in use (native and the
  GitHub CLI connection), and the description shows GitHub's answer through
  ordinary synchronization. A body changed on GitHub since editing began is
  not overwritten silently: the editor says so and offers to keep editing on
  the newer text.
- Refusals say why in words (permission, connection, conflict).
- Out of scope unless the owner asks: the title (consider beside this),
  labels, reviewers.

## Acceptance (draft)

- The control is offered exactly when GitHub grants the viewer the edit.
- Save performs exactly one update on fixtures for github.com and GHES 3.18,
  and a live course on MikeMarcin/BPR-Test.
- A concurrent change is detected and not overwritten.
- The look is accepted by the owner on a frozen build (attended review).

## Decisions (coordinator, 2026-10-26, not reopened)

- Edit is offered to the pull request's author (the snapshot's
  `PullRequest.Author` login equals the signed-in account), not by a
  `viewerCanUpdate` read. Save first asks GitHub through `ObserveDescription`
  (node by the pull request's node: number, repository, body,
  `viewerCanUpdate`, `updatedAt`), refuses in words when GitHub says no, and
  refuses as a conflict when GitHub's body is no longer the one editing began
  with. The title stays out of scope.

## Built (2026-10-27)

- Core: `bpr/core/description_edit.hpp` (`description_edit_offered`,
  `decide_description_edit`, `decide_description_save`).
- GitHub adapter: `ObserveDescription` and `UpdateDescription`
  (`updatePullRequest`, body only), bound to the accepted target and actor;
  `review_mutation_session::edit_description` observes, lets the core decide,
  writes once, and settles a lost answer with one read back. The GitHub CLI
  connection admits both documents through `gh api graphql`.
- Application: `review_description_request`/`receipt`, the port's
  `edit_description`, the coordinator, and the host's `edit_description`
  under a one-shot `issue_description` grant.
- Qt: Overview's Edit description (reader corner) and Add a description; the
  app's Markdown prose editor (`scoped_prose_editor`) in the reader's place,
  Cancel and Save, words beside it, Edit GitHub's newer text on a conflict.
- Composition: offer, core gate, host write, refresh after success.
- Docs: product contract "Editing the description"; provider contract.

## Known gaps

- `rfc0016_test_inventory` (`gh_cli_git_smart_transport` command absent) and
  `command_correctness_registry_contract` (workbench case inventory stale for
  `workbench_window_tests.cpp`) fail in this worktree on files this card did
  not touch.
- The core-only preset was not configured here (disk); the core file is plain
  C++23 with no new dependency.

## Owner's look, 2026-10-27: rejected

> I can't tell but it looks like you can just edit always? Edit should
> require an explicit action to enter the mode.

Editing already needs the pencil, but the read view shows a text caret and the
pencil has no label, so it reads as editable. Rework: the read view shows no
caret (selectable, not editable-looking), and the action is a labelled
**Edit** button; then a fresh look.

Owner, in chat, 2026-10-27: "Edit doesn't need the label the pencil and
tooltip are sufficient I just didn't see it and the cursor through me off".
So Edit stays the pencil with its tooltip, and the reader shows no caret.
