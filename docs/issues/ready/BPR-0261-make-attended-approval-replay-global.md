---
id: BPR-0261
title: Make attended approval replay global and path-safe
status: Ready
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
- No approval, replay identity, or path contains a credential or reaches argv,
  environment, diagnostics, logs, Qt presentation state, or SQLite.

## Non-goals

- Do not authenticate the supervisor-to-app launch; BPR-0262 owns that boundary.
- Do not change GitHub mutation/recovery semantics; BPR-0263 owns reconciliation.
- Do not materialize an approval or execute the designated live lane.

## Verification

- Bootstrap tests exercise same bytes across two private roots, removal and
  recreation of the original artifact parent, cross-process/concurrent spend,
  unsafe replay-root substitution, every symlink ancestor position, and all
  existing approval failure families.
- Architecture and production-composition cuts prove replay state is supplied
  only by the selected composition path and no ambient fallback exists.
- The focused BPR-0256 matrix, format/tidy, documentation, and diff gates pass.

