---
id: BPR-0362
title: The Windows preset and its test disagree about the generator
status: Blocked
rfc: RFC-BPR-0014
depends_on: []
blocked_by: platform-windows
---

# The Windows preset and its test disagree about the generator

`platform_configuration_contract` fails in the default macOS graph:

```
FAIL: test_windows_preset_selects_visual_studio_2022_x64
AssertionError: 'Visual Studio 18 2026' != 'Visual Studio 17 2022'
```

`CMakePresets.json:81` says `Visual Studio 18 2026`.
`tests/scripts/platform_configuration_tests.py:46` still expects
`Visual Studio 17 2022`. The preset was changed by `7cf8356`
("fix(windows): repair never-compiled Windows branches", [[BPR-0336]]) and the
test was not.

Found while running the full `dev` matrix for [[BPR-0348]]; it is unrelated to
that slice and predates it.

## Why this is blocked rather than a one-line test edit

The test name asserts 2022 and the preset now says 2026, so one of them
encodes an intention that is no longer true, and the two answers are not
equivalent:

- if the supported toolchain really moved to VS 2026, the test, its name, and
  every document naming the Windows toolchain move with it;
- if the preset change was incidental to repairing compile branches, the preset
  is what should go back.

Deciding needs a Windows host to say which generator actually builds this tree,
and that host does not exist here. Guessing from macOS would turn a visible
disagreement into an invisible one.

## Required final state

- The preset, the test, its name, and the Windows toolchain documentation agree
  on one generator.
- That generator is the one a real Windows x64 host builds with, confirmed
  there rather than inferred.
- `platform_configuration_contract` passes.
