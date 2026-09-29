# Packaging Instructions

Packaging preserves the core/application/adapter/composition dependency graph.

- Restrict Qt plugin and library search to trusted installation paths.
- Do not introduce an embedded browser, network server, telemetry or general remote
  loader. RFC-BPR-0014 records the existing bounded owner-authenticated profile
  AF_UNIX activation listener as a declared listener exception, and RFC-BPR-0045
  the separate tour command listener as the only other; retain their
  owner/peer/frame/lifetime checks and closed command sets. RFC-BPR-0035 permits only its qualified,
  user-confirmed macOS updater;
  other platforms and managed packages retain manual distribution. This scoped
  exception grants no publication or installed-application replacement authority.
- Keep signing credentials and private keys outside the repository.
- Generate and verify packages, SBOMs, checksums, notices, and signatures from
  committed source through documented release workflows.
- Keep corresponding source as a separate version-bound release asset, with
  exact installed references/digest and equivalent recipient access/retention.
  Preserve replacement/relinking instructions and candidate-specific approvals.
