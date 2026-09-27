---
schema: "implement-rfc.card.v1"
id: "BPR-0603"
title: "Add advanced diagnostic retention privacy and controls"
status: "Done"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-1215", "BPR-1216", "BPR-1217", "BPR-1218", "BPR-1219"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
acceptance: [{"criterion":"AC-01: Standard/Verbose, independent identifying-detail privacy, Off, Open logs, Clear and export controls are reachable while signed out; accepted settings survive restart without increasing disclosure on invalid data.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"PASS: automated controls/restart/invalid-value courses pass; files human Accept at cursor23 and revised controls Accept at cursor34; producing revisions `75700f4d`, `92c98361`, `0fb249a2`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-02: A composed failed GitHub sign-in yields a correlated local log with useful sanitized unknown GraphQL error detail, readable directly without UI copying, while UI wording and session/retry authority remain unchanged.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"Composed unfamiliar-error failure reaches correlated JSONL; useful classified prose, unchanged UI failure and explicit retry pass; fixture only; producing revisions `92c98361`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-03: Forbidden credentials and review content never reach queues, sinks, recent context, exports or fallback output under any detail/privacy combination, including malformed provider data and sanitizer failure.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"M1–M4 canary, fixed-capacity encoding, queue/ring/export and fail-closed parser checks pass within the exercised inputs; producing revisions `ac64e5ad`, `75700f4d`, `3fd1224e`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-04: Standard and Verbose retain their declared distinct detail; privacy changes, Off, Clear and sign-out honor acknowledged barriers and do not retain hidden transient context.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"M3 barriers pass; M5 fixes deferred refresh and stamps captured epoch generation; native verification recorded below; producing revisions `75700f4d`, `92c98361`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-05: Anonymized and identifying-detail exports use the exact previewed scope, preserve useful event joins, handle malformed records safely, disclose irreversible capture-time redaction and never upload automatically.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"PASS: frozen rich exports and direct Help preview course pass; revised export human Accept at cursor34; producing revisions `3fd1224e`, `189d24f9`, `0fb249a2`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-06: Event, queue, ring, file, aggregate and age bounds hold under overload, error storms, concurrent sessions, disk-full and rotation failures without blocking core decisions or Sign out.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"M2 bounds/rotation/concurrency and M5 actual isolated macOS ENOSPC pass; native Windows courses pass; producing revisions `3381a02c`, `92c98361`, `7ab11993`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-07: Startup, cancellation, shutdown, partial-tail recovery and logger failure produce truthful availability/loss status without claiming unflushed events survived a crash.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"Startup/cancel/shutdown and partial-tail checks pass; native killed-process restart preserves only explicitly flushed records; producing revisions `3381a02c`, `92c98361`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-08: Core/host/adapter/Qt boundaries, explicit Quill lifetime, safe file ownership and scope separation hold through the shipped composition, with no raw logging bypass in protected transports.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"Architecture and composed lifetime/ownership checks pass; M5 repairs tests-disabled state visibility and Windows active-file sharing; producing revisions `189d24f9`, `7ab11993`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-09: The selected Quill dependency is pinned with license/provenance and packaging evidence; each claimed platform has native file, lifecycle and diagnostic-control evidence.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"PASS: pinned Quill/package proof, native macOS/Windows/Linux developer courses and all three human judgments accepted; release/package limits retained below; producing revisions `189d24f9`, `7ab11993`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}}]
---

# Add advanced diagnostic retention privacy and controls

The owner reauthorized the full advanced phase on 2026-09-26. Mandate r2
requires each accepted milestone to land on main. BPR-1215 through BPR-1219
split the remaining work; no live sign-in or PR mutation is authorized.

## Accepted implementation

M1–M4 landed structured JSONL, per-session bounded retention, typed identity
anonymization, Standard/Verbose, privacy and Off controls, recent context,
frozen anonymized/identifying exports, and signed-out Open logs/Clear UI.
M5 qualified native behavior and packaging, fixed observed portability and
revocation gaps, and received all three attended Accept judgments at cursor34. The provider sanitizer and
no-authentication-body rule are unchanged.

The complete evidence map is [BPR-0601](../30-in-progress/BPR-0601-close-rfc-0027.md) and the
[verification ledger](../../verification/RFC-BPR-0027.md). BPR-1219 is Done. The aggregate product acceptance is complete;
BPR-0601 retains the separate canonical-order terminal RFC hold.

## Acceptance and verification

Satisfy the full numbered RFC criteria and BPR-0601's planned direct checks,
reusing unchanged foundation evidence where valid. Allocate needed slices and
update closeout dependencies before implementation. Record actual native and
attended evidence; no full RFC or platform claim comes from phase 1.

All nine product criteria pass under the adopted plan, as mapped on BPR-0601.
Unchanged foundation and BPR-0971 evidence remain bound to their original scope.
Accepted M5 source was pushed to main at `e2eec1d69340bbf0f519b6cae28eb462fb40966e`;
final public integration passed 4/4 and the documentation gate passed.
The final ledger preserves exact native/package/crash limits and human identities.
