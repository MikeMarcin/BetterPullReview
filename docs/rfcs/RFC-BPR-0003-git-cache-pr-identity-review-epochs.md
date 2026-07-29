---
id: RFC-BPR-0003
title: Local Git Cache, Pull Request Identity, and Review Epochs
status: Proposed
area: BPR
milestone: M1 Native Read-Only Workbench
depends_on: [RFC-BPR-0001, RFC-BPR-0002]
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

- **Core:** owns the current PR identity, selected left/right epochs, default `Branch base`/`Latest` policy, comparison legality, navigation context, and acceptance of Git/epoch completions by request/revision/generation.
- **Host:** owns the installed-Git process boundary, app-owned bare repositories, retained refs, Git-object residency, observed-time input, epoch persistence, cancellation, and cache pruning. Time is passed to decisions explicitly rather than read by the core.
- **Qt adapter:** owns the independent epoch selectors, exact-OID presentation, unavailable/history states, popup mechanics, and translation of epoch choices into semantic intents.
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
- Left and right selectors enumerate retained epochs and show exact OIDs.
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

## Documentation impact

Cache layout, retention, privacy, pruning, epoch identity, force-push limits, base updates, Git safety, and comparison semantics are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
