---
id: "RFC-BPR-0062"
title: "Saved Repository Onboarding from Home"
status: "Proposed"
area: "BPR"
milestone: "Owner four-draft mandate"
depends_on: ["RFC-BPR-0026"]
pack_version: "four-drafts-v1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
---

# RFC-BPR-0062: Saved Repository Onboarding from Home

## Adoption and current obligations

Registered on main under mandate four-drafts-r1 at baseline `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. The [adopted plan](plans/bpr-four-drafts-v1/milestone-plan.json) preserves every acceptance criterion; the [import record](assets/bpr-four-drafts-v1.LIVE_IMPORT.md) retains provenance and exclusions. The owner selected direct entry of a repository URL or owner/name on 2026-10-07. This resolves the draft's selection choice; no provider-wide browsing inventory is authorized.

The source Handoff below is retained as authoring history, not current execution state. Current acceptance is tracked by the new issue cards and [ledger](../verification/RFC-BPR-0062-0065.md). Existing Done cards and plan exits remain historical evidence for their exact candidates. Formal dependencies use accepted capabilities: RFC-BPR-0026 for saved discovery.

## Problem and grounded current behavior

The owner asked on 2026-10-07 for a better onboarding flow for saved
repositories because adding them is hidden deep in Settings. A new reviewer
should be able to make Home useful without knowing the Settings structure or
having a pull-request link ready.

Before this adoption, [the saved-repository editor](../../src/adapters/qt/src/workbench_window.cpp)
(`build_saved_repositories_editor`) combines a saved-list selector, an
`owner/name` field, Save repository and Remove in Settings → Reviews & queue.
Home's empty sentence and Filter footer send the reviewer to that page, as
specified by [the calm Home RFC](RFC-BPR-0032-calm-home-and-accounts-surfaces.md).
The Pull Request menu also offers Save Repository, but only for an already
opened authenticated PR.

The former editor's unavailable tooltip (`update_saved_repository_actions`) says
“Open a pull request signed in first, so there is an account to save for.”
That wording is at odds with the existing
[discovery host](../../src/application/src/personal_discovery_host.cpp):
`personal_discovery_session_of` derives an authority from an authenticated
session, and `adopt` supplies it to the queue without loading a PR. This
proposal requires the first-save course to work from that accepted sign-in
authority and removes the misleading prerequisite wherever it survives.

[Personal discovery](RFC-BPR-0026-personal-review-home-pr-url-navigation.md)
already defines saved repositories as private, profile-owned records bound to
host and account, with authored and directly requested PR discovery. The
[queue host](../../src/application/src/review_queue_enrollment.cpp)
persists a save before projecting it and treats an existing identity as a
no-op. These behaviors are the foundation for this change.

## Proposed behavior and boundaries

### A visible starting point

Signed-in Home always offers **Add repository**, beside its personal-discovery
status below the Open bar. It remains visible with a populated queue, with
filters applied, and while discovery is unavailable or running. At a narrow
width it moves onto its own line, never into an overflow menu.

When the active host/account has no saved repositories, that same row says:
“Add a repository to find my pull requests and review requests.” It uses the
same Add repository action, with stronger emphasis, rather than duplicating
the button in another welcome card. Recently viewed, observer-backed queue
rows and Review Cycles remain available. Once repositories exist, an empty
queue keeps the ordinary nothing-needs-me explanation and the normal Add
repository action; it does not restart onboarding. A filtered-empty queue is
not evidence that no repository has been saved.

Signed-out Home keeps its existing focused sign-in and public-reading layout.
The existing Sign in action leads to the first-repository prompt after accepted
authentication. Public preview does not claim personal discovery or create
account-bound saved repositories. There is no mandatory launch wizard and no
automatic modal after sign-in.

### One focused addition flow

Add repository opens a small native dialog titled **Add repository**, focused
on the repository input. It shows the destination account and host in plain
language, explains “Find my pull requests and review requests here”, and offers
**Add repository** and **Cancel**. One connected account is shown as a fact;
where several eligible account/host choices exist, the reviewer can select
one explicitly. Saving does not depend on an open PR.

The repository-selection method remains the material open choice below.
Under the recommended direct-entry option, accept `owner/name` or a canonical
HTTPS repository-root URL, with an optional trailing slash. A bare name uses
the displayed host. A URL names its host explicitly; it cannot silently save
against a different host or configure a server. Other URL shapes are refused
beside the input with a concrete correction. The existing Open bar continues
to mean opening a PR; its grammar and default focus do not change.

If the selected server needs sign-in, use the existing focused sign-in flow
and retain the draft in the originating window. An unknown URL host offers
the existing explicit enterprise-server setup before sign-in. Parsed text
alone authorizes neither configuration nor credential routing. After an
account, host or profile change, require the destination to be shown and a
fresh Add repository gesture; a late sign-in result cannot save on its own.
Cancelling any preparatory step returns to the repository draft without a
save. Cancelling the addition dialog discards its draft and restores focus to
its invoking action without changing the open review.

### A saved result with an honest discovery status

The final gesture stores through the existing saved-repository path. Keep the
dialog open while saving, with one visible progress state that prevents
duplicate submissions. A durable success returns to Home, shows a brief
confirmation naming the repository, and asks the existing profile discovery
service to include it, honoring its cancellation, freshness and backoff rules.
The confirmation describes the save; the existing discovery sentence describes
the read. No PR opens automatically.

An already-saved exact identity produces “This repository is already added”
and no second record or store write. A local storage refusal keeps the input
and offers retry, without a saved confirmation. Saving requires syntactically
valid input and accepted account authority, not a new network existence check:
offline or rate-limited discovery does not undo a successful local save.
An inaccessible or missing target stays saved with the existing discovery
failure explanation, never a claim that GitHub access was verified. A complete
empty discovery result means no matching personal PRs, not an unsuccessful save.

Settings → Reviews & queue remains the management location for inspecting and
removing saved repositories and editing observer rules. Its add control opens
the same addition flow rather than keeping a competing inline form. The
existing Save Repository action for an open PR remains a direct shortcut
through the same durable save/result semantics. Saving never creates a
watch-all observer rule or changes GitHub subscriptions.

### Native presentation and ownership

Use the application font, semantic Qt palette roles and established Home row
spacing from [the Qt instructions](../../src/adapters/qt/AGENTS.md).
Repository onboarding gains visibility through placement and clear verbs,
not a new theme, mascot animation, step counter or dashboard card. This design
was developed using `frontend-design:frontend-design`; the repository's native
visual conventions take precedence over generic web styling guidance.

Qt presents the dialog, focus, accessible names and status. Core owns semantic
admission and stale-identity decisions; application hosts own normalization,
account/setup continuation, persistence and discovery effects. Reuse the
existing sign-in, queue and profile services under the
[system architecture](../architecture/system-architecture.md). No widget
parses URLs, reads a store or invokes a provider. Existing repository identities,
saved data, bounds and discovery scope remain authoritative. A schema migration
is not an intended outcome; any necessary internal change is implementation work.

### Owner selection decision — 2026-10-07

The owner selected direct entry of a canonical HTTPS repository URL or `owner/name` (recommended). The direct-entry contract above is adopted. Provider-backed browsing/search is outside this implementation; no account-wide inventory is required. No runtime or owner visual acceptance follows from the selection decision.

## Acceptance criteria

- **AC01:** In the composed application, a newly signed-in profile adds its
  first repository from Home without visiting Settings or opening a PR; a
  returning reviewer can find the same action with populated or filtered rows.
- **AC02:** The selected repository-entry method reaches an exact displayed
  account/host destination; invalid input, cancelled setup and stale sign-in
  completions cannot create a record under another authority.
- **AC03:** Saving, duplicate entry and storage refusal produce the defined
  result states through the real persistence path, with successful records
  surviving restart and appearing in other windows of the same profile.
- **AC04:** Discovery after a save shows personal results or an accurate
  empty/offline/limited/unavailable state without undoing the save, opening a
  review, expanding discovery beyond saved repositories or mutating GitHub.
- **AC05:** Home and Settings invoke the same addition flow, and the existing
  open-PR shortcut retains its direct save behavior without a competing store
  or contradictory account prerequisite.
- **AC06:** The real Home and dialog support keyboard completion and cancel,
  announced progress/refusal, long identities and narrow layouts at 100–200%
  scale in the supported appearances; the owner judges each changed visual
  surface through a fresh short attended session with a frozen build.

## Handoff and scope

This is an unadopted authoring draft, separate from live RFCs and issue cards.
Registration, live dependency assignment, milestone decomposition and runtime
acceptance belong to a later authorized implementation session. No new
repository-wide dashboard, bulk import, local-checkout scan, subscription
change, credential path or clipboard monitoring is proposed. Browsing/search
is conditional on the open choice rather than silently included in that list.

Implementation updates the saved-repository operator guidance, the Home
section of the workbench specification, and the conflicting Settings-link
clauses identified in [the proposed amendment](assets/bpr-four-drafts-v1/saved-repository-onboarding/proposed-home-amendment.md).
Adoption makes that reconciliation explicit. It preserves that
RFC's unrelated contracts and RFC-BPR-0026's saved/discovery authority.
Internal API and widget decomposition are deferred to implementation.
