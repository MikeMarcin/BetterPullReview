---
id: BPR-0107
title: Implement UI and code font settings
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0106]
---

# Implement UI and code font settings

Implement separate UI and code font family, size, weight, line height, tab width, ligature, bold, and italic settings with immediate preview and persistence.

A font change must invalidate the Qt-local measured axis and ribbon geometry while preserving semantic selection, then reproject the one shared comparison position without changing mapping or confidence. Prove the selection and mapping survive.
