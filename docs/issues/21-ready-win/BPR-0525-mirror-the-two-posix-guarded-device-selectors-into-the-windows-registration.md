---
id: BPR-0525
title: Mirror the two POSIX-guarded device-authorization selectors into the Windows registration
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# Mirror the two POSIX-guarded device-authorization selectors into the Windows registration

## Why

[[BPR-0524]] named the two course selectors absent from the Windows Debug
tree: `github_device_authorization_refresh_contract` and
`native_https_device_authorization_fixture`. Both are registered inside
`if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")` in
`tests/github_contract/CMakeLists.txt`, a platform guard with no comment
whose reason is a `find_program(openssl REQUIRED)`; [[BPR-0446]]'s
`if(WIN32)` block below it already finds Git for Windows' `openssl` by
hints and mirrored one selector out of that block, documenting one
deliberate omission (RFC-BPR-0017's credential-free one). These two are
neither mirrored nor mentioned, and the executable they drive is already
built and registered on Windows as
`native_https_device_authorization_contract`. So the exclusion the course
now records for them ([[BPR-0524]]'s table) is "not yet mirrored,
unmeasured on Windows" — a reason with an expiry.

## Required final state

- Both selectors are registered in the Windows block with the same
  arguments as their POSIX registrations, carrying `WINDOWS_UNCONTAINED`
  only if measurement shows [[BPR-0362]]'s cause applies, with the
  measurement recorded.
- Both pass on the Windows station, or the reason one cannot is written
  into the exclusion table in `scripts/verify_rfc0016_platform_course.py`
  as a permanent exclusion with its cause.
- The Windows entries for them leave the exclusion table in the same
  change (the course refuses a stale exclusion), and the Windows course
  reports `RFC0016_COURSE_SELECTORS:17`.

## Verification

```text
ctest --test-dir build\windows-dev -C Debug -R '^(github_device_authorization_refresh_contract\|native_https_device_authorization_fixture)$' --output-on-failure
py -3 scripts/verify_rfc0016_platform_course.py --platform windows --dev-preset windows-dev --release-preset windows-app-release --build-root build/rfc0016-windows-enablement --expected-device-selection enabled --fresh
```
