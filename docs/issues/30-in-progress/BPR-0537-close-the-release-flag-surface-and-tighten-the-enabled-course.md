---
id: BPR-0537
title: Close the Release flag surface and tighten the enabled course
status: In Progress
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

## Lane — 2026-09-03

Claimed straight into `30-in-progress/` by the integrator under the
transition exception, its one dependency terminal. Split as the board's
rule allows: the script half — the RFC-0016 inventory's forbidden set and
the platform course's `cancelled` expectation with their contracts — is the
integrator's and lands first; the flag gating in `main.cpp` and the opener
validation test follow [[BPR-0535]] so the two slices do not edit the same
composition file at once.

## Part one landed — 2026-09-03

The script half, at `a31aee2d` and this commit. `scripts/verify_rfc0016_test_inventory.py`
now forbids by string the ten tests-enabled flags that are already absent
from a Release — `--network-denied`, the browser and panel `-absent` and
`-probe` flags, the four `--public-preview-*` flags, `--test-application-data-root`,
and `--github-fixture` — proven both ways on the integrator host: the
release image at the closeout head still passes (`RFC0016_TEST_INVENTORY_OK`)
and the tests-enabled image fails on eleven strings. The two course flags a
Release still parses, `--device-authorization-registered-course` and
`--device-authorization-panel-course`, were confirmed present in the
release image by `strings` and join the list when the second half gates
them.

`scripts/verify_rfc0016_platform_course.py`'s `enabled` expectation now
requires `DEVICE_FLOW_STAGE:cancelled` — the stage only a minted device code
reaches before the run budget cancels it — and keeps `DEVICE_FLOW_REQUESTS:0`
as the refusing-transport negative control, refusing `failed` and
`unavailable` by name (`device-selection-not-reached:<stage>`). The
contract gained four cases for it. Every enabled course this RFC has
recorded — macOS, Linux, and Windows, before and after BPR-0515 — landed on
`cancelled`, so no recorded run changes verdict; an offline station now
cannot pass the enabled course, which is the point.
