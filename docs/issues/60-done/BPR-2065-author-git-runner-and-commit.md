---
schema: "implement-rfc.card.v1"
id: "BPR-2065"
title: "The author-Git runner, its one-time question, and Commit"
status: "Done"
rfcs: ["RFC-BPR-0114"]
metadata: {"criteria":["AC01","AC02","AC05","AC06"],"milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"reopened_choices":["PUBLISH-01"],"reservation":"docs/issues/_reservations/BPR-2062--BPR-2071--3110069c9cbb.json","imported_from":"docs/rfcs/assets/local-draft-authoring-v1/cards/DRAFT-0004-author-git-runner-and-commit.md"}
depends_on: ["BPR-2020"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"local_changes: the author-Git runner, separate from the isolated reader, commits with the repository's hooks; owner accepted local-changes-commit-push in session 767b9233"}},{"criterion":"AC02","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"local_changes: the one-time hooks question and its declined path (declined)"}},{"criterion":"AC05","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"local_changes: Commit of exactly the staged files, a refusing hook leaves no commit, Amend while GitHub lacks it; owner accepted agent-commit-message in session 767b9233"}},{"criterion":"AC06","outcome":"pass","revision":"6887928bd","evidence":{"kind":"command","detail":"local_changes stopped_and_timed_out_pushes: a timed-out (1.5 s bound) and a stopped push against a hanging pre-push hook each kill it, say why, read GitHub again and repeat nothing; application_dependable_session_public_path step 2b: a review submission completes while a push holds the author's Git lane"}}]
---

# The author-Git runner, its one-time question, and Commit

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0114](../../rfcs/RFC-BPR-0114-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md), criteria `AC01`, `AC02`, `AC05`, `AC06`.

The second Git runner with the author's environment, separate in code, state and configuration from the isolated runner; the one-time hooks-and-configuration question and its declined path; bounded waits, cancellation and observe-before-repeat; and Commit… of exactly the selected files with its preconditions and its after-commit comparison. The operation catalogue holds commit and push only.

## Intended verification

Against real temporary repositories with real hooks: a commit hook that passes, one that refuses, one that rewrites a file; a signing configuration; other staged and unstaged changes left alone; a prompt that would need a terminal. The existing isolated-runner tests still pass unchanged. No live GitHub is needed.

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

## Owner look (session 6d00826a…, 2026-11-13)

Rejected: the local draft page, the Edit view, Local Changes with commit and push, the agent commit message; Home 1 accepted with a selection fault. Verdicts verbatim in the [RFC-BPR-0113 ledger](../../verification/RFC-BPR-0113.md) and the [RFC-BPR-0114 ledger](../../verification/RFC-BPR-0114.md). Reworked in the PR window round.

## Owner verdict (2026-11-13)

Session `767b9233…`: `local-changes-commit-push` and `agent-commit-message` accepted ([RFC-BPR-0108 ledger](../../verification/RFC-BPR-0108.md)). Still In Progress: AC06 has no course for a stopped or timed-out push, nor for a review submission completing while a push is in flight.

## AC06 (2026-11-13)

Courses added: a timed-out and a stopped push against a hanging pre-push hook
(`local_changes`), and a review submission completing while a push holds the
author's Git lane (`application_dependable_session_public_path` step 2b). The
author's Git has its own lane; every push outcome reads the hosted branch
again. Evidence: [RFC-BPR-0114 ledger](../../verification/RFC-BPR-0114.md#ac06-courses-2026-11-13).
