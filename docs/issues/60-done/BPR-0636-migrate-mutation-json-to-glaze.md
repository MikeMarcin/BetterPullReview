---
schema: "implement-rfc.card.v1"
id: "BPR-0636"
title: "Migrate legacy mutation JSON contracts to Glaze"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0610"]
acceptance: [{"criterion":"typed-mutation-contracts","outcome":"pass","revision":"25b72eaafbf6132937da00be67d59475e18a9c8d","evidence":{"kind":"observation","detail":"No Qt JSON remains in the production mutation contract. Exact typed uint64 responses and one-pass comment recovery replace DOM and double normalization; raw fractional regression fails old code and passes migrated code."}},{"criterion":"canonical-byte-compatibility","outcome":"pass","revision":"25b72eaafbf6132937da00be67d59475e18a9c8d","evidence":{"kind":"observation","detail":"Independent Qt oracle verifies every control character, literal backslash-u sequences and Unicode produce identical line-comment bytes after preserving lowercase Unicode escapes. Existing fingerprint and durable recovery formats unchanged."}},{"criterion":"public-verification","outcome":"pass","revision":"25b72eaafbf6132937da00be67d59475e18a9c8d","evidence":{"kind":"observation","detail":"Builds pass. Mutation contract 0.40s, synthetic public provider 0.33s, restart recovery 0.18s and application round-trip 53.12s pass. Broader composition has five unchanged passing methods; its stale first-link-only inventory method is corrected and passes the isolated registered-command rerun in 0.665s."}},{"criterion":"scope-and-cleanup","outcome":"pass","revision":"25b72eaafbf6132937da00be67d59475e18a9c8d","evidence":{"kind":"observation","detail":"Architecture docs, formatting, documentation and diff checks pass. No production accounts, live GitHub, Actions, push or paid CI. Task logs consumed and removed. Full migration and remaining RFC work stay open; next BPR-0637."}}]
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

## Acceptance evidence

Implemented in `25b72eaafbf6132937da00be67d59475e18a9c8d`. Plan `806152ee`; claim `9afad6f4`.
The mutation contract has no remaining Qt JSON includes, values, readers or
writers. Private typed Glaze records cover target/viewer/comment responses and
comment requests; comment-list recovery parses once and calls the same typed
identity validator as a direct comment response. Existing whole-review records,
preflight bounds, status/header policy and inward application types are retained.

Raw tests added before migration fail against the old code on a fractional
viewer identity. The migrated path rejects raw fractional IDs/lines, wrong
scalar types, zero/negative IDs, negative exponents and overflow, missing/null
required data, duplicate keys, trailing documents, malformed UTF-8 and unpaired
surrogates, including skipped extension data. Exact nonnegative integer
exponents remain accepted by Glaze without floating-point conversion. Exact
IDs above double precision through uint64 maximum survive viewer/comment and
recovery paths; adjacent authors cannot match. Ordinary extension decimals and
valid surrogate pairs remain accepted.

The temporary independent Qt writer oracle exposed uppercase Glaze Unicode
escapes versus lowercase Qt compact escapes. The line-comment writer preserves
the previous lowercase spelling while skipping escaped backslashes; all 32
control characters, literal backslash-u text, quotes, slashes and Unicode now
compare byte-for-byte. Canonical member order is retained. No request fingerprint
or durable recovery schema changes. BPR-0640 will replace the Qt oracle with
literal independent fixtures as part of the remaining helper migration.

Verification on the local Mac, synthetic and network-isolated:

- Contract/provider/application targets build successfully.
- `github_mutation_contract` passes (0.40s), including the new raw-input and
  canonical-byte cases and existing whole-review/target/line-boundary cases.
- `live_review_mutation_provider` passes (0.33s) and
  `live_review_mutation_restart_recovery` passes (0.18s), exercising the public
  provider with synthetic transport and durable restart reconciliation.
- `application_review_round_trip_composition_contract` passes (53.12s),
  driving the real application draft/Publish/refresh path with its fixture
  provider and verifying production selection/isolation.
- The broader `application_composition_contract` ran all six methods (147.85s).
  Five passed; one inventory method failed four dependency assertions because
  its regex read only the newly preceding private fixture Glaze link statement.
  The required production links were present in the following statement. The
  contained fix in `tests/end_to_end/composition_contract_tests.py` collects all
  declarations for the exact target. The affected method passes its isolated
  registered-command rerun (0.665s). Unchanged passing methods were not repeated;
  this is not reported as a fresh all-green aggregate CTest run.
- Documentation gate and `git diff --check` pass; touched C++ is formatted.

Architecture semantics are updated in `docs/architecture/review-round-trip.md`.
No core/host policy, credential, UI action or network-effect ownership changed.
BPR-0636 is complete; BPR-0637 through BPR-0640 and full migration closeout
BPR-0612 remain open. Further performance work remains behind migration.
No production session, Actions, push, paid CI or live GitHub operation occurred.
Task build/test/recheck logs are consumed here and removed. The composition
wrappers cleaned their private temporary roots; reusable build trees remain.
