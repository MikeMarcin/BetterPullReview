---
id: "RFC-BPR-0091"
title: "React to comments and resolve conversations as GitHub's web interface does"
status: "Auto-Verified"
area: "BPR"
depends_on: ["RFC-BPR-0008", "RFC-BPR-0015"]
metadata: {"origin": {"date": "2026-10-22", "channel": "owner chat, relayed by the coordinator"}, "card": "BPR-1741", "revision": "r2 2026-10-22, owner look accepted", "draft": "docs/rfcs/assets/reactions-and-resolve-v1/RFC-DRAFT-0027-comment-reactions-and-resolve-conversation.md", "import": "docs/rfcs/assets/reactions-and-resolve-v1.LIVE_IMPORT.md", "authoring": "author-rfc; frontend-design:frontend-design loaded for the Qt surfaces", "closeout": "BPR-1741 (2026-10-22)", "owner_look": "attended-review session 3b59571c0c371e211a07e6a1c88e5711 (closed): AR1-reaction-chips, AR1-reaction-picker and AR1-resolve-conversation accepted"}
---

# React to comments and resolve conversations as GitHub's web interface does

Auto-Verified, revision r2, closed by [[BPR-1741]] on 2026-10-22.
Adopted from
[RFC-DRAFT-0027](assets/reactions-and-resolve-v1/RFC-DRAFT-0027-comment-reactions-and-resolve-conversation.md)
([import record](assets/reactions-and-resolve-v1.LIVE_IMPORT.md)). The draft
was built as written; r2 records the owner's look and closes the RFC.
Every criterion passes on the evidence the card records: fixture rows and the
composed course through the shipped composition on github-com and ghes-3.18,
the live course on MikeMarcin/BPR-Test #132 with a second identity, and the
owner's Accept of all three looks on a frozen build. No owner question remains
open. Windows and Linux native runs are not claimed.

## Problem and grounded current behavior

The owner, 2026-10-22, verbatim:

> we're missing the ability to react to a comment with an emoji from
> GitHub's web interface and also missing the resolve conversation

Observed at `92162efed`:

- **Reactions are not read, shown or written anywhere.** No read asks for
  `reactionGroups`, the read model (`github_read_model.hpp`) has no reaction
  field, and the mutation catalog (`review_mutation_operation`,
  `github_mutation_contract.hpp`) has no reaction operation.
- **Resolve exists but is hidden on the GitHub CLI connection.** [[BPR-0657]]
  built `core::decide_thread_resolution`, the `resolve_thread` port, host and
  GraphQL documents (`ResolveThread` / `UnresolveThread`), the open card's
  header glyph `threadResolve` and the card menu's Resolve item. The
  composition offers it only when `!cliProfile`
  (`github_application.cpp`, `set_thread_resolution_provider`), refuses a
  gesture on that connection with "Thread resolution is unavailable through
  the GitHub CLI profile.", and `gh_cli_transport::perform_review_mutation`
  refuses `resolve_review_thread`, `unresolve_review_thread` and
  `observe_review_thread`. The only GraphQL write `gh api graphql` admits is
  [[BPR-1368]]'s pending-comment edit. On a native connection Resolve is an
  unlabelled header glyph and a context-menu item; there is no labelled
  "Resolve conversation" where GitHub puts it.
- **The per-thread permission is read but unused.** Reads keep
  `viewerCanResolve` / `viewerCanUnresolve` as
  `ViewerResolvePermission` / `ViewerUnresolvePermission`; nothing offers or
  refuses Resolve on them.
- **Resolved threads already collapse.** A resolved closed card is one line
  with the green check ([[BPR-1471]]), and Hide resolved filters them
  ([[BPR-1290]]). This RFC keeps that.

GitHub's interface, which the owner names as the reference: every pull
request comment, review comment and review body with text shows its
non-zero reactions under the body as emoji-and-count chips in a fixed order
(👍 👎 😄 🎉 😕 ❤️ 🚀 👀), a chip I reacted with is tinted, its tooltip names
who reacted, a click on a chip toggles mine, and a smiley button opens the
eight-emoji picker. A review thread ends with "Resolve conversation"
(or "Unresolve conversation"), shown only to someone GitHub lets resolve it.

