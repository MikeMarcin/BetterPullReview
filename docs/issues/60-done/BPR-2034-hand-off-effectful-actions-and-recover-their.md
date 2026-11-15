---
schema: "implement-rfc.card.v1"
id: "BPR-2034"
title: "Hand off effectful actions and recover their outcomes"
status: "Done"
rfcs: ["RFC-BPR-0112"]
metadata: {"criteria":["AC05","AC06","AC08"],"resolved_choices":["ACTIONS-01"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0019","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0046-hand-off-effectful-actions-and-recover-their.md"}
depends_on: ["BPR-2031", "BPR-2033"]
acceptance: [{"criterion":"AC05","outcome":"pass","revision":"001c08ad1","evidence":{"kind":"command","command":"ctest --preset dev -R 'core_workflow|application_local_draft_workflow_desktop_public_path'","detail":"With the workflow's publisher chosen the built-in creation refuses (external_publisher) and GitHub saw no creation; Hand off the plan writes the recorded plan, confirmed by its digest, and the companion's log holds exactly that digest, title and body; BPR starts no program (the course runs the named argument array as the author's terminal); the area says the program runs under the author's own terminal and is neither started nor restricted by BetterPullReview; agent actions are refused for effectful slots"}},{"criterion":"AC06","outcome":"pass","revision":"001c08ad1","evidence":{"kind":"command","command":"ctest --preset dev -R 'core_workflow|application_local_draft_workflow_desktop_public_path'","detail":"A lost publisher answer is recorded uncertain; a new handoff is refused until Check GitHub reads the branch's pull requests; nothing found reopens it under a new confirmation; a reported pull request links only after the read, as one BPR did not create (a different base is refused); a follow-up with an updated ticket and a failed upload keeps both results, refuses a retry until reconciled, and the retry carries the made ticket so only the upload is made; the pull request link is untouched by the follow-up failure"}},{"criterion":"AC08","outcome":"pass","revision":"001c08ad1","evidence":{"kind":"command","command":"ctest --preset dev -R 'application_local_draft_publication_desktop_public_path|application_local_draft_workflow_desktop_public_path|application_local_draft_desktop_public_path'","detail":"With no workflow selected (the Workflow area composed and empty) the built-in journey from the worktree through Create Draft PR to a linked draft pull request passes unchanged on the scripted catalog; in the workflow course an unrelated draft keeps no workflow while the configured draft uses its integration. The live BPR-Test run of RFC-BPR-0111 AC01 (#179) is reused: the built-in route changed only by the external_publisher guard, which a draft without a workflow never meets"}}]
---

# Hand off effectful actions and recover their outcomes

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0112](../../rfcs/RFC-BPR-0112-optional-workflow-extensions.md), criteria `AC05`, `AC06`, `AC08`.

Interactive handoff of effectful actions to the configured harness, current-plan authority and external result reconciliation, plus end-to-end built-in independence after native publication is delivered. No BPR-supervised effectful action.

## Intended verification

Demonstrate one publisher, truthful runtime capability, uncertain outcome recovery and independent remaining follow-up with a fixture companion. Also complete worktree-to-GitHub-draft publication with extensions absent and unselected; separately configured drafts retain their optional integration.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

`ACTIONS-01` was answered on 2026-11-10: effectful actions use an interactive handoff. See the [decision record](../../rfcs/assets/v0-5-0-prerelease-targets-v1/local-drafts/DECISIONS.md#resolved-product-choices).
