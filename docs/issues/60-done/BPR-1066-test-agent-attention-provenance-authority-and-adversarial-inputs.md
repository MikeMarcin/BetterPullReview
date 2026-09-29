---
schema: "implement-rfc.card.v1"
id: "BPR-1066"
title: "Test agent attention provenance authority and adversarial inputs"
status: "Done"
rfcs: ["RFC-BPR-0046"]
depends_on: ["BPR-1064", "BPR-1065"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-15","milestone":"pr-tour-skill","acceptance":["AC46-07","AC46-08","AC46-09"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC46-07","outcome":"pass","revision":"0799fa1660d2f824eca2a93a039199d3ff174d3a","evidence":{"kind":"command","detail":"tour_cli_composition through the shipped CLI and a real store: reads, source reads, preview and context never mark a stop visited or ready; a body-free skeleton cannot be readied; readiness is one stop against the revision just read (no batch, ready-all, observed origin or stale revision) and is recorded as relayed; outline skeletons stay suggested and unvisited. Provenance is not overstated: identity is reported unverified."}},{"criterion":"AC46-08","outcome":"pass","revision":"0799fa1660d2f824eca2a93a039199d3ff174d3a","evidence":{"kind":"command","detail":"Publish refuses eight approval/identity flags, a machine publish is interaction_required with the exact terminal command and says stdin yes is not authority; the existing PTY course covers the real confirmation path; no CLI path writes GitHub or the database. No agent-host approval bridge exists, so the terminal fallback is the only publication path."}},{"criterion":"AC46-09","outcome":"pass","revision":"0799fa1660d2f824eca2a93a039199d3ff174d3a","evidence":{"kind":"command","detail":"Hostile text is escaped evidence and inert as a replacement; results naming another target, extra keys, repeated keys or trailing text are invalid; tickets answer only for their own draft; other stops' comments, outline prompts, earlier text and undo history are never exported; an oversized request is refused without disturbing a pending one; a host policy (manual-only, and fail-closed on unknown values) withholds export whatever identity is claimed while manual authoring works; interrupted sessions leave the draft unchanged and are superseded. Real agent/provider verification is not offered by BPR and not claimed."}}]
---

# BPR-1066: Test agent attention provenance authority and adversarial inputs

Pack alias: **PACK-PRTOUR-15** (live `BPR-1066`, allocated at import).

## Outcome

Add hostile-source and conversation-boundary tests for read≠visit, agent-relayed current-content completion, request-scoped rewrites, outlined skeletons, explicit publish, no manufactured authority and no provider-policy bypass. Keep required host/provider verification separate from claimed text metadata.

## Existing ownership seams

Shared CLI command admission and parent publication guard; skill command trace harness; assistance context policy.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Attempts to batch-ready, pipe yes, call gh directly, set fake approval/provider, return another target or leak private notes fail in the relevant boundary. Interrupted skill sessions recover shared drafts; unsupported approval preserves terminal fallback.

Acceptance ownership: AC46-07, AC46-08, AC46-09

## Execution boundary

Milestone: **pr-tour-skill**. Pack alias PACK-PRTOUR-15 was allocated as live `BPR-1066` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
