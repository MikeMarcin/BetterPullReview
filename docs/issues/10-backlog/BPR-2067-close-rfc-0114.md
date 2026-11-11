---
schema: "implement-rfc.card.v1"
id: "BPR-2067"
title: "Verify and close RFC-BPR-0114"
status: "Backlog"
rfcs: ["RFC-BPR-0114"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07","AC08"],"milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"reopened_choices":["PUBLISH-01"],"reservation":"docs/issues/_reservations/BPR-2062--BPR-2071--3110069c9cbb.json","imported_from":"docs/rfcs/assets/local-draft-authoring-v1/cards/DRAFT-0006-close-commit-and-push.md","draft_title":"Close commit and push through the author's own Git"}
depends_on: ["BPR-2065", "BPR-2066"]
closeout_for: ["RFC-BPR-0114"]
---

# BPR-2067: Verify and close RFC-BPR-0114

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0114](../../rfcs/RFC-BPR-0114-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`, `AC08`.

Acceptance assessment for commit and push from a local draft, and for the reconciled PUBLISH-01 text.

## Intended verification

Judge every criterion on the composed application, state the platforms the runner is qualified on, and confirm the amendment's before-and-after text is applied to the live RFCs it names.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/local-draft-authoring-v1/IMPLEMENTATION_PROMPT.md).

`PUBLISH-01` was reopened on 2026-11-10; see the [decision record](../../rfcs/assets/local-draft-authoring-v1/DECISIONS.md#decisions) and the [proposed amendment](../../rfcs/assets/local-draft-authoring-v1/AMENDMENTS.md).

## Closeout checklist

RFC base: `b0d54697a6db6b17b459a751515de60aff14d65d`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** With selected uncommitted files, Commit… makes one commit on the draft's branch holding exactly those files' captured content, recorded with the author's Git identity and signing configuration and after the repository's commit hooks ran, and every other staged or unstaged change is as it was before. | Pending | Pending | Not implemented |
| **AC02:** Commit… runs no Git when the branch, head or a selected file differs from the candidate. A refusing hook leaves no commit and its output is shown. A hook that changes content leaves a commit BPR reports as differing from the candidate, with the refresh offered. | Pending | Pending | Not implemented |
| **AC03:** Push sends the draft's branch to the resolved hosted repository, including from an isolated local clone whose `origin` is the primary clone, runs the pre-push hooks from the worktree, authenticates through the author's own credential helper, and leaves the clone's configuration and branches unchanged; BPR then observes the hosted head and Create draft PR… proceeds without the author leaving the application. | Pending | Pending | Not implemented |
| **AC04:** A push that is not a fast-forward is refused and reported with its reason, and BPR's own-Git operation catalogue holds no force, delete, tag, reset, stash or checkout operation. | Pending | Pending | Not implemented |
| **AC05:** The first Commit or Push in a local draft asks the hooks-and-configuration question once. Declining runs nothing and shows the exact commands. The isolated runner still runs with no hooks, credential helper, protocol or system configuration. | Pending | Pending | Not implemented |
| **AC06:** An operation that needs typed input fails within its bounded wait with that reason. After a cancellation or a lost answer BPR observes the result and repeats nothing. A review submission elsewhere completes while a push is in flight. | Pending | Pending | Not implemented |
| **AC07:** From a local draft associated with a pull request, later commits reach the pull request's branch only on the author's Push. | Pending | Pending | Not implemented |
| **AC08:** On a frozen build, the owner accepts the Commit… and Push previews, the one-time question and a failed-hook banner. | Pending | Pending | Not implemented |
