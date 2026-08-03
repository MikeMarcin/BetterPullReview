---
id: BPR-0106
title: Implement selectable appearance profiles with immediate preview
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0105]
---

# Implement selectable appearance profiles with immediate preview

Implement System, Light, Dark, and high-contrast profiles in the Qt adapter with immediate preview. A fresh installation defaults to System.

System mode follows the operating-system color scheme; an explicit mode does not. The static fresh-install default is unattended; the live scheme-change behaviour needs an owner-attended toggle and belongs to its own card.
