---
schema: "implement-rfc.card.v1"
id: "BPR-0568"
title: "Implement native-first off-hunk negotiation and fallback"
status: "Done"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0566","BPR-0567"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
acceptance: [{"criterion":"legacy-native","outcome":"pass","revision":"12c5953959fb00b8d90f3afe103a9422ec4b2dda","evidence":{"kind":"command","detail":"GitHub.com/GHES SQLite-backed session courses keep legacy lines native despite cached unsupported evidence. Logs: build/rfc0022/m2-integration-tests.log (13 relevant tests pass; optional global binary sweep lacks unbuilt unrelated binaries), m2-polish-tests.log (3/3), m2-changed-isolation.log (10/10), m2-tidy-final.log, m2-precommit-docs.log."}},{"criterion":"proven-native","outcome":"pass","revision":"12c5953959fb00b8d90f3afe103a9422ec4b2dda","evidence":{"kind":"command","detail":"Canonical native receipts and observed-success cache evidence retain native representation; native and permalink wire suites pass. Logs: build/rfc0022/m2-integration-tests.log (13 relevant tests pass; optional global binary sweep lacks unbuilt unrelated binaries), m2-polish-tests.log (3/3), m2-changed-isolation.log (10/10), m2-tidy-final.log, m2-precommit-docs.log."}},{"criterion":"unknown-negotiation","outcome":"pass","revision":"12c5953959fb00b8d90f3afe103a9422ec4b2dda","evidence":{"kind":"command","detail":"Unknown support uses the authorized native child first; synthetic exact rejection is the only configured fallback transition. Production no-contract course sends no fallback. Logs: build/rfc0022/m2-integration-tests.log (13 relevant tests pass; optional global binary sweep lacks unbuilt unrelated binaries), m2-polish-tests.log (3/3), m2-changed-isolation.log (10/10), m2-tidy-final.log, m2-precommit-docs.log."}},{"criterion":"durable-fallback-authorization","outcome":"pass","revision":"12c5953959fb00b8d90f3afe103a9422ec4b2dda","evidence":{"kind":"command","detail":"Exact native rejection is committed before fallback; fresh exact-anchor evidence admits a parent without a native child. Both admission paths resume across restart. Logs: build/rfc0022/m2-integration-tests.log (13 relevant tests pass; optional global binary sweep lacks unbuilt unrelated binaries), m2-polish-tests.log (3/3), m2-changed-isolation.log (10/10), m2-tidy-final.log, m2-precommit-docs.log."}},{"criterion":"ambiguous-native-recovery","outcome":"pass","revision":"12c5953959fb00b8d90f3afe103a9422ec4b2dda","evidence":{"kind":"command","detail":"Generic422/403/429/503, loss, cancellation and every possibly-sent restart perform no blind retry or fallback. Legacy/logical alias courses prevent a fresh POST in either direction. Logs: build/rfc0022/m2-integration-tests.log (13 relevant tests pass; optional global binary sweep lacks unbuilt unrelated binaries), m2-polish-tests.log (3/3), m2-changed-isolation.log (10/10), m2-tidy-final.log, m2-precommit-docs.log."}},{"criterion":"existing-authority-preserved","outcome":"pass","revision":"12c5953959fb00b8d90f3afe103a9422ec4b2dda","evidence":{"kind":"command","detail":"Host source propagation, coordinator and public composition, duplicate gestures, two target observations per child, stale head, actor and lease refusal, one-shot witnesses, server refresh, original provider/restart and Qt editor checks pass. Logs: build/rfc0022/m2-integration-tests.log (13 relevant tests pass; optional global binary sweep lacks unbuilt unrelated binaries), m2-polish-tests.log (3/3), m2-changed-isolation.log (10/10), m2-tidy-final.log, m2-precommit-docs.log."}}]
---

# Implement native-first off-hunk negotiation and fallback

Extend the existing review mutation coordinator/provider session with one serialized logical-publication state machine.

## Acceptance

- Legacy lines remain native regardless of off-hunk capability evidence.
- Proven supported off-hunk lines publish natively.
- Unknown support uses the user-approved native write as negotiation.
- Native negotiation durably records exact unsupported rejection before fallback
  is prepared. A first attempt with proven off-hunk placement and fresh scoped
  unsupported evidence uses the separate durable evidence-admission path.
- All ambiguous/native-possibly-created outcomes perform recovery without fallback.
- Existing double target observation, stale-head policy, acting identity, transport lease, cancellation, one-use production carrier, and server refresh remain intact.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.

## Import boundary

Imported 2026-09-04 under an import-only direction. The 2026-09-26
implementation mandate r1 activates RFC-BPR-0022; live dependencies and lanes
remain authoritative. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

Cover both first-attempt unsupported-evidence admission and exact native-rejection
negotiation. Persist the authorization before a fallback child exists; never
fabricate a rejected native child or authorize fallback from a lost response.

## Implementation evidence

Source candidate `12c5953959fb00b8d90f3afe103a9422ec4b2dda`: the serialized session
uses schema-4 logical parents and exact child witnesses, with actor/target
observation before admission and immediately before each POST. GitHub.com/GHES
synthetic negotiation, exact-anchor cache scope, lost-response/cancellation
recovery, all durable restart boundaries and both legacy/logical alias directions
pass. Host/coordinator/public composition and the existing provider restart
courses pass. Complete bounded native HTTPS 422 reaches the classifier; truncated
422 remains a transport failure. Focused tidy and documentation checks pass.

Production supplies **no admitted unsupported tuple**. BPR-0573 still owns exact
observed or approved recorded-contract evidence before production fallback can
activate; generic 422 is never authority. No remote mutation occurred here.
[Acceptance](../_checkpoints/BPR-0575/bpr0568-acceptance.json).
