---
id: BPR-0339
title: Make the windows-dev multi-config preset generate
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0338]
---

# Make the windows-dev multi-config preset generate

[[BPR-0336]] retargeted `windows-dev` from the uninstalled `Visual Studio 17
2022` generator to `Visual Studio 18 2026`. That was necessary and not
sufficient, which only became visible once the preset was actually run.
[[BPR-0036]] names this preset specifically, so it has to generate.

`windows-dev` is the only preset in the tree that uses a **multi-config**
generator; `dev`, `core-only`, and `app-release` all use Ninja. Configuration
succeeded and generation then failed, once per configuration:

```
CMake Error in tests/CMakeLists.txt:
  Evaluation file to be written multiple times with different content.
   .../build/windows-dev/_bpr/automated-executables.txt
```

`tests/CMakeLists.txt` writes the automated-executable manifest with
`file(GENERATE)`, and its content is a list of `$<TARGET_FILE:...>` values.
Those differ per configuration, so a multi-config generator evaluates the same
fixed output path four times with four different bodies and CMake refuses.

The manifest path is now keyed by `$<CONFIG>`. That is correct for
single-config generators too, where `$<CONFIG>` is simply the one build type,
and it matches what the manifest actually describes: the binaries of one
configuration. The sole consumer outside that file,
`tests/end_to_end/CMakeLists.txt`, passes the path inside a test command where
generator expressions evaluate, so it takes the same change.

## Evidence

- `cmake --preset windows-dev` now reports `Generating done` and writes build
  files. Four manifests appear, one each under `_bpr/Debug/`,
  `_bpr/MinSizeRel/`, `_bpr/Release/`, and `_bpr/RelWithDebInfo/`.
- `cmake --preset dev` and `cmake --preset core-only` still configure and
  generate, writing a single manifest under `_bpr/Debug/`.
- `file(GENERATE)` appears exactly once in the tree, so no other evaluation
  file carries the same defect.

## What this card does not claim

The `windows-dev` graph has been **configured and generated, not built**. The
Ninja `dev` graph is the one [[BPR-0338]] compiled and linked. Building all
four Visual Studio configurations is a separate cost that [[BPR-0036]] should
decide on, and other multi-config assumptions may exist beyond this one — the
manifest is simply the first that generation reached.

No test executed. The Windows suite remains fully gated by the boundary
[[BPR-0235]] owns.
