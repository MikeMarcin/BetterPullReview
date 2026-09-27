---
schema: "implement-rfc.card.v1"
id: "BPR-0564"
title: "Implement the canonical immutable code-permalink representation"
status: "In Progress"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0562","BPR-0563"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
---

# Implement the canonical immutable code-permalink representation

Implement bounded compile and parse contracts for the same-host, same-repository, full-commit line permalink and the visible fallback body grammar.

## Acceptance

- Compiler emits canonical web-origin URLs, one-based line fragments, and `?plain=1` only for Markdown source.
- Path segments are encoded without allowing authority changes, userinfo, alternate ports, encoded separators, dot segments, arbitrary query, or fragment injection.
- Parser accepts exactly one leading canonical link, one blank line, and non-empty byte-preserved prose.
- Authority, commit, path, blob, and line are validated before an anchor is produced.
- Invalid, foreign, ambiguous, oversized, or malformed input returns no anchor and performs no mutation.

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
