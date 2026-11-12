---
schema: "implement-rfc.card.v1"
id: "BPR-2129"
title: "Linux Release compilation rejects the unused ownership-upgrade boundary parameter"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux", "win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2129--BPR-2129--e8a3fc0ae13e.json","source_baseline":"15e3f61ae2ed99d83d2fba4a028f6c61f8c5994f","mandate_revision":"bpr2129-nonmac-upgrade-parameter-r1","related_cards":["BPR-2118","BPR-2125"],"milestone_plan":{"id":"bpr-2129-nonmac-upgrade-parameter","path":"docs/rfcs/plans/bpr-2129-nonmac-upgrade-parameter/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"35e5cc19f3adfab33e3b22ce4ac51ef4bd47d63d","evidence":{"kind":"artifact","detail":"Exactly one inserted static_cast<void>(aLive) in non-Mac #else; removing it recovers all original engine bytes55e81e… from corrected5c7892…. Mac body/version loop/identity/authority/migration semantics unchanged; Windows shares that branch."}},{"criterion":"AC2","outcome":"pass","revision":"35e5cc19f3adfab33e3b22ce4ac51ef4bd47d63d","evidence":{"kind":"command","detail":"Actual full /usr/bin/c++ GCC15.2.0 Release TU compile exits0 with original -O3/-Werror and every warning flag; only private path relocations. Private object975672bytes SHA354567df…;893 enumerated inputs and protected producer paths exact, normal source59bd7 stays clean, owned compiler group terminal. Original compile failure and setup-only attempts retained."}},{"criterion":"AC3","outcome":"pass","revision":"35e5cc19f3adfab33e3b22ce4ac51ef4bd47d63d","evidence":{"kind":"command","detail":"Private documentation, clang-format22, conditional identifiers, actual app inventory/adversarial guard and explicit owned MASK965 plan pass. No src/app/pin/hook changes; global unrelated2120 plan limitation retained. Native MSVC/full native release graph checks remain coordinator qualification."}},{"criterion":"AC4","outcome":"pass","revision":"35e5cc19f3adfab33e3b22ce4ac51ef4bd47d63d","evidence":{"kind":"artifact","detail":"Metadata76088daf4 and source c42dc83e10f14ed3ea7e154e5f4bc76034b31974 landed on current main, source private35e5cc19f. Current foreign7c2a6fa work and NEXT2130 preserved; no producer59/W edits, release allocation, signing/package/publication or remote push. Native lease returned and commands terminal."}}]
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

## Verification

[Exact source change and actual native compiler proof](../../verification/BPR-2129-nonmac-upgrade-parameter.md).
