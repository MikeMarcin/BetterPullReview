---
schema: "implement-rfc.card.v1"
id: "BPR-0960"
title: "Qualify the Windows x64 package"
status: "Blocked"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-0957"]
milestone: "M8 UX/UI and clean-target verification"
platforms: ["win"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
blocked_by: "clean-windows-desktop-vault"
---

# Qualify the Windows x64 package

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

## Owner amendment 2026-10-16 — Windows 11 on JaegAW

The owner selected Windows 11 x64 on JaegAW as the tested Windows environment
and removed Windows 10 from the required matrix. The owner confirmed it under
[BPR-1743](../60-done/BPR-1743-confirm-or-withdraw-the-october-16-qualification-matrix-amendment.md)
on 2026-10-23 UTC: "Windows 11 and ubuntu wsl2 are tested. If it works on other
things is unproven, but not disallowed." Windows 10 is untested, not refused:
the amendment adds no install check and changes no package floor.

AC1 before this amendment read "On clean native Windows 10/11 x64
environments". The other words of AC1–AC3 are unchanged. The amendment
supplies no evidence, so every criterion below stays pending on a real
non-elevated ordinary-user Windows 11 desktop. Elevated SSH and administrative
MSI extraction do not satisfy them. See the
[RFC amendment](../../rfcs/RFC-BPR-0038-native-package-release-security-qualification.md#owner-amendment-2026-10-16--tested-windows-and-linux-environments).

## Acceptance

- AC1: On a clean native Windows 11 x64 environment (JaegAW) install the exact MSVC-built candidate per user without unnecessary administrator rights, launch, authenticate through an authorized safe route and open a fixture PR to the exact selected-file comparison.
- AC2: Exercise real high-DPI source/navigation, one shared alignment controller and DiffLinkMap, keyboard/focus/accessibility, diagnostic preview and maintenance interactions; cross-compilation and hosted output cannot replace native evidence.
- AC3: Uninstall and explicitly purge disposable owned roots and inspect documented Credential Manager/file/cache residuals; bind evidence to package/source/OS/toolchain identities.

## Verification and execution record

Imported untested under mandate r1 at RFC base
`d0a8a6dda005af12b3818234f4878a9f48a44146`. M8 r3 used JaegAW Windows 11
10.0.26200 AMD64, MSVC 19.51.36257.0/14.51.36231, Qt 6.10.3 and task-local
WiX 4.0.6 through a station lease and external guarded clone. The live checkout
and its pre-existing dirty MASK lock were preserved.

Final source `6ad716350c7f2e02a1b00370ea49cd5b77ce4c15` produces the unsigned
0.1.3 MSI through `packaging/package.py`; SHA-256
`ee2e304b9ab84c36105b7f365c6685caf1c5222313af927693d8a620dc0ac698`.
Native work fixed an MSVC narrowing error, missing portable entry include,
custom install-root loss on uninstall, and test-only CRT support incorrectly
compiled into shipped helpers. Exact MSI product
`{67A42F3D-0F59-437D-BAE4-34DCEAB3898F}` installed to owned scratch with state
5 and the correct location; uninstall returned 0, state -1, and no install root.
Bounded verification passes 23 file hashes, 222 test-free compile units,
16 native PE dependency checks and the exact app's producer-map composition.

AC1–AC3 remain pending: SSH runs elevated and cannot establish ordinary-user
installation or an attended clean desktop. Windows 10 (since removed from the
matrix by the 2026-10-16 amendment), native rendered/auth
fixture work, Credential Manager continuity/purge and residual inspection
remain unsupplied. The full installed-package audit also refuses stripped PE
symbol inspection (`llvm-nm --demangle produced an empty symbol table`);
bounded mechanics are not a waiver of that security evidence. No graphical
production profile or real credential was opened. See the
[M8 ledger](../../verification/RFC-BPR-0014.md#milestone-8-native-candidates-and-pending-acceptance)
and handoff for exact retained artifacts and the next safe native session.

Post-review hand-back 454 corrected first-load failure presentation in the
shared Qt window (`babcb3dc`, documentation clarification `80481e80`). No
Windows station was changed or requalified in that round. The retained MSI
above predates that correction; rebuild it from the next exact candidate
before complete Windows qualification. The existing native gaps and full
stripped-PE audit refusal remain unchanged.
