---
id: BPR-0032
title: Verify RFC-BPR-0002 security and authority boundaries
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0021, BPR-0022, BPR-0023, BPR-0024, BPR-0025, BPR-0026, BPR-0027, BPR-0029, BPR-0030, BPR-0037]
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
transports, binding between selected CLI digest evidence and the image
actually launched, persisted CLI transport configuration such as
`http_unix_socket`, selected CA file/directory identity and contents across
validation-to-open replacement or mutation, binding between the correlated
credential generation and the CLI's active host account, working-directory
identity/ownership/permission and validation-to-launch replacement, Unix
initial-process-group escape, PID/process-group identity reuse across cleanup
signals, and parsed REST/GraphQL operation types.

Complete direct target-native checks on the available macOS and Linux
surfaces. By product-owner direction, BPR-0036 owns the equivalent Windows
10/11 x64 runtime, binary-dependency, Credential Manager, HTTPS, and
secret-absence evidence plus CLI exact-environment, descendant containment,
crash suppression, and hard in-flight stream bounds. Its open state keeps the
affected Windows claims and RFC closeout open without blocking this
cross-platform security-review slice.

Only read-only GitHub operations are in scope; no mutation permission or live
mutation target is required.
