---
id: BPR-0556
title: Support macOS 26 retained-directory spawn without dropping older targets
status: Done
rfc: RFC-BPR-0008
depends_on: []
---

# Support macOS 26 retained-directory spawn without dropping older targets

The current macOS 26 SDK deprecates `posix_spawn_file_actions_addfchdir_np`.
The warnings-as-errors build fails at both retained-root spawn sites, blocking
BPR-0256's capsule refresh and the BPR-0316 platform course.
The same SDK deprecates `posix_spawn_file_actions_addchdir_np` in the native
Keychain fixture's isolated child launcher, so that test uses the same
version-selection pattern for its existing pathname action.

## Required final state

- Use the standardized descriptor-based spawn action on macOS 26 and retain
  the existing descriptor-based API on older SDKs and deployment targets.
- Both verifier and application still spawn relative to the retained root
  descriptor; no pathname reopen or process-global directory change is added.
- The native continuity test, architecture boundary checks, scoped formatting,
  documentation gate, and development build pass.

## Non-goals

No live launch, approval consumption, credential access, or remote mutation.

## Implementation — 2026-09-04

Two local helpers select the standard API only when the SDK declares it and
the running OS is macOS 26 or newer. Older systems keep the existing extension;
only that fallback call suppresses the SDK's deprecation diagnostic. The
supervisor still takes a retained descriptor, while the Keychain fixture
still takes its own disposable directory path. The architecture checker and
its hostile deletions require both descriptor API implementations and both
retained-root call sites. The production-source digest was recomputed from
the reviewed source list.

The code-simplifier pass found no further change needed. Runtime evidence is
macOS 26.6.2 on arm64; macOS 13 compatibility is compilation evidence only.

## Verification

At the implementation based on `b03bc04e`:

- `cmake --build --preset dev`: passed with the macOS 26 SDK and warnings as errors.
- `ctest --preset dev -R '^(macos_supervisor_identity_continuity|architecture_boundaries|architecture_boundary_contract|application_review_round_trip_composition_contract)$' --output-on-failure`: 4/4 passed in 111.76 seconds, including the real composed application path.
- `ctest --preset dev -R '^macos_keychain_native(_setup|_cleanup)?$' --output-on-failure`: 3/3 passed, including exact fixture cleanup.
- `/usr/bin/c++ -std=c++23 -mmacosx-version-min=13.0 -Wall -Wextra -Werror -fsyntax-only src/app/src/designated_live_macos_supervisor_identity.cpp`: passed.
- `clang-format --dry-run --Werror` on the two changed C++ files, `python3 scripts/verify_documentation.py --root .`, and `git diff --check`: passed.

The implementation commit containing this record is the evidence commit.
No temporary checkout or build tree was created. Transient build/test logs in
`/tmp/bpr-0556-{focused,keychain}.log` and `/tmp/bpr-current-dev-build.log`
are consumed and removed after recording these results.
