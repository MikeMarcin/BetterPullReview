---
id: BPR-0075
title: Implement host diff execution cache and stale rejection
status: Backlog
rfc: RFC-BPR-0004
depends_on: [BPR-0074]
---

# Implement host diff execution cache and stale rejection

Add exact blob materialization, immutable worker inputs, bounded asynchronous
diff execution, cache keys containing repository/PR/epochs/file/blobs/options/
algorithm revision, cooperative cancellation, serialized completion delivery,
and deterministic stale-result rejection.

Workers never touch Qt or mutable core state. Binary, missing, decode,
oversized, generated-policy, cancellation, invariant, and worker failures
remain typed and reason-coded.

## Required direct evidence

A manual executor reorders completions across repository, PR, epoch, file,
blob, options, generation, cancellation, shutdown, and cache hit/miss
boundaries. Same-path collision fixtures must miss when any exact identity
changes. Core/worker dependency scans must pass without Qt.
