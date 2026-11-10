---
schema: "implement-rfc.card.v1"
id: "BPR-2022"
title: "Verify and close RFC-BPR-0108"
status: "Backlog"
rfcs: ["RFC-BPR-0108"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07","AC08"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0007","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0034-close-local-pr-draft-entry-and-persistence.md","draft_title":"Close local PR draft entry and persistence"}
depends_on: ["BPR-2019", "BPR-2020", "BPR-2021"]
closeout_for: ["RFC-BPR-0108"]
---

# BPR-2022: Verify and close RFC-BPR-0108

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0108](../../rfcs/RFC-BPR-0108-local-pr-drafts.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`, `AC08`.

Acceptance assessment for the local PR draft subject and entry flow.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Judge all owning RFC criteria, including the real initial diff and retained Review Cycles behavior.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** From Home, a signed-out author selects a clean Git worktree, including an isolated local clone of their primary clone, and reaches the correctly based proposed branch diff with source, target and Local draft identity visible, without launching an agent or changing the checkout. | Pending | Pending | Not implemented |
| **AC02:** Dirty and untracked material is distinguishable from the branch candidate; only selected working files enter a captured candidate, with exact bytes and honest coverage/transform qualifications. | Pending | Pending | Not implemented |
| **AC03:** Forks, tracking branches, local-clone remote chains, missing bases, detached heads and ambiguous destinations cannot produce a falsely identified target or comparison; a valid local-clone chain resolves to its hosted remote without fetching, and specific recovery leaves existing preparation intact. | Pending | Pending | Not implemented |
| **AC04:** Notes and candidate material survive restart and external edits; refresh, competing completions, branch switches, moved folders and path reuse do not silently retarget the draft. | Pending | Pending | Not implemented |
| **AC05:** The folder chooser and local harness handoff reach the same resumable draft behavior; context submission neither publishes nor creates a duplicate session by a path-only identity guess. | Pending | Pending | Not implemented |
| **AC06:** Home makes PR preparation and In preparation prominent while existing Review Cycles remain reachable and unchanged, with native keyboard/accessibility and ordinary PR navigation preserved. | Pending | Pending | Not implemented |
| **AC07:** Discarding a draft and the existing profile resets remove its private preparation and leave the worktree, branches and any linked pull request untouched; a full draft or candidate store refuses new admission with a reason and evicts nothing silently. | Pending | Pending | Not implemented |
| **AC08:** With local drafts present, Home's four sections and their authored rows are as they are without local drafts; a local draft associated with a pull request appears once, as that pull request's row with a mark that opens its preparation; and a resumed local draft opens on Changes with no chat open. | Pending | Pending | Not implemented |
