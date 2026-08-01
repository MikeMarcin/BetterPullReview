---
id: BPR-0050
title: Verify RFC-BPR-0003 Git and cache security boundaries
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0042, BPR-0043, BPR-0044, BPR-0045, BPR-0047, BPR-0048, BPR-0055, BPR-0058, BPR-0060, BPR-0061, BPR-0062, BPR-0063]
---

# Verify RFC-BPR-0003 Git and cache security boundaries

Audit executable and helper identity, argv/environment/cwd/config isolation,
native-HTTPS credential and authority binding, smart-protocol request
admission, TLS/proxy/redirect behavior, process descendants, cancellation,
output/pack/disk/time bounds, cache permissions, path/symlink/junction races,
manifest/config/alternates/replace-object drift, ref and pkt-line injection,
catalog/ref transactions, crash recovery, and exact prune/delete scope.

Use malicious canaries to prove repository or user/system configuration cannot
run hooks, helpers, filters, LFS, submodules, textconv, external diff, SSH,
file/ext transports, editors, pagers, fsmonitor, traces, or worktrees.
Inspect retained evidence for credentials, private repository content, remote
URLs, raw provider/process errors, and broad deletion.

Complete macOS and available Linux target-native checks locally. Windows
execution remains BPR-0053 and is nonblocking for later implementation while
blocking only affected Windows claims and RFC closeout. GitHub Actions has no
role.

The fresh Ubuntu x86_64 GCC matrix exposed BPR-0060's actor-capture evaluation-
order defect. BPR-0060 is done in `a34d1b4` with direct GCC, Clang, sanitizer,
and architecture evidence, so this target-native security matrix has resumed.
The resumed 26-case GCC matrix then exposed BPR-0061's eager stale-preparation
context dereference. This card is temporarily blocked on that separately
tracked core remediation. BPR-0061 is done in `02ed3e8` with direct GCC,
Clang, sanitizer, core-only, and architecture evidence, so this matrix has
resumed again.
The first slow target-native group then exposed BPR-0062's stale independent
review database-ID expectation. This card is temporarily blocked on that
separately tracked contract remediation. BPR-0062 is done in `65d95e5` with
direct Clang, sanitizer, GCC, and architecture evidence, so the remaining
slow target-native matrix has resumed.
The refreshed installed-Git/cache group then exposed BPR-0063's dangling
canonical-OID view in the force-push fixture. This card is temporarily blocked
on that separately tracked contract remediation. BPR-0063 is done in
`d21f869` with direct Clang, sanitizer, GCC, Qt OID, Git/cache/maintenance,
and architecture evidence, so the remaining matrix has resumed.
