# BPR-2087: bounded focused-layout preference completion

Mandate `release56-uiforetw-completion-r1`; frozen56 source
`48bd9b3f2b82128e6b43527812f165eacc4f8b47`, cohort `bab13d48…`.
Original full Release fails the focused-layout predicate in high-contrast dark;
the first private course fails earlier in high-contrast light. The all-profile
observer then captures the actual request: before busy0/quiescent1, after the
single original trigger busy1, and still busy1 at 683ms/65phase calls. Accepted,
window and host Line Details remain shown0, checked1, delivery/refusal/closing
flags0; persisted layout count4→5. Light/dark complete at590/524ms. This proves
that the fixture's call-count cutoff expires during admitted asynchronous work;
it does not prove a storage hang or Qt defect.

## Correction and unchanged contract

Private CPP `01f1bd0bd677ef214488433f33add5350187f63e`, exact SHA-256
`5f9bd72e0c7a4d42907f07dcd5da66d763c414527a7ae644847d04ac623d740d`.
Only the focused phase replaces its 64-call cutoff with the existing five-second
`wait`/phaseWait helper. `advance` remains its sole reset. The original focused
conjunction is byte-identical, as are gestures, profile40s and CTest300s budgets.
Failure facts are emitted through the existing helper. No production policy,
new API, extra gesture, event pump or diagnostic instrumentation is added.

The changed functions lie inside the existing BPR_BUILD_TESTING guard. The TU
still appears in composition sources; no claim is made that its filename is
absent from tests-disabled graphs. Its changed function bodies are excluded
when that macro is absent. Outside-guard code remains unchanged. The required
142-content test-contract pin is private `44ec…`→`9e58…` in `5f0620b9e`; ordered
142/complete197 inventories and full-app3ba90/mutation6a1c pins remain exact.

## Causal control and native proof

One private negative arms the existing shared Workbench effect hold immediately
before the original Line Details trigger. WorkbenchEffects is a reference to
Profile->WorkbenchEffects, the preferences host's actual executor. The unchanged
focused predicate rejects the withheld completion at5008ms/cap5000/calls478,
busy1; the control releases before the original failure/shutdown. Actual native
CTest8/20.423s is expected rejection, with shutdown_complete1/cleanup_failed0.
No negative course was rerun.

The uninstrumented corrected original full four-profile plus cut-wire course
passes once in Release86.527s and once in Debug116.910s. Both retain original
300s/40s, configured argv/network/environment/working directory and actual
normal MSVC compile/module/library/link settings. Private outputs/source and
one test-enabled composition object are substituted; every other original
normal constituent, archive, runtime and tool input is held. Protected normal
inputs are1394 Release and1613 Debug, unchanged after each; candidate hashes
remain exact, all three tasks Ready/operator0, owned forests empty.

The program sentinel and exactly one passed JUnit case bind each positive;
all original normal and diagnostic failures remain immutable. These corrective
proofs have separate source/image identities. Root's frozen56 test-only route
owns qualification mapping; no original raw FAIL is relabelled and no actual
frozen producer, distribution input or published asset is changed.

## Source and workflow gates

Format22, scoped tidy22, strict source/inventory and async-mutation adversarial
controls, documentation and scoped board checks pass. The explicit adopted plan
is structurally valid. The general checkpoint binder retains its failure on an
existing associated plan field `amendments`; no tooling or unrelated plan was
changed and that failure is not reported as a passing gate.

Canonical source is `7442b2ecff95606b1ca7cc44f6b4fbc56e460dfb`, same CPP5f9.
Canonical-only pin `b6cab7026851e80d21a72128ea6831d42f52d77b` binds exactly the
four already-landed BPR-2055 files plus this fixture. Independent review305a4f54
covers their composition ownership, not whole-feature/native acceptance. It
preserves inventories/other pins and uses canonical4cb4 rather than private9e58.
No foreign source or canonical pin is imported into frozen56 or the private
release lineage. Actual allocation is BPR-2087/main01ea66bf1; later canonical
highwater2092 is preserved, without recycling IDs or importing foreign cards.

External evidence: the v0.4.2 release run's
`implementer56-windows-selected-diff-triage-r1/ui-fixture-fix-bpr2087`, including
`native-proof-summary.json` SHA6a2c2d8a…, three complete native evidence folders,
`pin-review.json`, and the separate canonical-only review. SourceFind and Tour
proofs remain separate. All BPR-2087 commands/borrows are terminal; the agent's
separately authorized Tour operator continues independently before PC handback.
