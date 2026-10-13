---
id: "RFC-BPR-0075"
title: "Refresh the review title row when accepted content arrives"
status: "In Progress"
depends_on: []
metadata: {"rfc_base_sha":"d9c93c2dbfe29c5fcd6e11b21df4be0a0aead5f1","mandate_revision":"title-row-publication-r1","milestone_plan":{"id":"title-row-publication-v1","path":"docs/rfcs/plans/title-row-publication-v1/milestone-plan.json"}}
---

# Refresh the review title row when accepted content arrives

## Problem and grounded current behavior

On 2026-10-12 the owner reported that the review title and Open on GitHub
button did not appear until they visited Home and returned, or resized the
window. Investigation reproduced both symptoms and both recoveries in the
existing Qt window, offscreen on macOS, at widths of 1280 and 1920 and
at 100%, 125% and 200% scale, without changing product source.
The owner's installed binary and its opening route were not independently
identified.

The decisive paths are in
[workbench_window.cpp](../../src/adapters/qt/src/workbench_window.cpp):

- `update_home_landing` can enter the review surface while a first read is
  still loading. Its toolbar visibility pass runs when `StripHome` changes,
  or while Home is shown. `apply_strip_width` then hides the identity because
  `IdentityText` is empty, and hides the button with it.
- `apply_github_pull_request` later fills `IdentityText` and `PageContext`.
  `update_strip_identity` updates the label text and browser action's
  availability, but does not show the label's widget action. The following
  guard calls `apply_strip_width` only if the button's current visibility
  differs from `IdentityAction->isVisible() && PageContext.has_value()`.
  When the identity is already hidden, both sides are false, so it skips
  the pass that would restore both controls.
- An already offered workbench command keeps the window on the review
  surface through acceptance, so `update_home_landing` need not change
  `StripHome` again. This is an ordinary state: the
  [core command projection](../../src/core/src/workbench_interaction.cpp)
  offers individual drawer toggles independently of comparison availability.
  `resizeEvent` and a Home round trip both rerun `apply_strip_width`, which
  explains the reported recoveries.

The diagnostic sequence used a first-read loading status, two received-page
observations, the core's normal workbench command projection, and a ready
status plus accepted PR projection with a browser context. At a fixed width,
the label had the correct text and the browser action was enabled while both
widgets remained invisible. The direct-acceptance control showed them normally.
The read host's
[publication path](../../src/application/src/github_window_read_host.cpp)
publishes loading observations separately from the accepted snapshot; the
probe exercises the window's public projection interface rather than live
GitHub or a native browser.

The existing
[title-row tests](../../tests/adapters_qt/workbench_window_tests.cpp),
especially `test_open_on_github_beside_the_title`, publish a PR directly before
testing placement and availability. They do not cover the intervening loading
surface with an empty identity and retained offered commands.

## Proposed behavior and boundaries

Every accepted PR publication must settle the title row from the newly
accepted identity, browser context, current surface and current width. Text,
widget visibility, browser action availability and width allocation must
agree by the next normal event-loop presentation, without requiring a resize,
another navigation gesture or another content publication. Apply the same
rule when replacing or clearing accepted content, so the row cannot retain
the prior PR's identity or browser target.

This repairs the existing presentation contract in
[RFC-BPR-0052](RFC-BPR-0052-review-workbench-scaffolding.md#decided-design)
and the accepted
[Open on GitHub behavior](../issues/60-done/BPR-1338-open-the-pull-request-on-github-from-the-title-row-and-home.md#scope).
Retain their placement, width-dependent elision and yield rules, tooltips,
accessibility and shortcuts. A title-row button requires an accepted GitHub
browser context and a visible PR identity. Home keeps its existing row-based
browser command; local cycles keep their own identity without that button.
An accepted content update that preserves window navigation must leave Home,
the chosen modality and the selected file where the reviewer put them.

Qt owns this correction under the
[presentation rules](../architecture/system-architecture.md#qt-presentation-rules).
Reuse the existing title-row measurement and projection path. Browser target
admission remains in the core and application host. No new provider read,
persistent preference or product control is needed. The precise invalidation
mechanism belongs to implementation; it must respond to content changes
directly instead of relying on a later geometry change to repair the row.

No material product choice remains open. This proposal does not adopt a new
visual design. Any visual implementation or owner-facing candidate follows
the repository's required frontend-design skill workflow.

## Acceptance

- **AC01:** A first read that exposes its loading surface before acceptance,
  with normally offered workbench commands retained, shows the accepted
  identity and adjacent Open on GitHub button at a fixed roomy width. The
  observation occurs before any resize or Home round trip and fails against
  the investigated implementation; direct acceptance also succeeds.
- **AC02:** Replacement, same-PR republication and clearing settle the row
  without a geometry change. Its text and browser command refer to the latest
  accepted publication only, and a PR without a browser context offers no
  title-row browser button.
- **AC03:** Content publication preserves the defined Home, local-cycle and
  navigation behavior. Returning to a retained PR presents its current row
  immediately, without a second corrective gesture.
- **AC04:** The corrected transition retains existing title-row order,
  adjacency, accessibility and width-yield behavior at the registered
  100%, 125% and 200% scales; roomy, compact and platform-floor widths obey
  the existing contract without toolbar overflow or visibility oscillation.
