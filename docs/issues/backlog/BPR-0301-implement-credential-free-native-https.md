---
id: BPR-0301
title: Implement typed credential-free native HTTPS reads
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0035, BPR-0299]
---

# Implement typed credential-free native HTTPS reads

## Required final state

- Native HTTPS accepts an explicit anonymous read authority distinct from a
  missing credential and structurally omits authorization and all vault access.
- The existing authenticated transport and RFC-BPR-0002 TLS, proxy, redirect,
  cancellation, timeout, and response-limit rules remain unchanged.
- Provider admission refuses GraphQL, viewer identity, all mutations, private
  intent, and GHES before constructing a request.
- Loopback positive and hostile tests prove header absence with canaries and
  prove that absent/invalid authenticated credentials do not become anonymous.
