---
id: "RFC-BPR-0060"
title: "Removable Agent Adapters and Capability Isolation"
status: "Proposed"
area: "BPR"
milestone: "Review Cycles"
depends_on: ["RFC-BPR-0053", "RFC-BPR-0056", "RFC-BPR-0057", "RFC-BPR-0058", "RFC-BPR-0011", "RFC-BPR-0014", "RFC-BPR-0038"]
pack_version: "review-cycles-v1"
metadata: {"pack": "bpr-review-cycles-v1", "proposed_id_requires_live_reconciliation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# RFC-BPR-0060: Removable Agent Adapters and Capability Isolation

## Outcome and decisions

Make agent independence an actual dependency and package property. Deliver agent-free desktop/CLI configurations which omit agent launch/protocol/supervisor/MCP-only components while retaining local cycles and independently authorized GitHub publication. Local automation has its own enablement; disabling it too leaves GUI plus manual Copy/Save iteration complete.

Inspect and split the current composition graph instead of merely hiding the AI pane. Persistent cycle data and response interpretation cannot depend on an installed adapter. The optional direct Send path is a thin client of the same frozen payload and existing provider/process policies, not the origin of the workflow or a second agent coordinator.

Apply source/export policy consistently across all channels; a CLI is not a covert corporate-policy workaround. Local same-user checks prevent accidental cross-target mistakes but do not sandbox a process with filesystem access. Imported reports, code and Markdown cannot grant authority or execute scripts. Bound IPC, archives and every parsed payload, and keep diagnostics free of review content by default.

Qualify actual installed dependency closure, startup effects, native IPC and removal mid-cycle. Retain observed versus claimed model identity and delivered versus uncertain state. A missing direct adapter or unreachable external harness leaves ordinary human review functional; a forbidden disclosure remains forbidden rather than falling back to a new provider.

## Normative contracts

The detailed contracts [SECURITY-AND-PACKAGING.md](../../../specs/SECURITY-AND-PACKAGING.md) are normative parts of this proposal. The common [domain](../../../specs/DOMAIN.md), [decisions](../../../supporting/DECISIONS.md) and [security boundary](../../../specs/SECURITY-AND-PACKAGING.md) apply. If a live predecessor differs, record a specific compatibility decision; do not quietly implement a weaker alternate workflow.

## Acceptance criteria

- **RC60-01:** An agent-free build/package genuinely omits agent-specific libraries/resources/helpers and starts without agent/provider discovery or credentials.
- **RC60-02:** The complete two-round source/suggestion/Copy-Save/capture/verification/publication-preparation course works with integrations absent.
- **RC60-03:** Local automation is separately removable/disableable and does not remove core GUI/manual iteration or destroy stored cycles.
- **RC60-04:** GUI/CLI/file/clipboard/adapter disclosure uses consistent scope/enterprise policy and never treats a skill/provider claim as permission.
- **RC60-05:** Untrusted source/Markdown/imports cannot execute code, mint authority, alter original rounds or mark feedback verified.
- **RC60-06:** Private data, pins, IPC bounds/identity and diagnostic redaction follow documented native boundaries without false same-user sandbox/encryption claims.
- **RC60-07:** Optional Send uses the exact frozen payload, one selected assignment and the same response model as external/manual channels.
- **RC60-08:** Existing provider/harness/account/host restrictions, explicit session selection and separate credentials remain in force.
- **RC60-09:** Late, failed, uncertain or removed-adapter deliveries preserve the cycle and never auto-resend, auto-fallback, auto-capture or auto-verify.
- **RC60-10:** Native installed-package, startup effects, IPC, attack corpus and removal evidence distinguish real qualification from fixtures/cross-compilation.

## Implementation and evidence

Use the associated authored cards and imported milestone grouping. New names/types/paths
are proposals until reconciled with the live tree. Preserve functional-core/host/Qt
ownership and existing test isolation. Implement focused production-path unit/host/
native checks; use unchanged predecessor evidence where inputs have not changed.
Source inspection, reference fixtures and this archive's validator are not runtime
implementation, native/platform behavior, live provider acceptance or user approval.

Record command/gesture, result, producing revision, environment and limitations for
each acceptance criterion. Required live or attended observations stay open until
actually performed on authorized targets. Do not start models, alter shared skills,
write live repositories, or bypass corporate policy merely because a test card exists.
Keep historical RFC status/evidence unchanged; update current architecture forward.

See the [integration map](../../../supporting/INTEGRATION-MAP.md), [acceptance matrix](../../../verification/ACCEPTANCE-MATRIX.md) and [product verification contract](../../../verification/PRODUCT-VERIFICATION.md).
