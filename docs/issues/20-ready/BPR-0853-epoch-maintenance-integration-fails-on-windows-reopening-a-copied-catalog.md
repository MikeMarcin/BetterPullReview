---
schema: "implement-rfc.card.v1"
id: "BPR-0853"
title: "epoch_maintenance_integration fails on Windows reopening a copied catalog"
status: "Ready"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-0846 new defect 1","found_at":"e6b7193e19862ba3f0d27c3096ba30ddc0ea60cf","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0853: epoch_maintenance_integration fails on Windows reopening a copied catalog

## Defect

`epoch_maintenance_integration` fails on `bpr-win` at `e6b7193e` with
`construct real SQLite catalog`
(`docs/verification/dsp4-windows-confirmation.md`).

The failure is in `verify_real_migrated_retained_refs`
(`tests/git_contract/epoch_maintenance_integration_tests.cpp` ~996). Its
"migration" fixture opens its catalog. The "migrated" fixture then reopens a
copy of that catalog made with `std::filesystem::copy`, and the reopen
returns `epoch_catalog_failure::invalid_permissions` (2).

`aa3c6ef1` is not the cause: `catalog_database.cpp` at `aa3c6ef1^` fails the
same way. The migration course (`72d40b4f`, `776bc0d4`, `90d4ba28`, all
2026-09-12) is newer than the station's last recorded Windows pass
(2026-08-27).

A likely cause, not proven: the copied `state/v1` objects inherit a DACL
wider than the private one, and the adapter refuses them instead of repairing
them ([[BPR-0806]]). macOS and Linux pass the course.

## Scope

- Name the cause on Windows: the course's copy, or the product's refusal of
  a copied catalog.
- If the course is at fault, make it copy with the private DACL the product
  requires. If the product is at fault, fix the product without weakening
  the owner-private refusal. That is storage authority, so give it a review.

## Acceptance

- The course passes on Windows, 3 runs in a row, and still passes on macOS.
  The cause is named here.
