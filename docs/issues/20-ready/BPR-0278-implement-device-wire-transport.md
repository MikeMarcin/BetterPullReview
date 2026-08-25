---
id: BPR-0278
title: Implement device authorization wire and native HTTPS contracts
status: Ready
rfc: RFC-BPR-0016
depends_on: [BPR-0276]
---

# Implement device authorization wire and native HTTPS contracts

## Required final state

- A narrow GitHub adapter owns exact bounded form requests and sanitized typed
  responses for device-code and token endpoints.
- Both endpoints reuse RFC-BPR-0002 system trust, proxy, cancellation, response
  limits, certificate refusal, unknown-authority refusal, and query-redirect
  refusal without weakening authenticated API behavior.
- No secret or transient code enters URLs, process arguments, environment,
  diagnostics, logs, fixtures, settings, or SQLite.
- Native loopback/hostile contracts prove exact methods, paths, headers, bodies,
  bounds, redirect/certificate failures, and cancellation for both endpoints.
