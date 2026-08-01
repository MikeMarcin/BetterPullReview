---
id: BPR-0055
title: Implement core-authorized epoch retention maintenance
status: Backlog
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
