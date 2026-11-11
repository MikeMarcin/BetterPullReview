---
schema: "implement-rfc.card.v1"
id: "DRAFT-0004"
title: "The author-Git runner, its one-time question, and Commit"
status: "Backlog"
rfcs: ["RFC-DRAFT-0002"]
depends_on: ["BPR-2020"]
metadata: {"criteria": ["AC01", "AC02", "AC05", "AC06"], "milestone_plan": {"id": "local-draft-authoring-v1", "path": "docs/rfcs/assets/local-draft-authoring-v1/milestone-plan.json"}, "reopened_choices": ["PUBLISH-01"]}
---

# The author-Git runner, its one-time question, and Commit

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0002](../RFC-DRAFT-0002-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md), criteria `AC01`, `AC02`, `AC05`, `AC06`.

The second Git runner with the author's environment, separate in code, state and configuration from the isolated runner; the one-time hooks-and-configuration question and its declined path; bounded waits, cancellation and observe-before-repeat; and Commit… of exactly the selected files with its preconditions and its after-commit comparison. The operation catalogue holds commit and push only.

## Intended verification

Against real temporary repositories with real hooks: a commit hook that passes, one that refuses, one that rewrites a file; a signing configuration; other staged and unstaged changes left alone; a prompt that would need a terminal. The existing isolated-runner tests still pass unchanged. No live GitHub is needed.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).

`PUBLISH-01` was reopened on 2026-11-10; see the [decision record](../DECISIONS.md#decisions) and the [proposed amendment](../AMENDMENTS.md).
