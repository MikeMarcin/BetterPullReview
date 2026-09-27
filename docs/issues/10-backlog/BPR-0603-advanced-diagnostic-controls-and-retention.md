---
schema: "implement-rfc.card.v1"
id: "BPR-0603"
title: "Add advanced diagnostic retention privacy and controls"
status: "Backlog"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-1215", "BPR-1216", "BPR-1217", "BPR-1218", "BPR-1219"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
---

# Add advanced diagnostic retention privacy and controls

The owner reauthorized the full advanced phase on 2026-09-26. Mandate r2
requires each accepted milestone to land on main. BPR-1215 through BPR-1219
split the remaining work; no live sign-in or PR mutation is authorized.

## Remaining scope

Implement RFC-BPR-0027's rotation/age/aggregate retention; typed identity
anonymization; Standard/Verbose, privacy and Off controls; recent-context ring;
previewed anonymized/identifying exports; Open logs and Clear UI; expanded
operation coverage and applicable Settings/profile integration. Preserve the
foundation's credential filtering and no-authentication-body rule. The logger now has POSIX and Windows file adapters; qualify the revised path
on each claimed platform rather than inferring native support from source. The initial fixed log
uses typed availability states without a UI indicator.

## Acceptance and verification

Satisfy the full numbered RFC criteria and BPR-0601's planned direct checks,
reusing unchanged foundation evidence where valid. Allocate needed slices and
update closeout dependencies before implementation. Record actual native and
attended evidence; no full RFC or platform claim comes from phase 1.

Advanced acceptance is pending under the adopted milestone plan. Unchanged
foundation and BPR-0971 audit evidence remain reusable within their original scope.
