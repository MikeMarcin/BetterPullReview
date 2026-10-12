---
schema: "implement-rfc.card.v1"
id: "BPR-1408"
title: "Persist bounded remembered-connection metadata and selection policy"
status: "Ready"
rfcs: ["RFC-BPR-0071"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan": {"id": "remember-cli-connections-v1", "path": "docs/rfcs/plans/remember-cli-connections-v1/milestone-plan.json"}, "mandate_revision": "remember-cli-r1"}
---

# BPR-1408: Persist bounded remembered-connection metadata and selection policy

RFC base: `46eb49b312b35ef37e69ca4ace7354c04295a6dd`.

## Acceptance

- Canonical bounded metadata stores no credential or publication authority and enforces one connection per server with stable identity and selected order.
- Pure policy rejects stale selection, supports disabled reuse and Forget, and preserves independent profile state.
- Real SQLite reopen, transactional rollback, stale writes, cancellation and malformed/newer/refused storage preserve complete data or refuse without authorization.
- Profile maintenance inventories and purges the metadata with focused format, architecture and documentation checks.
