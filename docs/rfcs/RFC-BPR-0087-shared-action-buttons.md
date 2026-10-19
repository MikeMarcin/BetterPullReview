---
id: "RFC-BPR-0087"
title: "Carry the Overview button treatment through the app"
status: "In Progress"
area: "BPR"
depends_on: []
metadata: {"rfc_base_sha": "74ef7f4fb643b2fbbcd4753efcd1fe29c38bbddc", "mandate_revision": "shared-action-buttons-r1", "milestone_plan": {"id": "shared-action-buttons-v1", "path": "docs/rfcs/plans/shared-action-buttons-v1/milestone-plan.json"}}
---

# Carry the Overview button treatment through the app

Adopted for implementation under the owner’s 2026-10-19 request. Immutable
RFC_BASE_SHA: `74ef7f4fb643b2fbbcd4753efcd1fe29c38bbddc`. [Plan](plans/shared-action-buttons-v1/milestone-plan.json);
[import](assets/shared-action-buttons-v1.LIVE_IMPORT.md).

[Verification and continuation](../verification/RFC-BPR-0087.md) records the
source checkpoints, completed technical verification and the pending separate
integration authorization; [migration inventory](../ux/shared-action-buttons.md).

## Problem and grounded current behavior

The owner asked on 2026-10-19 to apply the recently restyled Overview Open
button throughout BetterPullReview, especially Settings → Review. The reference
is the Open/All counts switch, not Open on GitHub or the Home Open action.
BPR-1577 records the owner's acceptance of v002 on 2026-10-17, based on
`7066b48db`: a quiet, palette-derived chip replacing the heavy macOS bezel.
The page-local `counts_mode_button` in
`src/adapters/qt/src/pull_request_overview_page.cpp` implements that treatment.

`workbench_window::build_behaviour_editor` creates ordinary `QToolButton`s for
Review and Comments choices and Reset actions. Appearance's tab-width choices
and diagnostic preferences use other native button constructions. These
equivalent controls consequently differ with platform style and can pick up a
system-coloured bezel under profile-coloured text. Other app actions are a mix
of native push buttons and intentionally specialized controls.

## Proposed behavior and boundaries

Extract the accepted chip treatment into a shared Qt presentation component.
Use it for comparable text choices and secondary actions across Settings,
Overview, Home, and the remaining panels, with an explicit migration inventory.
The first implementation slice covers the reference and Settings → Review,
Comments, and Appearance; remaining inventory entries have a separate rollout
slice so an unfinished app-wide migration is never reported as complete.

Retain the reference's ordinary inherited font, 3 logical-pixel corner radius,
one-pixel border meeting 3:1 against the surface, 7% text-colour tint for a
selected/raised chip, 5% hover and 12% press tints, and inset keyboard focus
ring. A choice has a chip when selected and a bare word otherwise; an ordinary
secondary action has a chip at rest. Colours are derived from the active
profile palette and refresh with palette, font and style changes. Light, Dark,
High Contrast Light and High Contrast Dark remain supported. No QSS, forced
application style, hard-coded theme palette, or global replacement of buttons.

Preserve each control's command, eligibility, checked/exclusive semantics,
object identity, accessible name/description, tooltip, keyboard activation and
focus order. Disabled text and icons use disabled palette roles and do not
respond as enabled controls. Mnemonics, icons, menus, size hints and translated
text must remain functional wherever the shared component is adopted.

Keep deliberate primary/default, destructive, hyperlink, disclosure, icon-only,
toolbar, navigation, custom drawer and colour-swatch treatments distinct.
Inventory entries state either adoption or the concrete reason for retaining a
variant. Reuse the same surface painter beneath appropriate Qt button classes;
do not replace a push button with a tool button merely to share its appearance.
Keep all changes in the Qt adapter and its tests/documentation; no core policy,
settings schema, publication, authentication, release or package changes.

The work stays on an isolated branch. The user's initial request authorized drafting and beginning implementation;
the later disk-cleanup mandate authorized the whole planned rollout. Merge,
push and release remain prohibited. Objective offscreen
renders establish drawing and layout only; they do not establish native desktop
appearance or human acceptance. Native Windows/Linux results stay explicitly
unobserved until measured. There are no unresolved product choices for the
first slice.

## Acceptance criteria

- **AC01:** Overview's selected/unselected Open/All and the migrated Settings
  choices/actions visibly use one shared treatment in all four themes, without
  the heavy native bezel or changes to labels and meaning.
- **AC02:** The app-wide inventory accounts for comparable text buttons on
  every Qt surface. Each is migrated or has an intentional variant reason;
  deferred migrations are visible and prevent app-wide closeout.
- **AC03:** Existing gestures and command outcomes still hold. Keyboard focus,
  exclusivity, checked state, disabled state, accessibility, icon/menu content
  and translated sizing survive adoption, including theme/style changes.
- **AC04:** Representative composed Overview, Settings Review/Comments and
  another migrated page are rendered and inspected with hover, press, focus and
  disabled evidence at 1x, 1.25x and 2x. Available style variants are exercised;
  offscreen and native-platform claims are recorded separately.
- **AC05:** Focused composition and presentation checks, formatting and the
  documentation gate pass. The verification record names actual commands,
  revision, observed results, retained variants and remaining platform work.
