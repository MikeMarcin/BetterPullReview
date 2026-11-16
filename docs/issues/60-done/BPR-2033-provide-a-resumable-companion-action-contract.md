---
schema: "implement-rfc.card.v1"
id: "BPR-2033"
title: "Provide a resumable companion action contract"
status: "Done"
rfcs: ["RFC-BPR-0112"]
metadata: {"criteria":["AC03","AC04","AC07","AC09"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0018","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0045-provide-a-resumable-companion-action-contract.md"}
depends_on: ["BPR-2028", "BPR-2032"]
acceptance: [{"criterion":"AC03","outcome":"pass","revision":"001c08ad1","evidence":{"kind":"command","command":"ctest --preset dev -R 'core_workflow|application_local_draft_workflow_desktop_public_path'","detail":"The synthetic companion, run as the author's terminal would, returns context with an input request, a proposal and repeatable checks; the author answers through Continue and only that action is invoked again (the check's invocation is unchanged); a proposal on a field the author edited after the invocation is recorded stale and Accept refuses it; an earlier invocation's answer is refused"}},{"criterion":"AC04","outcome":"pass","revision":"001c08ad1","evidence":{"kind":"command","command":"ctest --preset dev -R 'core_workflow|application_local_draft_workflow_desktop_public_path'","detail":"A review marker only the upstream tool writes (limitation review_marker), a changed upstream rule (later answer with another fingerprint: upstream_changed), a missing helper and an unsupported dry run are honest unmet requirements; the companion owns the rules (tests/fixtures/workflow_companion/upstream), BPR holds none and writes no marker; the external publisher is not handed the plan while a required check is unmet, and the built-in route refuses while the workflow's publisher is chosen"}},{"criterion":"AC07","outcome":"pass","revision":"001c08ad1","evidence":{"kind":"command","command":"ctest --preset dev -R application_local_draft_workflow_desktop_public_path","detail":"The synthetic organization adapter qualifies preparation with no PR created and no enterprise account or script; the native Workflow area (Overview, Status) shows origin, the selected context a handoff sent, who acts and the declared effect scope, and each outcome as reported (renders workflow-selected, workflow-check-unmet, workflow-follow-up). Owner look filed (session recorded on BPR-2035)"}},{"criterion":"AC09","outcome":"pass","revision":"001c08ad1","evidence":{"kind":"command","command":"ctest --preset dev -R 'application_local_draft_workflow_desktop_public_path|core_workflow'","detail":"The agent action's Ask is marked agent_run_control (agent accent) and the command action's Hand off is not; the imported file asks for builds and the stored agent has them off; a handoff result reads Reported by the companion; not observed by BetterPullReview, and a reported pull request links nothing until GitHub is read"}}]
---

# Provide a resumable companion action contract

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0112](../../rfcs/RFC-BPR-0112-optional-workflow-extensions.md), criteria `AC03`, `AC04`, `AC07`, `AC09`.

Bounded context/proposal/check/input/result interface and synthetic organization companion; upstream compatibility observations stay outside core.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Pause and resume local preparation after author input or changed upstream rules without copying project policy, forging markers, creating a PR or using a publishing command as dry-run.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

Owner amendment, 2026-11-12 ("Mockups; cut Evidence"): RFC-BPR-0109 is Deferred and its closeout BPR-2025 is no longer a dependency; the evidence parts of this card are inactive (see the RFC's amendment). Owner decision, 2026-11-15: RFC-BPR-0109 is Rejected ("it shouldnt be deferred it should be rejected or something stronger so it doesnt come back"); the evidence parts of this card are void, not waiting.

## Owner look workflow-v002 (2026-11-15)

Candidate `BPR-2033-agent-accent-on-actions` (RFC-BPR-0112 AC09) in session
`1d68e30d0ac7b53a07163a1f10777bdf`: **Accepted**, no note.
