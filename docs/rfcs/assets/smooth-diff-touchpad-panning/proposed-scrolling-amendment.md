# Proposed amendment: visible fractional scrolling and horizontal intent

This accompanies [RFC-DRAFT-0012](RFC-DRAFT-0012-smooth-diff-touchpad-panning.md).
It proposes a visible scrolling repair and a new horizontal touchpad input
rule. Live RFCs, plans, cards, statuses and historical verdicts remain unchanged
during authoring.

## Sources inspected

The SHA-256 identities below bind the adopted sources inspected on 2026-10-12.
They are authoring provenance, not an implementation baseline or acceptance
evidence. Reconcile any concurrent drift before adoption.

| Source | SHA-256 |
|---|---|
| [RFC-BPR-0004 revision 2](../../RFC-BPR-0004-diff-computation-line-mapping-grouping.md) | `2850de0ada325aa26107c9fd76e136d226df8ccc1c5a372c028751c6fd5d04e1` |
| [RFC-BPR-0005](../../RFC-BPR-0005-native-workbench-virtualized-review-surface.md) | `964540dfb8e6b8c1ace68db8ce7577fda2a94b02b6e388c82f8bd23aa1d012b2` |
| [Diff scrolling UX contract](../../../ux/diff-link-map-and-alignment-space-scrolling.md) | `e4036eab803971d9b97d64ed3ade7cbb93aaae5af0905a81781dbd65a67f1eb1` |
| [BPR-0208](../../../issues/60-done/BPR-0208-link-map-does-not-align-with-its-blocks.md) | `9b61722ebe67b1476c427cf2c1322e30407350f61a40251a44892aff5b336c8a` |
| [BPR-1291](../../../issues/60-done/BPR-1291-the-shared-diff-scroll-bar-moves-while-the-panes-stand-still-and-there-is-no-scroll-past-end.md) | `6eb48a5a5fcbb76b5f481a44430280fb6b126e41ead38d702aeb77a225978ca7` |
| [BPR-1336](../../../issues/60-done/BPR-1336-zoom-the-diff-panes-code-text.md) | `1603e7737376aebc65199a899caad57a7b1aa88a1e69745037f874bf8ff23f4e` |

## Before and after

RFC-BPR-0004 §14.1–14.4 already specifies retained subpixel comparison
position, pixel input, a single vertical authority and independent horizontal
panning. Clarify that fractional state must reach the rendered source panes
and remain visible when a gesture stops. Controller accumulation alone does
not discharge that obligation. Add the draft's horizontal-intent rule to
§14.3: small incidental vertical movement during a clearly horizontal
touchpad pan is discarded, while slow vertical motion, deliberate diagonal
motion and reorientation remain usable.

The owner's follow-up keeps Up/Down line navigation discrete and requires
keyboard-induced viewport movement to finish on a line boundary. Extend
§14.3's keyboard rules accordingly: align the receiving source presentation
through the shared controller, leave an in-view cursor-only movement alone,
and preserve the other pane's measured-axis projection. Page-key destinations
also align; touchpad motion can then continue from the resulting position.

RFC-BPR-0005's Required interactions and UI architecture continue to compose
the same scroll authority. Apply the visible motion behavior to both shipped
source presentations; the current `QPlainTextEdit` implementation must provide
consistent fractional geometry for text, selection and input, or use a
suitable native-compatible presentation technique. This amendment does not
mandate replacing the renderer or introduce a second alignment model.

Reconcile the scrolling UX contract's Input details and the system
architecture's Qt-local scrolling description during implementation. Keep
their existing ownership and single-comparison authority. No new settings,
schema migration or core gesture policy is proposed.

The affected historical acceptance is RFC-BPR-0004's Alignment-space scrolling
and Link-map presentation groups, and RFC-BPR-0005's shared-scrollbar,
connector alignment and virtualized-renderer criteria. Preserve every
obligation; add the draft's distinguishing rendered fractional-offset,
horizontal-drift and keyboard-realignment checks. Existing whole-line evidence
cannot establish those new observations.

## Preserved decisions and reconciliation

BPR-0208's fix requires endpoints and source blocks to share one measured
origin; retain its exact endpoint/block agreement. Extend that same basis to
the fractional offset. BPR-1291's reachable last-line origins and live range
remain required. BPR-1336's zoom dispatch and reading-anchor behavior remain
required. These cards are terminal and must not be reopened or have their
historical evidence rewritten.

No adopted milestone plan is changed. The new RFC depends on the already
adopted diff/workbench foundation. Before source edits, the authorized
implement-rfc session should reconcile active writers on the scroll
controller, source pane, measured axis and composed tests, register the new
scope, and bind its own baseline. Existing candidates certify only their
unchanged behavior; a new frozen candidate supplies the touchpad-feel verdict.

The owner chose minor vertical drift suppression and subsequently specified
discrete keyboard navigation with viewport realignment. The filtering
constants and fractional rendering technique remain engineering work. First
prove behavior through actual pane geometry,
selection/hit testing and offscreen composition, then prepare the single
physical-touchpad judgment through the repository's short attended-review
workflow. No new permission boundary is created by this proposal.