## Proposed behavior and boundaries

### GitHub surface

- **Reactions** are GraphQL for all three comment kinds: `addReaction` /
  `removeReaction` (`subjectId`, `content`), with `ReactionContent` values
  `THUMBS_UP THUMBS_DOWN LAUGH HOORAY CONFUSED HEART ROCKET EYES`, and
  `Reactable { viewerCanReact reactionGroups { content viewerHasReacted
  reactors(first:) { totalCount nodes { login } } } }`. REST has
  `POST/DELETE /repos/{o}/{r}/pulls/comments/{id}/reactions` and
  `/issues/comments/{id}/reactions` but no review-body endpoint and no
  viewer state on the REST rollup, so REST is not used.
- **Resolve** is GraphQL only (`resolveReviewThread` / `unresolveReviewThread`)
  and stays on [[BPR-0657]]'s documents.
- **GHES.** Every field and mutation above is in the GHES GraphQL schema the
  GHES contract (3.18 and later, which includes 3.20) names; the GHES root is
  `/api/graphql`. A server whose schema refuses the reaction read reports
  reactions unavailable for that read; nothing is approximated.
- **Transports.** Native HTTPS sends both to the GraphQL root as it does
  replies. The GitHub CLI connection sends them through `gh api graphql`, as
  [[BPR-1368]] does: the catalog it admits gains exactly the fixed documents
  named here (thread observation, resolve, unresolve, reaction subject
  observation, add reaction, remove reaction), each compared for exact
  equality with its compiled body. This widens the CLI write catalog; it is
  the owner's request (Resolve is missing on that connection) and adds no
  other GraphQL write.

### Reading reactions

After the pull request's snapshot, an authenticated read asks for the
reactions of every review-thread comment, pull request comment and review
with a body, by node identity, in batches of at most 50, at most 400
subjects a read, through one fixed `ListReactions` document bound to the
pull request (`repository.pullRequest.id` must match). Each answered node
must be one of `PullRequestReviewComment`, `IssueComment`,
`PullRequestReview`, carry the asked identity and eight or fewer known
groups. The snapshot keeps per subject: `ViewerCanReact`, and for each
non-zero group its content, count (`totalCount`), whether the viewer reacted,
and up to the first 5 reactor logins. A refused, rate-limited or malformed
reaction read leaves reactions unavailable for that snapshot and fails
nothing else (an authentication failure fails the read as every optional
read does). Public preview asks nothing.

### Writing a reaction or a resolution

Decisions are the core's: a reaction is refused for an anonymous read, for
reactions not observed, when `viewerCanReact` is false, and while a change
to the same subject and content is in flight; a resolution additionally
refuses when GitHub's per-thread permission for the direction asked is
denied (existing refusals unchanged).

Effects go through the existing review mutation host under a one-shot
gesture grant bound to session, generation, target, actor and subject, as a
resolution's does ([[BPR-0657]]). The provider confirms the target, then
observes the subject (it must belong to the accepted pull request) and its
current state; when the state already is the one asked for, nothing is
written. Neither write keeps a recovery record: both are idempotent by state.
When the write's outcome is ambiguous (transport failure after it may have
left, a non-success status, or an unparseable answer) the provider reads the
subject's state back once and reports success only if GitHub shows the asked
state, and otherwise a failure that names it; it never repeats the write.

### Presentation (Qt)

- Under each post body in the open thread and in Discuss: the non-zero
  reaction chips in GitHub's order, emoji then count. A chip I reacted with
  uses the accent tint and border; others are quiet. Tooltip and accessible
  name: "You and octocat reacted with thumbs up", up to the observed logins,
  then "and N others". When I may react, a click (or Space/Enter on the
  focused chip) toggles mine. After the chips, an "Add reaction" smiley
  button opens the picker; with no reactions yet the button sits alone and
  shows only while the post is hovered or focused, like the post's other
  tools.
