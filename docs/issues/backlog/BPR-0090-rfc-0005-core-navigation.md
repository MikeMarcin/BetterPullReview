---
id: BPR-0090
title: Implement core workbench navigation and command projections
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0089]
---

# Implement core workbench navigation and command projections

Add the deterministic core transitions and command projections for file, change, epoch, drawer-semantic, and review actions: previous/next file, previous/next change, selection identity, drawer semantic state, and their enable/disable reasons.

The core stays Qt-free, effect-free, and compact. Selection follows the filtered actionable order. Command enablement is projected by the core, never inferred by a widget.
