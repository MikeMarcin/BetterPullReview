---
id: BPR-0032
title: Verify RFC-BPR-0002 security and authority boundaries
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0021, BPR-0022, BPR-0023, BPR-0024, BPR-0025, BPR-0026, BPR-0027, BPR-0029, BPR-0030]
---

# Verify RFC-BPR-0002 security and authority boundaries

Audit credential lifetime and storage, child-process arguments/environment,
network trust/proxy/redirect behavior, hostname/API-root validation, response
limits, diagnostics, settings, SQLite, UI text, target identity, cache keys,
and dependency direction with disposable canaries and retained secret-free
evidence.

Audit the exact CLI environment and argument allowlists, target-native HTTPS
backend and binary dependencies, failure-only vault states and fallback
absence, cross-authority redirect sinks, certificate rejection in both
transports, and parsed REST/GraphQL operation types.

Only read-only GitHub operations are in scope; no mutation permission or live
mutation target is required.
