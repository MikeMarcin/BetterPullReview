---
schema: "implement-rfc.card.v1"
id: "BPR-2055"
title: "Restore agent finding cards and their states after a restart"
status: "Done"
rfcs: []
depends_on: ["BPR-2052"]
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2055--BPR-2055--ce69aea099f2.json","source_commit":"95393374f"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"95393374f","evidence":{"kind":"command","detail":"qt_agent_finding_cards_restart: four findings, one drafted, fresh window/host/store over the same file shows 1 Drafted and 3 open via saved thread and native history; restored card drafts its own finding; draft and refusal survive another restart."}},{"criterion":"AC2","outcome":"pass","revision":"95393374f","evidence":{"kind":"command","detail":"core_agent_finding_card_memory, sqlite_agent_setup_storage, qt_agent_finding_cards_retained, architecture/core boundary tests, core-only preset, format and tidy pass."}}]
---

# Restore agent finding cards and their states after a restart

Follow-up to BPR-2052. Card states (drafted, note, dismissed, refused) are kept
in core memory keyed by the SHA-256 of the answer's findings block plus
ordinal, persisted by the application host in the additive
`agent_finding_cards` table of `agent-setup.sqlite3` (newest 4,096 rows), and
cards are rebuilt from the stored answer's findings block on restore.

## Acceptance

- AC1. After a restart a drafted card shows Drafted once and the others are
  open; a restored open card still drafts its own finding.
- AC2. Core stays deterministic and effect-free; boundaries pass.

Not verified: a live restart of the real application bundle.
