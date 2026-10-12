---
schema: "implement-rfc.card.v1"
id: "BPR-1408"
title: "Persist bounded remembered-connection metadata and selection policy"
status: "Done"
rfcs: ["RFC-BPR-0071"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan": {"id": "remember-cli-connections-v1", "path": "docs/rfcs/plans/remember-cli-connections-v1/milestone-plan.json"}, "mandate_revision": "remember-cli-r1"}
acceptance: [{"criterion":"Canonical bounded metadata stores no credential or publication authority and enforces one connection per server with stable identity and selected order.","outcome":"pass","revision":"44a909e721ab461bbdfe986515a66d285f1fc5f6","evidence":{"kind":"artifact","detail":"docs/issues/_checkpoints/remember-cli-connections-v1/m1-verification.json; exact focused output in m1-focused-evidence.json"}},{"criterion":"Pure policy rejects stale selection, supports disabled reuse and Forget, and preserves independent profile state.","outcome":"pass","revision":"44a909e721ab461bbdfe986515a66d285f1fc5f6","evidence":{"kind":"artifact","detail":"docs/issues/_checkpoints/remember-cli-connections-v1/m1-verification.json; exact focused output in m1-focused-evidence.json"}},{"criterion":"Real SQLite reopen, transactional rollback, stale writes, cancellation and malformed/newer/refused storage preserve complete data or refuse without authorization.","outcome":"pass","revision":"44a909e721ab461bbdfe986515a66d285f1fc5f6","evidence":{"kind":"artifact","detail":"docs/issues/_checkpoints/remember-cli-connections-v1/m1-verification.json; exact focused output in m1-focused-evidence.json"}},{"criterion":"Profile maintenance inventories and purges the metadata with focused format, architecture and documentation checks.","outcome":"pass","revision":"44a909e721ab461bbdfe986515a66d285f1fc5f6","evidence":{"kind":"artifact","detail":"docs/issues/_checkpoints/remember-cli-connections-v1/m1-verification.json; exact focused output in m1-focused-evidence.json"}}]
---

# BPR-1408: Persist bounded remembered-connection metadata and selection policy

RFC base: `46eb49b312b35ef37e69ca4ace7354c04295a6dd`.

## Acceptance

- Canonical bounded metadata stores no credential or publication authority and enforces one connection per server with stable identity and selected order.
- Pure policy rejects stale selection, supports disabled reuse and Forget, and preserves independent profile state.
- Real SQLite reopen, transactional rollback, stale writes, cancellation and malformed/newer/refused storage preserve complete data or refuse without authorization.
- Profile maintenance inventories and purges the metadata with focused format, architecture and documentation checks.

## Verification and execution record

Accepted the bounded `connection-metadata` milestone against product source
`44a909e721ab461bbdfe986515a66d285f1fc5f6` and registry integration
`6ebd33e57479a24aed91b930624bab030a436045`. The factory/port courses reopen
real private SQLite state and cover stale sign-out writes, rollback after full
uncommitted writes, process exit before commit, malformed/newer data and profile
purge. Four exact-candidate tests pass; seven focused integration checks pass.
A premature-COMMIT negative control fails the cancellation rollback assertion;
restoring the source restores the passing result. Format 22, focused tidy,
architecture and documentation evidence are recorded in
[the milestone checkpoint](../_checkpoints/remember-cli-connections-v1/README.md).

Application composition, success admission, prompt save scheduling, Quit drain,
sign-out/device resumption and every full RFC criterion remain with BPR-1409
and BPR-1410. No runtime restoration or owner acceptance is inferred here.
