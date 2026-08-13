---
id: BPR-0306
title: Verify anonymous preview security and public behavior
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0305]
---

# Verify anonymous preview security and public behavior

## Required final state

- Core, provider/read-contract, native HTTPS, installed Git, SQLite migration/
  restart, host, Qt, public composition, tests-disabled Release, sanitizer,
  runtime, dependency, and boundary matrices pass from clean state.
- Positive canaries prove zero Authorization header, vault lookup, credential
  enumeration/enrollment, mutation, GraphQL/viewer request, GHES request, and
  private-content disclosure.
- Target-native macOS evidence covers the production native-HTTPS path and a
  real rendered public-preview candidate course while production selection is
  still unavailable; offscreen evidence is layout-only.
- This card extends `scripts/verify_rfc0017_test_inventory.py` to the complete
  RFC selector set and creates the exact isolated
  `scripts/verify_rfc0017_sanitizer_course.py` and
  `scripts/verify_rfc0017_platform_course.py` interfaces seeded by the RFC
  verification ledger. Each script fails on missing, extra, or duplicate
  required selectors before execution.
- BPR-0314/BPR-0315 own named Linux/Windows unavailable-path proofs, and
  BPR-0316 owns macOS enablement after this card is terminal.
