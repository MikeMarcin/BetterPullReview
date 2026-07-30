---
id: BPR-0015
title: Preserve RFC-BPR-0001 platform-local verification
status: Done
rfc: RFC-BPR-0001
depends_on: [BPR-0014]
---

# Harden RFC-BPR-0001 platform-local verification

Give Windows an explicit Visual Studio 2022 CMake preset compatible with the
selected Qt toolchain and make PE dependency inspection tolerate supported
developer and release-environment tool layouts.

## Completion evidence

- CMake validates and lists the platform-specific presets.
- Focused tests prove the Windows dependency-inspection tool selection and
  fallback order.
- macOS `dev` and `core-only` matrices remain passing.

## Result

- Added a Windows-only Visual Studio 2022 x64 configure preset with Debug
  build/test presets.
- Added deterministic `dumpbin`, `llvm-readobj`, then `objdump` PE inspection
  fallback selection.
- A later product-owner clarification removed the temporary hosted workflow;
  the platform-local preset and inspector remain valid.

## Evidence

- JSON and CMake preset validation: passed.
- Platform configuration contract: passed.
- Windows dependency-tool selection contract: 4/4 passed.
- Complete Qt-enabled matrix: 14/14 passed.
- Complete Qt-free matrix: 9/9 passed.
- Documentation validator and `git diff --check`: passed.