- The picker is a small popup of the eight emoji in GitHub's order, each a
  checkable button named for its reaction ("Thumbs up, you reacted"); arrow
  keys move, Space/Enter toggles and closes, Esc closes. Light and dark come
  from the palette; sizes scale with the font.
- Counts change on the gesture (mine added or removed, marked pending), then
  take GitHub's answer from the write's own response, and a later read
  replaces both. A refused or failed change restores the observed state and
  names why on the panel's notice line.
- Resolve: the open thread ends with a labelled "Resolve conversation"
  (or "Unresolve conversation") button beside Reply, keyboard-focusable and
  in the tab order; the header glyph and menu item stay. Offered only when
  the session can resolve threads and GitHub's per-thread permission for
  the direction is not denied; a denied permission hides the button and
  disables the menu item with the reason. On the gesture the thread shows
  "Resolving…", on GitHub's answer it shows Resolved (the closed card
  collapses to its one line), and the following read reconciles.

### Decisions (taken under GitHub's convention; accepted by the owner's look)

- D1. The picker lives in each post's own row (chips row and the smiley
  button), not in a card-wide menu.
- D2. Who reacted is in the chip's tooltip and accessible name, up to five
  logins then "and N others"; no separate list.
- D3. Closed cards and the Threads list show no reactions; the open thread
  and Discuss do.
- D4. The pull request description's reactions are out of scope (it is not a
  conversation card here).
- D5. "Resolve conversation" sits at the thread's end beside Reply, like
  GitHub; no new keyboard shortcut is added (Tab reaches it, and the card's
  menu has it).
- D6. Resolved threads collapse as they do today.

## Acceptance

- **RA01:** A recorded `ListReactions` answer for a pull request yields, per
  subject, the viewer permission and the non-zero groups with counts,
  viewer flags and reactors; a wrong pull request, an unknown type or
  content, a foreign identity or an oversized answer is refused, and a
  refused reaction read leaves the rest of the snapshot intact.
- **RA02:** The reaction and resolution documents are fixed and compared for
  exact equality on native HTTPS and the GitHub CLI connection, at
  `/graphql` and `/api/graphql`; any other GraphQL write is still refused on
  the CLI connection.
- **RA03:** The core refuses a reaction for an anonymous read, unobserved
  reactions, `viewerCanReact` false and an in-flight change, and refuses a
  resolution when GitHub denies the viewer that direction; otherwise it
  admits it.
- **RA04:** A write whose outcome is ambiguous is confirmed by one read of
  the subject's state: success only when GitHub shows the asked state, a
  named failure otherwise, and the write is never sent twice. A subject
  already in the asked state is not written.
- **RA05:** In the open thread a reviewer adds and removes a reaction with
  the mouse and with the keyboard alone (chip focus, Space; smiley, picker,
  arrows, Enter, Esc); the count and tint change at once and reconcile to
  GitHub's answer; chips, picker and Resolve conversation render legibly in
  light and dark at 100%, 125% and 200% scale.
- **RA06:** Resolve conversation is offered on both connections when GitHub
  permits it, hidden when GitHub denies it, and a resolved thread collapses
  to its one-line card.
- **RA07:** A composed course through the shipped composition reacts,
  unreacts, resolves and unresolves on a recorded pull request and sees each
  state come back; a live course on `MikeMarcin/BPR-Test` does the same on
  threads it creates, watching `X-RateLimit-Remaining`, and cleans up.
- **RA08:** The owner looks at the chips, picker and Resolve conversation on
  a frozen build and accepts or rejects each.

## Owner look (2026-10-22)

Asked once each, Accept or Reject, in attended-review session
`3b59571c0c371e211a07e6a1c88e5711` (state
`/Users/jaeger/.mask/attended-review/owner-review-20261022-reactions-v001`,
closed), on a frozen tests-on build of `cdea2e551` (removed after the look).
The owner accepted all three with no note: `AR1-reaction-chips` (chips,
counts, mine tinted, who reacted, toggled by mouse and keyboard),
`AR1-reaction-picker` (the eight-reaction picker by mouse and keyboard) and
`AR1-resolve-conversation` (Resolve conversation beside Reply, shown at once,
collapsed when closed). That accepts decisions D1 to D6 as built.
