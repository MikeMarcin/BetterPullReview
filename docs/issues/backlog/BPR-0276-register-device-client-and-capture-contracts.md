---
id: BPR-0276
title: Register the device client and capture real wire contracts
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0275, BPR-0035]
---

# Register the device client and capture real wire contracts

Use only the authority recorded by BPR-0275 to create the selected public
client and capture sanitized contracts from the endpoints the product will call.

## Required final state

- The registered owner, public client identifier, exact permission/scope set,
  installation policy, device-flow setting, and refresh policy match the
  approved subject; no client secret is shipped or committed.
- Sanitized real-service fixtures cover device-code success, token success,
  `authorization_pending`, one deliberately early bounded poll producing
  `slow_down`, denied, expired, and disabled responses. `slow_down` is the
  service's rate-limit contract; no separate rate-limit body is invented.
- Malformed, truncated, duplicated, and oversized bodies are explicitly
  synthetic hostile parser fixtures, not falsely attributed to GitHub.
- Capture tooling and evidence prove tokens, refresh tokens, device codes, user
  codes, cookies, and credentials are absent from repository artifacts and logs.
- The public identifier and fixture provenance are recorded without compiling
  the identifier into any product artifact or granting repository-mutation
  authority; BPR-0281 owns compiled production selection after this gate is
  terminal.
- Read-only app-token probes prove Metadata, Contents, Checks, pull-request REST,
  and the exact review-thread GraphQL document. Pull requests write is verified
  as configured in the registration but no POST is sent; actual review-comment
  publication belongs to the separate identity-bound designated-live packet and
  cannot be inferred from this card.
