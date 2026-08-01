---
id: BPR-0055
title: Implement core-authorized epoch retention maintenance
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0041, BPR-0043, BPR-0044, BPR-0045]
---

# Implement core-authorized epoch retention maintenance

Add explicit deterministic core intents, transitions, request identity, and
terminal policy for user-authorized epoch pruning and whole-repository cache
deletion. Compose those decisions through the epoch application host to the
existing narrow catalog and Git-cache ports; an adapter method alone is not a
product authorization path.

Pruning must seal the exact epoch/ref pair and expected catalog revision before
deleting refs, then atomically publish catalog availability. Cache deletion
must build and seal the complete bounded affected-catalog set before the exact
repository cache is quarantined/tombstoned. Both flows must preserve
authority isolation, serialized completions, cancellation only before their
durable point of no return, restart recovery, repair-required ambiguity, and
incremental presentation. No automatic or quota-triggered deletion is allowed.

Use manual executors and fault-injected fake ports to reorder every phase,
identity switch, cancellation, duplicate, stale completion, restart, and
catalog/ref failure. Add a real-cache/catalog integration fixture proving exact
cleanup without broad filesystem deletion or false catalog visibility.

## Implementation audit frontier

The BPR-0043 Git-cache and BPR-0044 catalog adapters already expose narrow,
typed prune and cache-delete phase operations, but no active semantic intent
can authorize them and the BPR-0045 host intentionally leaves maintenance
recovery uncomposed. This slice must reuse those durable and physical facts;
it must not introduce a competing deletion algorithm or infer authority from
paths, ref prefixes, adapter availability, or quota pressure.

Implementation begins with the deterministic intent/request/terminal and
compact pending-state algebra. Host orchestration follows only after direct
core tests establish exact targets, revision/generation handling,
pre-point-of-no-return cancellation, mandatory post-point-of-no-return
completion, stale rejection, and incremental deltas. The existing real
cache/catalog contracts remain the physical boundary and will be extended
only where a composed integration claim cannot otherwise fail when false.
