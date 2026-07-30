---
id: BPR-0034
title: Simplify the RFC-BPR-0002 change set
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0030, BPR-0031, BPR-0032, BPR-0033]
---

# Simplify the RFC-BPR-0002 change set

Run the code-simplifier skill over the completed RFC slice. Remove duplicated
policy, provider leakage, broad service access, unnecessary serialization,
whole-state refreshes, unsafe convenience paths, stale comments, and avoidable
complexity without changing behavior, then replay focused verification.
