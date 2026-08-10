---
id: BPR-0238
title: Complete private ledger persistence for notes heads and defects
status: Done
rfc: RFC-BPR-0007
depends_on: []
---

# Complete private ledger persistence for notes heads and defects

The current review-ledger schema persists scope plus file and conversation
receipts. RFC-BPR-0007 also requires local notes, exclusions, last-reviewed
heads, and local defect metadata; classification persistence is also required
by RFC-BPR-0009. Those durable authorities do not exist yet.

## Required final state

- Add versioned, identity-scoped persistence for local notes, exclusions,
  last-reviewed heads, and defect severity/category/lifecycle metadata.
- Migrate forward from every prior schema without dropping existing receipts.
- Keep presentation coordinates and shared GitHub state out of the schema.
- Prove restart, crash recovery, identity isolation, deletion, and changed-head
  behavior for every new record type.
- Supply bounded observation/store ports for BPR-0228 and BPR-0230; neither Qt
  nor the core opens SQLite directly.
