---
id: BPR-0261
title: Make attended approval replay global and path-safe
status: Done
rfc: RFC-BPR-0008
depends_on: [BPR-0255]
---

# Make attended approval replay global and path-safe

The BPR-0256 critical review proved that the same approval identity can mint a
second grant when its exact bytes are materialized under another owner-private
parent, or when the original parent is removed and recreated. The spent marker
currently lives below that mutable parent. It also proved that approval opening
rejects symlinks only at the immediate parent and leaf, not in every ancestor.

## Required final state

- One approval ID has one durable replay state for the current user and product,
  independent of the approval artifact's parent or leaf path.
- The replay-state root is explicit application configuration, owner-private,
  retained by descriptor, schema-bound, and fails closed on absence,
  substitution, unsafe permissions, links, capacity, or durability failure.
- Two private approval roots containing identical approved bytes produce at
  most one grant; deleting/recreating either artifact parent does not erase the
  spent state.
- Every approval and replay-state path component is traversed descriptor by
  descriptor without following symlinks. The final file and immediate parent
  invariants remain unchanged.
- Concurrent processes cannot both spend one approval identity, and a failed
  durability/consumption sequence never leaves a reusable grant.
- No approval bytes, replay identity, or replay path contains a credential or
  reaches argv, environment, diagnostics, logs, Qt presentation state, or
  SQLite. BPR-0262 remains the owner of replacing the existing supervisor-to-app
  approval-artifact path handoff while authenticating that launch.

## Non-goals

- Do not authenticate the supervisor-to-app launch; BPR-0262 owns that boundary.
- Do not change GitHub mutation/recovery semantics; BPR-0263 owns reconciliation.
- Do not materialize an approval or execute the designated live lane.

## Capability-critical review result

The 2026-08-12 closeout review rejected the local-filesystem implementation.
Three counterexamples remain under the card's stated current-user threat model:

- the attended composition recreated an absent replay directory, so deleting or
  rolling back that directory made a later launch indistinguishable from first
  use and allowed the same approved bytes to mint another grant;
- another same-user process could unlink or replace the exclusively created
  marker after its last name/inode check, leaving no reachable spent identity;
- an `fsync` failure followed by a crash could lose the marker while an
  identical approval copy remained reusable elsewhere.

Retaining descriptors and adding inode/link revalidation can narrow in-flight
path races, but no state stored wholly below a current-user-mutable namespace
can provide deletion- or rollback-resistant compare-and-set semantics. The card
was therefore held until an attended product/security decision selected a
monotonic authority outside that namespace or explicitly narrowed the threat
model and reset semantics. The rejected implementation was not merged, and no
live lane, network request, credential, or approval was used.

The narrowed local-authority option was approved for implementation and
hermetic verification only through the exact operator statement recorded in
[designated-live-replay-threat-model-v001](../../verification/reviews/designated-live-replay-threat-model-v001/operator-packet.md).
The accepted subject explicitly excludes intentional same-user or administrator
deletion, rollback, restoration, or mutation of replay state. It grants no live
GitHub, credential, publication, packaging, or RFC-closeout authority.

## Verification

- Bootstrap tests exercise same bytes across two private roots, removal and
  recreation of the original artifact parent, cross-process/concurrent spend,
  unsafe replay-root substitution, every symlink ancestor position, and all
  existing approval failure families.
- Architecture and production-composition cuts prove replay state is supplied
  only by the selected composition path and no ambient fallback exists.
- The focused BPR-0256 matrix, format/tidy, documentation, and diff gates pass.

## Completed evidence

Proving implementation commit: `2962c58`.

### What landed

- One approval ID now has one replay state per user and product, keyed by the
  namespace rather than by the approval artifact's parent or leaf. Every
  approval subject binds `replay_schema`, `replay_namespace_path`, and
  `replay_namespace_id`, so a reset invalidates every approval bound to the
  prior identity.
- `designated_review_replay_initializer::initialize_or_reset` is reachable only
  from the explicit `--initialize-or-reset-designated-live-replay-state`
  operator command. The attended path calls `load` only; it never creates,
  repairs, or resets, and refuses when the namespace is absent.
