# Proposed amendment: remember CLI connection selection

This accompanies [the draft](RFC-DRAFT-0010-remember-cli-connections.md),
authored for the owner's 2026-10-12 report that choosing CLI must be repeated
after reopening. Live RFCs, statuses, issue cards and plans remain unchanged.

## Sources inspected

These SHA-256 identities describe the live files read during authoring.
Reconcile drift before adoption; they are not an implementation baseline or
acceptance evidence.

| Source | SHA-256 |
|---|---|
| [RFC-BPR-0015](../../RFC-BPR-0015-github-cli-production-enablement.md) | `65576e39cce9c0006e9ade1404681cda344a246b8400596d82719e1b13c73961` |
| [RFC-BPR-0024](../../RFC-BPR-0024-bpr-profiles-multiple-windows.md) | `3e519045a81de66b740683a50409d2666f3ddb131a369a3dd2727eccef0b50c3` |
| [RFC-BPR-0032](../../RFC-BPR-0032-calm-home-and-accounts-surfaces.md) | `b0d8cb50c2234fda079d367c687e7ee7f0f884d7c49d717dac15dabf5be8287b` |
| [Product contract](../../../architecture/product-contract.md) | `d1444738a74be65b0b362989245468b3995ce35f0da8ac4d7a661900e3933e65` |
| [System architecture](../../../architecture/system-architecture.md) | `9ab40714ed9cf74c433fc7c19d9b3c33963d70cd913fd4080084d1c74b64ec48` |
| [GitHub host implementation](../../../architecture/github-host-implementation.md) | `4396026f7f137bb262add140d7eabf4afcbfb70b1b812006c20abea8c3956570` |
| [BPR-1373](../../../issues/40-parked/BPR-1373-pin-each-gh-call-to-a-chosen-account.md) | `f6fad66c7b7f35e6038dc75b7085dbbd65dd89f513b7da81c62c1a1e6bbeba03` |
| [CLI-profile plan](../../plans/rfc-bpr-0015-cli-profile/milestone-plan.json) | `eeb2c94607c72f91e10617dab0b01f8d03e5660854283b614a39c151593af831` |

## Requirements to reconcile on adoption

| Existing contract | Proposed change |
|---|---|
| RFC-BPR-0015, 2026-09-13 amendment: only an explicit CLI gesture or selected CLI profile may execute `gh`; read opt-in and exact-account refusal | A durably recorded successful selection also permits bounded targeted startup validation for that same host/account. Preserve first-use explicit opt-in and every existing process/credential/publication constraint. |
| RFC-BPR-0015, 2026-10-09 account-configuration amendment | Retain the numeric account binding across application lifetimes and compare it before restored-session acceptance. The private `GH_CONFIG_DIR` remains per call, contains no token and is removed after the call. Full inventory is not a startup requirement. |
| RFC-BPR-0015, six existing unnumbered acceptance criteria | Preserve their obligations and historical evidence. Add fresh acceptance for the changed persistence/startup scope using the draft's criteria; the closed RFC supplied no CLI restart proof. |
| RFC-BPR-0024, account ownership, Sign out and AC-07 restoration | Reuse per-profile/per-server ownership and existing window startup-host selection with the remembered transport/account. A sign-out disables automatic CLI reuse as well as any prior device resume for that server. Retained private state still requires current exact-account acceptance. |
| RFC-BPR-0032, shared sign-in amendment and Accounts & hosts | First-time CLI discovery still goes to account selection and read trust. Reusing a saved connection skips those choices; existing status/recovery controls report validation and persistence failures. No page-layout amendment is proposed. |
| Product contract, Credential acquisition and GitHub CLI profile milestone | Distinguish persisted CLI selection metadata from device-vault material and live authenticated authority. State when selection is remembered, revalidated, replaced and disabled. |
| System architecture, profile/window ownership and GitHub CLI read milestone; GitHub host implementation, startup/restoration | Replace native-only startup selection with exact remembered transport selection. Document host-owned metadata, effect-lane ordering, stable identities, refusal and shutdown behavior. |

Update the owning domain/state, threat, Settings authority and maintenance
documents where implementation adds a record or changes an authority path.
The established device-grant rules, manual-token session lifetime, GitHub
collaborative authority, publication recovery and supported-platform limits
keep their scope.

## Work, dependencies and pending evidence

Implementation consumes existing pinned CLI execution, authenticated viewer
checks, private transactional storage, profile ownership and window
restoration. Register only actual capability dependencies during authorized
execution; unrelated RFC closeout is not a gate.

BPR-1373 is currently Parked for human review. Its account-page candidate and
per-call pinning obligations remain separately owed. Reconcile any overlapping
writer or protected candidate before changing its inputs; this draft neither
answers its owner question nor marks it Done. Earlier CLI, native and release
evidence continues to describe its original candidate and scope.

The adopted CLI-profile plan remains historical. Do not replay adoption over
it or silently rewrite its completed milestone exits. This proposal adds
connection persistence and restart acceptance as new scope; it requires no
restructuring of that plan during authoring.

The next authorized implement-rfc session should reconcile these source
identities, register the new scope, publish the narrow contract amendments and
implement through the real profile startup and persistence paths. Use isolated
fixtures for restart/account/refusal evidence before any designated live course.
No live call, new visual candidate or implementation acceptance is supplied by
this authoring pass.
