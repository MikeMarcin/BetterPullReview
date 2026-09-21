---
schema: "implement-rfc.card.v1"
id: "BPR-0957"
title: "Verify package contracts and hostile fixtures"
status: "Done"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0956"]
milestone: "M7 Focused tests and fixtures"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"0b2323845a2d326da3f4b545c2a4c0f50b69d697","evidence":{"kind":"observation","detail":"12/12 exact-candidate focused integration (20.801s): core/host replay-recovery-cancel, store preservation, Qt scales, actual comparison/Settings diagnostic canaries. Guarded relocated production package and real packaged MCP course passes (17.291s). docs/verification/RFC-BPR-0014.md#milestone-7-focused-tests-and-hostile-fixtures and handoff-m7.json retain commands and exact artifact identity."}},{"criterion":"AC2","outcome":"pass","revision":"0b2323845a2d326da3f4b545c2a4c0f50b69d697","evidence":{"kind":"observation","detail":"Actual altered copies refuse three missing helpers, absent/escaping plugins, five loader overrides, wrong identity, four byte markers and real external RPATH even after counterfeit rehash. Positive loader/MCP evidence, path-escape/stale refusal and native process/socket/library probes prevent false success. Windows/WSL probe is inspector-contract evidence only, not package qualification."}},{"criterion":"AC3","outcome":"pass","revision":"0b2323845a2d326da3f4b545c2a4c0f50b69d697","evidence":{"kind":"observation","detail":"Exact unchanged tests-off artifact source da12f5568f8ab207d8a75451cda9b3de1a49df63; executable SHA256 d31b371b1aad6caef369bdf60212b0a156b913ee7834d1f88b7424c8d8221852. All 43 file hashes, 13 binaries, 34 production anchors, 227 compile units, pinned static dependency provenance and contained Mach-O dependencies pass; fixture symbols/bytes/defines and core effect edges absent. Signature, crash, native matrix and distribution approval remain M8/M9."}}]
---

# Verify package contracts and hostile fixtures

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Add or reuse focused core replay, host recovery/cancellation, Qt composition, package relocation and hostile boundary fixtures chosen from the changed public graph.
- AC2: Demonstrate meaningful negative controls for missing required packaged capabilities, untrusted plugin/library paths, invalid package identity and source/secret canaries, with real failure on a false success claim.
- AC3: Bind checks to exact candidate and artifact identities; tests-disabled byte/symbol/graph inspection proves fixture seams and undeclared dependencies are absent.

## Verification and execution record

Accepted engineering candidate `0b2323845a2d326da3f4b545c2a4c0f50b69d697`
under mandate r3, coordinator-implementer rev2. The immutable RFC base remains
`d0a8a6dda005af12b3818234f4878a9f48a44146`. See the
[M7 evidence ledger](../../verification/RFC-BPR-0014.md#milestone-7-focused-tests-and-hostile-fixtures)
and [durable handoff](../../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m7.json).

- AC1 passes: exact-candidate focused integration 12/12, including core replay,
  host cancellation/recovery, corrupt/future/foreign store preservation, Qt
  scales 100/125/200 and the actual Settings/selected-comparison course. The
  public course now seeds diagnostic source/comment/secret bodies and verifies
  the production preview/export excludes them. Relocated tests-disabled package
  and actual packaged MCP stdio courses pass without a product fixture switch.
- AC2 passes: altered disposable copies reject each missing helper, absent or
  escaping plugin roots, five loader CLI overrides, wrong bundle identity,
  four byte canaries and a real external Mach-O RPATH. Counterfeit file hashes
  deliberately bypass the digest layer so the byte/origin guard itself must
  fail. Positive packaged Qt loading and exact MCP snapshot reads prevent
  empty-observation false success; traversal and stale binding are refused.
- AC3 passes: tests-disabled artifact source `da12f5568f8ab207d8a75451cda9b3de1a49df63`
  is unchanged across product/build/package inputs. All 43 installed hashes,
  13 native binaries, 34 production composition anchors, 227 first-party
  compile units, declared static provenance and contained dependencies pass.
  Fixture symbols/bytes/testing definitions and core effect dependencies are
  excluded. App SHA-256 is `d31b371b1aad6caef369bdf60212b0a156b913ee7834d1f88b7424c8d8221852`.

Final integration took 20.801s; final guarded package course took 17.291s.
Native process/socket/library inspector contracts also pass on macOS arm64,
Windows 11 x64 and WSL Linux x86_64. The Windows connected-TCP observation now
includes established connections, not listeners alone. Windows uses the actual
installed station guard (the repository symlink is unresolved there). Native
OS-path fixture eligibility is explicit; skips do not certify another platform.
All probe children were reaped and selected live configuration hashes stayed
unchanged. These are M7 verifier/capability checks, not native package baseline,
authentication, install/uninstall, crash, signing or distribution approval.
Those M8/M9 obligations remain pending. No human verdict is required for M7.

## M2 allocation for the successor

Use the [nine-row public-check map](../../architecture/release-readiness.md)
and G1–G9 to bind each negative control to a claimed terminal state. Include
missing packaged helper, untrusted library/plugin origin, mismatched snapshot/
selection, canary content, corrupt/future/foreign stores, partial cleanup and
preserved sibling/unknown data. Tests-disabled packages must contain no fixture
entry; public fixture evidence is not native authentication/trust evidence.

M2's real epoch-maintenance and composed-host checks passed with unchanged
product source. The checkout architecture scan failed only on an ignored nested
unrelated worktree; it passed on an exact tracked-source archive. Preserve that
known scanner/worktree limitation and qualify any future verifier change in this
verification scope; never delete another owner's checkout to make a scan pass.

## M6 accepted handoff

BPR-0956 is Done at guarded finish `2589f07f`. Read the
[M6 handoff](../../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m6.json)
and [artifact/public-path ledger](../../verification/RFC-BPR-0014.md#milestone-6-composition-and-public-path).
The clean source candidate is `da12f556`; its tests-disabled universal local
package at `build/releases/m6-da12f556/` reached the exact fixture comparison
through native UI and normal Quit. Reuse unchanged focused and artifact evidence.
M7 still owns its hostile/missing-capability controls and comprehensive
tests-disabled seam assertions. The older runtime-surface script forces an
unshipped offscreen plugin; select a native platform for package inspection
without expanding the shipped plugin allowlist. Windows/Linux native and
release-trust/license gates remain M8/M9, not waived by the macOS result.
