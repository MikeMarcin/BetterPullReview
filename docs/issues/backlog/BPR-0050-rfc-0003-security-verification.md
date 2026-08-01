---
id: BPR-0050
title: Verify RFC-BPR-0003 Git and cache security boundaries
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0042, BPR-0043, BPR-0044, BPR-0045, BPR-0047, BPR-0048, BPR-0055, BPR-0058]
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
