---
schema: "implement-rfc.card.v1"
id: "BPR-1615"
title: "Verify and close RFC-BPR-0086"
status: "Backlog"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1606", "BPR-1607", "BPR-1608", "BPR-1609", "BPR-1610", "BPR-1611", "BPR-1612", "BPR-1613", "BPR-1614"]
closeout_for: ["RFC-BPR-0086"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
---

# Close Insights

## Scope and acceptance

Cover RFC-BPR-0086 AC01 to AC12 with actual evidence during authorized
execution.

- AC11: an anonymized export from a fixture profile with unique names.
- AC12: one recorded idle hour on macOS against the idle budget. A miss is
  filed as a card naming the operation that caused it.

Baseline, live IDs, revisions and evidence remain unassigned during
drafting.

RFC base: `0378ab977a7ffc74e0ac5ac2f70ee37a9cf00a66`.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01 Window.** - The window opens from its menu entry and shortcut, once per profile. - Switching through every tab and range, and letting 3 refreshes pass, never changes the window's frame or content size. An automated test enforces this. - A screen smaller than the size clamps it. - With the window hidden, no refresh runs. - A refresh issues no GitHub request, Git operation or process launch. - Escape closes it. | Pending. |
| **AC02 Charts.** The Charts behaviour holds, checked on a rendered window: lanes and maximum labels, the stacked foreground and background, gaps, hover hysteresis and edge flipping, the calendar size and colour rules, the theme roles, and an accessible description. The owner judges the rendered charts against AllTheThings' Energy chart in an attended review, and accepts or rejects them. | Pending. |
| **AC03 Summary budget.** With fixture responses from two clients in one reset window, the split matches the Summary definition. Each of these shows its stated state rather than a number, and no request is issued for it: - a restart inside the window; - a reading crossing a reset; - no reading in the window. | Pending. |
| **AC04 GitHub tab.** - Request counts by resource and category match the requests a fixture run issued. - `gh` failure kinds, quarantines and abandoned children appear as they occur. - GraphQL shows no split. | Pending. |
| **AC05 Storage.** - Every store in `state/v1`, the Git cache by repository, logs, Markdown images, Review Cycles and the Insights store report bytes on disk within 1% of a filesystem walk of the same paths. - Measuring runs only while Summary or Storage is visible. - Manage cached repositories opens the existing controls. | Pending. |
| **AC06 Review stats.** - Each Reviews count changes exactly once for its event, and never for a duplicate receipt or a retry. - An `indeterminate` submission counts only after it is recovered as published. - Totals survive cache prune, cache delete, Clear local diagnostics and a restart. - Profile purge removes them. | Pending. |
| **AC07 Reviews tab.** - The three ranges, calendar, "How reviews ended" and records match a seeded store. - Day buckets follow the local time zone, including across a daylight-saving change. - No streak is shown. | Pending. |
| **AC08 Energy sampling.** - A forced foreground/background switch splits the interval at the transition. - Child CPU from a `gh` launched in the background counts as background even if it finishes in the foreground. - Invalid deltas are dropped. - A platform without wakeups shows "Not reported on this platform". - Retention matches the 2 h / 48 h / 93 d windows. | Pending. |
| **AC09 Energy tab.** - The tiles show background figures only. - The fact table pairs foreground and background on every row. - Cost per idle hour is judged against the budget line. - A seeded over-budget range is marked. | Pending. |
| **AC10 Work tab.** - A seeded 250 ms main-thread block appears as one stall with that duration. - A diff fixture that hits each work limit is counted under it. - The five slowest files and the syntax and search figures match their host statistics. | Pending. |
| **AC11 Privacy and export.** - The anonymized export preview's Insights section contains no repository, account, pull request, path or body text. Checked against a fixture profile whose names are unique strings. - The identifying export adds nothing from Insights. | Pending. |
| **AC12 Idle cost.** - On macOS, with one signed-in profile and no window, one hour in the background stays within the idle budget for background CPU and wakeups. The run is recorded with the build and the measured figures. - A miss is filed as a card naming the operation that caused it. | Pending. |

## First milestone checkpoint

Window and Storage engineering is complete; the owner shell judgment remains
pending in a fresh short attended session. See the [durable continuation](../_checkpoints/insights-window-v1/continuation.md)
and [verification ledger](../../verification/RFC-BPR-0086.md). This is partial
evidence for AC01/AC05/AC11; the full criterion table remains pending until
each authored milestone and the closeout procedures are satisfied.
