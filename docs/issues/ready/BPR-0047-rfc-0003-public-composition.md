---
id: BPR-0047
title: Compose the public Git cache and epoch path
status: Ready
rfc: RFC-BPR-0003
depends_on: [BPR-0042, BPR-0043, BPR-0044, BPR-0045, BPR-0046, BPR-0055]
---

# Compose the public Git cache and epoch path

Wire the concrete native smart transport, bounded app-owned protocol-v2
client, credential-free installed-Git adapter, cache/catalog ports, epoch
host, core state, and Qt comparison controls in the composition root with
explicit worker and shutdown order.

Drive a real QAction through the recorded GitHub fixture and a real installed
Git process against a local deterministic smart-protocol fixture into cache
creation/reuse, durable epochs, default/alias/arbitrary pair selection, and
visible exact OIDs for both GitHub.com and GHES authority shapes. A second
process invocation must prove persistence and cache reuse.

The tests-disabled Release application must select production factories while
excluding fixture flags, paths, symbols, and fake factories. No GitHub
mutation or live private repository is required.
