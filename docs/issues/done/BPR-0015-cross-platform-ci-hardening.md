---
id: BPR-0015
title: Harden RFC-BPR-0001 cross-platform CI selection
status: Done
rfc: RFC-BPR-0001
depends_on: [BPR-0014]
---

# Harden RFC-BPR-0001 cross-platform CI selection

Give Windows an explicit Visual Studio 2022 CMake preset compatible with the
selected Qt archive, pin every directly selected action to a full commit, and
make PE dependency inspection tolerate the supported runner tool layouts.

## Completion evidence

- CMake validates and lists the platform-specific presets.
- The workflow selects the Windows preset and `win64_msvc2022_64` Qt archive.
- Qt installation and Python setup actions are directly SHA-pinned.
- Focused tests prove the Windows dependency-inspection tool selection and
  fallback order.
- macOS `dev` and `core-only` matrices remain passing.

## Result

- Added a Windows-only Visual Studio 2022 x64 configure preset with Debug
  build/test presets.
- Selected `win64_msvc2022_64` explicitly in the Windows matrix entry.
- Replaced the wrapper Qt action with its SHA-pinned bundled implementation and
  added a SHA-pinned Python setup action.
- Added deterministic `dumpbin`, `llvm-readobj`, then `objdump` PE inspection
  fallback selection.

## Evidence

- JSON, CMake preset, and workflow syntax validation: passed.
- CI configuration contract: 4/4 passed.
- Windows dependency-tool selection contract: 4/4 passed.
- Complete Qt-enabled matrix: 14/14 passed.
- Complete Qt-free matrix: 9/9 passed.
- Documentation validator and `git diff --check`: passed.
