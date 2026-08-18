---
id: BPR-0363
title: The Windows preset contract names a Visual Studio the preset does not use
status: In Progress
rfc: RFC-BPR-0013
depends_on: []
---

# The Windows preset contract names a Visual Studio the preset does not use

`platform_configuration_contract` fails with:

```
FAIL: test_windows_preset_selects_visual_studio_2022_x64
AssertionError: 'Visual Studio 18 2026' != 'Visual Studio 17 2022'
```

`CMakePresets.json`'s `windows-dev` preset declares
`"generator": "Visual Studio 18 2026"`. `tests/scripts/platform_configuration_tests.py`
asserts `Visual Studio 17 2022`, and its test is named for that expectation.

One of the two is wrong and the test cannot tell you which, which is the point of
this card. The preset is what actually configured and built on this host, so the
generator string is not the mistake. The contract was written when 17 2022 was
the toolchain and was never revisited when the preset moved.

This only surfaced now because the assertion had never executed on a host where
the `windows-dev` preset could be configured at all.

## Required final state

- The contract asserts the generator the preset actually declares, and the test
  name matches the assertion rather than a superseded toolchain.
- The contract still fails if the preset's generator or architecture drifts —
  the repair must not become "accept any generator". `architecture: x64` stays
  asserted exactly.
- If the project's supported Visual Studio range is wider than one exact string,
  the contract says so deliberately and names the range, rather than pinning
  whichever version the current machine happens to have.
- `platform_configuration_contract` passes.

## Not in scope

Changing which Visual Studio the project supports, or adding a second Windows
preset. This card reconciles a contract with the preset beside it; it makes no
toolchain decision. [[BPR-0311]] separately owns adding a native
`windows-app-release` preset, which does not exist yet.

## Verification

```sh
ctest --preset core-only --output-on-failure -R '^platform_configuration_contract$'
py -3 tests/scripts/platform_configuration_tests.py
```
