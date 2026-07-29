---
id: BPR-0013
title: Run RFC-BPR-0001 cross-platform CI evidence
status: Ready
rfc: RFC-BPR-0001
depends_on: [BPR-0011, BPR-0014]
---

# Run RFC-BPR-0001 cross-platform CI evidence

Push the committed RFC branch to the repository and let the pinned GitHub
Actions matrix run on `ubuntu-24.04`, `windows-2025`, and `macos-15`, including
the separate Qt-free Ubuntu job.

## Completion evidence

- Every matrix job configures, builds, and passes its full test suite.
- The Windows and Linux application launch/render smoke tests pass through the
  real `better_pull_review` executable.
- Dependency scans on each platform show no browser runtime or local HTTP
  server dependency.
- The executed workflow URL and commit SHA are recorded in the RFC verification
  ledger.

Remote publication is an external repository mutation and requires user
authorization before the workflow stage of this task can start.
