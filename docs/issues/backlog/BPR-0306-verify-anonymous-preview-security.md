---
id: BPR-0306
title: Verify anonymous preview security and public behavior
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0305]
---

# Verify anonymous preview security and public behavior

## Required final state

- Core, provider/read-contract, native HTTPS, host, Qt, public composition,
  tests-disabled Release, sanitizer, runtime, dependency, and boundary matrices
  pass from clean state.
- Positive canaries prove zero Authorization header, vault lookup, credential
  enumeration/enrollment, mutation, GraphQL/viewer request, GHES request, and
  private-content disclosure.
- Target-native macOS evidence covers the production native-HTTPS path and a
  real rendered public-preview course; offscreen evidence is layout-only.
- Every platform where preview is enabled has target-native proof or remains
  explicitly unavailable under a named open platform card.
