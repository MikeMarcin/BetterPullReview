---
id: BPR-0042
title: Implement hardened installed-Git process boundary
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0040]
---

# Implement hardened installed-Git process boundary

Replace the Git adapter placeholder with a concrete narrow application port
and one designated `QProcess` implementation. Resolve and validate the
installed Git image and exec path, construct the child environment from
nothing, isolate system/global/local configuration and templates, use only
fixed verb/argument shapes against validated app-owned bare repositories, and
bound stdin, stdout, stderr, duration, cancellation, descendants, and
shutdown.

Keep credentials, URLs containing credentials, GitHub responses, proxy
authority, and network trust out of the Git child. Use a separately bounded
RFC-BPR-0003 Git smart-HTTP transport backed by the native-HTTPS authority so
installed Git performs local pack/protocol work without becoming another
production authentication transport. Do not weaken the RFC-BPR-0002
query-bearing redirect rejection.

Direct canaries must prove that user/system configuration, credential helpers,
askpass, hooks, filters, LFS, submodules, textconv, external diff, alternates,
replace objects, SSH/file/ext protocols, editors, pagers, traces, and
repository-provided executables are not invoked.
