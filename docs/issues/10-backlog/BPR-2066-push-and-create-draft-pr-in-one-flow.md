---
schema: "implement-rfc.card.v1"
id: "BPR-2066"
title: "Push, and Create draft PR without leaving the application"
status: "Backlog"
rfcs: ["RFC-BPR-0114"]
metadata: {"criteria":["AC03","AC04","AC07","AC08"],"milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"reopened_choices":["PUBLISH-01"],"reservation":"docs/issues/_reservations/BPR-2062--BPR-2071--3110069c9cbb.json","imported_from":"docs/rfcs/assets/local-draft-authoring-v1/cards/DRAFT-0005-push-and-create-draft-pr-in-one-flow.md"}
depends_on: ["BPR-2065", "BPR-2029"]
---

# Push, and Create draft PR without leaving the application

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0114](../../rfcs/RFC-BPR-0114-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md), criteria `AC03`, `AC04`, `AC07`, `AC08`.

Push of the draft's one branch to the resolved hosted repository by its configured URL, create or fast-forward only; the preview; refusals and their reasons; observation of the hosted head and the hand-over to Create draft PR…; Push from a local draft already associated with a pull request; and the reporting form of RFC-BPR-0099. Reconciles RFC-BPR-0111's live text and BPR-2029 under the pack's amendment. Load `frontend-design:frontend-design` before the Qt work.

## Intended verification

A local bare repository as the hosted side covers create, fast-forward, a refused non-fast-forward, a pre-push hook and an isolated local clone whose `origin` is a primary clone. One live course on `MikeMarcin/BPR-Test` pushes a branch through the author's credential helper, observes the head and creates a draft pull request, then closes what it made. The owner's look (AC08) is an attended review on a frozen build.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/local-draft-authoring-v1/IMPLEMENTATION_PROMPT.md).

`PUBLISH-01` was reopened on 2026-11-10; see the [decision record](../../rfcs/assets/local-draft-authoring-v1/DECISIONS.md#decisions) and the [proposed amendment](../../rfcs/assets/local-draft-authoring-v1/AMENDMENTS.md).

## Pulled forward (owner redesign, 2026-11-12)

Delivered on lane `claude/v0-5-0-prerelease-targets-52265d` under the owner's
redesign of local drafts ([accepted design](../../design/local-draft-pr-window-v1/README.md));
the RFC's amendment of 2026-11-12 is authoritative for scope. The
local-draft-authoring milestones must not build this again. Evidence:
[RFC-BPR-0113 ledger](../../verification/RFC-BPR-0113.md),
[RFC-BPR-0114 ledger](../../verification/RFC-BPR-0114.md).

## Hygiene audit 2026-11-15

Verdict: **STILL-VALID**. Needs refreshing: Push was delivered and run live (RFC-BPR-0114 ledger). Still open: Create draft PR after a push (not rerun), the AC06 stop and timeout courses, and AC07 (push from an associated draft).
This is a read-only tracker audit: it changes no status, folder or metadata.
