---
schema: "implement-rfc.card.v1"
id: "BPR-0569"
title: "Normalize compatibility comments to semantic source anchors"
status: "Done"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0564","BPR-0565","BPR-0568"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e12e0fda7885b47b4da0379ccd1549e4ad2f585a","evidence":{"kind":"observation","detail":"Distinct canonical native/file subjects; review_comment_permalink + Qt conversation panel/map + public composition pass; see m3-frontier.md and card"}},{"criterion":"AC2","outcome":"pass","revision":"e12e0fda7885b47b4da0379ccd1549e4ad2f585a","evidence":{"kind":"observation","detail":"Immediate and fresh-state exact source placement; review_comment_permalink + Qt conversation panel/map + public composition pass; see m3-frontier.md and card"}},{"criterion":"AC3","outcome":"pass","revision":"e12e0fda7885b47b4da0379ccd1549e4ad2f585a","evidence":{"kind":"observation","detail":"Root-inherited replies with no reply parsing; review_comment_permalink + Qt conversation panel/map + public composition pass; see m3-frontier.md and card"}},{"criterion":"AC4","outcome":"pass","revision":"e12e0fda7885b47b4da0379ccd1549e4ad2f585a","evidence":{"kind":"observation","detail":"External edits/resolution/deletion and unavailable source; review_comment_permalink + Qt conversation panel/map + public composition pass; see m3-frontier.md and card"}},{"criterion":"AC5","outcome":"pass","revision":"e12e0fda7885b47b4da0379ccd1549e4ad2f585a","evidence":{"kind":"observation","detail":"Invalid grammar stays file-level; review_comment_permalink + Qt conversation panel/map + public composition pass; see m3-frontier.md and card"}},{"criterion":"AC6","outcome":"pass","revision":"e12e0fda7885b47b4da0379ccd1549e4ad2f585a","evidence":{"kind":"observation","detail":"No inferred native historical identity; review_comment_permalink + Qt conversation panel/map + public composition pass; see m3-frontier.md and card"}}]
---

# Normalize compatibility comments to semantic source anchors

Extend read normalization and the Qt thread map so a file-subject fallback carries honest remote subject plus a validated semantic anchor.

## Acceptance

- Native and file-subject roots normalize as distinct remote subjects under canonical GitHub IDs.
- BPR-created records place immediately; database-loss refresh can rehydrate from the visible canonical grammar.
- Replies inherit root placement and never parse their own links.
- External prose/link edits, resolution, deletion, foreign/ambiguous links, and unavailable commits follow server truth.
- Invalid fallback grammar degrades to file-level with no nearest-line or text-search guess.
- General historical native-comment revision identity remains RFC-BPR-0021 work.

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

## Implementation and evidence (2026-09-26, milestone C)

The selected-source application projection retains remote subject and raw body,
adds a separate exact semantic anchor and reviewer prose, and caches only the
current root's body under the complete sealed source. Fresh hosts reconstruct
from the same public body without local state. Replies inherit; resolution and
edits preserve canonical identity; removed, foreign, ambiguous, deleted or
unavailable anchors become file-level. Qt's map/model and selected comparison
consume those facts incrementally.

The tests-disabled graph exposed inherited M1 testing-only handoff/material
members and helper. Their production lifetime is corrected; no new mutation
issuer or production unsupported classifier was added. Self-review of the full
2deb889b-to-slice app delta verified accepted-source checks, source revocation,
raw-body authority, decoder confinement and unchanged mutation-branch bytes;
the composition pins were resealed for that reviewed delta.

Verification: `github_review_comment_permalink`,
`qt_github_conversation_panel`, `qt_composed_github_scale_100`,
`qt_github_thread_map_scale_100`, `architecture_boundaries` and
`application_review_round_trip_composition_contract` pass. The new Qt test first
caught an older-comparison filter excluding valid semantic anchors; the fix and
recheck pass. App/Qt targets build, tests-disabled `app_better_pull_review`
builds, and scoped placement/codec clang-tidy passes. Source grammar hostile
cases and exact native distinction remain covered. The final C pass will bind
these checks to its integrated source. Evidence logs and remaining obligations
are in the [milestone checkpoint](../_checkpoints/BPR-0575/m3-frontier.md).
