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

The production host must exhaust every REST `Link` continuation and GraphQL
`pageInfo` cursor under the aggregate record and byte ceilings before staging
one snapshot. It validates raw status, content type, headers, pagination
envelopes, cursor progress, and repository authority, projects only the
allowlisted normalized fields, and rejects unknown completeness, repeated or
missing cursors, partial pages, excess pages, and truncated totals atomically.
The one-page BPR-0019 recorded facade is test scaffolding, not evidence that a
larger live pull request loaded completely.
