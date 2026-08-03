---
id: BPR-0092
title: Implement the native main window and primary toolbar
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0091]
---

# Implement the native main window and primary toolbar

Build the native main window and top primary toolbar as Qt event and action adapters, not controllers. Previous/next file and previous/next change are top-level and keyboard addressable at supported widths, moving into an explicit compact overflow without losing keyboard access rather than disappearing.

Every action translates to a semantic intent; `QAction` state follows core command projections.
