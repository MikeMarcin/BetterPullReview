---
id: BPR-0027
title: Implement GitHub application-host orchestration
status: Ready
rfc: RFC-BPR-0002
depends_on: [BPR-0018, BPR-0019, BPR-0020, BPR-0021, BPR-0022, BPR-0023, BPR-0024, BPR-0025, BPR-0026]
---

# Implement GitHub application-host orchestration

Execute typed probe and read requests through narrow ports; marshal normalized
completions through the serialized decision lane; own cancellation, retry,
rate-limit, session, cache, and lifecycle behavior; and invalidate
identity-sensitive state on host or account changes.

Production session establishment must bind three independently validated
facts in order: a validated host profile, the authenticated account observed
from the strict `/user` contract, and normalized capability-probe evidence
for that exact profile revision/host/account/server/probe-contract key. The
BPR-0019 recorded normalizer's caller-supplied context is fixture scaffolding;
the production host must not treat caller-provided host, account, or
capability values as observations or use them to launder provider data.

Native enrollment must solve account discovery without weakening the vault
key. Treat submitted material as transient host-owned input, use it only
through the credential-bearing transport to observe `/user`, then store it
under that exact observed account authority. A login string or caller-claimed
account cannot select a vault entry. Profile replacement or identity
invalidation must delete the previous exact authority so a revision-keyed
credential cannot become an orphaned secret.

Production GraphQL reads must be compiled from that exact capability
observation. Omit unsupported schema fields, preserve review-thread reads when
only resolve or unresolve is unsupported, and normalize omitted per-file
viewed state as `unavailable` and omitted per-thread target permission as a
safe unavailable/false fact. Absence caused by capability-shaped selection
must never be staged as an authoritative empty collection. The BPR-0019
single-query recorded facade covers only the fully supported selected
GitHub.com/GHES 3.18 contract and is not the production fallback for a
partially supported schema.

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
