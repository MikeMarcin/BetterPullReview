---
id: BPR-0462
title: The inventory scripts read an empty manifest on a multi-config generator
status: Done
rfc: RFC-BPR-0016
depends_on: []
---

# The inventory scripts read an empty manifest on a multi-config generator

The first Windows matrix over a graph that registers the RFC-BPR-0016
anchor failed `rfc0016_test_inventory` six ways:

```
rfc0016-test-inventory:fail:core_device_authorization: expected 1 registration(s), found 0 (graph builds neither Qt nor the loopback fixture)
```

— while the matrix itself was running `core_device_authorization`, and
fourteen other RFC-BPR-0016 selectors, green. Both inventory scripts run
`ctest --show-only=json-v1` with no configuration, and on the Visual
Studio generator that lists nothing: the six graph-independent selectors
fail against an empty manifest, and every anchored expectation passes
vacuously — `rfc0017_test_inventory` "passed" this matrix while checking
nothing. Single-config macOS and Linux never see either face of it.

The same run exposed that [[BPR-0446]] changed what the anchor means:
`native_https_transport_fixture` now registers on Windows without the
APPLE/Linux wire block, so "anchor present" no longer implies the wire
selectors (`github_device_authorization_refresh_contract`,
`native_https_device_authorization_fixture`) or RFC-BPR-0017's
credential-free selector.

## Required final state

- Both scripts take `--build-config`, pass it to `ctest -C`, and their
  registrations supply `$<CONFIG>`.
- An empty inventory is a typed `ctest-inventory-empty` failure, never a
  vacuous pass.
- The wire-block selectors and RFC-BPR-0017's selector are expected
  exactly where the APPLE/Linux wire block configures, and expected
  absent on Windows, matching BPR-0446's recorded non-goal.
- `rfc0016_test_inventory` and `rfc0017_test_inventory` pass on the
  Windows host non-vacuously.

## Closed — 2026-08-26

Fixed at `6eb05993`. Direct script runs against the live Windows tree
print both OK lines with `--build-config Debug` and fail typed
`ctest-inventory-empty` without it, and the focused rerun at `31b0c7d0`
passed both registered tests — fifteen RFC-BPR-0016 selectors counted,
the two wire-block selectors and RFC-BPR-0017's selector expected absent
on this platform per [[BPR-0446]]'s recorded non-goal. Closing promoted
nothing.
