---
id: BPR-0018
title: Implement deterministic GitHub identity and capability policy
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0017]
completed: 2026-07-29
---

# Implement deterministic GitHub identity and capability policy

Add opaque host, account, repository, and pull-request identities; semantic
open, refresh, and identity-selection intents; capability-dependent command
legality; generation-tagged requests and completions; structured diagnostics;
identity invalidation; incremental deltas; and deterministic replay tests.

The implementation must remain free of Qt, URLs, REST, GraphQL, process,
network, credential, filesystem, clock, thread, timer, and persistence types.

## Result

- Added strong profile, host, account, repository, pull-request, snapshot,
  capability-revision, identity-generation, request, and application-session
  identities.
- Added bounded repository locators, pull-request numbers, and four independent
  tri-state capability facts that fail closed on malformed representations.
- Added typed profile activation, session completion/failure/cancellation,
  open, refresh, load completion/failure/cancellation, identity invalidation,
  session establishment, and read-only fetch vocabulary.
- Implemented deterministic command legality, distinct unknown/unsupported
  capability reasons, safe failure reasons, explicit canceled states, compact
  status/identity/capability/summary deltas, and exact staged-snapshot
  acceptance.
- Profile renewal advances identity before effects, clears every
  identity-sensitive fact, emits invalidation before establishment, preserves
  monotonic request identity, and rejects delayed work in constant time.
- Initial open clears an old target, refresh preserves the accepted snapshot
  until exact replacement, and failures, cancellation, malformed results,
  canonical target mismatches, duplicates, and stale events emit no acceptance
  authority.
- The only RFC-BPR-0002 core effects are identity invalidation, session
  establishment, and read-only pull-request fetch; no mutation command or
  request exists.
- Expanded the core boundary gate and hostile contract for Qt, provider,
  filesystem, clock, concurrency, process, environment, socket, endpoint, and
  native-vault dependencies.

## Evidence

- Exercised all 81 combinations of the four capability tri-states.
- Independently mismatched every session and pull-request context field.
- Replayed activation, session, open, load, refresh, cancellation, failure,
  switch, canonical mismatch, and stale/duplicate paths.
- State-size assertion: `sizeof(github_state) <= 512`.
- Qt-enabled `dev` matrix: 17/17 passed.
- Qt-free `core-only` matrix: 12/12 passed.
- Hostile core-boundary harness: 4/4 passed.
- Documentation and source-pack validators passed.
- `git diff --check`: passed.
