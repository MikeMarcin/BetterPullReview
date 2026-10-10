# BetterPullReview Product Contract

Private review conversations are local profile state. In the workbench's
**Private history** panel, the reviewer can inspect immutable revisions and
actor qualifications, select exact sources, and compose a local draft. Publishing
is a separate action through the existing review editor. Local deletion does
not remove GitHub comments. The [operator workflow](../ux/private-review-workflow.md)
explains stale anchors, provenance, split targets, and public-body behavior;
the [RFC-BPR-0023 ledger](../verification/RFC-BPR-0023.md) records the remaining
designated live-publication acceptance gap.

Review Cycles ([RFC-BPR-0053 through RFC-BPR-0061](../rfcs/RFC-BPR-0053-review-cycles-domain-and-local-authority.md))
is built into the desktop and the `bpr cycle` terminal. Those RFCs stay
Proposed until their closeout cards judge every criterion; the
[execution ledger](../verification/RFC-BPR-0053-0061.md) records what is
qualified, on which platform, and what is still owed.

Review Cycles is deliberately narrow. A review cycle is a local subject in one
BPR profile, opened with File > Open Project… or Home's Review Cycles section on
a folder, a single file or a Git checkout, with no account, remote, commit or
agent. Captured source revisions, notes and suggestions, frozen rounds,
responses and the reviewer's verifications stay in the profile's private store
on this machine. They leave it only when the reviewer copies, saves or exports a
frozen round, grants one round to a harness that runs `bpr`, sends one to an
agent (Send to Agent, in a build with agent integrations), or publishes. The
only GitHub write is creating a new branch and pull request on github.com from a
selected final result, through the existing native transport and the account
signed in, after the reviewer confirms the exact plan. It does not merge, close
or push to an existing pull request, comment or review on GitHub, run Git in the
reviewer's checkout, host shared review, or execute captured code. A cycle may be
linked to an existing pull request without inheriting its publication authority,
and the reviewer may add a linked cycle's notes to that pull request's Review
Tour as an unpublished local tour edit. GHES pull requests cannot be published
or linked yet. Pull request identity, credentials, epochs and the shared-state
authority in the table below are unchanged, and GitHub still wins every shared
fact. Local cycle history is not recoverable from GitHub. The
[domain model](domain-and-state-model.md#review-cycle-identities-rfc-bpr-0053),
[threat model](security-threat-model.md#review-cycles) and
[terminal guide](../development/cycle-cli.md) state the details, and the
[Review Cycles workflow](../ux/review-cycles-workflow.md) is the reviewer's guide.

## Definition

BetterPullReview is an alternative native client for reviewing code contained in GitHub pull requests, and, as Review Cycles, a local workbench for reviewing a folder, file or checkout of one's own over repeated rounds. It is not a replacement pull-request server, a second collaborative review database, a merge client, or an IDE.

## Authority model

| State class | Authority | Examples |
|---|---|---|
| Shared collaborative state | GitHub pull request | Commits, files, comments, replies, reviews, approvals, requested changes, thread resolution, checks |
| Private retained revision history | Local BetterPullReview Git cache and epoch catalog | Exact base/head objects that BetterPullReview observed, epoch identity and cause, availability, and current-viewer review associations |
| Provider-evidenced update history | GitHub timeline, reconciled in the local epoch catalog | Portable force-push occurrence identity and event time; explicit incomplete or ambiguous history when the read or association lacks evidence |
| Private workflow state | Local BetterPullReview ledger | Read receipts, acceptance receipts, observer rules, private notes, local defect classification, AI drafts |
| Local review cycles | The profile's private cycle store (`review-cycles.sqlite3`) | Captured source revisions and their bytes, notes, suggestions, round drafts, frozen rounds, delivery observations, responses, reviewer verifications, publication plans and journals, links to pull requests |
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

## Local maintenance and removal

Settings → Storage & privacy provides a local diagnostic preview/export and
explicit diagnostic cleanup/profile purge. Diagnostic export saves only the
exact previewed classified metadata to a new local file; it uploads nothing.
The readable log and external OS crash reports have separate privacy limits.

Help → Diagnostics opens the same profile Settings while signed out. Help also
offers anonymized and identifying export actions directly. Each presents a
frozen preview with Save and Cancel, without requiring Settings navigation. Recording
starts On, Standard and Anonymized. Detail and identity privacy are separate
persistent choices, with an explicit disclosure before Include locally. Off
stops new records and recent context after acknowledgement, preserving existing
files. Clear local diagnostics resets the ring and removes the admitted profile
logs under the logger barrier; it preserves external exports and the profile's
other state. Refused or partial cleanup is visible. This immediate diagnostic
control is distinct from the stopped-profile prepared maintenance below.

Preparing destructive maintenance stops the selected profile and drains its
producers before capturing an exact inventory. Confirmation applies only to
that inventory; cancellation does not resume the profile. Purge deletes known
credential bindings before their metadata and requires recovery after an
irreversible failure. Shared registry/updater state, updater draft recovery,
sibling profiles, exports and documented empty roots/locks remain. Uninstall
removes program files, not all local data or reviews already on GitHub.

The [operator guide](../development/storage-and-diagnostics.md) and
[maintenance contract](release-maintenance.md) distinguish these implemented
controls from pending native vault, crash and full-removal acceptance. The
[corporate security packet](../security/corporate-security-overview.md) describes
the current release posture; no pending platform behavior is promised here.

## GitHub CLI profile milestone

RFC-BPR-0015's amended first read milestone has a qualified macOS fixture
path. Native HTTPS remains the default. The tests-disabled macOS composition
has explicit
CLI read opt-in on the selected GitHub.com or GHES host, with a Connect warning,
observed `/user` identity, pre/post-request account checks and a read-only lease.
It invokes the configured `gh` directly with bounded arguments, input, output
and time; account discovery and API use require `gh` 2.96 or later
(including later majors)
because the account inventory uses `gh auth status --json hosts`
(every account on each host; `--active` only for the default choice).
Older versions are refused before account status runs and the UI requires an
upgrade. BetterPullReview never passes `--show-token`.
This is a version check, not proof of an official executable's provenance.
The r2 trust-boundary amendment permits a bounded exact-repository `gh` Git
operation for objects; `gh` may internally launch Git and its shell credential
helper. Every macOS `gh` launch must run under OS-enforced file-write denial,
with a narrowly owned temporary object-store exception only for the bounded
clone; an unavailable boundary refuses the CLI profile. This covers `gh`'s
possible first-use global configuration migration, which a post-request check
cannot prevent. BetterPullReview must not read the helper's token exchange or
change global authentication. Full Git comparison requires `gh` 2.96 or later
(RFC-BPR-0015's account and API floor) and Git 2.49 or later
(`clone --revision`), not one exact vendor or banner. Before any credentialed
`gh` use, bounded write-denied local `gh api --help`, `gh repo clone --help`
and `gh auth git-credential --help` probes must show the flags and usage the
app invokes, and `git clone -h` must show the required clone options. A failed
check and an unsupported tool reach the comparison as distinct typed reasons.
Capability admission retains actual clone and exact-object validation. Native
qualification evidence remains installation-specific. The 32 MiB per-file
ceiling and supervised 96 MiB/128-entry temporary-store abort threshold
remain; the aggregate threshold is not a hard disk quota. For every validated PR shape, a
sparse-capable CLI connection retains the exact base/head commit trees and
loads only the selected file blobs. A small PR does not imply a small repository
history; connections without sparse support keep complete acquisition. Local
refs reflect the accepted API snapshot, not fresh remote advertisement. A tests-disabled macOS fixture
now renders a full PR diff through the CLI profile; an isolated build with
the process adapter removed from composition visibly refuses the same healthy
fixture. A macOS native read-only probe now verifies one official `gh` HTTPS clone of a
small owner-controlled private repository through `gh auth git-credential`;
`gh` reports a keyring token source and a matching Keychain item exists.
The probe does not bind the precise Security API calls, credential generation,
or concurrent account selection. The private repository name and objects
were not retained. On macOS, the opted-in CLI profile now routes whole-review and
right-side inline-comment publication through the existing mutation admission,
a separate exact-host/account/PR/operation confirmation for each ordinary write, and
server reconciliation. The configured gh account may still change between
checks; an ambiguous attempted write is not posted twice. Fixture actions
exercise this path; no live PR was written. Windows and Linux remain
unavailable. Mac qualification is recorded by BPR-0787. RFC-BPR-0014 is now
Auto-Verified, and RFC-BPR-0015 became Auto-Verified on 2026-09-23 through
BPR-0788. The product owner approved the pending review of the CLI
authority and protocol changes made after that qualification. That
approval is an owner decision, not an independent reviewer's report.

Tour Publish is a bounded grouped-confirmation extension: the preview binds one
immutable plan to the exact actor, head, sources, prose and allowed returned-link
slots. Each journaled write consumes its own one-use child grant. Stop or an
identity/draft change revokes remaining authority; restart reconciles read-only
and needs an explicit preview and Resume. Compatibility comments are explicitly
non-atomic, and an optional description edit is a separately named operation.
These Tour operations remain unavailable on an unqualified host/transport.

BetterPullReview does not extract or copy CLI tokens or change global CLI
authentication. Opt-in trusts the machine's configured executable, configuration,
socket routing, CA and redirect behavior, active account and helper processes.
Pre/post observations can detect some account changes but cannot bind a credential
generation or prevent a concurrent account switch. Executable/configuration/CA
replacement and process escape remain uncontained risks. Read opt-in grants no
publishing authority. RFC-BPR-0013 keeps a 250-record limit for commits returned by the pull-request
endpoint. A larger reported total is accepted only when that endpoint
returns exactly 250 records ending at the accepted head. The UI names the
reported total and returned count separately; this does not establish that
any particular private pull request can render.

## Review-tour preparation

Authors can prepare and privately save Problem, Solution and an ordered source
tour without AI. The [operator workflow](../development/review-tours.md) describes
editing, exact source/fallback subjects, cold import, website conflicts and
partial recovery. Explicit description updates are a narrow **body-only**
extension; the original title, draft status and other PR-management state have
no tour mutation operation. Production Tour writes remain unavailable until the
exact connected operations are qualified.

An explicit tour writing action may replace one captured local editor span as
one native undo transaction. It grants no publication authority. Outline entries
have no generated bodies and require visible per-stop author completion.

## Explicit non-goals

- Creating, changing PR metadata other than the explicitly previewed description body, merging, closing, or rebasing pull requests.
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

## Source-line comments

A reviewer selects one current/right source line in a text file already changed
by the pull request and uses the ordinary draft and Publish action. The exact
head, changed-file membership, retained path/blob/line and acting identity are
revalidated before the write. Painted diff geometry supplies no server capability.

The semantic source anchor is distinct from GitHub's remote LINE or FILE subject.
An ordinary file review comment can carry a visible immutable code permalink
followed by the reviewer's byte-preserved prose. Its valid link reconstructs
placement from GitHub after local database loss; removing or invalidating that
link restores honest file-level placement. GitHub IDs and server edits remain
authoritative. The comment and visible link survive uninstall without a private
service or hidden marker. The designated GitHub-only collaborator course is
accepted and its external reply was read by the signed app at the same source
line. Actual desktop uninstall remains unobserved, separately from the passing
reconstruction fixtures.

The [review round-trip contract](review-round-trip.md#semantic-source-line-publication-policy-rfc-bpr-0022)
owns representation selection and recovery. Native writes are attempted under
unknown capability. The owner-approved classifier2 contract admits only the designated PR2, exact
base/head, actor and profile2 with revalidated immutable source and recorded
provider-patch proof. All other scopes remain unknown; generic422 cannot enable
fallback. Earlier unresolved attempts remain GET-only.
The [RFC-0022 ledger](../verification/RFC-BPR-0022.md) records this limitation
and the GitHub.com-only owner amendment; native GHES is unobserved.
