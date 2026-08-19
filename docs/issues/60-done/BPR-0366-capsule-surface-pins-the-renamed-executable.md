---
id: BPR-0366
title: The capsule surface still pins the pre-BPR-0328 executable name
status: Done
rfc: RFC-BPR-0008
depends_on: []
---

# The capsule surface still pins the pre-BPR-0328 executable name

Found by BPR-0256's regeneration, the first capsule build since [[BPR-0363]]
unblocked the tests-disabled graph. [[BPR-0328]] renamed the tests-disabled
macOS executable to `BetterPullReview` so the Dock tooltip stops falling back
to the filename, and the bundle's `CFBundleExecutable` now says exactly that.
The designated-live capsule surface still pins the old name in four places:

- `scripts/designated_live_macos_capsule.py:21` — `APP_RELATIVE`
- `src/app/src/designated_live_macos_supervisor.cpp:40`
- `src/app/src/designated_live_macos_provenance.cpp:22`
- `tests/platform/macos_capsule_identity_tests.cpp:60`

The capsule `build` subcommand therefore fails against a real fresh bundle:
`otool load-command inspection failed`, because the deployed tree has no
`Contents/MacOS/better_pull_review` to inspect. Nothing caught this because
the hermetic capsule tests construct synthetic trees from `APP_RELATIVE`
itself, so they stay self-consistently green while disagreeing with the real
bundle — the same shape as the linking-is-not-composing lesson.

## Required final state

- Every pinned capsule path names `BetterPullReview.app/Contents/MacOS/BetterPullReview`,
  keeping the exact-path fail-closed design rather than reading the name
  dynamically.
- `designated_live_macos_capsule_contract`, `macos_supervisor_provenance`,
  and `macos_capsule_identity` pass against the updated constants.
- The real proof is the one that failed: the capsule `build` subcommand
  completes against a freshly built tests-disabled Release bundle.

## Verification

```sh
ctest --preset dev -R '^(designated_live_macos_capsule_contract|macos_supervisor_provenance|macos_capsule_identity)$' --output-on-failure
cmake --build build/bpr0256-release --target designated_live_macos_capsule
```

## Result — 2026-08-18

All four pins now name `Contents/MacOS/BetterPullReview`. The fix moved the
capsule build past the name inspection immediately, which surfaced the next
real defect — universal Mach-O framing, tracked and closed as [[BPR-0367]] —
and with both fixes in place the capsule `build` subcommand prints
`designated-live-macos-capsule:pass` against the freshly built tests-disabled
Release bundle. The three focused selectors pass 3/3.

The BPR-0255 production app-source digest drifted as a direct consequence:
since the last pin at `47c6d9e`, the only changed app sources are exactly
this card's `designated_live_macos_supervisor.cpp` and
`designated_live_macos_provenance.cpp`, so the expected manifest digest is
repinned to `ef7a210a` following the BPR-0360 ceremony, and
`application_composition_contract` passes again at the repinned value.
