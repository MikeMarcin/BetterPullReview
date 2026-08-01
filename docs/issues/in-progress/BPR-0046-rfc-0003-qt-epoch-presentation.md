---
id: BPR-0046
title: Implement exact epoch comparison controls in Qt
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0041, BPR-0045, BPR-0056]
---

# Implement exact epoch comparison controls in Qt

Add a reusable provider/Git/filesystem-free comparison-controls presentation
with stable epoch identities, two independent selectors, named-comparison
actions, exact non-elided/copyable OIDs, cause and availability facts, and
loading, retained-refresh, empty, canceled, failed, pruned, cache-deleted, and
never-observed states.

Qt dispatches semantic epoch IDs or alias intents and applies core/host
projections; it never compares OIDs, classifies causes, resolves aliases,
chooses fallback epochs, reads a cache, or launches Git. Update rows
incrementally with exact insert/remove/move/data-change signals and preserve
stable selection, popup/focus state where possible, keyboard operation,
visible native focus, accessible relationships, and one bounded deduplicated
announcement.

Embed the controls in the interim loaded-content surface for public reachability
without claiming RFC-BPR-0005's final workbench or RFC-BPR-0006's selectable
themes/fonts/syntax.

## Implementation audit frontier

Begin from the provider/Git/filesystem-free epoch projection introduced by
BPR-0045 and the bounded history-page deltas introduced by BPR-0056. Audit the
live GitHub window's retained-content composition, model-update conventions,
focus/accessibility behavior, appearance invalidations, and deterministic
render harness before selecting widget boundaries. Reuse the existing public
window only as the interim reachability surface; do not make it an epoch
authority or pre-implement the final RFC-BPR-0005 workbench.
