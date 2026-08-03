---
id: BPR-0093
title: Implement collapsible resizable file and conversation drawers
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0092]
---

# Implement collapsible resizable file and conversation drawers

Implement independently collapsible and resizable file and conversation drawers with stable-identity incremental model and view adapters. Collapsing returns space to the centre; reopening restores the persisted width across restart.

Bounded row and state changes use incremental model signals rather than whole-model resets.
