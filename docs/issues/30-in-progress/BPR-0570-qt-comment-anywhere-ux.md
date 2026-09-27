---
schema: "implement-rfc.card.v1"
id: "BPR-0570"
title: "Present one representation-blind comment-anywhere UX"
status: "In Progress"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0562","BPR-0563","BPR-0569"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
---

# Present one representation-blind comment-anywhere UX

Expose the ordinary review-comment gesture on every eligible current-side source line in a changed text file and keep provider representation out of the normal workflow.

## Acceptance

- Hunk and off-hunk lines use the same affordance, editor, Publish action, pending state, and resulting selected conversation.
- Qt receives command projections and typed outcomes only; it owns no capability decision, permalink builder, or provider call.
- Representation appears only in an expandable support diagnostic.
- Line-only incompatibility is explained with zero POST.
- Keyboard, focus, accessibility, high-DPI, light/dark/high-contrast, and bounded incremental-update evidence passes through the shipped workbench.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.

## Import boundary

Imported 2026-09-04 under an import-only direction. The 2026-09-26
implementation mandate r1 activates RFC-BPR-0022; live dependencies and lanes
remain authoritative. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

## Implementation and evidence — milestone C, mandate r2

The existing Comment/editor/Publish/pending/receipt controls remain the only
review-comment workflow. Qt consumes typed source anchors and outcomes. The
selected post shows reviewer prose; a collapsed **Support details** disclosure
alone names the file subject. Its text is keyboard selectable. The line-only
body refusal keeps the draft and explains the needed anchor without offering a
representation choice. Host/session tests prove zero POST for incompatible
bodies before a write; Qt's refusal test proves no dispatched operation.

The shipped workbench test revealed a missing reverse-selection condition:
choosing an exactly placed compatibility thread on a retained comparison
revealed code but cleared the post because that endpoint was not native-current.
An indexed set of exact semantic anchors now admits that source selection;
native-current eligibility is unchanged. Unchanged refresh preserves selected
post, map identity, focus and incremental models.

Verification on this product slice:

- `cmake --build --preset dev --target test_better_pull_review_qt_github_tests test_better_pull_review_qt_github_conversation_panel_tests test_better_pull_review_qt_review_draft_editor_tests -j 6` passed.
- `ctest --preset dev --output-on-failure -R '^qt_composed_github_scale_(100|125|200)$'`: three scales, each light/dark/high-contrast, rendered through `workbench_window`, exact source reveal, keyboard disclosure/Tab, complete disclosure reachable by drawer scrolling, selected identity and no model reset.
- `ctest --preset dev --output-on-failure -R '^(qt_review_draft_editor|qt_github_conversation_panel|github_review_comment_permalink)$'`: 3/3 passed, including typed body refusal and session zero-POST incompatibility.
- Nine synthetic full-window rasters are under ignored `build/dev/tests/adapters_qt/composed-github-*-semantic-*.png`. Light100, dark125 and high-contrast200 were visually inspected; the support text needed drawer scrolling, which the final rendered test now exercises and bounds.
- Initial target-name typos failed before compilation; initial semantic selection and disclosure-extent tests exposed the issues recorded above. Final logs: `build/rfc0022/m3-ux-final-tests.log`, `m3-editor-tests.log`; no earlier failure is counted as acceptance.

BPR-0571 extends the already passing existing ordinary gesture probe through the
real session/SQLite wire variants and server-read return. C's final verification
will bind all of these paths to the same candidate. No live or native-platform
acceptance is inferred from offscreen rendered evidence.
