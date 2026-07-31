---
id: BPR-0042
title: Implement hardened installed-Git process boundary
status: Ready
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
app-owned protocol-v2 client and RFC-BPR-0003 smart-HTTP transport backed by
the native-HTTPS authority. Installed Git performs only fixed local
pack/object/ref work, including bounded strict `index-pack`; it never runs
`fetch-pack`, parses a remote advertisement, or becomes another production
authentication transport. Do not weaken the RFC-BPR-0002 query-bearing
redirect rejection. Journal the validated request-owned pack spool and
preexisting-pack fact before import so crash residue is exactly recoverable
without broad object deletion.

Direct canaries must prove that user/system configuration, credential helpers,
askpass, hooks, filters, LFS, submodules, textconv, external diff, alternates,
replace objects, SSH/file/ext protocols, editors, pagers, traces, and
repository-provided executables are not invoked.
