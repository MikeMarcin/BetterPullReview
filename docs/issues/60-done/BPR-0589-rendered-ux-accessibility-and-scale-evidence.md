---
schema: "implement-rfc.card.v1"
id: "BPR-0589"
title: "Verify rendered private-thread and composition UX across scale, theme, keyboard, and accessibility modes"
status: "Done"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0585","BPR-0586","BPR-0587"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
acceptance: [{"criterion":"Deterministic rendered states","outcome":"pass","revision":"80d4eb4da5459a47f9b467c48cebafe74cea1f1d","evidence":{"kind":"artifact","detail":"Shipped three-process course captured reviewer, agent, composition and publication; Qt projection captured legacy, incomplete, stale, unplaceable and conflicting provenance. See docs/verification/RFC-BPR-0023.m9-rendered-evidence.md and 13 checksummed PNGs."}},{"criterion":"Scale, themes, constrained width and native metrics","outcome":"pass","revision":"80d4eb4da5459a47f9b467c48cebafe74cea1f1d","evidence":{"kind":"command","detail":"Public path passed at 100/125/200 across system, light, dark and both high-contrast appearances; Cocoa passed at 125 dark and 200 high-contrast-dark. Qt projection ran at 420 logical pixels. One intermittent Cocoa fixture modality stall was recorded; fresh rerun passed."}},{"criterion":"Color-independent distinction and accessibility","outcome":"pass","revision":"80d4eb4da5459a47f9b467c48cebafe74cea1f1d","evidence":{"kind":"observation","detail":"Rendered private/shared/local and provenance-basis text inspected; Qt accessible names distinguish private body, provenance and local Accept, and all semantic action buttons have names and focus policies."}},{"criterion":"Keyboard actions and selection","outcome":"pass","revision":"80d4eb4da5459a47f9b467c48cebafe74cea1f1d","evidence":{"kind":"command","detail":"Qt test passed Tab transitions in History, Sources and Write and distinct Split/Accept intents. Offscreen 200% shipped-app course passed Tab-to-Accept plus scroll visibility; native Cocoa background capture passed scroll visibility but could not own key focus."}},{"criterion":"No clipping, hidden action, focus or selection loss, or full-model reset","outcome":"pass","revision":"80d4eb4da5459a47f9b467c48cebafe74cea1f1d","evidence":{"kind":"observation","detail":"Dark palette and Cocoa selector width defects fixed; final native 200% images show target text and scrolled Accept/Cancel. Qt bounded actor/state updates retained selected index and zero model resets; offscreen Tab retained focus on Accept."}},{"criterion":"Documentation impact","outcome":"pass","revision":"80d4eb4da5459a47f9b467c48cebafe74cea1f1d","evidence":{"kind":"artifact","detail":"RFC ledger and m9-rendered-evidence.md list changed source/test paths, operator behavior, limits, commands, synthetic captures and external gate separation. Documentation gate passed DOCUMENTATION_OK."}}]
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

Imported 2026-09-04 with implementation deferred at that time. Owner mandate
r1 on 2026-09-26 now authorizes implementation, plan revision and milestone
integration. The [live audit](../../verification/RFC-BPR-0023-live-audit.md)
supersedes stale baseline assumptions; required acceptance is unchanged. Follow
the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
