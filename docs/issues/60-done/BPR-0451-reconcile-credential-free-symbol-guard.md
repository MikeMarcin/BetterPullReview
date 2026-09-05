---
schema: "implement-rfc.card.v1"
id: "BPR-0451"
title: "Reconcile the credential-free symbol guard with the shipped device transport"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: ["BPR-0318"]
---

# Reconcile the credential-free symbol guard with the shipped device transport

## What was found, and where

`scripts/verify_rfc0017_test_inventory.py --release-binary <tests-disabled
Release artifact>` fails:

```
rfc0017-test-inventory:fail:symbol present in release binary: credential_free_request
```

This is **not** caused by RFC-BPR-0016's later slices. It is present at
`11f4936`, the [[BPR-0279]] closing commit, and was introduced by that card's
own integrator repair: the production
`make_native_https_credential_free_transport` factory referenced a
testing-gated type, so the tests-disabled Release build did not compile, and
the repair shipped `credential_free_request` unconditionally with
credential-free *reads* structurally refused. The commit message records the
repair; what it did not notice is that RFC-BPR-0017's guard lists that exact
symbol as one that must never reach a shipped binary.

Discovered while closing [[BPR-0281]]. It belongs to neither that card nor
[[BPR-0282]]: no RFC-BPR-0016 slice touched `native_https_transport.cpp`, and
the failing command is an RFC-BPR-0017 closeout gate.

## Why the guard does not simply fire correctly

The guard was written when the only credential-free entry in the tree was
RFC-BPR-0017's own demonstration seam, so "this symbol exists in Release" and
"the demonstration escaped into the product" were the same statement. They are
no longer the same statement. RFC-BPR-0016 gives the product a *legitimate*
credential-free request path — the device-authorization endpoints, which by
construction run before any credential exists — and that path is shipped on
purpose.

The script already anticipates exactly this shape: `KNOWN_PRODUCT_CREDENTIAL_FREE`
records `make_designated_review_public_observer` as a credential-free product
capability that predates RFC-BPR-0017 and is not its evidence. The device
transport needs the same treatment, and needs it stated rather than assumed.

## Required final state

- The guard distinguishes RFC-BPR-0017's *demonstration seam*, which must stay
  out of Release, from RFC-BPR-0016's *shipped device-authorization* path,
  which must stay in it. Narrowing the check to
  `make_native_https_credential_free_transport_for_test` alone is not
  sufficient on its own: whatever replaces `credential_free_request` in
  `FORBIDDEN_SYMBOLS` must still fail if the anonymous-read seam reaches
  Release.
- The exception is recorded in `KNOWN_PRODUCT_CREDENTIAL_FREE` (or an
  equivalent explicit list) with its owning RFC, so a future reader can neither
  flag it as a regression nor quote it as an RFC-BPR-0017 capability.
- A negative control proves the narrowed guard still fires: a build or fixture
  in which the demonstration seam *is* present must fail the scan.
- `docs/verification/RFC-BPR-0017.md`'s non-product row and
  `docs/development/build-and-test.md`'s documented command agree with whatever
  the guard now checks.

## Why it depends on BPR-0318

The shipped credential-free path is currently unreachable at runtime — every
platform is `device_flow_unavailable` until [[BPR-0318]] records a
distributable identity. Narrowing the guard before then would relax a check
while the thing it now permits still cannot run, which is the wrong order.
After BPR-0318 the permission is about a live capability and can be judged as
one.

## Verification

```sh
cmake --preset app-release --fresh && cmake --build --preset app-release --clean-first
python3 scripts/verify_rfc0017_test_inventory.py \
  --build-root build/dev \
  --release-binary build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview
ctest --preset dev --output-on-failure -R '^rfc0017_test_inventory$'
python3 scripts/verify_documentation.py --root .
```

## Repair and what proves it — 2026-09-01

`FORBIDDEN_SYMBOLS` now names the two `BPR_NATIVE_HTTPS_TESTING` factories —
`make_native_https_credential_free_transport_for_test` and its bootstrap
sibling — which are the only entry points that construct a transport under
the test policy and therefore the only way the demonstration seam can be
reached. `credential_free_request` is gone from the list: it is the binding
the production factory `make_native_https_credential_free_transport`
constructs for RFC-BPR-0016's device flow, shipped deliberately since
[[BPR-0279]] and made truthful in Release by [[BPR-0318]]. That factory is
recorded in `KNOWN_PRODUCT_CREDENTIAL_FREE` with its owning RFC beside the
designated-live observer, and the docstring says why.

The narrowed guard keeps its teeth. The negative control is registered as
`rfc0017_release_absence_negative_control` in the ordinary graph: the same
scan against the tests-enabled application, which links both factories, and
the test passes only when the scan fails naming the credential-free `_for_test`
factory. A scan that could pass against that binary would pass against
anything, and this test would go red.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The shipped Release artifact passes the narrowed scan | `python3 scripts/verify_rfc0017_test_inventory.py --release-binary build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview` against the artifact at `sha256:1d2fab547fdb42abe385be17da3e90d802ff6cf84c5732e935e42aef01b425f0` (10,075,792 bytes); `cmake --build --preset app-release` reported `ninja: no work to do`, so the artifact is the committed tree's, not a `--fresh --clean-first` rebuild | `RFC0017_TEST_INVENTORY_OK`; the binary carries 9 `credential_free_request` symbol lines, all device-flow, and neither `_for_test` factory | this commit |
| The seam still fails the scan when present | `ctest --test-dir build/dev -R '^rfc0017_release_absence_negative_control$'` (the tests-enabled `app_better_pull_review`) | Passed 1.45 s — the scan exits 1 naming `make_native_https_credential_free_transport_for_test`; run directly it also names the bootstrap factory and the marker string | this commit |
| The inventory half is unchanged | `ctest --test-dir build/dev -R '^rfc0017_test_inventory$'` | Passed 0.11 s, `RFC0017_TEST_INVENTORY_OK` | this commit |
| The ledger and the build guide say what the guard checks | `python3 scripts/verify_documentation.py --root .` after editing `docs/verification/RFC-BPR-0017.md`'s non-product row and `docs/development/build-and-test.md`'s guard section | `DOCUMENTATION_OK` | this commit |

Not run: the card's `cmake --preset app-release --fresh && cmake --build
--preset app-release --clean-first`. The claim is about which symbols the
committed tree's Release artifact carries, and an incremental build that has
nothing to do answers it from the same objects a fresh build would produce;
RFC-BPR-0017's own closeout row still names the fresh build and inherits this
repaired guard when it runs.
