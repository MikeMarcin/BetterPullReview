---
schema: "implement-rfc.card.v1"
id: "BPR-0958"
title: "Qualify the macOS Apple Silicon package"
status: "Parked"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0957"]
milestone: "M8 UX/UI and clean-target verification"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pending","revision":"6ad716350c7f2e02a1b00370ea49cd5b77ce4c15","evidence":{"kind":"observation","detail":"Exact 43-file package reaches closed safe CLI fixture review.cpp comparison in isolated HOME; clean OS/account with working native Keychain and authorized auth remains pending. See docs/verification/RFC-BPR-0014.md#milestone-8-native-candidates-and-pending-acceptance"}},{"criterion":"AC2","outcome":"pending","revision":"6ad716350c7f2e02a1b00370ea49cd5b77ce4c15","evidence":{"kind":"observation","detail":"Agent-native rendered diff/map/shared scroll, keyboard focus, metadata-only diagnostics, retirement and normal Quit pass. Exact human decisions BPR-0958-native-diff-v001 and BPR-0958-maintenance-clarity-v001 are ready/protected in session cbd48b91000b00f1e72e1ad62c7c384c, cursor 451; neither verdict supplied."}},{"criterion":"AC3","outcome":"pending","revision":"6ad716350c7f2e02a1b00370ea49cd5b77ce4c15","evidence":{"kind":"observation","detail":"Recoverable uninstall preserved all profile/cache hashes and prior diagnostics stayed absent. Full purge is correctly unavailable without isolated HOME default Keychain; real vault continuity/deletion and residual policy remain pending."}}]
---

# Qualify the macOS Apple Silicon package

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: On a clean macOS 13+ Apple Silicon environment install the exact universal candidate per user, launch, authenticate using an authorized safe route and open a fixture PR to the exact selected-file comparison.
- AC2: Exercise real rendered high-DPI source/navigation, the shared alignment controller and DiffLinkMap, keyboard/focus/accessibility, diagnostic preview and maintenance interactions with objective native evidence.
- AC3: Uninstall and explicitly purge in disposable owned roots; inspect credential/file/cache residuals against the documented policy and retain exact package/source/environment identities.

## Verification and execution record

Imported untested under mandate r1 at RFC base
`d0a8a6dda005af12b3818234f4878a9f48a44146`. M8 under r3 prepared source
`6ad716350c7f2e02a1b00370ea49cd5b77ce4c15`, version 0.1.3, with the existing
offline `tools/release.sh --prepare-only` route. No publication or trust approval.

On native arm64 macOS 26.6.2 (25G83), an isolated HOME installation reached the
closed CLI fixture's exact review.cpp comparison (11–16 versus 21–26), one map,
one shared controller, keyboard focus, metadata-only diagnostic preview,
confirmed retirement/clear and normal Command-Q exit. This exposed and fixed
the missing Quit menu after last-workbench retirement. All 43 final payload
file hashes match the exercised candidate; the final installed audit passes.
Recoverable bundle removal preserved every profile/cache file hash and the
cleared diagnostic file stayed absent.

AC1–AC3 remain pending in full: the fixture is not a clean OS/account with a
working native vault; its default Keychain is absent, so full purge is correctly
unavailable. Real credential continuity/deletion and exact residual policy
still need a disposable clean account. Two exact AC2 human questions are
prepared in `rfc0014-m8-native-v001.json`: `BPR-0958-native-diff-v001` and
`BPR-0958-maintenance-clarity-v001`. No human decision was supplied.
See the [M8 ledger](../../verification/RFC-BPR-0014.md#milestone-8-native-candidates-and-pending-acceptance)
and the durable M8 handoff for hashes, commands and retained review resources.
