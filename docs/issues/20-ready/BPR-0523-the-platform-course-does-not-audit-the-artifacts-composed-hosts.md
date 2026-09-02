---
id: BPR-0523
title: The platform course does not audit the composed hosts of the artifact it ships
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# The platform course does not audit the composed hosts of the artifact it ships

## Why

While recording [[BPR-0516]]'s Linux rows, `bpr-linux` found that
`COURSE_SELECTOR_PATTERN` in `scripts/verify_rfc0016_platform_course.py`
names sixteen selectors and `composed_application_hosts` is not among
them, and that nothing in the course runs `verify_composed_hosts.py` under
its production profile against the tests-disabled artifact the course
otherwise measures. So `RFC0016_COURSE_SELECTORS:16` says nothing about
the opener's anchor, and a platform whose shipped graph had lost a composed
host would pass the course. The station proved the anchor by hand in both
binaries instead.

## Required final state

- The course's focused set includes `composed_application_hosts`, so the
  tests-enabled binary's audit runs on every platform course.
- The course runs `verify_composed_hosts.py --profile production` against
  the shipped artifact and refuses by name when it fails.
- Both are covered by `rfc0016_platform_course_tests.py`, and the macOS
  course passes with them.

## Repair — 2026-09-02

`check_composed_hosts_of_the_artifact` runs after `check_platform_surfaces`
and before the release-artifact scan; `composed_application_hosts` joins the
focused pattern.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The course's own contract | `python3 tests/scripts/rfc0016_platform_course_tests.py` | 9/9; `rfc0016_platform_course_launch_contract` Passed 0.10 s | this commit |
| The macOS course passes with the audit, against the BPR-0516 build root | `python3 scripts/verify_rfc0016_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/rfc0016-macos-browser --expected-device-selection enabled` | exit 0 in 65 s: `RFC0016_COURSE_SELECTORS:17`, `RFC0016_DEVICE_STAGE:cancelled`, `RFC0016_DEVICE_REQUESTS:0`, `RFC0016_PLATFORM_COURSE_OK`, the production-profile audit passing against the shipped artifact | this commit |
| Linux and Windows courses with the audit | the stations' next course runs ([[BPR-0285]]'s replay at the latest) | Pending | Pending |
