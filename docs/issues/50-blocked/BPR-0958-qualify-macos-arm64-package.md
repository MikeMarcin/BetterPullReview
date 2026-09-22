---
schema: "implement-rfc.card.v1"
id: "BPR-0958"
title: "Qualify the macOS Apple Silicon package"
status: "Blocked"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-0957"]
milestone: "M8 UX/UI and clean-target verification"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pending","revision":"80481e80d60f739db4c7eaea5e2eb8716ace2b4d","evidence":{"kind":"observation","detail":"Fresh revised 43-file installed package passes source/SBOM audit. Prior 6ad71635 closed synthetic CLI comparison remains exact-candidate evidence only. Clean macOS arm64 OS/account, working default Keychain and authorized real authentication remain pending; see docs/verification/RFC-BPR-0014.md#finalization-at-hand-back-460."}},{"criterion":"AC2","outcome":"pending","revision":"80481e80d60f739db4c7eaea5e2eb8716ace2b4d","evidence":{"kind":"observation","detail":"All three prepared human questions accepted only for their exact candidates. Original 6ad71635: maintenance 75ea6265-3302-49cd-8741-c23e1c583341 and source/map fcd51e30-b163-4136-9e50-923d1a13de85. Revised 80481e80: recovery f9c2ddba-40bd-4d4d-b8f8-295a7056db79 at event 459, hand-back 460, fingerprint ee8486b7d89634bf1a8c707190daabbe7d48a0fc6a59f8c8fb24f40bb515b824, executable ba8facaec459aa10464fadd76396dae372349dfd89c2c4a53ddc4eda281e7618. Focused regression passes 2/2; broader UI suites retain independently reproduced baseline failures. The narrow recovery accept is not a complete exact-final-package native course or transfer of original accepts."}},{"criterion":"AC3","outcome":"pending","revision":"80481e80d60f739db4c7eaea5e2eb8716ace2b4d","evidence":{"kind":"observation","detail":"Prior 6ad71635 recoverable uninstall retained profile/cache hashes; diagnostic deletion and accepted maintenance wording are not full purge or credential deletion. No real credential touched. Isolated HOME lacks default Keychain; clean-account native vault continuity/deletion and documented full-purge residuals remain pending."}}]
blocked_by: "clean-macos-account-vault"
---

# Qualify the macOS Apple Silicon package

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Owner amendment r4 — successor qualification

This unchanged-criteria standalone follow-up now belongs to
[RFC-BPR-0038](../../rfcs/RFC-BPR-0038-native-package-release-security-qualification.md).
RFC-0014 closes only its implemented foundation. No pending criterion, native
observation, trust/security gate or human decision is accepted by this transfer.
Paths, blocked state and exact historical evidence remain intact. The
[successor ledger](../../verification/RFC-BPR-0038.md) records the latest Qt 6.11.2
candidates, completed PE mechanics, raw 32/30 Grype failures and unanswered
Windows licensing-v002 decision; earlier records below remain historical.

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
`BPR-0958-maintenance-clarity-v001`.

At attended hand-back 454 on 2026-09-21, both bound questions were accepted:
maintenance decision `75ea6265-3302-49cd-8741-c23e1c583341`, fingerprint
`b352efe45d939c97aff4425a6919afd5f1bd20dfac2ffb7d10a89e1fe72f9274`, and
source/map/focus decision `fcd51e30-b163-4136-9e50-923d1a13de85`, fingerprint
`daf5be5cb08311e758960c67e6ecdb07071b95b4aff925b5324acbb734049e77`.
They accept only their exact AC2 questions and original candidate. The second
decision also reported a broken Overview before choosing GitHub CLI. The fresh
post-review implementer reproduced it on the same executable: the synthetic
link takes the native anonymous route before CLI selection, receives 404, and
leaves Overview selected over empty comparison panes with the error hidden on
Home. `babcb3dc` corrects this real presentation defect: a first failed or
cancelled load returns to Home even while drawer commands remain enabled;
accepted reviews and active comparisons are retained. The actual Open-bar
contract still clears a submitted destination; `80481e80` corrects the new
documentation wording without changing that behavior.

The added regression fails before the fix and passes afterward. Focused loading
and public maintenance checks pass 2/2; broader composed/window suites retain
two independently reproduced pre-existing failures. A native run of `babcb3dc`
shows the initial anonymous 404 returning to Home with the refusal and Copy
details visible. The Mac then locked during subsequent CLI consent and the
bounded launcher timed out; this is partial evidence, not a completed course.

The final tests-disabled universal package is source
`80481e80d60f739db4c7eaea5e2eb8716ace2b4d`, executable SHA-256
`ba8facaec459aa10464fadd76396dae372349dfd89c2c4a53ddc4eda281e7618`.
Its fresh installed copy passes the full package audit. The single new question
`BPR-0958-initial-load-recovery-v002` is ready/protected in session
`cbd48b91000b00f1e72e1ad62c7c384c`, cursor 458, fingerprint
`ee8486b7d89634bf1a8c707190daabbe7d48a0fc6a59f8c8fb24f40bb515b824`.
At the second hand-back, event 460 on 2026-09-21, human decision
`f9c2ddba-40bd-4d4d-b8f8-295a7056db79` accepted that exact recovery question
at event 459 (`2026-09-21T17:11:51.232600+00:00`, no note). The installed
executable was rehashed to the recorded identity. The original two accepts
remain bound to their original candidate; none is transferred. All three
prepared questions are answered, satisfying M8's human-decision binding exit.
Complete AC1–AC3 remain pending: no clean-account Keychain/authentication,
full-purge/residual evidence or complete revised-package native course was
supplied by this narrow verdict. The card now waits on
`clean-macos-account-vault`, rather than another answer to the recovery question.
See the [M8 ledger](../../verification/RFC-BPR-0014.md#milestone-8-native-candidates-and-pending-acceptance)
and the durable M8 handoff for hashes, commands and retained review resources.
