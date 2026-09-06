---
schema: "implement-rfc.card.v1"
id: "BPR-0636"
title: "Migrate legacy mutation JSON contracts to Glaze"
status: "Ready"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0610"]
---

# Migrate legacy mutation JSON contracts to Glaze

## Bounded acceptance and ownership

Own src/adapters/github/src/github_mutation_contract.cpp and focused tests in
tests/github_contract/github_mutation_contract_tests.cpp. Replace every Qt JSON
read/write in that production file with typed Glaze records. Parse comment
reconciliation once without a DOM round trip. Preserve exact outbound bytes and
fingerprints, required fields, failure categories, duplicate/full-input/Unicode
and structural limits. Reject raw fractional and overflowing identities and
accept exact uint64 identities without double normalization. Verify focused
contract, public synthetic provider, restart recovery and composition lanes.
Test-helper Qt JSON migration belongs to BPR-0640; retain a temporary Qt writer
oracle here to compare canonical outbound bytes across escapes and Unicode.

Migration anchor: BPR-0612. Migration base `4e6a6013f3589711f614fba7b9c46b8967313796`.
Preserve core/host boundaries and pinned dependency provenance. All execution
is local with synthetic fixtures; no Actions, push, paid CI or production
account/process operations. Full migration acceptance remains on BPR-0612.
