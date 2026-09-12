---
id: "RFC-BPR-0003"
title: "Local Git Cache, Pull Request Identity, and Review Epochs"
status: "Auto-Verified"
area: "BPR"
milestone: "M1 Native Read-Only Workbench"
depends_on: ["RFC-BPR-0001","RFC-BPR-0002"]
pack_version: 2
---

# RFC-BPR-0003: Local Git Cache, Pull Request Identity, and Review Epochs

## Summary

Create app-owned bare Git caches, retain observed pull-request revisions, and model independent left/right review epochs including branch base, previous upload, latest head, and last-reviewed head.


## Motivation

Review quality depends on knowing exactly which revision was inspected. Mutable PR branches, rebases, force-pushes, and base updates otherwise erase the equivalent of CodeCollaborator uploads.

## Goals

- Maintain app-owned bare Git caches.
- Retain every observed PR head and relevant base through private refs.
- Model review epochs with exact base/head OIDs and causes.
- Detect previous upload, latest, branch base, and last-reviewed comparisons.
- Allow independent left/right epoch selection.
- Preserve historical review material after force-push where objects were observed.

## Non-goals

- Editing or checking out developer worktrees.
- Running repository hooks, filters, submodules, LFS smudge, builds, tests, or external diff tools.
- Reconstructing a head that BetterPullReview never observed and GitHub no longer exposes.

## Architectural allocation

- **Core:** owns the current PR identity, selected left/right epochs, default `Branch base`/`Latest` policy, comparison legality, explicit newest/older history-page navigation, and acceptance of Git/epoch completions by request/revision/generation.
- **Host:** owns the installed-Git process boundary, app-owned bare repositories, retained refs, Git-object residency, observed-time input, epoch persistence, cancellation, and cache pruning. Time is passed to decisions explicitly rather than read by the core.
- **Qt adapter:** owns the independent epoch selectors, one 100-epoch/200-anchor resident page, exact-OID presentation, unavailable/stale history states, popup mechanics, and translation of epoch choices into semantic intents.
- The core stores stable epoch identities and bounded summaries rather than Git blobs or complete history.

## Git policy

All Git operations execute against BetterPullReview-owned repositories. The adapter uses the installed Git executable with explicit safe arguments and sanitized configuration. It records fetch origin, requested refs/OIDs, result identity, and cancellation without logging credentials.

## Epoch rules

A review epoch records base OID, head OID, observed time, ordinal, cause, and material summary. A head change creates an author-upload epoch. A base-only change creates a base-update epoch. Equivalent compared material may reuse file receipts while remaining a distinct historical observation.

The default pane choices are left `Branch base` and right `Latest`. Named comparisons are aliases over exact epoch pairs.

## Acceptance criteria

- Opening a fixture PR creates/reuses the correct app-owned bare repository.
- Observed heads receive durable private refs and remain available after the simulated remote force-push removes them.
- Base update, author upload, force-push, and base-branch-change cases receive distinct cause labels.
- Left and right selectors reach all retained epochs through core-authorized bounded pages and show exact OIDs even when the selected anchors leave the resident page.
- `Previous upload -> latest` and `My last reviewed -> latest` resolve to expected OIDs.
- Git process tests prove hooks, textconv, external diff, submodules, filters, and worktree execution are disabled.
- Repository cache deletion and ref pruning are explicit user/admin operations with documented consequences.
- Core epoch-selection tests run without Git or Qt; host tests prove Git/ref/time effects and stale completion rejection; Qt tests prove selector event translation and projection only.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Old head survives force-push | App-owned cache | Compare removed old head to current head after remote ref deletion |
| Base/head causes are correct | Epoch service | Feed known history and inspect chronological cause labels |
| Selector identity is exact | Workbench projection | Chosen label, base OID, head OID, and diff request agree |
| Git does not execute repo behavior | Process policy fixture | Malicious hooks/filters/ext-diff canaries are never invoked |
| Epoch work follows request identity | Manual-executor integration | Reorder Git/epoch completions and prove only matching generation updates selection |
| Large retained history remains bounded and reachable | Core/host paging plus rendered Qt composition | Traverse more than 1,000 pages to ordinal one, return to newest, and retain at most 100 epochs/200 anchor rows with zero routine resets |

## Documentation impact

Cache layout, retention, privacy, pruning, epoch identity, force-push limits, base updates, Git safety, and comparison semantics are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.

## Owner direction, 2026-09-12: epoch identity is the pull request's, not the reader's

This RFC is Auto-Verified and its baseline is not reopened by this note. The
note records a requirement the owner stated on 2026-09-12, and the card that
carries it.

Asked, after [[BPR-0646]]'s third unjudgeable attended round, whether the
application should import a pull request's earlier updates or whether the
epoch line should stay a record of what this install has observed, the owner
answered neither, verbatim:

