---
id: "RFC-BPR-0067"
title: "Make draft pull requests obvious on Home"
status: "Auto-Verified"
area: "BPR"
depends_on: ["RFC-BPR-0010","RFC-BPR-0026","RFC-BPR-0066"]
metadata: {"mandate_revision":"home-draft-r1","milestone_plan":{"id":"home-draft-v1","path":"docs/rfcs/plans/home-draft-v1/milestone-plan.json"}}
---

# RFC-BPR-0067: Make draft pull requests obvious on Home

## Adoption

Adopted on 2026-10-11 under mandate `home-draft-r1`, including the accompanying Home amendment. RFC_BASE_SHA: `2beb98cc9a58eeeeeebb8bdd08cc0af4a49c4e20`. [[BPR-1389]] owns AC01–AC04 and closeout. The [import record](assets/home-draft-v1.LIVE_IMPORT.md) preserves the source proposal. Historical RFC0032 A8 obligations remain open.

## Problem and grounded current behavior

The owner asked on 2026-10-11 for draft PRs to be called out obviously on
Home, suggesting a badge or chip saying Draft before the PR name/number.

The provider's [pull-request and search records](../../src/application/include/bpr/application/github_read_model.hpp)
already represent GitHub's draft flag. However, Home's
[personal discovery parser and retained metadata reader](../../src/adapters/github/src/github_paged_read.cpp)
omit it from [their returned rows](../../src/application/include/bpr/application/personal_discovery_port.hpp).
The [queue observation](../../src/core/include/bpr/core/review_queue.hpp)
and [recent-entry projection](../../src/application/include/bpr/application/window_review_restoration_ports.hpp)
also have no draft-status fact. Adding paint alone would therefore leave
unopened PRs and restored Home rows without the information they need.

The [shared Home row delegate](../../src/adapters/qt/src/home_row_delegate.cpp)
draws the PR identity followed by its title, with the activity explanation
below. It has neutral account and Reviewing treatments, but no Draft marker.
[Calm Home](RFC-BPR-0032-calm-home-and-accounts-surfaces.md#home)
reserves colour for attention state. The accompanying
[proposed amendment](assets/home-draft-v1/proposed-home-amendment.md) identifies the additions to
the adopted row contract without changing live requirements during authoring.

## Proposed behavior and boundaries

### A leading Draft chip

Every visible Home PR row whose last accepted metadata says draft displays a
compact, outlined **Draft** chip immediately before its repository/PR identity
and title. This applies to every active queue section and Recently viewed,
including rows discovered before the PR has ever been opened. A representative
first line is:

```text
[Draft] owner/repository#42 — Improve startup recovery
```

The chip is always visible when its row is visible; hover, selection and
opening are not prerequisites. Use neutral theme ink and border roles, the
row's readable text scale and compact padding. In selection it uses the
native selected text treatment. Keep the existing attention colours and
Reviewing tag distinct. Do not add a new palette, font family or animation.

The chip precedes the start of the identity even when the title wraps. Its
label remains complete at narrow widths and large scales; preserve the
existing literal-text wrapping, native focus and selection painting, account
chips and row actions. It is a status annotation within the row, with no
separate action or keyboard stop. The row's accessible text identifies it as
a draft pull request before reading its identity and title.

### Carry an observed fact to Home

Carry known draft/non-draft metadata through the existing bounded discovery,
retained metadata and accepted PR-read paths into provider-free Home facts.
GitHub's accepted metadata is the authority; titles, labels, author membership,
attention buckets and private comment/review drafts cannot establish this flag.
Reuse the [existing discovery limits](../../src/application/include/bpr/application/personal_discovery_port.hpp)
and request acceptance rules. The annotation requires neither an additional
provider request nor a full PR snapshot, file, diff or conversation load.

Persist the last observed fact with the appropriate existing private metadata
so it remains available after restart. A recent entry with no current queue
row uses its last accepted PR metadata; a recent entry also present in the
queue reflects the same accepted draft fact. Scope observations and reuse to
the exact profile, host, account and PR authority, including the existing
access-mode distinction for public reading history.

An accepted change to ready removes the chip; an accepted change back to
draft restores it. Update by stable identity and retain the selected row and
opening target. Failed, cancelled, stale or mismatched reads do not replace
the last accepted fact. Cached rows retain their chip under the existing
freshness reporting. Missing or never-observed draft status is unknown,
including older stored entries on upgrade: it displays no chip and must not
be converted into an observed non-draft value. No Ready badge is introduced.

### Scope and ownership

This is a Home status annotation. It does not change enrollment, filtering,
sorting, section placement, attention counts, notifications or the ability to
open and review a draft PR. [The authored Home separation](RFC-BPR-0066-home-authored-pull-requests.md)
continues unchanged. Review Cycles and the open review's chrome are outside
this proposal.

The core retains explicit semantic facts and existing acceptance policy; the
application host carries observations and private metadata; Qt presents the
chip and accessible wording. Concrete types and storage changes belong to
implementation. The proposed visual treatment follows the loaded
frontend-design skill within Home's existing native theme and row conventions.
No material design questions remain open; the chip treatment is proposed,
not an owner acceptance of a rendered candidate.

## Acceptance criteria

- **AC01:** On composed Home, an unopened draft PR has the leading chip and
  an otherwise comparable non-draft PR does not. Active sections and Recently
  viewed use the defined treatment, including a recent-only draft entry.
- **AC02:** Accepted draft-to-ready and ready-to-draft changes update the
  corresponding Home rows while retaining exact selection and opening
  targets. The annotation adds no provider calls or full PR loads and does
  not change attention, section membership or row actions.
- **AC03:** Restart and an unsuccessful refresh retain a previously observed
  draft chip. Older entries with no fact remain unknown. Late completions
  and changes of profile, account, host or access mode cannot supply another
  row's draft fact; queue and recent representations agree for the same
  accepted authority.
- **AC04:** Rendered Home keeps the complete chip and wrapped identity/title
  readable at narrow widths and 100–200% scale in the existing Light, Dark
  and high-contrast appearances, with Group and State headers settings,
  selection, Reviewing, account chips and opening states. Keyboard and
  accessible row output preserve the defined status and existing actions.

## Accepted implementation

Product candidate `f170654e651cb57e262d6c4e4696378c5464c365` carries the accepted
Home draft metadata and row treatment. Test refinement
`b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` changes no product source. The
[verification ledger](../verification/RFC-BPR-0067.md) records passing AC01–AC03,
automated AC04 checks and the owner’s Accept of exact frozen candidate
`bpr1389-home-draft-v001` in session `df622dfed34516817f93fa5ac168bf03`,
decision `809ee3e4-ba73-49a3-941a-e0052c0c63cc` at event 4. Hand-back at event 5
left no pending decision or requested change. [[BPR-1389]] was finished through
the workflow guard at `1cc5c3e71cbc18c1ebb07b31c9460ec5b04c8b36`. AC01–AC04
and the single milestone’s exits are satisfied; RFC-BPR-0010, RFC-BPR-0026 and
RFC-BPR-0066 are Auto-Verified. Historical native/human obligations retain their
existing scope.
