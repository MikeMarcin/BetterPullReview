---
id: BPR-0537
title: Close the Release flag surface and tighten the enabled course
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0515]
---

# Close the Release flag surface and tighten the enabled course

## Why

Three residuals the independent review of `28fa0741` ([[BPR-0285]],
2026-09-03) named and the integrator adjudicated as work rather than
acceptance:

- `--device-authorization-registered-course` is parsed unconditionally in
  `src/app/main.cpp`; in a Release it suppresses the probe's cancel, so
  `--self-test --device-authorization-probe --device-authorization-registered-course`
  polls GitHub until the run budget expires. It must be gated like
  `--device-authorization-browser-probe`.
- RFC-BPR-0016's inventory forbids none of `--network-denied`,
  `--device-authorization-browser-absent`, `--device-authorization-panel-absent`,
  `--device-authorization-registered-course`, `--public-preview-*`, or
  `--test-application-data-root`; only the RFC-0017 inventory forbids the
  first, and the platform course runs only the RFC-0016 scan. All are
  structurally absent today; the scan must say so.
- The platform course's `enabled` expectation accepts any stage but
  `unavailable`, so an offline station passes without proving the flow
  reached GitHub; and `system_browser_opener::open`'s address validation
  has no unit test, its only evidence being the three owner observations.

## Required final state

- Every tests-enabled flag is parsed only under `BPR_BUILD_TESTING`, and the
  RFC-0016 inventory's forbidden set names every such flag string; the
  positive control still fires on the tests-enabled binary.
- The platform course's `enabled` expectation requires `cancelled` — the
  stage only a minted device code reaches — and the replay row records the
  one bounded request that implies, as accepted traffic.
- A unit test drives `system_browser_opener`'s validation: an absolute
  `https` address with a host and no userinfo is the only shape it
  forwards, and nothing else reaches the launch service.