> An Epoch is an update to the PR, epochs identity should be stable and
> repeatable if I look at the PR on different machines. A purely observed
> epoch is not useful and terribly confusing.

What that changes about this RFC's model, stated plainly so no later reader
has to derive it again:

- This RFC's epoch is an observation. "A review epoch records base OID, head
  OID, observed time, ordinal, cause, and material summary" describes what
  *this install* saw when it read the pull request, and the list grows only as
  this install reads again. The owner's direction makes the epoch a property
  of the pull request: the same pull request read on two machines yields the
  same epochs, with the same identities, in the same order.
- Importing history is necessary but not sufficient. `core::review_epoch_id`
  is `{Authority, Incarnation, Ordinal}`
  (`src/core/include/bpr/core/epoch_model.hpp:444-450`). `Incarnation` is a
  random value minted per catalogue lifetime, added deliberately by
  [[BPR-0044]] so recreated ordinals cannot collide with stale ids; `Ordinal`
  is minted locally by the SQLite scope row under compare-and-swap
  (`src/adapters/sqlite/src/epoch_catalog.cpp:3030-3045`, `:3147-3156`), in
  observation order. Both variable parts of the identity are machine-local, so
  a producer placed behind the existing model would produce a longer list with
  identities that still differ between machines.
- The reserved capability this RFC already names — `epoch_origin` and
  `epoch_primary_cause`'s `explicit_historical_import`, validated in
  `src/core/src/epoch_transition.cpp` and persisted in
  `src/adapters/sqlite/src/epoch_catalog.cpp` — has no producer anywhere
  outside a test. The only place `Origin` is set on a read is
  `src/application/src/github_window_read_host.cpp:480`, and it writes `live`.
- The read the product would need does not exist. `GET /repos/{o}/{r}/pulls/{n}`
  returns one base/head pair; `list_pull_request_commits` exists end to end and
  is admitted anonymously, but a force push rewrites a commit list rather than
  appending to it, so commits cannot reconstruct the updates a force push
  overwrote. The timeline — `headRefForcePushedEvent`, `baseRefForcePushedEvent`,
  `baseRefChangedEvent` — is asked for nowhere in the tree, and signed out every
  GraphQL-routed operation is refused by
  `src/adapters/github/src/github_anonymous_read_admission.hpp:99-108`.

[[BPR-0743]] (Ready, P1) carries the work. It opens with a bounded design pass
whose two decisions — the derivation of a portable identity, and whether the
locally minted ordinal moves or stays as the ordering key — are recorded before
the model changes, because the ordinal is encoded into git retained refs, the
review ledger, window restoration, the MCP context and the pinned history
paging. If that pass concludes the change is larger than an owner direction on
a closed RFC can carry, it comes back for the owner's decision on whether it
wants an RFC of its own rather than reopening this one from inside a card.

## The design pass fired the conditional, 2026-09-12

The bounded design pass the note above hands to [[BPR-0743]] has run, and its
conclusion is recorded there in full. The conditional this RFC set for it —
"if that pass concludes the change is larger than an owner direction on a
closed RFC can carry, it comes back for the owner's decision" — **fires**, and
this RFC's baseline is still not reopened.

The signed-out obstacle turned out not to exist: the pull request timeline is
readable **anonymously over REST**, confirmed by running it
(`GET /repos/{o}/{r}/issues/{n}/timeline`, HTTP 200 with no credential,
against the 60-per-hour unauthenticated budget). The correction above, that
every GraphQL-routed operation is refused anonymously, still stands — the
route is REST, and the admission switch is at
`src/adapters/github/src/github_anonymous_read_admission.hpp:56-74` with the
GraphQL refusal at `:83-110`.

What fires the conditional is not the size of the change but its kind. Of the
six things this RFC's sentence pins — "A review epoch records base OID, head
OID, observed time, ordinal, cause, and material summary" — the work moves
four:

- **Identity.** A portable identity derived from the pull request is added
  beside `review_epoch_id`, which becomes the local storage key rather than
  the identity.
- **Base OID.** GitHub's timeline names the head a force push moved *to* and
  nothing before it, so a historical epoch's base at that time is not
  observable. It must become a derived value, or absent — and today it is
  mandatory and origin-blind (`src/core/src/epoch_transition.cpp:2199-2218`,
  `:2284-2300`; `epoch_anchor.object_digest` is `NOT NULL`).
- **Observed time.** A historical epoch's only honest timestamp is GitHub's
  push time, not this install's observation.
- **The catalogue schema version**, against a migration mechanism that refuses
  unknown versions by design
  (`src/adapters/sqlite/src/catalog_database.cpp:1018-1062`).

Nothing is implemented on the design pass until the owner decides whether this
wants an RFC of its own or an amendment recorded against this one. The
question is on [[BPR-0743]], which is parked on it.
