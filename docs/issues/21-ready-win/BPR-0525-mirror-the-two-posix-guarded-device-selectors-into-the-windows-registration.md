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

## Windows — 2026-09-02, station `bpr-win` at `1818d6a0`

Both selectors are registered in `tests/github_contract/CMakeLists.txt`'s
`if(WIN32)` block with their POSIX arguments verbatim, and both pass:

```text
1/2 Test #159: native_https_device_authorization_fixture ......   Passed   10.19 sec
2/2 Test #160: github_device_authorization_refresh_contract ...   Passed    3.74 sec
100% tests passed out of 2
```

`cmake --preset windows-dev` exit 0 and `cmake --build --preset windows-dev
--config Debug` exit 0, with zero errors and zero warnings, before each run.

### `WINDOWS_UNCONTAINED` was measured, not inherited

The card allows the keyword only if measurement shows [[BPR-0362]]'s cause
applies. It was therefore left off first, and both selectors were registered
contained and run:

```text
1/2 Test #159: native_https_device_authorization_fixture ......***Failed  0.19 sec
    native-https-device-authorization:fail:unavailable-executable
2/2 Test #160: github_device_authorization_refresh_contract ...***Failed  0.13 sec
    github-device-authorization-refresh:fail:unavailable-executable
```

Both refuse at once, before reaching their loopback origin, with the driver
unable to resolve its own executable — the drive-letter translation a lowbox
token is refused, which is exactly the cause the wire fixture beside them
records. The keyword was added only then, and the comment beside it says what
was observed rather than what was expected. With it, the two runs above pass;
the label moves from `bpr_network_isolated` to `bpr_windows_uncontained`, as
its sibling's does.

Neither selector reaches a network it is not entitled to. Both drive the same
`verify_native_https_device_authorization.py` against their own loopback
origin, which is what their POSIX registrations do and what the uncontained
exemption is for.

### What this card still needs, and why this station stopped short

| Row | State |
|---|---|
| Both registered in the Windows block with POSIX arguments | Done, this commit |
| Both pass on the Windows station | Done — 10.19 s and 3.74 s above |
| `WINDOWS_UNCONTAINED` only where measurement justifies it | Done — measured contained first, recorded above |
| The Windows entries leave `PLATFORM_SELECTOR_EXCLUSIONS` | **Not done** — that table is in `scripts/**`, which this station may not commit |
| The Windows course reports `RFC0016_COURSE_SELECTORS:17` | **Not run** — see below |

The course was deliberately **not** run after registering these two. The
exclusion table still lists both names, and [[BPR-0524]]'s new check refuses a
name that is both registered and excluded — so a course run at this lane head
would fail with `course-exclusion-stale:windows:...` and that failure would be
an artifact of the split ownership rather than a fact about the platform.
Recording it as a course result would be recording a staged failure. The
integrator prunes the two entries on merge and asks for one final course run,
which is this card's closing row.
