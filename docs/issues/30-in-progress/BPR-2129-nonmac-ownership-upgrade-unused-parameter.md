---
schema: "implement-rfc.card.v1"
id: "BPR-2129"
title: "Linux Release compilation rejects the unused ownership-upgrade boundary parameter"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux", "win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2129--BPR-2129--e8a3fc0ae13e.json","source_baseline":"15e3f61ae2ed99d83d2fba4a028f6c61f8c5994f","mandate_revision":"bpr2129-nonmac-upgrade-parameter-r1","related_cards":["BPR-2118","BPR-2125"],"milestone_plan":{"id":"bpr-2129-nonmac-upgrade-parameter","path":"docs/rfcs/plans/bpr-2129-nonmac-upgrade-parameter/milestone-plan.json"}}
acceptance: []
---

# Linux Release compilation rejects the unused ownership-upgrade boundary parameter

The native Build59 Linux Release compile exited1 with GCC15.2 and
`-Werror=unused-parameter`: `upgrade_version_one` uses its `aLive` argument
only in its macOS branch. Linux and Windows intentionally preserve the exact
stored identities and change only the record version, leaving that parameter
unused. The failure prevents native qualification.

## Correction

Explicitly consume `aLive` with `static_cast<void>(aLive)` in the existing
non-Mac branch. Keep the version loop, every macOS check, all identity/authority
rules and compiler warnings unchanged. No warning suppression or new runtime
behavior is needed.

## Acceptance

- AC1: The only production change is the explicit discard inside the existing non-Mac branch. The macOS body and non-Mac version loop are byte-identical; Windows shares the corrected branch.
- AC2: The full actual native GCC15.2 Release translation unit compiles with the original warning flags and pinned includes/dependencies, using the private corrected source and exclusively private object, dependency, module and temporary outputs. Preserve the original Build59 failure and source/input receipts.
- AC3: Focused format, conditional/source and documentation checks pass against the private candidate; app composition source/pin and test hooks remain unchanged. Native MSVC and full release qualification remain separate obligations.
- AC4: Land only this issue's correction and bookkeeping on current main; preserve external main work, counters, frozen Build59 producer/staging and its rejected evidence. Return exact private/canonical mapping and an empty command state without allocating a release cohort or publishing.
