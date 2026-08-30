---
id: BPR-0492
title: The platform course cannot run under a multi-config generator
status: In Progress
rfc: RFC-BPR-0016
depends_on: []
---

# The platform course cannot run under a multi-config generator

## Why

`docs/verification/RFC-BPR-0016.md` gives [[BPR-0311]] an exact Windows command:

```text
py -3 scripts/verify_rfc0016_platform_course.py --platform windows \
    --dev-preset windows-dev --release-preset windows-app-release \
    --build-root build/rfc0016-windows-enablement \
    --expected-device-selection unavailable --fresh
```

`windows-dev` is the only preset in `CMakePresets.json` that names a generator,
`Visual Studio 18 2026`, and the only one that sets no `CMAKE_BUILD_TYPE` —
because a multi-config generator chooses the configuration at build time
instead. `scripts/verify_rfc0016_platform_course.py` assumes the single-config
shape everywhere it touches a build tree:

- `configure_and_build` runs `cmake --build <dir>` with no `--config`, so a
  multi-config tree builds its default configuration whatever the preset meant.
- `check_course_selectors` runs `ctest --test-dir <dir>` with no `-C`, so ctest
  has no configuration to select tests from.
- `application_binary` probes `src/app/BetterPullReview.exe` and never
  `src/app/<Config>/BetterPullReview.exe`, which is where a multi-config
  generator actually writes it.

The course therefore cannot reach its first assertion on Windows. The failure
is `application-binary-missing:release` at best, and a silently wrong
configuration at worst — a Debug tree scanned as though it were the shipped
Release artifact, which is the more dangerous outcome because it does not look
like a failure.

This is the same class of defect as done [[BPR-0462]], where the inventory
scripts read an empty manifest under multi-config. That one was found and fixed
on the manifest side; this one sits in the course driver and was never
exercised, because no Windows station had ever run the course.

Missing `windows-app-release` is **not** part of this card. Adding and
recording that preset is [[BPR-0311]]'s own work and has to land with that
card's commit.

## Required final state

- The course resolves the build configuration for each tree it configures and
  passes it to `cmake --build`, to `ctest`, and to the artifact probe, so that
  the tree built, the tests run, and the binary scanned are the same
  configuration.
- Multi-config is detected from the configured tree rather than assumed from
  the platform or the preset name; a Ninja tree on Windows and a Visual Studio
  tree both behave correctly.
- A configuration mismatch fails loudly. Scanning a Debug artifact while
  claiming a Release course is the outcome this card exists to make impossible.
- macOS and Linux single-config behavior is unchanged, proven by the existing
  course still passing on this host.
- The Windows course reaches its first real assertion rather than
  `application-binary-missing`, proven target-native.

## Notes

Found 2026-08-29 while dispatching [[BPR-0311]] to `bpr-win-jaegaw`. The
station read the script and reported the three gaps; they were then confirmed
independently against `main` before this card was written. `scripts/*` is on
the station's must-not-commit list, so the repair is the integrator's on
`main` and the station's reproduction is the evidence it is verified against.
