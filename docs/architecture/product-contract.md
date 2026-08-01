# BetterPullReview Product Contract

## Definition

BetterPullReview is an alternative native client for reviewing code contained in GitHub pull requests. It is not a replacement pull-request server, a second collaborative review database, a merge client, or an IDE.

## Authority model

| State class | Authority | Examples |
|---|---|---|
| Shared collaborative state | GitHub pull request | Commits, files, comments, replies, reviews, approvals, requested changes, thread resolution, checks |
| Private retained revision history | Local BetterPullReview Git cache and epoch catalog | Exact base/head objects that BetterPullReview observed, epoch identity and cause, availability, and current-viewer review associations |
| Private workflow state | Local BetterPullReview ledger | Read receipts, acceptance receipts, observer rules, private notes, local defect classification, AI drafts |
| Derived state | Recomputed from GitHub plus private retained/local state | Selected retained comparison, changed since my review, unread conversation, needs verification, next actionable item |

When local and GitHub state disagree about a shared fact, GitHub wins. A
retained epoch is an immutable private record of a revision BetterPullReview
actually observed, not a claim that the revision is still GitHub's current
head. A force-pushed observed head may therefore remain comparable, while
history that was never observed is never invented.

## Application architecture invariant

BetterPullReview is implemented as a deterministic functional core inside an imperative shell containing an application host and a Qt presentation adapter. The governing rule is **The core decides. The host performs. Qt presents.**

- The core owns compact interaction state, semantic policy, transitions, request identity, and stale-result decisions.
- The host owns GitHub, Git, SQLite, AI processes, clocks, jobs, caches, persistence, cancellation, and lifecycle.
- The Qt adapter owns widgets, actions, models, focus, layout, accessibility, themes, and event translation.
- GitHub, the private retained-revision catalog, and the local workflow ledger remain authorities for their respective state classes; neither a widget nor raw cache topology becomes a competing authority.
- Effects and asynchronous completions cross the boundary as typed requests and events, while Qt receives incremental projections and semantic deltas.

## Interoperability invariants

- Published actions use normal GitHub API primitives under the acting user's identity.
- A GitHub-only collaborator can understand and respond to every published action.
- No BetterPullReview marker, bot, label, status check, hidden HTML metadata, or custom syntax is required.
- GitHub IDs are canonical identities for published comments, threads, and reviews.
- BetterPullReview must tolerate comments, edits, deletions, replies, resolutions, and reviews created by other clients.
- Uninstalling BetterPullReview may lose private workflow state but cannot lose or corrupt shared review history.

## Explicit non-goals

- Creating, editing, merging, closing, or rebasing pull requests.
- Replacing GitHub notifications for all repository activity.
- Running builds, tests, repository scripts, hooks, filters, or arbitrary code.
- Maintaining a hosted BetterPullReview account or synchronization service.
- Requiring repository administrators to install a GitHub App.
- Automatically publishing AI output or automatically changing review disposition.

## Architectural acceptance tests

1. **Uninstall test:** the review remains fully usable in GitHub after BetterPullReview is removed.
2. **GitHub-only collaborator test:** a non-user can understand and continue every published thread.
3. **Mixed-client test:** a thread can be created, replied to, resolved, reopened, and edited across multiple clients.
4. **Database-loss test:** deleting the local database loses no shared review content.
5. **Identity test:** published actions appear under the actual user identity.
6. **Server-truth test:** refresh corrects local shared-state disagreement in favor of GitHub.
7. **No-adoption test:** BetterPullReview remains valuable when only one participant uses it.
