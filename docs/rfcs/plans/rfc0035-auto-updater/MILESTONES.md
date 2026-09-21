# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **rfc0035-auto-updater** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## m1-core-contract

Bind the release/auth/version/asset contract and implement core policy with fake host ports, including failures and stale completion rejection.

**Work:** `BPR-0946`

**Prerequisites:** None

**Completion:**

- Fixed distribution, native release-access authority, version grammar, exact asset identity, bounds and redirect contract are documented and represented by narrow contracts.
- Core version, selection, scheduling, command and stale-identity rules pass deterministic failure and boundary tables.
- The public application host composes fake release/download/verification ports through accepted projection, cancellation and shutdown; removing its release capability prevents an offer.
- Focused core-only and architecture checks pass, card acceptance and semantic documentation are current, and task-owned commands are quiescent.

**Boundary:** Hand back the accepted component foundation; no production updater or native install claim.

## m2-download-staging

Compose real check/download controls, native private-asset transport and signed macOS staging. Demonstrate the menu-to-host-to-projection path with fixtures.

**Work:** `BPR-0947`

**Prerequisites:** m1-core-contract

**Completion:**

- Real UI, persistence, authority, private transport, redirect containment and signed staging pass the bounded fixture/public-path course with native limits recorded.

**Boundary:** Verified staging is available; replacement remains gated on milestone 3 qualification.

## m3-install-recovery

Add the signed helper, shutdown barrier, journal, recovery and relaunch receipt; qualify on disposable signed installations before enabling production packages.

**Work:** `BPR-0948`

**Prerequisites:** m2-download-staging

**Completion:**

- Disposable signed installs prove cooperative shutdown, crash recovery, retained state, exact startup receipt and transaction-owned cleanup before production enablement.

**Boundary:** Return exact qualified package and failure evidence; unavailable native acceptance stays pending.

## m4-native-closeout

Record native upgrade evidence, package/security documentation and dependency-ordered closeout. Windows/Linux implementation remains a separate future scope.

**Work:** `BPR-0949`, `BPR-0911`, `BPR-0912`

**Prerequisites:** m3-install-recovery

**Completion:**

- All RFC acceptance has exact native/public-path evidence and documentation; implementation cards are complete and canonical RFC dependency order permits closeout.

**Boundary:** Close only accepted, dependency-eligible scope; retain every unavailable external/native/human obligation within the amended acceptance. Owner amendment r3 defers Intel Mac updater validation to BPR-0950 outside this plan and removes it from M4 and RFC-BPR-0035 closeout gates. Live private-release/vault evidence, RFC-BPR-0014 dependency and canonical closeout order remain required.
