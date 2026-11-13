---
schema: "implement-rfc.card.v1"
id: "BPR-2022"
title: "Verify and close RFC-BPR-0108"
status: "Done"
rfcs: ["RFC-BPR-0108"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07","AC08"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0007","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0034-close-local-pr-draft-entry-and-persistence.md","draft_title":"Close local PR draft entry and persistence"}
depends_on: ["BPR-2019", "BPR-2020", "BPR-2021"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"af839e73d","evidence":{"kind":"command","detail":"local_draft; application_local_draft_desktop_public_path in the pull request window; owner accepted the draft in the pull request window (session 767b9233)"}},{"criterion":"AC02","outcome":"pass","revision":"af839e73d","evidence":{"kind":"command","detail":"core_local_draft; local_draft; desktop course"}},{"criterion":"AC03","outcome":"pass","revision":"3585dedc7","evidence":{"kind":"command","detail":"core_local_draft; local_draft"}},{"criterion":"AC04","outcome":"pass","revision":"3585dedc7","evidence":{"kind":"command","detail":"local_draft; desktop course (restart)"}},{"criterion":"AC05","outcome":"pass","revision":"3585dedc7","evidence":{"kind":"command","detail":"Desktop handoff; bpr draft open from a terminal; forwarding to a running desktop has no course"}},{"criterion":"AC06","outcome":"pass","revision":"af839e73d","evidence":{"kind":"observation","detail":"Desktop course and Review Cycles course; owner accepted Home 1 (6d00826a) and home-selection (767b9233)"}},{"criterion":"AC07","outcome":"pass","revision":"3585dedc7","evidence":{"kind":"command","detail":"local_draft; core_profile_data_reset; sqlite_profile_data_reset_files; desktop course"}},{"criterion":"AC08","outcome":"pass","revision":"af839e73d","evidence":{"kind":"command","detail":"core_local_draft; desktop course; a drawn row chip has no course"}}]
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
| **AC01:** From Home, a signed-out author selects a clean Git worktree, including an isolated local clone of their primary clone, and reaches the correctly based proposed branch diff with source, target and Local draft identity visible, without launching an agent or changing the checkout. | `local_draft`, `application_local_draft_desktop_public_path`, attended look line on the shipped composition | Pass | `3585dedc7` |
| **AC02:** Dirty and untracked material is distinguishable from the branch candidate; only selected working files enter a captured candidate, with exact bytes and honest coverage/transform qualifications. | `core_local_draft`, `local_draft`, desktop course | Pass | `3585dedc7` |
| **AC03:** Forks, tracking branches, local-clone remote chains, missing bases, detached heads and ambiguous destinations cannot produce a falsely identified target or comparison; a valid local-clone chain resolves to its hosted remote without fetching, and specific recovery leaves existing preparation intact. | `core_local_draft`, `local_draft` | Pass | `3585dedc7` |
| **AC04:** Notes and candidate material survive restart and external edits; refresh, competing completions, branch switches, moved folders and path reuse do not silently retarget the draft. | `local_draft`, desktop course (restart) | Pass | `3585dedc7` |
| **AC05:** The folder chooser and local harness handoff reach the same resumable draft behavior; context submission neither publishes nor creates a duplicate session by a path-only identity guess. | Desktop course (handoff after restart), `bpr draft open` from a terminal | Pass; forwarding to a running desktop has no course | `3585dedc7` |
| **AC06:** Home makes PR preparation and In preparation prominent while existing Review Cycles remain reachable and unchanged, with native keyboard/accessibility and ordinary PR navigation preserved. | Desktop course, `application_review_cycle_desktop_public_path`; owner accepted Home 1 (session `6d00826a…`) and its one selection with Escape (session `767b9233…`, `home-selection`) | Pass | `af839e73d` |
| **AC07:** Discarding a draft and the existing profile resets remove its private preparation and leave the worktree, branches and any linked pull request untouched; a full draft or candidate store refuses new admission with a reason and evicts nothing silently. | `local_draft`, `core_profile_data_reset`, `sqlite_profile_data_reset_files`, desktop course | Pass | `3585dedc7` |
| **AC08:** With local drafts present, Home's four sections and their authored rows are as they are without local drafts; a local draft associated with a pull request appears once, as that pull request's row with a mark that opens its preparation; and a resumed local draft opens on Changes with no chat open. | `core_local_draft`, desktop course | Pass for In preparation, the prepared set and opening state; a drawn row chip has no course | `3585dedc7` |

Ledger: [RFC-BPR-0108](../../verification/RFC-BPR-0108.md). Closed
2026-11-13 after the owner accepted every look of session `767b9233…`
(the draft in the pull request window, the Edit view, Local Changes with
commit and push, the agent's commit message and Home's selection); the
courses drive the pull request window since `af839e73d` (the separate draft
page was deleted at the owner's direction). Recorded limits stay as written:
forwarding `bpr draft open` to a running desktop and a drawn row chip have no
course.
