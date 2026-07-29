# Packaging Instructions

Packaging preserves the core/application/adapter/composition dependency graph.

- Restrict Qt plugin and library search to trusted installation paths.
- Do not introduce an embedded browser, listener, telemetry, remote loader, or
  automatic updater.
- Keep signing credentials and private keys outside the repository.
- Generate and verify packages, SBOMs, checksums, notices, and signatures from
  committed source through documented release workflows.