- Approval and replay paths are opened component by component with
  `O_NOFOLLOW`. A spend takes the namespace directory lock, creates the record
  `O_CREAT|O_EXCL` at mode 0600, and revalidates the retained descriptor
  against name, inode, and single link before the approval inode is consumed.
- The namespace holds a fixed 128-record capacity and typed
  `replay_state_absent`, `unsafe_replay_state`, `replay_state_mismatch`,
  `replay_state_capacity`, and `replay_state_substituted` failures.

### Independent capability-critical review

The closeout review returned one blocking finding, reproduced and repaired
before commit: `durable_sync` used bare `fsync`, which Apple documents as
handing the write to the drive rather than committing it. Power loss or a panic
after a reported spend could lose the record while the namespace survived,
letting identical approved bytes mint a second grant without any intentional
act the operator subject excludes. `durable_sync` now issues `F_FULLFSYNC` on
Apple and fails closed when the barrier fails, and the architecture, security,
and build documentation no longer claim that all accidental loss fails closed.

Two review nits were repaired in the same commit: `retained_regular_matches`
was `noexcept` yet built an allocating `std::string` from a 64-character marker
name on the spend revalidation barrier, and the `readdir` loop cleared `errno`
once instead of before every call. A third — a foreign directory entry that
overflows the entry count reporting `replay_state_capacity` rather than
`unsafe_replay_state` — was deliberately left alone; both are fail-closed
refusals and retyping it would weaken the existing capacity coverage.

The review also confirmed the two operator packets' recorded verdicts bind only
their `decision-subject.txt` digests, both byte-identical. Their annotations
are now dated post-decision notes rather than in-place edits to decision-time
prose. No verdict text was altered.

### Gates

- `cmake --build --preset dev` and `--preset core-only` clean under `-Werror`;
  `cmake --preset app-release && cmake --build --preset app-release` clean,
  which is the only build that compiles the production `#else` branch.
- `ctest --preset dev` 184/184 before the durability repair. After it, 183/184
  with `application_native_notification_attended_contract` failing on
  `/bin/ps: Operation not permitted` under load; it passes in isolation and
  touches nothing in this slice. `ctest --preset core-only` 85/85 twice.
- `application_designated_review_authority_bootstrap`,
  `application_review_round_trip_composition_contract`,
  `application_composition_contract`, `designated_live_mutation_gate`,
  `designated_live_macos_capsule_contract`, and `core_review_round_trip` pass.
- `coding-format-check`, `coding-tidy`, `verify_documentation.py`
  (`DOCUMENTATION_OK`), and `git diff --check` pass.
- The composition contract's pinned `github_application.cpp` digest and
  production app-source manifest digest were re-pinned deliberately
  (`6c1c70c8…`→`5f3ea428…`, `7c46e847…`→`6289e40a…`). The third pin, the
  tests-disabled `compose_review_mutation` production branch, is unchanged and
  still passes, which is direct evidence that GitHub mutation semantics were
  not touched.

### Runtime evidence, shipped tests-disabled Release app

- `--initialize-or-reset-designated-live-replay-state` prints
  `DESIGNATED_LIVE_REPLAY_STATE_READY` and creates a mode-0700 namespace with
  mode-0600 metadata; a second run re-randomizes `namespace_id`, proving reset
  invalidates prior approvals.
- Passing a path argument, repeating the flag, or combining it with
  `--attended-designated-live-approval` each prints `STARTUP_FAILED` with exit
  1 and leaves the identity unchanged. Neither the namespace path nor its
  identity is printed on any path.
- With the namespace directory removed, an attended launch prints
  `STARTUP_FAILED` and does not recreate it.
- The initializer succeeds on the real APFS application-data volume after the
  `F_FULLFSYNC` change, so the barrier is supported for both the record file
  and the namespace directory there.

### What this does not establish

No live lane ran; no approval was materialized; no network request, credential,
or GitHub mutation was used. The operator subject's exclusion of intentional
same-user or administrator deletion, rollback, restoration, or mutation of
replay state stands, and loss the filesystem never reports remains outside the
guarantee. BPR-0262 still owns authenticating the supervisor-to-app launch, and
target-native Linux and Windows evidence remains deferred.
