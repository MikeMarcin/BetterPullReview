---
id: BPR-0020
title: Implement host profiles and capability probing
status: Ready
rfc: RFC-BPR-0002
depends_on: [BPR-0019]
---

# Implement host profiles and capability probing

Implement explicit GitHub.com and GHES host profiles, GHES 3.18 REST version
selection, HTTPS-only hostname/API-root validation, account identity, bounded
capability probes, cache keys and expiry policy, and explicit unavailable
results for review threads, viewed-file state, resolve/unresolve, and
unsupported features. Reject roots, profiles, and host changes derived from
pull-request content or redirect input.

Capabilities must come from recorded or observed contracts rather than a host
name assumption.
