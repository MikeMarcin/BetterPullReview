---
schema: "implement-rfc.card.v1"
id: "BPR-0720"
title: "Comments are green and includes are grey on the dark palette"
status: "Parked"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-10","attended_decision":"65f3f881-91a3-4388-8b8c-0c69fc0a57d6","needs_owner_confirmation":true}
parked_on: "human-review"
acceptance: [{"criterion":"The appearance role tests pin the confirmed values for comment and preprocessor on the dark palette and fail on the current ones","outcome":"pass","revision":"working tree over dab5fa68","evidence":{"kind":"command","detail":"qt_appearance_profile: test_the_dark_palette_follows_the_visual_assist_scheme pins comment/documentation 66B559, preprocessor/import A3A3A3, keyword 6DA9DB, type 4EC9B0, function DCDCAA, string D69D85, number B5CEA8 on dark and the untouched light comment/preprocessor; it failed against the previous values before the palette edit and passes after; the contrast audit and headroom ratchet (dark 1 hundredth) pass."}},{"criterion":"qt_appearance_* and the theme customization course pass; the two application digests are repinned if the defaults are part of them","outcome":"pass","revision":"working tree over dab5fa68","evidence":{"kind":"command","detail":"qt_appearance_profile, qt_appearance_settings_panel, application_appearance_host, application_appearance_profile_consumers, core_appearance_contrast, core_appearance_policy, sqlite_appearance_settings_storage (version-3 reset once; version 3 in the legacy refusal sweep), application_appearance_store_fallback_contract, application_appearance_gesture_contract, application_review_round_trip_composition_contract, application_composition_contract, qt_composed_github_scale_100 pass in build/dev; no digest drifted (github_application.cpp untouched); check_boundaries, clang-format --Werror, strict tidy on the three touched TUs and git diff --check clean. application_appearance_theme_customization_contract is red at cc31119b before this change with BPR-0723's STARTUP_FAILED signature and is recorded on BPR-0723."}},{"criterion":"The owner accepts the rendering on the signed bundle in the next attended session","outcome":"pending","revision":"working tree over dab5fa68","evidence":{"kind":"observation","detail":"Candidate BPR-0720-dark-palette in the v005 attended packet on the tests-disabled bundle rebuilt at this head; the owner's store (schema 3, light-bound stored palette) is reset once on first launch by the schema-4 migration."}}]
---

# Comments are green and includes are grey on the dark palette

The owner, 2026-09-10, in the v003 verdict on
[BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md):
"Also comments a grey instead of green like was requested with visual
studio dark mode with visual assist color scheme. And includes are green not
grey, seems like those colors got swapped maybe? Or the theme in general
just got messed up."

## What the code says

Nothing was swapped and nothing changed recently. The dark palette in
`src/adapters/qt/src/appearance_profile.cpp:146-170` has had comment
`0xA5B0BA` (light grey) and import `0x44C756` (green) since ff7e9946
(BPR-0199, 2026-08-07); preprocessor `0xD0A0E8` (lavender) is unchanged since
e05c385b, and the style-to-role mapping in
`src/adapters/syntax/src/packaged_syntax_classifier.cpp:41-105` is unchanged
since 5c08db3c. The green part of an `#include` line is most likely the
`<path>` or `"path"` token, which the highlighter classes as import or
string, both green. The owner's screenshots on the decision show comments
in grey and `const auto` keywords in orange on the dark palette. No source
mentions Visual Studio or Visual Assist; the earlier "diff colour themes are
finished" note covers the diff row and gutter themes, not this request,
which is about the syntax roles.

So this is a requirement statement, not a regression: the owner wants the
dark palette's syntax roles to follow Visual Studio dark with the Visual
Assist scheme, where comments are green and preprocessor lines (including
the include path) are grey.

## Held until the owner confirms

