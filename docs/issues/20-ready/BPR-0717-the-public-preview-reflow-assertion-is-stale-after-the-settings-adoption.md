---
schema: "implement-rfc.card.v1"
id: "BPR-0717"
title: "The public-preview reflow assertion is stale after the Settings adoption"
status: "Ready"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"stale_since":"BPR-0646 Settings adoption","first_seen":"97c8f9f1"}
---

# The public-preview reflow assertion is stale after the Settings adoption

`qt_anonymous_public_preview_scale_{100,125,200}` are red at `97c8f9f1` on
one assertion, "compact connection buttons use a separate row below the
profile" (`tests/adapters_qt/anonymous_public_preview_tests.cpp`, the
420-pixel reflow claim). The selector was recorded on
[BPR-0715](../30-in-progress/BPR-0715-opening-a-pull-request-signed-out-no-longer-reaches-a-comparison.md)
so it would not be lost; it is not that regression.

## Cause

`github_session_panel` decides its rows from its own width in
`resizeEvent` (`apply_compact_layout(width() < IdentityLayout->sizeHint().width())`).
Since [BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
adopted the panel onto Settings > Accounts and hosts, the panel is a child
of a laid-out page inside a resizable `QScrollArea` under a window whose
minimum size is 760x480 with a 196-pixel sidebar. The test's
`panel->resize(420, …)` is undone by the page layout on the same
`processEvents()` the test uses to settle: the panel is re-laid at the
viewport's ~720 pixels, never below its identity row's hint, and the
following "wide … beside the profile" assertion passes for the same reason.
[BPR-0685](../60-done/BPR-0685-fit-settings-controls-at-minimum-width.md)'s
`settings_window::resizeEvent` re-fits the sidebar and forces one more
relayout, which is the likeliest point the assertion flipped from
accidentally passing to failing. The product reflow itself is unchanged.

## Scope

- Make the compact claim exercise the panel where it owns its geometry:
  impose the 420-pixel width as a constraint the page layout keeps
  (`setFixedWidth`), release it before the wide claim. Keep the visible,
  accessible-name, focus-order and intent assertions as they are.
- Owned paths: `tests/adapters_qt/anonymous_public_preview_tests.cpp`.

## Acceptance

- `qt_anonymous_public_preview_scale_{100,125,200}` pass, and restoring the
  plain `resize(420, …)` fails the compact assertion again.
- The selectors are part of the pack's integration pass alongside
  `application_anonymous_public_preview_public_path`.
