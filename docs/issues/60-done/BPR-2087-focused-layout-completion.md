---
schema: "implement-rfc.card.v1"
id: "BPR-2087"
title: "Wait for focused-layout preference completion through the bounded phase helper"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"milestone_plan":{"id":"bpr-2087-focused-layout-completion","path":"docs/rfcs/plans/bpr-2087-focused-layout-completion/milestone-plan.json"},"mandate_revision":"release56-uiforetw-completion-r1","source_baseline":"48bd9b3f2b82128e6b43527812f165eacc4f8b47","allocation_main":"01ea66bf165ab69bc8ad3fcb513624ce11e98ce7","reservation":"docs/issues/_reservations/BPR-2087--BPR-2087--a576089b42d2.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"b6cab7026851e80d21a72128ea6831d42f52d77b","evidence":{"kind":"observation","detail":"OriginalfullRelease andbothcausaldiagnostic failures retained; preferencesbusy at683ms/65calls provesbadcallcutoff. Exactlysamefocusedpredicate/gestures and40s300s; existingbounded5s helper only. Private44ec→9e58 reviewed142content, inventories/fullapp/mutation unchanged.","ledger":"docs/verification/BPR-2087-focused-layout-completion.md","external_evidence":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer56-windows-selected-diff-triage-r1/ui-fixture-fix-bpr2087"}},{"criterion":"AC2","outcome":"pass","revision":"b6cab7026851e80d21a72128ea6831d42f52d77b","evidence":{"kind":"observation","detail":"ActualwithheldWorkbenchEffects negative native8/20.423s rejectsunchangedfocusedpredicate at5008ms/cap5000/calls478/busy1; releasebeforeshutdowncomplete1/cleanupfailed0. Uninstrumentedfull4profile+cutwire onceRelease0/86.527s+Debug0/116.910s, originalnormalflags/context/network/300s40s;1394/1613normalafterseals exact/forests empty. Format22/tidy22/source/adversarial/docs/board/explicitplan0; checkpointunsupportedamendments retainednotgreen.","ledger":"docs/verification/BPR-2087-focused-layout-completion.md","external_evidence":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer56-windows-selected-diff-triage-r1/ui-fixture-fix-bpr2087"}},{"criterion":"AC3","outcome":"pass","revision":"b6cab7026851e80d21a72128ea6831d42f52d77b","evidence":{"kind":"observation","detail":"OwnedCPP01f1 landed/pushedcanonical7442 exact5f9; separatecanonicalpinb6cab bindsROOT-reviewed4foreign2055files+ownfixture, private9e58 distinct andfrozenR56unchanged. Source2087borrow/commands terminal; separateTouroperatorleaseindependent. Counterhighwater2092/foreignwork preserved. Root owns corrective-proofmapping/no originalFAIL relabel/no asset/sourcecohort mutation.","ledger":"docs/verification/BPR-2087-focused-layout-completion.md","external_evidence":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer56-windows-selected-diff-triage-r1/ui-fixture-fix-bpr2087"}}]
---

# Wait for focused-layout preference completion through the bounded phase helper

The normal56 Release focused-layout course and both private diagnostics fail
while the admitted Line Details preference request is still pending. The
all-profile trace reaches 65 calls in 683ms with preferences busy, accepted
Line Details shown and no delivery/refusal/closing flag. Light/dark complete
at590/524ms. A call count does not bound asynchronous completion meaningfully.

## Acceptance

- AC1. Preserve all raw failures and scalar evidence. Replace only the focused
  phase's 64-call cutoff with its existing five-second phase helper. Keep the
  exact focused-layout conjunction, original gestures and 40s/300s budgets.
  Update only the reviewed142 content pin from frozen44ec lineage; inventories,
  full-application and mutation pins remain unchanged.
- AC2. A private negative uses the existing shared Workbench effect hold before
  the original Line Details trigger. It reaches the unchanged focused failure
  at the existing5s bound while busy; release on every exit before shutdown.
  One uninstrumented corrected full four-profile plus cut-wire native course
  in each eligible Release and Debug configuration passes with actual normal flags, context, network and input seals. No retry
  or native qualification credit; all reached/unreached outcomes remain explicit.
- AC3. Format22/tidy/scoped source, docs and board checks pass. Land only owned
  fixture/pin/workflow changes on main, preserve unrelated work and highwater,
  and return task-owned commands/borrows/writers. Root owns fresh common-source
  qualification and publication; no product hang or Qt cause is claimed.
