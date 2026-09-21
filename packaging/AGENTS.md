# Packaging Instructions

Packaging preserves the core/application/adapter/composition dependency graph.

- Restrict Qt plugin and library search to trusted installation paths.
- Do not introduce an embedded browser, listener, telemetry or general remote
  loader. RFC-BPR-0035 permits only its qualified, user-confirmed macOS updater;
  other platforms and managed packages retain manual distribution. This scoped
  exception grants no publication or installed-application replacement authority.
- Keep signing credentials and private keys outside the repository.
- Generate and verify packages, SBOMs, checksums, notices, and signatures from
  committed source through documented release workflows.
