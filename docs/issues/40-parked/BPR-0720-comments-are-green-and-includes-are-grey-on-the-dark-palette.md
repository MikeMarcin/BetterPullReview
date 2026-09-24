---
schema: "implement-rfc.card.v1"
id: "BPR-0720"
title: "Comments are green and includes are grey on the dark palette"
status: "Parked"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-10","attended_decision":"65f3f881-91a3-4388-8b8c-0c69fc0a57d6","accepted_as":{"candidate":"BPR-0744-directive-ranges","decision":"4f1d1354-c3c1-43d9-af11-358347147453","date":"2026-09-12"},"blocked_links":["docs/verification/reviews/rfc-bpr-0028-attended-v005/operator-packet.md","docs/verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md"]}
parked_on: "immutable-review-link"
acceptance: [{"criterion":"The appearance role tests pin the confirmed values for comment and preprocessor on the dark palette and fail on the current ones","outcome":"pass","revision":"working tree over dab5fa68","evidence":{"kind":"command","detail":"qt_appearance_profile: test_the_dark_palette_follows_the_visual_assist_scheme pins comment/documentation 66B559, preprocessor/import A3A3A3, keyword 6DA9DB, type 4EC9B0, function DCDCAA, string D69D85, number B5CEA8 on dark and the untouched light comment/preprocessor; it failed against the previous values before the palette edit and passes after; the contrast audit and headroom ratchet (dark 1 hundredth) pass."}},{"criterion":"qt_appearance_* and the theme customization course pass; the two application digests are repinned if the defaults are part of them","outcome":"pass","revision":"working tree over dab5fa68","evidence":{"kind":"command","detail":"qt_appearance_profile, qt_appearance_settings_panel, application_appearance_host, application_appearance_profile_consumers, core_appearance_contrast, core_appearance_policy, sqlite_appearance_settings_storage (version-3 reset once; version 3 in the legacy refusal sweep), application_appearance_store_fallback_contract, application_appearance_gesture_contract, application_review_round_trip_composition_contract, application_composition_contract, qt_composed_github_scale_100 pass in build/dev; no digest drifted (github_application.cpp untouched); check_boundaries, clang-format --Werror, strict tidy on the three touched TUs and git diff --check clean. application_appearance_theme_customization_contract is red at cc31119b before this change with BPR-0723's STARTUP_FAILED signature and is recorded on BPR-0723."}},{"criterion":"The owner accepts the rendering on the signed bundle in the next attended session","outcome":"pass","revision":"aff9fc64","evidence":{"kind":"observation","detail":"The v008 rejection (decision fed7a1bf, bundle e4fdb0ea at 35438244) stays recorded on this card against its own revision. Accepted in attended session cbd48b91000b00f1e72e1ad62c7c384c (v009, packet docs/verification/reviews/rfc-bpr-0028-attended-v009) as candidate BPR-0744-directive-ranges, revision app-release-c2381f32-2026-09-12.5, fingerprint a321ed197669c28e51b7945dd1352a9af634ae12cf63b63b71e1338513692eec equal to the prepared candidate's in that packet's session-identity.json, decision 4f1d1354-c3c1-43d9-af11-358347147453, actor human, 2026-09-12T16:06:15Z: accept, note empty. The candidate told the owner it 'carries the criterion BPR-0720 and BPR-0738 have both been carrying since v005', and its criteria were a stdexec C++ header on the dark palette (directive keyword grey, ALL_CAPS macro purple, #include path salmon) and the identifier roles in ordinary code (functions FF8000, types FFD700, macros purple, variables BDB76B, attributes 4EC9B0, comments green, strings salmon). The bundle was build/app-release from source aff9fc64, BUILD_TESTING=OFF, executable sha256 c2381f325093200563544ce3bb9e1ba04f847102e2f5b599ca6cc51c6f555653, codesign --verify --deep --strict exit 0; the owner had it open from 16:03:30Z to 16:06:34Z (diagnostics.log op=application). The third question that rode the candidate (every identifier in #if/#ifdef/defined() purple) is not answered by the accept and stays on BPR-0748.","path":"docs/issues/60-done/BPR-0744-preprocessor-directives-are-grey-but-their-arguments-take-their-own-roles.md"}}]
---

