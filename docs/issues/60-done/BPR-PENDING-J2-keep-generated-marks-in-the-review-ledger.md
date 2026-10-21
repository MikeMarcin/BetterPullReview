---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-J2"
title: "Keep generated-file marks in the review ledger"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-PENDING-J1"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"milestone":"RFC-BPR-0089 M2","related_cards":["BPR-PENDING-J1","BPR-PENDING-J3"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8e296033dd8586fa127a1994f3f03e1ca8fe2697","evidence":{"kind":"command","command":"ctest --preset dev -R '^(sqlite_review_ledger_storage|sqlite_private_review_storage|sqlite_private_store_open|application_review_ledger_host|application_agent_invocation_host)$'","detail":"Schema 11 adds generated_mark (STRICT, scope foreign key, a row that says nothing refused by the table); a schema 10 ledger migrates with an empty table; marks round-trip by scope in identity order, replace, erase idempotently and survive restart; a mark that says nothing or an invalid file is refused. The ledger host persists one mark through the private write lane, keeps it in place in the private observation, erases a mark that says nothing, and a refused or invalid write changes nothing; the observing store signals each accepted mark write once. macOS."}}]
---

# BPR-PENDING-J2: Keep generated-file marks in the review ledger

RFC-BPR-0089 M2: "The reviewer's per-review overrides, kept in the ledger".

## Change

- Ledger schema 11: `generated_mark(scope_id, file_identity, mark,
  review_anyway)`, one row per file the reviewer said something about. The
  [review ledger](../../architecture/review-ledger.md) records it.
- `review_ledger_store_port::store_generated_mark` and
  `erase_generated_mark`; `private_ledger_observation::GeneratedMarks`, loaded
  with the review's other private records.
- `review_ledger_application_host::persist_generated_mark` writes or erases
  and adopts the mark once durable.

## Acceptance

- **AC1.** Marks persist per review, migrate from schema 10, and change no
  receipt; a generated mark is never the ledger's Excluded.
