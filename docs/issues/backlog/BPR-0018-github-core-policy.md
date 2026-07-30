---
id: BPR-0018
title: Implement deterministic GitHub identity and capability policy
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0017]
---

# Implement deterministic GitHub identity and capability policy

Add opaque host, account, repository, and pull-request identities; semantic
open, refresh, and identity-selection intents; capability-dependent command
legality; generation-tagged requests and completions; structured diagnostics;
identity invalidation; incremental deltas; and deterministic replay tests.

The implementation must remain free of Qt, URLs, REST, GraphQL, process,
network, credential, filesystem, clock, thread, timer, and persistence types.

