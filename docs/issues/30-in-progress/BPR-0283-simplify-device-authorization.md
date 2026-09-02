---
id: BPR-0283
title: Simplify the device-authorization implementation
status: In Progress
rfc: RFC-BPR-0016
depends_on: [BPR-0281, BPR-0282, BPR-0310, BPR-0311, BPR-0318]
---

# Simplify the device-authorization implementation

## Required final state

- A focused simplifier pass removes duplicated policy, broad service access,
  unnecessary serialization, stale comments, whole-state refreshes, and test
  seams that can bypass production selection.
- Core, host, adapter, Qt, and composition ownership remains explicit; security
  checks, erasure, cancellation, and bounded-state code are not collapsed into
  clever or unauditable helpers.
- The pass occurs after macOS, Linux, and Windows production-selection changes,
  including `windows-app-release`, so it covers the complete implementation.
- Scoped format, strict tidy, focused tests, Release contracts, documentation,
  and `git diff --check` pass after simplification.

## Scouted inputs — 2026-09-02

Every dependency is done since the three platform flips landed
([[BPR-0318]], [[BPR-0310]], [[BPR-0311]]). A read-only scout of the
implementation found these, each with the smallest change and the selector
that proves it stays correct; they are the pass's worklist, not its
conclusion.

1. **Command legality is re-derived in Qt.** The core publishes which of
   begin/cancel is legal and why in `device_authorization_transition_output::Commands`
   (`src/core/src/device_authorization_transition.cpp`), but
   `device_authorization_presentation` carries no such field, so the host's
   `deliver()` drops it and `device_authorization_panel.cpp`'s `live()`
   re-derives "is a flow active" from `Stage`. Thread the command projection
   through the presentation and delete `live()`; proven by
   `qt_device_authorization_scale_(100|125|200)` and `core_device_authorization`.
2. **The device host takes the whole vault port and uses three methods**
   (`store_refresh`, `retrieve_refresh`, `cancel`); `store`, `retrieve`,
   `delete_credential`, `delete_refresh` are reachable but unused, so nothing
   stops the device host from touching the base-token slot. Extract a
   refresh-only port the full vault also satisfies; proven by
   `application_device_authorization_host`, `application_device_authorization_refresh`,
   `application_github_credential_vault`.
3. **The verification authority is two independent literals** —
   `expected_verification_authority` in the adapter's contract parser and
   `default_device_verification_authority` in the host's handoff — for one
   deliberate double check. Keep both checks, one declaration; proven by
   `github_device_authorization_contract` and `application_device_authorization_host`.
4. **The composition comment "no browser capability is composed in this
   slice" predates the flips.** `github_application.cpp` composes
   `device_authorization_browser_handoff{ .Browser = nullptr }` on every
   platform, so "Open browser" always resolves `unavailable` where the flow
   can now start. Confirmed a missing port, not a stale comment: nothing in
   `src/` opens a browser. Carded as [[BPR-0516]]; this pass only rewrites
   the comment to say so.
5. **Confirmed not a defect:** the `RegisteredDeviceCourse` fixture override
   is compile-time gated on `BPR_BUILD_TESTING` end to end and cannot reach
   the tests-disabled Release.

Not confirmed, so not on the list: the per-poll copy of `DeviceCode` is the
port's stated owned-material contract; `deliver()` republishing the whole
presentation is the atomic-publication rule; the ledger's "initial
production selection" table is headed as historical.

Focused verification for the pass: `core_device_authorization`, the three
host selectors above, `github_device_authorization_(contract|refresh_contract)`,
`native_https_device_authorization_(contract|fixture)`,
`qt_device_authorization_scale_(100|125|200)`,
`application_device_authorization_(public_path|unavailable|release_contract|registered_course)`,
the macOS platform course with `--expected-device-selection enabled --fresh`
in a new build root, `verify_rfc0016_test_inventory.py` if any selector name
changes, and the two app-source digest pins.
