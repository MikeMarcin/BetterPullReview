---
schema: "implement-rfc.card.v1"
id: "BPR-1410"
title: "Verify and close RFC-BPR-0071"
status: "Done"
rfcs: ["RFC-BPR-0071"]
depends_on: ["BPR-1409"]
priority: "P1"
metadata: {"milestone_plan": {"id": "remember-cli-connections-v1", "path": "docs/rfcs/plans/remember-cli-connections-v1/milestone-plan.json"}, "mandate_revision": "remember-cli-r1"}
closeout_for: ["RFC-BPR-0071"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"62bd6a5a0aefbca78a8678a6884ccd6b7575e885","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0071.md; docs/issues/_checkpoints/remember-cli-connections-v1/m3-verification.json; exact-source15/15 command capture in m3-focused-evidence.json. Final test-only follow-up recheck: m3-final-candidate-evidence.json."}},{"criterion":"AC02","outcome":"pass","revision":"62bd6a5a0aefbca78a8678a6884ccd6b7575e885","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0071.md; docs/issues/_checkpoints/remember-cli-connections-v1/m3-verification.json; exact-source15/15 command capture in m3-focused-evidence.json. Final test-only follow-up recheck: m3-final-candidate-evidence.json."}},{"criterion":"AC03","outcome":"pass","revision":"62bd6a5a0aefbca78a8678a6884ccd6b7575e885","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0071.md; docs/issues/_checkpoints/remember-cli-connections-v1/m3-verification.json; exact-source15/15 command capture in m3-focused-evidence.json. Final test-only follow-up recheck: m3-final-candidate-evidence.json."}},{"criterion":"AC04","outcome":"pass","revision":"62bd6a5a0aefbca78a8678a6884ccd6b7575e885","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0071.md; docs/issues/_checkpoints/remember-cli-connections-v1/m3-verification.json; exact-source15/15 command capture in m3-focused-evidence.json. Final test-only follow-up recheck: m3-final-candidate-evidence.json."}},{"criterion":"AC05","outcome":"pass","revision":"62bd6a5a0aefbca78a8678a6884ccd6b7575e885","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0071.md; docs/issues/_checkpoints/remember-cli-connections-v1/m3-verification.json; exact-source15/15 command capture in m3-focused-evidence.json. Final test-only follow-up recheck: m3-final-candidate-evidence.json."}},{"criterion":"AC06","outcome":"pass","revision":"62bd6a5a0aefbca78a8678a6884ccd6b7575e885","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0071.md; docs/issues/_checkpoints/remember-cli-connections-v1/m3-verification.json; exact-source15/15 command capture in m3-focused-evidence.json. Final test-only follow-up recheck: m3-final-candidate-evidence.json."}},{"criterion":"Every RFC-BPR-0071 AC01–AC06 is demonstrated against the RFC baseline through composed macOS fixture courses, including fresh processes and last-window closure.","outcome":"pass","revision":"62bd6a5a0aefbca78a8678a6884ccd6b7575e885","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0071.md; docs/issues/_checkpoints/remember-cli-connections-v1/m3-verification.json; exact-source15/15 command capture in m3-focused-evidence.json. Final test-only follow-up recheck: m3-final-candidate-evidence.json."}},{"criterion":"Tokenless execution, publication consent, global CLI state, storage/refusal and profile maintenance canaries pass; all owning contracts describe implemented scope.","outcome":"pass","revision":"62bd6a5a0aefbca78a8678a6884ccd6b7575e885","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0071.md; docs/issues/_checkpoints/remember-cli-connections-v1/m3-verification.json; exact-source15/15 command capture in m3-focused-evidence.json. Final test-only follow-up recheck: m3-final-candidate-evidence.json."}},{"criterion":"Focused integration and documentation evidence cover the exact final candidate; required acceptance remains explicit.","outcome":"pass","revision":"62bd6a5a0aefbca78a8678a6884ccd6b7575e885","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0071.md; docs/issues/_checkpoints/remember-cli-connections-v1/m3-verification.json; exact-source15/15 command capture in m3-focused-evidence.json. Final test-only follow-up recheck: m3-final-candidate-evidence.json."}}]
---

