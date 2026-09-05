---
id: BPR-0569
title: Normalize compatibility comments to semantic source anchors
status: Backlog
rfc: RFC-BPR-0022
depends_on: [BPR-0564, BPR-0565, BPR-0568]
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

Imported 2026-09-04; implementation is deferred by the user. This card remains
Backlog. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
