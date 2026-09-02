---
id: BPR-0495
title: The syntax provider builds every Visual Studio configuration and MinSizeRel does not compile
status: In Progress
rfc: RFC-BPR-0016
depends_on: []
---

# The syntax provider builds every Visual Studio configuration and MinSizeRel does not compile

## Why

With Smart App Control off on the JaegAW host ([[BPR-0493]]), a fresh
`windows-dev` configure gets past the `katehighlightingindexer.exe` refusal
for the first time and fails further on, for an unrelated reason.

`windows-dev` is the only multi-config preset. Under a multi-config generator
`cmake/BprSyntaxHighlightingProvider.cmake` builds and installs the vendored
KSyntaxHighlighting dependency once per entry of `CMAKE_CONFIGURATION_TYPES`
(the `foreach(configuration IN LISTS configurations)` loop near line 516),
and Visual Studio defaults that list to all four of
`Debug;Release;MinSizeRel;RelWithDebInfo`. The `MinSizeRel` build fails
deterministically — four reproductions on 2026-09-01, including a serial
non-parallel rebuild — with

```text
error C1083: Cannot open include file:
'kquicksyntaxhighlightingplugin_org_kde_syntaxhighlightingPlugin.moc'
```

The project never builds `MinSizeRel` or `RelWithDebInfo`: every preset and
every course selects Debug or Release. The provider is doing work nothing
consumes, and the work it does not need is the work that fails.

This is the third syntax-provider multi-config defect on this host.
[[BPR-0428]] taught the nested configure to see all four configurations;
[[BPR-0429]] stopped four installs landing in one library. Both made the loop
correct for the configurations it iterates. This card is about which
configurations it iterates at all.

## Required final state

- A fresh `windows-dev` configure completes on the JaegAW host with Smart App
  Control off, reaching the `bpr_syntax_execute` build/install steps for only
  the configurations the project builds.
- The provider builds and installs the dependency for exactly the
  configurations the tree can select — Debug and Release under the
  multi-config preset — and the mechanism is declared where a reader looks
  for it (the preset's `CMAKE_CONFIGURATION_TYPES`, the provider's loop, or
  both), not inferred from a default the generator happens to supply.
- Selecting a configuration the provider did not build fails loudly at
  configure or build time rather than linking against the wrong install.
- macOS and Linux single-config behavior is unchanged; the existing
  `SINGLE_CONFIGURATION` branch still runs once.
- [[BPR-0311]]'s exact ledger command
  (`verify_rfc0016_platform_course.py --platform windows ... --fresh`)
  reaches [[BPR-0492]]'s first real assertion on the Windows host.

## Non-goals

- Making `MinSizeRel` or `RelWithDebInfo` build. Nothing selects them.
- Changing the single-config `windows-app-release` path, which forwards
  `CMAKE_BUILD_TYPE=Release` and is unaffected.

## Verification

```text
cmake --preset windows-dev --fresh
cmake --build --preset windows-dev --config Debug
ctest --test-dir build\windows-dev -C Debug -R '^(syntax_classification_contract|dependency_modernization_contract)$' --output-on-failure
py -3 scripts/verify_rfc0016_platform_course.py --platform windows --dev-preset windows-dev --release-preset windows-app-release --build-root build/rfc0016-windows-enablement --expected-device-selection unavailable --fresh
```

The last command is BPR-0311's; this card is done when it gets past
`configure-failed:windows-dev` and the dependency build logs under
`_bpr/ksyntaxhighlighting/.../logs/` show only the configurations the tree
selects.

## Notes

Reported 2026-09-01 by the hand-started JaegAW session working BPR-0311 in a
`main` checkout; minted here on `main` by the integrator because that session
holds no reservation. Found the same hour Smart App Control was turned off,
which is why no earlier Windows run could have reached it.
