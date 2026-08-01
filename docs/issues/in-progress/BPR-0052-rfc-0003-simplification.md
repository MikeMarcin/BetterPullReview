---
id: BPR-0052
title: Simplify the RFC-BPR-0003 change set
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0048, BPR-0049, BPR-0050, BPR-0051]
---

# Simplify the RFC-BPR-0003 change set

Run the code-simplifier skill over the completed RFC slice. Remove duplicated
epoch policy, authority drift, broad Git/filesystem/persistence access,
unnecessary serialization, retained whole-history copies, full Qt model
resets, unsafe process/config convenience paths, stale comments, and
avoidable complexity without changing behavior.

Replay core, host, real-Git, smart-transport, catalog/recovery, Qt,
composition, security, sanitizer, documentation, and target-native
available-platform verification after simplification.
