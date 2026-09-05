---
provisional_id: PACK-BPR-0022-14
title: Verify rendered private-thread and composition UX across scale, theme, keyboard, and accessibility modes
status: Backlog
rfc: RFC-BPR-0022
milestone: Post-Pack v2
live_id: allocate-on-import
depends_on: [PACK-BPR-0022-10, PACK-BPR-0022-11, PACK-BPR-0022-12]
---

# Verify rendered private-thread and composition UX across scale, theme, keyboard, and accessibility modes

## Objective

Prove the workflow is readable and operable in the real workbench rather than merely structurally present.

## Required final state

- Deterministic fixtures render reviewer, agent-reported, requested-only, unknown, conflicting, legacy, incomplete, stale, multi-target, composed, draft, and published states.
- Evidence covers 100%, 125%, and 200% scale, supported themes, constrained drawer widths, long bounded text, and platform-native font metrics.
- No important local/shared/provenance/publication distinction relies only on color.
- Keyboard traversal, selection, history, details, composition, target/split, accept, cancel, and publish are complete and stable.
- Accessible names/descriptions/states distinguish private local content from shared GitHub content and qualify provenance basis.
- No clipping, hidden action, focus loss, selection loss, or full-model reset appears under a bounded update.

## Non-goals

- Static mockups as sole proof
- Pixel-identical rendering across platforms
- Unbounded transcript stress in the UI process

## Completion evidence

- Rendered capture set
- Qt automation output
- Accessibility tree/focus evidence
- Model statistics
- Reviewed UX findings and fixes

## Documentation impact

Update the RFC-BPR-0022 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.
