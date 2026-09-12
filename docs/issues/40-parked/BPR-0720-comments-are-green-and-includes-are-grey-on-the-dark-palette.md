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
acceptance: [{"criterion": "The appearance role tests pin the confirmed values for comment and preprocessor on the dark palette and fail on the current ones", "outcome": "pass", "revision": "working tree over dab5fa68", "evidence": {"kind": "command", "detail": "qt_appearance_profile: test_the_dark_palette_follows_the_visual_assist_scheme pins comment/documentation 66B559, preprocessor/import A3A3A3, keyword 6DA9DB, type 4EC9B0, function DCDCAA, string D69D85, number B5CEA8 on dark and the untouched light comment/preprocessor; it failed against the previous values before the palette edit and passes after; the contrast audit and headroom ratchet (dark 1 hundredth) pass."}}, {"criterion": "qt_appearance_* and the theme customization course pass; the two application digests are repinned if the defaults are part of them", "outcome": "pass", "revision": "working tree over dab5fa68", "evidence": {"kind": "command", "detail": "qt_appearance_profile, qt_appearance_settings_panel, application_appearance_host, application_appearance_profile_consumers, core_appearance_contrast, core_appearance_policy, sqlite_appearance_settings_storage (version-3 reset once; version 3 in the legacy refusal sweep), application_appearance_store_fallback_contract, application_appearance_gesture_contract, application_review_round_trip_composition_contract, application_composition_contract, qt_composed_github_scale_100 pass in build/dev; no digest drifted (github_application.cpp untouched); check_boundaries, clang-format --Werror, strict tidy on the three touched TUs and git diff --check clean. application_appearance_theme_customization_contract is red at cc31119b before this change with BPR-0723's STARTUP_FAILED signature and is recorded on BPR-0723."}}, {"criterion": "The owner accepts the rendering on the signed bundle in the next attended session", "outcome": "fail", "revision": "2cdc0e57", "evidence": {"kind": "observation", "detail": "Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0720-dark-palette, revision app-release-64d9e46a-2026-09-11.3 (bundle built at 2cdc0e57, executable sha256 64d9e46a0fa7cff7e37d778d329a8e71bcf6ff13547bbc82eadb547ebb252827), fingerprint 77a5c7fd3191fb571f792048148c1573c56171c048f95a8857b5073989374f7e, decision 7789e40f-d61d-4900-813c-f23c508db56a, actor human, 2026-09-11T23:45:40Z, three screenshots: reject. Verbatim: 'It's better but still off macros are still grey, functions arent orange etc'. BPR-0726 (which words get a role) was accepted 35 s later; what is rejected is the colour the macro and function roles paint against the owner's Visual Studio + Visual Assist reference (macros purple BD63C5, functions gold FFD700 in the reference; A3A3A3 and DCDCAA here), carded as BPR-0738. The reset and the other emitted colours were not disputed."}}]
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

## Attended candidate, 2026-09-11 (v005)

The pending criterion above is candidate `BPR-0720-dark-palette` in
[rfc-bpr-0028-attended-v005](../../verification/reviews/rfc-bpr-0028-attended-v005/operator-packet.md)
on the bundle built from the source at 25e1525d (executable sha256
`d238842d…`), candidate revision `app-release-d238842d-2026-09-11.1`;
prepared, undecided, no host running until the owner asks for the session.
The v004 packet is historical.

## The theme customization contract, rerun, 2026-09-11

