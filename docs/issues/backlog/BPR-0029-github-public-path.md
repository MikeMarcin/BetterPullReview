---
id: BPR-0029
title: Wire the RFC-BPR-0002 public application path
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0027, BPR-0028]
---

# Wire the RFC-BPR-0002 public application path

Select explicit host profiles, transport, credential adapter, provider facade,
application host, core transitions, and Qt projection in the composition root.
Prove a read-only fixture PR traverses the real QAction-to-provider-to-rendered
application path for GitHub.com and GHES 3.18.

The composition root is the only layer allowed to select concrete transports
and platform credential adapters.
