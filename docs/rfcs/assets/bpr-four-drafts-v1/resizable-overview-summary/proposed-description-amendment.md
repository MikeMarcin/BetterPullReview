# Proposed amendment: resizable Overview description

This note accompanies [the draft](RFC-DRAFT-0002-resizable-overview-summary.md).
The owner explicitly requested resizing on 2026-10-07. It supersedes the
earlier argument against a splitter for normal Overview, while preserving the
full-area reader. Live sources remain unchanged during authoring.

## Sources read

SHA-256 values identify the exact source content; reconcile drift before
adoption. Runtime files establish current behavior rather than acceptance of
the proposed change.

| Source | SHA-256 |
|---|---|
| [RFC-BPR-0037](../../../RFC-BPR-0037-first-pr-review-workflow.md) | `b28f7a8559c10c03eb4a78db810c380a8f4215d73080015dbe26d68243a0b58f` |
| [Description layout study](../../../plans/rfc-bpr-0037-first-pr-workflow/overview-description-layout-study.md) | `b6264cad289e04c243fdb478ac3d5ef56f230620f8adbab0e05324475c168660` |
| [First-PR workflow plan](../../../plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json) | `c3a2909df913b687828a55b010dccdc37f43f236991f7c24da15ad7dfecf2c84` |
| [BPR-0928](../../../../issues/60-done/BPR-0928-expanded-overview-description-reader.md) | `5a666275fffb5b0e3212cd7ea7dc2e5036db3723bcf0b4231cde6d41f1addf88` |
| [Overview page](../../../../../src/adapters/qt/src/pull_request_overview_page.cpp) | `e8d03b36f2feae5439ddcf435db725c70d0639a77a156c78d94a0e64b76547da` |
| [Overview tests](../../../../../tests/adapters_qt/pull_request_overview_page_tests.cpp) | `a1b45755440e65273d801f793a917e21b7d2ed0d1a122d4dd39b781bbd38bea0` |

## Targeted reconciliation

RFC-BPR-0037's description work belongs to its AC-01 and the
`overview-conversation-decisions` milestone, with BPR-0928 as the selected
runtime follow-up. The adopted design requires a compact preview without an
active scrolling well and selects expanded reading instead of user resizing.
Its implementation now uses a four-line budget, and its tests codify that
behavior.

The new draft replaces that normal-view compact/non-scrolling contract with
a taller, resizable description and sibling description/Files scrolling. It
adds a window-owned persistent height preference. The existing expanded
reader, fixed Return to Files action, document identity, literal copy,
authority clearing, selection/reading retention, native appearance and
file-navigation guarantees remain obligations. Markdown rendering and
provider admission remain owned by their existing contracts.

At adoption, record this delta in the live RFC rather than rewriting the
historical study or BPR-0928's exact accepted result. Register new work and
acceptance for the changed normal Overview; completed cards remain terminal.
Do not replay adoption over the existing plan or alter its original milestone
scope/exits to imply that resizing has already passed.

RFC-BPR-0037 remains Proposed with its existing dependencies and unrelated
criteria. This proposal neither closes it nor invalidates unrelated prepared
acceptance candidates. Reconcile any active work touching the Overview or
window-state schema before changes, and obtain new rendered/attended evidence
for the modified surface. A passing draft/documentation check supplies no
runtime or owner acceptance.