# Comments are green and includes are grey on the dark palette

The owner, 2026-09-10, in the v003 verdict on
[BPR-0646](../60-done/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md):
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

The two application screenshots show this card's values where a role is emitted and [BPR-0726](../60-done/BPR-0726-functions-types-and-macros-are-uncoloured-in-c-and-c-plus-plus.md)'s roles on every call, type and macro; BPR-0726 was accepted thirty-five seconds after this verdict, so the classification is settled and the rejection is the colours. The reference is Visual Studio 2022 dark with Visual Assist, which paints macros purple (`BD63C5` sampled) and functions gold (`FFD700`); this card chose Visual Studio's own `DCDCAA` for functions and, because BPR-0726 classifies an ALL_CAPS macro as the preprocessor role, macros paint the directive grey `A3A3A3`. Carded as [BPR-0738](BPR-0738-macros-and-functions-take-the-visual-assist-colours-on-the-dark-palette.md), which lists the sampled reference values and lets the slice decide whether macros need a role of their own. The reset and the other emitted colours were not disputed.

This card stays parked on human-review; its candidate is asked again on a bundle carrying BPR-0738.

## Owner answer, 2026-09-11 (mandate r9)

The owner confirmed that a distinct macro role is necessary and corrected
the v007 reading: in the reference, functions are orange and class/struct
types are yellow. A per-token resample of the reference agrees (macros
`BD63C5`, functions `FF8000`, types `FFD700`); the corrected table and the
values the slice pins are on
[BPR-0738](BPR-0738-macros-and-functions-take-the-visual-assist-colours-on-the-dark-palette.md).
The sentence above that calls functions "gold (`FFD700`)" is the
misattribution it corrects. This card's candidate is asked again in v008
on a bundle carrying BPR-0738.

## Attended candidate, 2026-09-11 (v008)

Asked again as candidate `BPR-0720-dark-palette` in
[rfc-bpr-0028-attended-v008](../../verification/reviews/rfc-bpr-0028-attended-v008/operator-packet.md)
on the bundle built from the source at 35438244 (executable sha256
`e4fdb0ea…`, tests disabled, codesign clean), candidate revision
`app-release-e4fdb0ea-2026-09-11.4`; prepared and protected at sequence
341, undecided, no host running until the owner says so. The pull request
is NVIDIA/stdexec#1846, the owner's answer of mandate r9. The roles themselves are judged
on BPR-0738's candidate; the v007 rejection stays recorded against its own
revision.

## Attended verdict, 2026-09-11 (v008): rejected

Session `cbd48b91…`, revision `app-release-e4fdb0ea-2026-09-11.4` (bundle built at 35438244, executable sha256 `e4fdb0ea…`), actor human; handed back at sequence 348 with all six candidates decided. Candidate `BPR-0720-dark-palette`, decision `fed7a1bf`, 2026-09-12T04:28:03Z, two attachments
([a stdexec header in Read](../../verification/reviews/rfc-bpr-0028-attended-v008/captures/owner-dark-palette-preprocessor-2026-09-11.png)
and [the v007 Visual Studio reference](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-visual-studio-reference-2026-09-11.png)
re-attached byte-identical): **reject**. Verbatim:

> comments are green, things are mostly right, but preprocess being grey is taken too far compared to the reference. The preprocessor directives themselves should be grey but not necessarily their arguments. Macros in preprocessor should still color as macros (purple). Strings in #include statements should color as the reference image

Comments are settled at last ("comments are green"), and so is the rest of
the palette in his reading ("things are mostly right"). What is rejected is
one thing: a directive line paints grey end to end.
`#include "../../stdexec/__detail/__config.hpp"`, `#if STDEXEC_USE_MODULES()`
and `# include <concepts>` are all one grey run in the capture, where the
reference greys `#include` and `#if` and paints the macro purple and the path
salmon.

