---
schema: "implement-rfc.card.v1"
id: "BPR-0614"
title: "Execute whole reviews with durable indeterminate recovery"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0610","BPR-0613"]
---

# Execute whole reviews with durable indeterminate recovery

BPR-0610 defines the Glaze wire contract and BPR-0613 defines anchor-free core
and application intent. Connect those contracts to a production provider and
durable recovery in one slice. A POST-only implementation would leave a lost
response unsafe to retry. Async host, Qt controls and application selection
follow immediately in a separate slice before BPR-0144's live checks.

## Ownership and boundaries

Application: new whole-review submission/recovery ports and their validation,
using the BPR-0613 request and distinct review receipt. GitHub: sibling session
and live provider, canonical review-list request/parser, provider admission and
relevant contract registration. SQLite: sibling submission recovery store and
transactional schema migration in the existing review recovery database.
Own corresponding focused tests and CMake registrations. The integrator owns
board moves, documentation and commits. Preserve source-line provider behavior.
Do not touch Qt, the app composition, credentials, live services or BPR-0612's
backlogged Qt JSON migration. New JSON uses Glaze with private adapter DTOs.

## Acceptance

- Add narrow whole-review submit/reconcile and durable recovery ports carrying
  exact draft/attempt/request context. A successful receipt remains a review ID,
  never a comment ID. Return a typed distinction between definitive non-send
  and indeterminate publication so later hosts cannot turn a timeout into retry
  permission. Do not lose request correlation because a receipt lacks Body or
  Attempt fields.
- Keep target/account configuration immutable. Reuse the existing opaque lease,
  transport admission and cancellation boundary. Validate the live binding,
  canonical request and frozen request/authority fingerprints before effects.
  Freshly observe the viewer, then the exact PR/head immediately before POST.
  Target or actor drift, stale/revoked lease and cancellation refuse the write.
- Compile canonical GET `/repos/{owner}/{repo}/pulls/{number}/reviews?per_page=100`
  and parse a bounded complete baseline with typed Glaze JSON. Retain distinct
  positive exact uint64 review IDs; inspect ordinary returned review states
  without mistaking pending/dismissed or another actor's review for success.
  Match submitted candidates by exact actor, target/head, disposition and body.
  Preserve submitted timestamp validation, strict numeric/Unicode handling and
  response byte/depth limits. If a Link header or capacity limit means the list
  is incomplete, fail closed without POST or successful reconciliation. This
  initial pagination refusal is an explicit supported-capacity limitation.
- Atomically admit the exact request and sorted unique baseline review IDs
  before transmission. Durably witness possibly-transmitted at the transport's
  existing POST boundary. Preserve unresolved state across process exit,
  cancellation, malformed responses, transport errors and local write failures.
  A previously admitted attempt must never issue another POST.
- After possible transmission, observe server reviews regardless of POST
  response success. Subtract the persisted baseline and resolve only one exact
  new matching review. Zero or multiple matches, incomplete reads and mismatches
  remain unresolved. Prepared/baseline-incomplete records cannot adopt an
  unrelated later review. Stored success replays its original receipt without
  another write; changed content/authority under an existing key conflicts.
- Use sibling attempt/baseline tables in schema v3 of
  `review-mutation-recovery.sqlite3`, reusing the existing authority scope.
  Preserve existing source-line rows through transactional v1/v2 migration;
  fail closed for schema drift/future versions. Keep admission, witness and
  resolution transactional and preserve capacity, corruption and scope checks.
  Empty Approve bodies are valid; no fabricated anchor or nonempty-comment-body
  constraint leaks into submission storage.
- Exercise the production session/provider boundary through isolated fixtures,
  including all three dispositions, target/account drift, durable-witness
  failure, cancellation before/after send, lost response, ambiguous/absent
  candidates, old identical reviews, restart recovery and duplicate submit.
  Prove migrated source-line behavior still passes its existing storage/provider
  lane. No live credentials or GitHub mutation are needed.

## Verification and continuation

Build the affected dev targets. Run `github_mutation_contract`, the existing
`live_review_mutation_provider` and `sqlite_review_mutation_recovery_storage`,
new whole-review provider/storage selectors, and a two-process restart-recovery
fixture. Include the native HTTPS transport witness/cancellation selectors if
that boundary changes, plus architecture boundaries. Use the production session
as the direct selected path, scoped formatting, documentation and diff checks;
no full matrix or separate retained build tree.

Next implement the async host, per-request authority and PR-level summary /
Comment / Approve / Request Changes controls, with ordinary application
composition and public-path fixtures. BPR-0144 stays open until that workflow
and its designated real-account observations are complete.

## Implementation and verification — 2026-09-05

The candidate based on claim `7e3e3420` implements the sibling provider/session,
full request correlation and fingerprints, Glaze baseline parsing, and shared
SQLite schema-v3 migration with separate whole-review tables. The result names
published, definitely-not-published or indeterminate; pending reconciliation
never reissues POST. Existing comment tables and provider behavior are retained.

The five affected dev targets built successfully:

```sh
cmake --build --preset dev --target test_better_pull_review_github_mutation_contract_tests test_better_pull_review_live_review_mutation_provider_tests test_better_pull_review_review_mutation_recovery_storage_tests test_better_pull_review_live_review_submission_provider_tests test_better_pull_review_review_submission_recovery_storage_tests
ctest --test-dir build/dev --output-on-failure -R '^(github_mutation_contract|live_review_mutation_provider|sqlite_review_mutation_recovery_storage|live_review_submission_provider|live_review_submission_restart_recovery|sqlite_review_submission_recovery_storage|architecture_boundaries|architecture_boundary_contract)$'
```

All eight selectors passed in 70.73 seconds. After regenerating the three exact
Windows fixture registrations, `test_network_isolation_manifest` and
`test_network_isolation_structure_contract` passed 2/2 in 3.91 seconds. The
integrator reproduced the three new provider/storage/restart selectors, passing
3/3 in 0.73 seconds. Scoped simplifier review, clang-format 23.1.0 on 15 C++
files and `git diff --check` passed. Documentation structure passes. Native
transport code did not change, so its existing witness/cancellation evidence
is reused; the new session exercises that contract through its hermetic transport.

The list capacity is one complete page of 100 reviews. Restart fixtures use
fresh processes with normal exit, not abrupt-kill/power-loss simulation. All
three SQLite-backed tests reuse the established Windows `dos-volume-name`
exemption via the unchanged retained-ancestor path (BPR-0362); there is no new
Windows or Linux execution claim. Only ordinary build/dev outputs remain;
fixture temporary roots clean themselves and no task-specific logs are retained.
No live call, credential operation or application UI selection occurred.

The next host/UI slice must preserve or discover the original pending request
and key across restart. Current recovery also refuses a moved head before
querying the historical exact review; this remains indeterminate and never
retries POST. Address historical receipt discovery with that integration.

Independent critical review found no blocker against duplicate/false-success
publication or v1/v2-to-v3 data loss. It confirmed the moved-head recovery
limitation above as conservative and non-blocking. The reviewer inspected
source, fixtures, registrations and the integrator's passing three-selector
log; it did not run further tests. Opposite-family review was unavailable.
Closeout review runtime: native collaboration (gpt-6-astra, xhigh).
