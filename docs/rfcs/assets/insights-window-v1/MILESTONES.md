# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **insights-window-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## window-and-storage

A fixed-size, per-profile Insights window opens with its six tabs, and Storage shows bytes on disk for every store against its budget.

**Work:** `DRAFT-0001`, `DRAFT-0002`

**Prerequisites:** None

**Completion:**

- Switching every tab and range never changes the window's size, and refresh issues no network, Git or process effect.
- Storage figures match a filesystem walk within 1%, and profile purge removes the Insights store.
- The owner accepts the window shell in an attended review.

**Boundary:** A usable window whose Storage tab and Summary storage tiles are real; the other tabs show their empty states.

## github-budget

The Summary budget bar and the GitHub tab show this hour's Core budget split between BetterPullReview, other clients and what's left, drawn with the shared chart component.

**Work:** `DRAFT-0003`, `DRAFT-0004`

**Prerequisites:** window-and-storage

**Completion:**

- Fixture runs from two clients produce the defined split, including the restart, reset and no-reading states, without any request issued for Insights.
- The owner accepts the charts against AllTheThings' Energy chart in an attended review.

**Boundary:** The shared charts and the complete GitHub cost view.

## review-stats

The Reviews tab shows lifetime and ranged counts, the calendar and records from a store that survives cache prune and delete.

**Work:** `DRAFT-0005`, `DRAFT-0006`

**Prerequisites:** github-budget

**Completion:**

- Each count changes exactly once per event and survives prune, delete, Clear local diagnostics and restart.
- Ranges, calendar and records match a seeded store across a daylight-saving change.

**Boundary:** The complete Reviews tab and its two Summary tiles.

## energy

The Energy tab judges background cost against the idle budget, with foreground and background split on every row.

**Work:** `DRAFT-0007`, `DRAFT-0008`

**Prerequisites:** github-budget

**Completion:**

- Transitions split intervals, and background-launched child CPU counts as background.
- Tiles show background only, and an over-budget range is marked against the budget line.

**Boundary:** The complete Energy tab and the Summary idle-cost tile.

## work

The Work tab shows main-thread stalls against the 100 ms target, diff work limits, the slowest files, and syntax and search figures.

**Work:** `DRAFT-0009`

**Prerequisites:** github-budget

**Completion:**

- A seeded 250 ms block appears as one stall, and the diff limit fixtures are counted under their limits.

**Boundary:** The complete Work tab.

## closeout

RFC-DRAFT-0024 is closed with evidence for every criterion, including export privacy and a measured idle hour on macOS.

**Work:** `DRAFT-0010`

**Prerequisites:** review-stats, energy, work

**Completion:**

- Every acceptance criterion has evidence, and the idle hour meets the idle budget or has a filed card naming the cause.

**Boundary:** RFC closeout.