This revises the owner direction this card recorded on 2026-09-11, "the whole
palette, yes reset", whose written form was "grey preprocessor lines including
the include path". That stays true of the directive and is no longer true of
its arguments; RFC-BPR-0006 carries the revision.

Carded as [BPR-0744](../60-done/BPR-0744-preprocessor-directives-are-grey-but-their-arguments-take-their-own-roles.md):
the directive keyword keeps `syntax_preprocessor`, an ALL_CAPS argument takes
the `syntax_macro` role BPR-0738 added, and an `#include` path is painted as
the reference paints it. The classifier can already emit roles at sub-line
ranges; what collapses the line is the grammar's default Preprocessor
attribute and our identifier post-pass running only over `syntax_normal` runs
(`packaged_syntax_classifier.cpp:435-448`). This card stays parked on
human-review; its criterion is asked again on a bundle carrying BPR-0744.

## Attended candidate, 2026-09-12 (v009)

This card's attended criterion is asked as candidate
`BPR-0744-directive-ranges` in [rfc-bpr-0028-attended-v009](../../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md), on the
bundle built from the source at aff9fc64 (executable sha256 `c2381f32…`, tests
disabled, codesign clean), candidate revision `app-release-c2381f32-2026-09-12.5`;
prepared and protected at sequence 357, undecided, no host running until the
owner says so. It is one candidate for this card, [[BPR-0738]] and [[BPR-0744]]
together: the owner's v008 notes on the two separate candidates were one
requirement stated twice, and asking it twice again would repeat the mistake.
The v008 rejection stays recorded against its own revision.

The latest [attended v010 packet](../../verification/reviews/rfc-bpr-0028-attended-v010/operator-packet.md)
asks only [[BPR-0729]]'s one-pane layout criterion. This card's v009
candidate remains bound to its recorded v009 revision and verdict; it is not
re-asked on the v010 bundle.

## Current packet pointer, 2026-09-12 (v011)

The current [v011 attended packet](../../verification/reviews/rfc-bpr-0028-attended-v011/operator-packet.md) asks only [[BPR-0646]]'s epoch-handle criterion. This card is not a candidate; its earlier bundle verdict remains bound to that revision.

## Accepted; parked on immutable-review-link (main, 2026-09-24)

Every criterion now passes. The attended one was accepted on 2026-09-12 as
candidate `BPR-0744-directive-ranges` in the protected
[v009 packet](../../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md):
revision `app-release-c2381f32-2026-09-12.5`, fingerprint `a321ed19…` equal to
the prepared candidate's, decision `4f1d1354-c3c1-43d9-af11-358347147453`,
`actor: human`, 16:06:15Z, **accept**, note empty. The bundle was built from
`aff9fc64` with tests disabled, executable sha256 `c2381f32…`, and
`codesign --verify --deep --strict` was clean. The candidate told the owner it
carried the criterion this card and its sibling had held since v005. The
reasoning and the owner's open bundle are recorded on [[BPR-0744]] (Done). The
question that rode the candidate, whether every identifier in `#if`, `#ifdef`,
`#ifndef`, `#elif`, `#undef` and `defined()` should be purple, is not answered
by the accept and stays on [[BPR-0748]].

**Why this card is not in Done.** Moving it to `60-done` would change a link
inside a protected historical review packet, and the board refuses that
(`move would change protected content`). Per the project's
`immutable-review-link` rule, the card is **parked on
`immutable-review-link`**, not on human-review. The packet's bytes stay
intact, and nothing is waiting on the owner. The exact blocked links:

- [rfc-bpr-0028-attended-v005/operator-packet.md](../../verification/reviews/rfc-bpr-0028-attended-v005/operator-packet.md),
  line 7: `../../../issues/40-parked/BPR-0720-comments-are-green-and-includes-are-grey-on-the-dark-palette.md`
- [rfc-bpr-0028-attended-v009/operator-packet.md](../../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md),
  line 62: the same path.

When a path-preserving migration exists, the card moves to Done with the
acceptance above unchanged. This was recorded when the main integrator
consumed the 2026-09-24 hand-back (session `cbd48b91`, sequence 589); the
preparer flagged this card as board upkeep, not as a question.