Before any edit, the owner confirms two things this card cannot infer:
whether the whole dark syntax palette should move to the Visual Assist
scheme (keywords, types, strings, numbers too) or only comments and
includes; and whether the saved custom palette on the owner's machine
(8280c964's customization store) should be reset to the new defaults or
left, since a saved customization overrides the defaults.

## Scope

- The dark syntax roles for comment and preprocessor (directive and its
  path) match the confirmed scheme; the light palette is untouched unless
  the owner says otherwise.
- Owned paths: `src/adapters/qt/src/appearance_profile.cpp`, the
  appearance tests that pin role colours, and the customization defaults if
  a reset is confirmed.

## Acceptance

- The appearance role tests pin the confirmed values for comment and
  preprocessor on the dark palette and fail on the current ones.
- `qt_appearance_*` and the theme customization course pass; the two
  application digests are repinned if the defaults are part of them.
- The owner accepts the rendering on the signed bundle in the next attended
  session.

## Owner's answer, 2026-09-11

Asked whether the whole dark syntax palette moves to the Visual Assist
scheme or only comments and includes, and whether the saved custom
palette is reset or left, the owner answered verbatim: "the whole palette,
yes reset".

## Implementation, 2026-09-11

The dark profile's syntax roles in `src/adapters/qt/src/appearance_profile.cpp`
now follow Visual Studio's dark theme with Visual Assist colouring: normal
`DCDCDC`, keyword and builtin `6DA9DB`, control flow `D8A0DF`, operator
`B4B4B4`, function `DCDCAA`, variable `9CDCFE`, type `4EC9B0`, constant
`B8D7A3`, number `B5CEA8`, string and character `D69D85`, escape `FFD68F`,
comment and documentation `66B559`, annotation `8FBF72`, preprocessor,
import and region marker `A3A3A3`, attribute `C8C8C8`. Three of Visual
Studio's exact values fall under the product's 4.5:1 floor on the added
and removed row fills (keyword `569CD6` at 3.94:1, comment `57A64A` at
3.85:1 through the selection wash, preprocessor `9B9B9B` at 4.18:1), so
they are lightened along their own hue to the first value that clears it;
the profile's contrast audit and its headroom ratchet (dark stays at 1
hundredth) pass. The four note roles (information, warning, alert, error)
and `other` are not part of the Visual Studio scheme and keep their
colours; the light and high-contrast palettes are untouched; the code,
gutter and diff backgrounds are the settled diff themes and are untouched.

The reset: the owner's store (`state/v1/appearance-settings.sqlite3`,
schema 3) holds a stored palette bound to the light variant, so the dark
palette on screen was already the built-in one, and "reset" means that
stored customisation goes. `appearance_settings_schema_version` is now 4
with the same columns; opening a version 1, 2 or 3 file carries the row
across a fresh table and clears `palette_stored` once, and a
current-version file is left alone (`appearance_settings_database.cpp`).
An older build opened afterwards refuses the version-4 store rather than
reading it. `sqlite_appearance_settings_storage` adds the version-3 reset
(once, not on every open) and version 3 to the legacy refusal sweep;
`test_the_dark_palette_follows_the_visual_assist_scheme` pins the values
above and fails on the previous ones.

Verification at this head: `qt_appearance_profile`,
`qt_appearance_settings_panel`, `application_appearance_host`,
`application_appearance_profile_consumers`, `core_appearance_contrast`,
`core_appearance_policy`, `sqlite_appearance_settings_storage`,
`application_appearance_store_fallback_contract`,
`application_appearance_gesture_contract`,
`application_review_round_trip_composition_contract`,
`application_composition_contract` and `qt_composed_github_scale_100`
pass; `check_boundaries`, `clang-format --Werror`, strict tidy on the
touched translation units and `git diff --check` are clean. The two
application digests did not drift (no `github_application.cpp` edit).
`application_appearance_theme_customization_contract` is red at HEAD
before this change with BPR-0723's `STARTUP_FAILED` signature and is
recorded there, not here. The attended criterion (the owner accepts the
rendering on the signed bundle) is candidate `BPR-0720-dark-palette` in
the v005 packet.
