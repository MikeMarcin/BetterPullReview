# BetterPullReview Product Contract

## Definition

BetterPullReview is an alternative native client for reviewing code contained in GitHub pull requests. It is not a replacement pull-request server, a second collaborative review database, a merge client, or an IDE.

## Authority model

| State class | Authority | Examples |
|---|---|---|
| Shared collaborative state | GitHub pull request | Commits, files, comments, replies, reviews, approvals, requested changes, thread resolution, checks |
| Private retained revision history | Local BetterPullReview Git cache and epoch catalog | Exact base/head objects that BetterPullReview observed, epoch identity and cause, availability, and current-viewer review associations |
| Provider-evidenced update history | GitHub timeline, reconciled in the local epoch catalog | Portable force-push occurrence identity and event time; explicit incomplete or ambiguous history when the read or association lacks evidence |
| Private workflow state | Local BetterPullReview ledger | Read receipts, acceptance receipts, observer rules, private notes, local defect classification, AI drafts |
| Derived state | Recomputed from GitHub plus private retained/local state | Selected retained comparison, changed since my review, unread conversation, needs verification, next actionable item |

When local and GitHub state disagree about a shared fact, GitHub wins. A
retained epoch is an immutable private record of a revision BetterPullReview
actually observed, not a claim that the revision is still GitHub's current
head. A force-pushed observed head may therefore remain comparable, while
history that was never observed is never invented.
An evidenced force-push occurrence can appear in the update line without a
retained comparison. Its provider event and head identify the update across
catalogs; the catalogue-local epoch id and ordinal identify retained material
only. A missing original historical base remains unknown. Until cached
historical material is durably imported, that occurrence remains visible with
comparison unavailable, so the line does not promise that every node can move
a comparison handle.

## Public preview

Public preview is a credential-free reading mode for public GitHub.com pull
requests. macOS enables it; Linux and Windows select typed unavailability. GHES
is refused before an anonymous request is constructed. Activation sends nothing;
opening or refreshing a target performs bounded REST reads and public Git smart
HTTP for exact selected-file comparisons.

Inline REST comments and their reply groups are observable; GraphQL review
threads and resolution, viewer identity, viewed-file state, account-relative
review associations, triage, and publishing are unavailable. An unobserved
conversation is distinct from an observed empty one. No default viewer or
false resolution state substitutes for missing authority. Sign in leaves this
mode through identity invalidation before authenticated state can be published.

The server-reported rate budget is visible, exhaustion is terminal for the
current read, and a 404 says private or missing without guessing which. A
read's pagination can cost more than the nine eligible operation kinds; the
live-candidate diagnostic's nine-request cap is not a production page limit.

## Credential acquisition

A profile is enrolled by one of two paths, and both end in the same platform
credential vault under the same authority key.

- **A personal access token the operator supplies.** RFC-BPR-0002's path, and
  still first-class: it is offered on every profile, including one the device
  grant has already enrolled.
- **The GitHub device grant.** RFC-BPR-0016's path. The reviewer selects it,
  reads a short user code off the screen, and confirms it in a browser they
  already trust. BetterPullReview asks the reviewer to choose no scope, mint no
  token, and move no secret through a clipboard; the requested permissions are
  compiled in and are the minimum the shipped operation catalog justifies.

Successful device sign-in is remembered across orderly quits; manual-token
sessions remain session-only. Tokens stay in the platform vault. Only exact
binding, device-grant provenance, and lifetime metadata persist in the private
state store. Startup validates the stored authority and authenticated account
before restoring a session, and schedules renewal from the remaining lifetime.
Expired or mismatched material requires sign-in. Sign out removes both token
slots and the resumable binding; manual replacement removes the prior grant.

Selecting the device path always produces a typed answer rather than silence.
The reviewer-visible terminals are authorized, denied, expired, disabled,
unavailable, cancelled, and failed ([[BPR-0277]],
`core_device_authorization`). A failed browser handoff is not one of them: the
code and the address stay on screen and the flow completes wherever the
reviewer finishes it ([[BPR-0280]], [[BPR-0516]],
`application_device_authorization_browser_handoff`).

A GitHub Enterprise Server profile answers `device_flow_unavailable` and sends
nothing. An enterprise instance needs an application registered on that
instance, which is an administrator action BetterPullReview does not perform
and does not require; GHES profiles keep the token path. The refusal is
structural — the device routes are admitted only for the GitHub.com server
contract, so no request exists to send ([[BPR-0278]],
`github_device_authorization_contract`). The end-to-end GHES selection is not
separately exercised today and its ledger row stays open under [[BPR-0285]].

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

## GitHub CLI profile milestone

RFC-BPR-0015's amended first read milestone has a qualified macOS fixture
path. Native HTTPS remains the default. The tests-disabled macOS composition
has explicit
CLI read opt-in on the selected GitHub.com or GHES host, with a Connect warning,
observed `/user` identity, pre/post-request account checks and a read-only lease.
It invokes the configured `gh` directly with bounded arguments, input, output
and time; the version preflight currently accepts `gh` 2.x with minor >= 96.
This is a version check, not proof of an official executable's provenance.
The r2 trust-boundary amendment permits a bounded exact-repository `gh` Git
operation for objects; `gh` may internally launch Git and its shell credential
helper. Every macOS `gh` launch must run under OS-enforced file-write denial,
with a narrowly owned temporary object-store exception only for the bounded
clone; an unavailable boundary refuses the CLI profile. This covers `gh`'s
possible first-use global configuration migration, which a post-request check
cannot prevent. BetterPullReview must not read the helper's token exchange or
change global authentication. Full Git comparison currently requires
`gh` 2.100.0 and Apple Git 2.50.1 (Apple Git-155), with a 32 MiB per-file
ceiling and supervised 96 MiB/128-entry temporary-store abort threshold; the
aggregate threshold is not a hard disk quota. Local refs reflect the accepted
API snapshot, not fresh remote advertisement. A tests-disabled macOS fixture
now renders a full PR diff through the CLI profile; an isolated build with
the process adapter removed from composition visibly refuses the same healthy
fixture. Native HTTPS helper exchange and Keychain backend behavior remain
unverified. CLI publishing remains unavailable. Windows and Linux remain
unavailable; publication and target-native qualification remain pending under
BPR-0788.

BetterPullReview does not extract or copy CLI tokens or change global CLI
authentication. Opt-in trusts the machine's configured executable, configuration,
socket routing, CA and redirect behavior, active account and helper processes.
Pre/post observations can detect some account changes but cannot bind a credential
generation or prevent a concurrent account switch. Executable/configuration/CA
replacement and process escape remain uncontained risks. Read opt-in grants no
publishing authority. RFC-BPR-0013's independent `MaximumCommits=250` parser limit
remains unchanged; CLI access alone cannot make a larger PR render.

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