# BPR-1410: Verify and close RFC-BPR-0071

RFC base: `46eb49b312b35ef37e69ca4ace7354c04295a6dd`.

## Acceptance

- Every RFC-BPR-0071 AC01–AC06 is demonstrated against the RFC baseline through composed macOS fixture courses, including fresh processes and last-window closure.
- Tokenless execution, publication consent, global CLI state, storage/refusal and profile maintenance canaries pass; all owning contracts describe implemented scope.
- Focused integration and documentation evidence cover the exact final candidate; required acceptance remains explicit.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** In a composed macOS course, choose a CLI account, accept read trust and connect, then quit and start a new process on the same profile. It accepts the same host/login/numeric account without discovery, account selection or another read-trust question. Repeat after explicitly closing the last window, with no retained review destination. | Pass: fresh OS processes, explicit inactive account selection, empty-inventory automatic restore, closure of every retained window and fresh profile restart. Exact source c27eb1356; m3-focused-evidence.json. |
| **AC02:** With two CLI accounts on one host, a native profile on that host and a second server, restart and exact-host PR navigation follow the saved choices despite changed discovery/configuration order and global CLI active account. Private content and connection identities remain correctly scoped; a sibling window shares the accepted connection. | Pass: two named accounts, native/CLI same-server replacement, distinct GitHub/GHES identities, exact-host private PR navigation and one shared connection across siblings. CLI process fixture preserves changed global active-account state. |
| **AC03:** A missing or mismatching saved account, unavailable tool, unsupported platform or failed validation cannot accept another authority or expose its private state. The failure is visible, the choice survives, and a subsequent successful retry restores that exact connection. | Pass: mismatch/numeric-only mismatch and unavailable capability visibly refuse without private Recent/snapshot or fallback; saved choice survives and later exact retry succeeds. Unsupported-platform limit unchanged; no native Windows/Linux support claimed. |
| **AC04:** Sign out, Forget server and successful replacement have their defined restart results, including a previous device grant and late completions. Cancellation or unsuccessful replacement preserves the prior durable choice; another BPR profile is unaffected. | Pass: Sign out beside old durable device grant and pending native authorization stays disabled after late completion/restart; pending GHES validation cannot undo Forget, sibling retirement or another server. Cancelled/offline native replacement preserves choice; successful native replacement settles existing grant durability and restarts with fresh ID. Separate-profile isolation passes. |
| **AC05:** Real storage reopen, interrupted writes, refused saves and refused loads demonstrate the defined durability and session-only behavior. Fresh and upgraded profiles without a selection do not run CLI implicitly. Profile maintenance accounts for the new metadata. | Pass: prompt real-store save, rival-lock session-only/failed-disable notices, newer-schema byte preservation, missing-metadata negative control, reopen/stale/cancellation/interrupted-write and exact profile maintenance contracts. |
| **AC06:** Fixture canaries demonstrate that persistence and restored execution retain the existing tokenless boundary and global CLI state; restored read consent cannot bypass ordinary publication confirmation. | Pass: tokenless/private per-call configuration and unchanged fixture global CLI state canaries; restored read trust still requires separate publication confirmation, retry confirmation and cancellation sends nothing. |

## Qualification and closeout

All AC01–AC06 and the three milestone exits are accepted under mandate
`remember-cli-r1`, including the accompanying proposed CLI amendment.
Source: `62bd6a5a0aefbca78a8678a6884ccd6b7575e885`.
[Acceptance map](../../verification/RFC-BPR-0071.md),
[verification ledger](../_checkpoints/remember-cli-connections-v1/m3-verification.json)
and [exact-source capture](../_checkpoints/remember-cli-connections-v1/m3-focused-evidence.json).
The composed courses found and repaired retiring-model persistence, stopped
exact-host validation and stale native capability identity.

No remaining authorized criterion or required owner/native/independent decision.
Evidence remains macOS network-denied fixtures; installed package/native vault
and Windows/Linux support are not claimed. BPR-1373 stays separately parked.
