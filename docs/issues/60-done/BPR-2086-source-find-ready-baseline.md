---
schema: "implement-rfc.card.v1"
id: "BPR-2086"
title: "Wait for source geometry before measuring Find scroll work"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"milestone_plan":{"id":"bpr-2086-source-find-ready-baseline","path":"docs/rfcs/plans/bpr-2086-source-find-ready-baseline/milestone-plan.json"},"mandate_revision":"release56-source-find-ready-r1","source_baseline":"48bd9b3f2b82128e6b43527812f165eacc4f8b47","allocation_main":"9323372ba","reservation":"docs/issues/_reservations/BPR-2086--BPR-2086--1c67e2892878.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c6e7832509612709a8cb4fa48ed15e68325978e7","evidence":{"kind":"observation","detail":"Original56threeDebugfailures retained. Once-only scalarprobe pending_before1/pane50to52 only atlineNumbersHide; exactoneprebaselinepublicgeometry-readywait. RemovingoneaddedlinerecoversoriginalTUbytes; alloriginalfourcounteroracles/gestures/180ms/120s unchanged.","ledger":"docs/verification/BPR-2086-source-find-ready-baseline.md"}},{"criterion":"AC2","outcome":"pass","revision":"c6e7832509612709a8cb4fa48ed15e68325978e7","evidence":{"kind":"observation","detail":"MeaningfulprivatepostbaselineFontChange negative native8/28.61s atORIGINALcounteroracle, geometry18to19/pane44to46/other2stable; collector2 separatelyreconciled/no rerun. Corrected6nativepositivePASSonce: Release29.336/30.147/32.632s, Debug103.144/109.339/117.848s alloriginal120s/actualCLmodule+orderedLINK/normalnetwork/privateoutputs; expectedJUnit+anchoredprogrammarkers. Format22/tidy22/docs+67tests/board/explicitplanPASS. Generalcheckpointunsupportedamendments failure retained, toolingunchanged.","ledger":"docs/verification/BPR-2086-source-find-ready-baseline.md","external_evidence":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer56-windows-selected-diff-triage-r1/source-find-baseline-fix"}},{"criterion":"AC3","outcome":"pass","revision":"c6e7832509612709a8cb4fa48ed15e68325978e7","evidence":{"kind":"observation","detail":"Ownedallocation9323372ba/NEXT2087 andplan/claim mirroredmain. Sourcee64118f33 landed/pushedc6e783250, canonicalforeignsource preserved and frozenengineeringbranch noforeignintake. Protected1202normalinputs unchanged, privatecandidatehashes exact, taskReady0/ownedforests empty; SourceFindborrowterminal. SeparateUIobserverleasecontinuesindependently; successorqualification/publicationremainroot.","ledger":"docs/verification/BPR-2086-source-find-ready-baseline.md"}}]
---

# Wait for source geometry before measuring Find scroll work

All three normal56 Windows Debug scales fail the same four-counter invariant.
The once-only 200% scalar diagnostic records geometry pending before the baseline
and only pane measurements changing 50 to 52, with geometry/index/matching/history
unchanged after hiding line numbers. The fixture must finish pre-existing work
before measuring work caused by its scroll/navigation gestures.

## Acceptance

- AC1. Retain original failures and scalar evidence. After the original 180ms
  settle and before the rectangle/statistics baseline, wait through the existing
  bounded helper for public `!geometry_pending()`. Keep every original gesture,
  workload, rectangle assertion and all four exact counter equalities.
- AC2. A private negative control delivers a real font-change geometry event
  after the baseline and is rejected by the unchanged counter oracle. Corrected
  normal Release and Debug courses at all three scales use actual normal flags,
  frozen product inputs, isolation, properties and original budgets. Keep actual
  Debug cutoffs explicit under standing policy, with no functional pass credit.
  Format22, tidy and scoped documentation/board checks pass.
- AC3. Land only owned test/workflow commits on main, preserving foreign work and
  counter highwater. Frozen56 producers remain unchanged; all owned processes and
  writers are accounted for and leases returned. Engineering proof does not replace
  fresh common-source qualification.
