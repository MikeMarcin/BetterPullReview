---
id: BPR-0589
title: Verify rendered private-thread and composition UX across scale, theme, keyboard, and accessibility modes
status: Backlog
rfc: RFC-BPR-0023
milestone: Post-Pack v2
depends_on: [BPR-0585, BPR-0586, BPR-0587]
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

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04; implementation is deferred by the user. This card remains
Backlog. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
