---
schema: "implement-rfc.card.v1"
id: "BPR-2037"
title: "Prove retained native workspace in the added Codex finding course"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-1974"]
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"mandate_revision":"release53-cleanup-accounting-r1","reservation":"docs/issues/_reservations/BPR-2037--BPR-2037--995529e99f3a.json","milestone_plan":{"id":"bpr-2037-agent-finding-native-retention","path":"docs/rfcs/plans/bpr-2037-agent-finding-native-retention/milestone-plan.json"},"source_baseline":"ac49b0b2f7192f3ea114b898bcd81e60467cf595","allocation_main":"ab605c39e88a3325ffc5e7b6bdfded164ab99baa","external_evidence":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer53-agent-finding-cleanup-r1"}
---

# Prove retained native workspace in the added Codex finding course

Frozen53 full Mac Release fails after46.61s at the unconditional `WorkspaceCleanups == 1` wait, although its one process finished successfully and the host is quiescent. The raw log reports no numeric cleanup count. The added Settings entry chooses the accepted native-session route, which deliberately retains its exact owned workspace; built-in fixture profiles remain disposable. RFC-BPR-0096 and BPR-1974 require retention across turn and window closure.

## Acceptance

- AC1. Preserve the original failure and trace the exact native retention lifecycle before correction. Alter only the owning test: added Codex requires one process/workspace, zero cleanup and cleanup failures, retained owned tree and selected one-turn native history. Built-in profiles keep exactly one cleanup.
- AC2. Corrected original codex-found and ordinary positive courses pass with unchanged wrappers, arguments, environment, all existing finding/draft/MCP/no-publication assertions,45s inner and120s outer limits. The native result records that the old cleanup-one predicate is false. Format22/tidy and scoped source/docs checks pass. Original53 remains failed; no rerun-to-green or projection qualification claim.
- AC3. Only this release-owned test/workflow lands authoritative main with exact commit provenance and monotonic counter; producers and other agents remain untouched. Commands finish and native leases return. Fresh common-source successor full qualification remains separate.

[Current handoff](../_checkpoints/BPR-2037/handoff.md). Native verification is pending coordinator lease; the card remains In Progress until actual proof is supplied.
