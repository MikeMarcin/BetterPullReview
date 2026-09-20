---
schema: "implement-rfc.card.v1"
id: "BPR-0937"
title: "Measure and reduce redundant CLI connection work safely"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"},"mandate":"r20-usable-review-amendments"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"ca000e79","evidence":{"kind":"artifact","detail":"Read-only production CLI probe independently measures discovery0.216/0.396s viewer0.545s capability13.078s versus direct authstatus0.38s; PR metadata3.920s/full48pages85.959s separately."}},{"criterion":"AC2","outcome":"pass","revision":"ca000e79","evidence":{"kind":"artifact","detail":"Nine capability operations and18pre/post live account checks explain most setup cost. No safe duplicate established; executable, account, sandbox, cancellation and authority checks retained. BPR0938 removes UI wait for slower independent observations."}},{"criterion":"AC3","outcome":"pass","revision":"ca000e79","evidence":{"kind":"artifact","detail":"gh_cli_transport and gh_cli_git_smart_transport tests retain identity switching, cancellation and refusal controls. Actual safe and live read-only production compositions succeed; exact timing fields and limitations committed."}}]
---

# Measure and reduce redundant CLI connection work safely

Owner amendment after observing a rendered file diff in the repaired signed r21
UnrealEngine#1773 CLI course. Preserve that success without asking for another
owner test. This card belongs to the existing selected-diff-exact-epoch-pair
milestone; no live GitHub mutations or private source in committed evidence.
Overview folder collapse remains separate in [[BPR-0934]].

## Acceptance

- AC1: Measure configured CLI discovery, account observation, capability probe and connection completion separately from PR pagination and Git acquisition using safe read-only access.
- AC2: Explain the difference from gh auth status and remove demonstrated redundant in-scope work where possible without weakening account checks, sandboxing, authority or cancellation.
- AC3: Deterministic production-composition tests retain before/after identity and failure/refusal behavior; fixed timing evidence records remaining network cost.

## Evidence

The production configured-CLI read-only probe measures discovery version/status 0.216/0.396 s, viewer/transport setup 0.545 s, and nine capability probes 13.078 s, versus direct `gh auth status` 0.38 s. Eighteen pre/post live account checks contribute roughly 7.1 s of capability setup. No duplicate was demonstrated that can safely be removed: rechecking executable support at transport construction and account continuity around each capability remains required. No authority, sandbox, cancellation or identity check was weakened.

The separate PR read took 85.959 s / 48 pages, but metadata was available in 3.920 s. BPR-0938 exposes that useful metadata asynchronously. CLI and smart-Git identity/failure/refusal regressions and the real production-composed Settings/CLI course pass. See the [stage measurements and boundaries](../../verification/rfc-bpr-0013-usable-review-20260920.md).
