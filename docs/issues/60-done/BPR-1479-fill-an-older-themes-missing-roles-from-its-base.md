---
schema: "implement-rfc.card.v1"
id: "BPR-1479"
title: "Fill an older theme's missing roles from its own base"
status: "Done"
rfcs: ["RFC-BPR-0034", "RFC-BPR-0072"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"owner_direction":"2026-10-13","found_by":"85350f0f8 on claude/interesting-germain-a84def","contract":"RFC-BPR-0034"}
acceptance: [{"criterion":"AC1: an older theme that omits roles, imported while another theme is on screen, previews them in its own base's built-in colours, Keep changes stores them, and a later export writes them as real colours","outcome":"pass","revision":"392256fbc","evidence":{"kind":"observation","detail":"application_appearance_older_theme_contract passes: older-theme.bprtheme (version 1, black canvas, no namespace or enum-member colour) is kept on Light and exported as bpr-theme 2, base light, every fixture line, syntax_namespace #b8d7a3ff and syntax_enum_member #d6a05aff (Dark's built-ins). application_appearance_host's test_older_theme_takes_omitted_roles_from_its_base previews the Dark fill and stores it on Light.","path":"docs/issues/60-done/BPR-1479-fill-an-older-themes-missing-roles-from-its-base.md"}},{"criterion":"AC2: a theme document names its base; version 1 still imports with an inferred base, and the format refuses a missing, unknown or repeated base","outcome":"pass","revision":"392256fbc","evidence":{"kind":"observation","detail":"core_theme_exchange passes: four-base version-2 round trip, missing_field base, unknown_base, duplicate base, base key in version 1, Light/Dark inference on both sides of the white/black text crossover, and with_base_defaults filling only omitted roles. The fuzz target asserts base determinism and the version-2 round trip (syntax-checked; dev builds no fuzz targets).","path":"docs/issues/60-done/BPR-1479-fill-an-older-themes-missing-roles-from-its-base.md"}},{"criterion":"AC3: a fill that cannot be read is refused at import rather than at Keep changes; explicit ink is still checked; stored palettes keep their per-slot rule","outcome":"pass","revision":"392256fbc","evidence":{"kind":"observation","detail":"application_appearance_host refuses a version-2 base light document on a black canvas at import with a syntax_namespace contrast rejection and refuses explicit unreadable namespace ink. sqlite_appearance_settings_storage and application_cpp_identifier_appearance pass with stored documents unchanged at version 1.","path":"docs/issues/60-done/BPR-1479-fill-an-older-themes-missing-roles-from-its-base.md"}}]
---

# BPR-1479: Fill an older theme's missing roles from its own base

## Problem

c58bdebda (RFC-BPR-0072) appended `syntax_namespace` and `syntax_enum_member`
as roles an older document may omit. The import left them
`unassigned_appearance_color`, the host's import contrast check skipped them
(bc4b4aafe), and the Appearance panel previewed them in the built-in colours
of the theme on screen. Keep changes then checked those real colours. A theme
with a dark code background, imported while Light was on screen, previewed
and was refused: "Light: syntax_namespace on code_background is 2.88:1". The
user could see the theme and could not keep it, because of a role they never
set. 85350f0f8 found it while repairing
`application_appearance_theme_customization_contract`.

## Owner direction, 2026-10-13

In the session that found it, the owner chose the rule: "theme should know its
base theme light/dark etc. and new roles should get filled with the base themes
colors on import", and "if you were to export that theme again after import I
would expect those colors to no longer be unassigned but have whatever the
colors they were assigned from the base theme on import". RFC-BPR-0034 records
it as an owner direction.

## What changed

- A theme document is version 2: a `base` line names the
  `appearance_variant` it was made against. Export writes the theme on screen
  as the base. Version 1 is still read.
- A version-1 document's base is inferred from its own `code_background`:
  Dark when white text contrasts with it more than black text does, Light
  otherwise, never high contrast. This rule is the implementer's reading of
  "the theme should know its base" for a document that predates the line.
- The core parses and decides the base and offers `with_base_defaults`. The
  composition hands the host the four built-in palettes, which Qt owns, and the
  host fills from the document's base before the contrast check. What is
  previewed is what Keep changes stores and a later export writes. The host's
  skip of the unassigned sentinel is removed; no import carries it.
- The store keeps writing version-1 documents per variant slot and keeps the
  sentinel for roles an older stored palette predates. Presentation fills
  those from the slot's own variant, which is that palette's base.

## Acceptance

- **AC1:** An older theme that omits roles, imported while another theme is on
  screen, previews them in its own base's built-in colours. Keep changes stores
  them, and a later export writes them as real colours.
- **AC2:** A theme document names its base. Version 1 still imports with an
  inferred base, and the format refuses a missing, unknown or repeated base.
- **AC3:** A fill that cannot be read is refused at import rather than at Keep
  changes, explicit ink is still checked, and stored palettes keep their
  per-slot rule.

## Evidence

Implementation `392256fbc` (with 85350f0f8 cherry-picked as `5f6be7e91`, which
completes `contrast-safe-theme` and prints the probe's commit facts), macOS,
2026-10-13, dev preset, targeted build.

```sh
ctest --test-dir build/dev -j1 -R '^(core_(theme_exchange|appearance_policy|appearance_contrast)|application_appearance_host|application_appearance_profile_consumers|sqlite_appearance_settings_storage|qt_(appearance_settings_panel|appearance_profile|appearance_role_display|bundled_code_font|settings_effect_boundary)|application_appearance_theme_customization_contract|application_appearance_older_theme_contract|application_cpp_identifier_appearance)$' --output-on-failure
```

13 of 14 pass. `application_appearance_older_theme_contract` prints
`APPEARANCE_THEME_EXPORT_BASE:light` and requires the exported document to be
`bpr-theme 2`, `base light`, every fixture line, then
`syntax_namespace #b8d7a3ff` and `syntax_enum_member #d6a05aff`. Before this
change the same fixture failed at `commit-dispatched` with the 2.88:1 finding
(85350f0f8's message). `application_appearance_theme_customization_contract`
checks the complete theme's export in the same shape.

`qt_appearance_profile` fails at "the reprojected position keeps its
comparison and visible reading line within one row", a font-change geometry
check. The binary built in `objective-sutherland-659b90` from a branch without
this change fails at the same line, so it predates BPR-1479.
`test_network_isolation_manifest` fails for `application_source_pr_*` and
`gh_cli_git_smart_transport` rows this slice does not touch; it reports no
finding for `application_appearance_older_theme_contract`, registered
`unmeasured (registered on macOS)`. `architecture_boundaries`, changed-file
clang-format 22, changed-file clang-tidy and the documentation gate pass.

## Limits

- A version-1 document exported from a high-contrast theme is read as Light or
  Dark, so its omitted roles take the standard variant's colours. Every import
  is still held to the high-contrast floors, so the fill is legible or refused.
- The fuzz target is not built by the dev preset; its source was compiled
  syntax-only against the storage test's flags.
- The two new courses ran on macOS only.
