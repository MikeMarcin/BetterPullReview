---
id: BPR-0032
title: Verify RFC-BPR-0002 security and authority boundaries
status: Ready
rfc: RFC-BPR-0002
depends_on: [BPR-0021, BPR-0022, BPR-0023, BPR-0024, BPR-0025, BPR-0026, BPR-0027, BPR-0029, BPR-0030, BPR-0031, BPR-0037]
---

# Verify RFC-BPR-0002 security and authority boundaries

Audit credential lifetime and storage, child-process arguments/environment,
network trust/proxy/redirect behavior, hostname/API-root validation, response
limits, diagnostics, settings, SQLite, UI text, target identity, cache keys,
and dependency direction with disposable canaries and retained secret-free
evidence.

Audit the target-native HTTPS backend and binary dependencies, failure-only
vault states and fallback absence, redirect sinks, certificate rejection,
query-bearing redirect rejection, parsed REST/GraphQL operation types, and
the complete fixed CLI-unavailable boundary.

For milestone one, the CLI security claim is categorical zero launch rather
than partial broker safety. Verify every visible CLI selection returns the
typed `cli_unavailable` result before CLI executable/configuration discovery,
transport-factory or effect scheduling, process creation, or descendants.
Verify the tests-disabled Release executable omits the dormant CLI transport,
configuration, request-compiler, and runner symbols. Retain the exact eight
availability reasons and current official `gh` 2.96.0 trust evidence as the
rationale for RFC-BPR-0015: executable-image identity, redirect preflight,
effective `http_unix_socket` rerouting, credential/account binding,
working-directory identity, descendant containment, and numeric PID/PGID
reuse all remain outside the milestone-one production contract.

Complete direct target-native checks on the available macOS and Linux
surfaces. By product-owner direction, BPR-0036 owns the equivalent Windows
10/11 x64 runtime, binary-dependency, Credential Manager, HTTPS,
public-composition, and secret-absence evidence. Positive Windows CLI
execution belongs only to deferred RFC-BPR-0015. BPR-0036's open state keeps
the affected Windows claims and final RFC closeout open without blocking this
cross-platform security-review slice or later RFC implementation.

Only read-only GitHub operations are in scope; no mutation permission or live
mutation target is required.

GitHub Actions has no build, test, package, release, or verification role.
