---
schema: "implement-rfc.card.v1"
id: "BPR-1380"
title: "Home button tooltip shows its shortcut"
status: "Ready"
rfcs: ["RFC-BPR-0005"]
depends_on: []
priority: "P2"
---

# Home button tooltip shows its shortcut

The owner asked on 2026-10-10 to work directly and show the Home button's
existing key combination in its tooltip. Use the action's native shortcut
text, as the adjacent toolbar actions do.

## Acceptance

- The Home button's displayed tooltip includes its action's current shortcut
  in the platform's native notation.
- Home retains its current shortcut and navigation behavior; the status tip
  and accessible description use the updated tooltip.

## Scope and verification

One Qt presentation wording change. Check the actual toolbar button and its
offscreen rendered tooltip, run the focused workbench checks, and format the
touched source with clang-format 22. No human judgment criterion is needed.
The required frontend-design skill was loaded; the existing native tooltip
appearance and action vocabulary govern this change.
