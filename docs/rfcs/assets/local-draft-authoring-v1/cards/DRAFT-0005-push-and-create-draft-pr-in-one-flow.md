---
schema: "implement-rfc.card.v1"
id: "DRAFT-0005"
title: "Push, and Create draft PR without leaving the application"
status: "Backlog"
rfcs: ["RFC-DRAFT-0002"]
depends_on: ["DRAFT-0004", "BPR-2029"]
metadata: {"criteria": ["AC03", "AC04", "AC07", "AC08"], "milestone_plan": {"id": "local-draft-authoring-v1", "path": "docs/rfcs/assets/local-draft-authoring-v1/milestone-plan.json"}, "reopened_choices": ["PUBLISH-01"]}
---

# Push, and Create draft PR without leaving the application

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0002](../RFC-DRAFT-0002-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md), criteria `AC03`, `AC04`, `AC07`, `AC08`.

Push of the draft's one branch to the resolved hosted repository by its configured URL, create or fast-forward only; the preview; refusals and their reasons; observation of the hosted head and the hand-over to Create draft PR…; Push from a local draft already associated with a pull request; and the reporting form of RFC-BPR-0099. Reconciles RFC-BPR-0111's live text and BPR-2029 under the pack's amendment. Load `frontend-design:frontend-design` before the Qt work.

## Intended verification

A local bare repository as the hosted side covers create, fast-forward, a refused non-fast-forward, a pre-push hook and an isolated local clone whose `origin` is a primary clone. One live course on `MikeMarcin/BPR-Test` pushes a branch through the author's credential helper, observes the head and creates a draft pull request, then closes what it made. The owner's look (AC08) is an attended review on a frozen build.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).

`PUBLISH-01` was reopened on 2026-11-10; see the [decision record](../DECISIONS.md#decisions) and the [proposed amendment](../AMENDMENTS.md).
