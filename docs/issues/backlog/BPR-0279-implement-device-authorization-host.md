---
id: BPR-0279
title: Implement bounded device-authorization host orchestration
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0277, BPR-0278]
---

# Implement bounded device-authorization host orchestration

## Required final state

- The application host owns device-code acquisition, a controlled-clock poll
  schedule, server interval and `slow_down`, a fixed total bound, cancellation,
  shutdown, and deterministic terminal cleanup.
- Success moves credential material exactly once into the existing
  `github_application_host::enroll_profile` vault boundary; no second storage or
  lifetime policy exists.
- The selected GitHub App design refreshes before expiry and stores refresh
  material under the same vault authority, with typed recoverable failure.
- Every non-success path erases transient material and leaves no partially
  enrolled profile. Direct host tests force all reorderings and failure cuts.
