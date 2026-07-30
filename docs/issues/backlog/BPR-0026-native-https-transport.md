---
id: BPR-0026
title: Implement native HTTPS transport and trust policy
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0020, BPR-0021, BPR-0022, BPR-0023, BPR-0024]
---

# Implement native HTTPS transport and trust policy

Implement the native HTTPS provider behind the same application port using
system trust and proxy behavior, bounded request/response handling, approved
API roots, explicit redirects, cancellation, and typed error classification.

Certificate failures and redirects to unknown hosts must fail closed. No
certificate-error bypass or unbounded response path may exist.

Run controlled target-native trust, proxy, certificate-failure, redirect,
cancellation, and selected-adapter tests on macOS, Windows, and Linux. Inspect
each target's linked/runtime Qt Network, TLS, proxy, and credential-vault
dependencies; configuration or another platform's backend is not evidence.
