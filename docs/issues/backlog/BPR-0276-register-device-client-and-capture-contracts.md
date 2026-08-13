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
- Sanitized bounded fixtures cover device-code success, token success,
  `authorization_pending`, `slow_down`, denied, expired, disabled, malformed,
  oversized, and rate-limited responses from the real selected service.
- Capture tooling and evidence prove tokens, refresh tokens, device codes, user
  codes, cookies, and credentials are absent from repository artifacts and logs.
- The compiled public identifier and fixture provenance are documented without
  granting repository-mutation authority.
