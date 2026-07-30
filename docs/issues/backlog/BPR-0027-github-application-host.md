---
id: BPR-0027
title: Implement GitHub application-host orchestration
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0018, BPR-0019, BPR-0020, BPR-0021, BPR-0022, BPR-0023, BPR-0024, BPR-0025, BPR-0026]
---

# Implement GitHub application-host orchestration

Execute typed probe and read requests through narrow ports; marshal normalized
completions through the serialized decision lane; own cancellation, retry,
rate-limit, session, cache, and lifecycle behavior; and invalidate
identity-sensitive state on host or account changes.

Fake-port tests must reorder completions, switch identity in flight, exhaust
bounded retries, cancel jobs, and prove workers never touch core or Qt state.
