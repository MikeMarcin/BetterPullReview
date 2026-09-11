---
schema: "implement-rfc.card.v1"
id: "BPR-0720"
title: "Comments are green and includes are grey on the dark palette"
status: "Ready"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-10","attended_decision":"65f3f881-91a3-4388-8b8c-0c69fc0a57d6","needs_owner_confirmation":true}
---

# Comments are green and includes are grey on the dark palette

The owner, 2026-09-10, in the v003 verdict on
[BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md):
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
