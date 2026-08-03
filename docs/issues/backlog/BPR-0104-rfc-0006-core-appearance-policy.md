---
id: BPR-0104
title: Implement core appearance and typography policy
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0103]
---

# Implement core appearance and typography policy

Add the deterministic core policy for appearance mode selection and typography settings: System, Light, Dark, and high-contrast resolution, and the UI and code font settings with their valid ranges.

The core decides which profile variant applies and whether a settings value is legal. It holds no `QFont`, `QColor`, Qt type, or KSyntaxHighlighting type, and the dependency scan must keep proving that.
