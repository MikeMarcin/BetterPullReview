---
id: BPR-0017
title: Establish RFC-BPR-0002 architecture allocation
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0016]
completed: 2026-07-29
---

# Establish RFC-BPR-0002 architecture allocation

Define the authoritative host/account/capability state, normalized provider
boundary, typed open/probe/fetch flow, asynchronous identity, bounded
observations, incremental projections, transport ports, and composition
selection before concrete effects are added.

Completion requires updated architecture records, target boundaries, and
dependency tests that keep provider, process, network, credential, and Qt
types out of the functional core.

## Result

- Defined the exact profile/session/open/refresh flow, complete asynchronous
  identity, tri-state capabilities, and state authority.
- Required normalized completions to remain host-staged until the core emits
  an exact accepted-snapshot delta.
- Allocated provider profiles, REST/GraphQL contracts, CLI, HTTPS, credentials,
  retries, rate limits, and normalization to the GitHub adapter behind narrow
  application-host ports.
- Defined incremental identity, capability, summary, conversation, command,
  status, and diagnostic projection behavior.
- Created a dedicated GitHub adapter target root and scoped agent
  instructions.
- Enforced designated ownership for `QProcess`,
  `QNetworkAccessManager`, and the GitHub CLI telemetry-disable setting while
  rejecting process/network APIs in core, application, and Qt presentation.
- Preserved target-native verification and zero GitHub Actions build,
  package, release, or verification involvement.

## Evidence

- Focused architecture boundary contract: 11/11 passed.
- Qt-enabled `dev` matrix: 15/15 passed.
- Qt-free `core-only` matrix: 10/10 passed.
- Documentation validator: `DOCUMENTATION_OK`.
- Source-pack validator: `OK: 14 RFCs, 17 normative documents, architecture v2
  enforced`.
- `git diff --check`: passed.
