---
id: BPR-0030
title: Add RFC-BPR-0002 focused tests and hostile fixtures
status: In Progress
rfc: RFC-BPR-0002
depends_on: [BPR-0027, BPR-0029]
---

# Add RFC-BPR-0002 focused tests and hostile fixtures

Add core replay/property tests, recorded GitHub.com and GHES 3.18 contract
tests, fake process/network/vault tests, reordered-completion tests, Qt event
and projection tests, and hostile fixtures for malformed data, oversized
responses, redirects, certificate failures, permissions, rate limits,
unsupported capabilities, and canary-secret leakage.

The protocol sink must reject every non-read REST verb and parse GraphQL
documents to reject `mutation` operations even though GraphQL reads use HTTP
POST.