`application_appearance_theme_customization_contract`, the direct course
for this palette on the application public path, passes (23.69 s and
23.80 s on the build with this card's change) once
[BPR-0723](../60-done/BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md)
narrows the contract's pre-created root; the second criterion's evidence
now includes it. The attended criterion is unchanged.

## Attended candidate, 2026-09-11 (v006)

The pending criterion above is candidate `BPR-0720-dark-palette` in
[rfc-bpr-0028-attended-v006](../../verification/reviews/rfc-bpr-0028-attended-v006/operator-packet.md)
on the bundle built from the source at d1ba791f (executable sha256
`cc2afa7f…`), candidate revision `app-release-cc2afa7f-2026-09-11.2`;
prepared, undecided, no host running until the owner asks for the session.
The ask is unchanged; the v005 packet is historical.

## Attended verdict, 2026-09-11 (v006): rejected

Session `cbd48b91…`, revision `app-release-cc2afa7f-2026-09-11.2` (bundle built at d1ba791f, executable sha256 `cc2afa7f…`), actor human; handed back at sequence 278 with all fifteen candidates decided. Candidate `BPR-0720-dark-palette`, decision `0f243620`, 18:39:22Z, one screenshot
([owner-dark-palette-2026-09-11.png](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-dark-palette-2026-09-11.png)): **reject**.
Verbatim: "Missing a lot of coloring, including class, functions. macros".

The screenshot shows this card's values wherever a role is emitted: comments `66B559`,
keywords `6DA9DB` (`auto`, `const`, `void`, `true`, `case`, `unsigned`), strings `D69D85`,
numbers. `ProcessString`, `GetTextBuffer`, `ScreenBufferTests`, `TextAttribute`,
`ServiceLocator`, `Log::Comment`, `VERIFY_ARE_EQUAL` and `BEGIN_TEST_METHOD_PROPERTIES` are in
the plain `DCDCDC` ink because the classifier only maps KSyntaxHighlighting's styles to roles
and the upstream C++ grammar has no Function style, types only its built-ins and `*_t` names,
and declares macros `dsNormal`, which `applyFormat` drops
(`src/adapters/syntax/src/packaged_syntax_classifier.cpp:41-110, 236-245`). The function
`DCDCAA` and type `4EC9B0` values this card set are never asked for on C++. Carded as
[BPR-0726](../60-done/BPR-0726-functions-types-and-macros-are-uncoloured-in-c-and-c-plus-plus.md)
(heuristic classification of calls, types and ALL_CAPS macros for C and C++). The reset and
the emitted colours were not disputed. This card stays parked on human-review; its candidate
is asked again on a bundle carrying BPR-0726.

## Attended candidate, 2026-09-11 (v007)

The pending criterion above is candidate `BPR-0720-dark-palette` in
[rfc-bpr-0028-attended-v007](../../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md)
on the bundle built from the source at 2cdc0e57 (executable sha256
`64d9e46a…`), candidate revision `app-release-64d9e46a-2026-09-11.3`;
prepared and protected at sequence 311, undecided, no host running until
the owner asks for the session. The ask: the palette on a bundle carrying BPR-0726's identifier roles; the values and the one-time reset are unchanged.

## Attended verdict, 2026-09-11 (v007): rejected

Session `cbd48b91…`, revision `app-release-64d9e46a-2026-09-11.3` (bundle built at 2cdc0e57, executable sha256 `64d9e46a…`), actor human; handed back at sequence 322 with all ten candidates decided. Candidate `BPR-0720-dark-palette`, decision `7789e40f`, 23:45:40Z, three screenshots
([terminalOutput.cpp](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-dark-palette-terminaloutput-2026-09-11.png),
[the Visual Studio reference](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-visual-studio-reference-2026-09-11.png),
[ScreenBufferTests.cpp](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-dark-palette-screenbuffertests-2026-09-11.png)): **reject**. Verbatim:

> It's better but still off macros are still grey, functions arent orange etc

The two application screenshots show this card's values where a role is emitted and [BPR-0726](../60-done/BPR-0726-functions-types-and-macros-are-uncoloured-in-c-and-c-plus-plus.md)'s roles on every call, type and macro; BPR-0726 was accepted thirty-five seconds after this verdict, so the classification is settled and the rejection is the colours. The reference is Visual Studio 2022 dark with Visual Assist, which paints macros purple (`BD63C5` sampled) and functions gold (`FFD700`); this card chose Visual Studio's own `DCDCAA` for functions and, because BPR-0726 classifies an ALL_CAPS macro as the preprocessor role, macros paint the directive grey `A3A3A3`. Carded as [BPR-0738](../20-ready/BPR-0738-macros-and-functions-take-the-visual-assist-colours-on-the-dark-palette.md), which lists the sampled reference values and lets the slice decide whether macros need a role of their own. The reset and the other emitted colours were not disputed.

This card stays parked on human-review; its candidate is asked again on a bundle carrying BPR-0738.

## Owner answer, 2026-09-11 (mandate r9)

The owner confirmed that a distinct macro role is necessary and corrected
the v007 reading: in the reference, functions are orange and class/struct
types are yellow. A per-token resample of the reference agrees (macros
`BD63C5`, functions `FF8000`, types `FFD700`); the corrected table and the
values the slice pins are on
[BPR-0738](../20-ready/BPR-0738-macros-and-functions-take-the-visual-assist-colours-on-the-dark-palette.md).
The sentence above that calls functions "gold (`FFD700`)" is the
misattribution it corrects. This card's candidate is asked again in v008
on a bundle carrying BPR-0738.
