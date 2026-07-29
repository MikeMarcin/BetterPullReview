---
id: BPR-0005
title: Implement the native Qt presentation adapter
status: Ready
rfc: RFC-BPR-0001
depends_on: [BPR-0004]
---

# Implement the native Qt presentation adapter

Build the native empty-state workbench and diagnostics surface. Translate a
real `QAction` into a semantic intent, apply command/status projections
incrementally, and cover event translation, accessibility, focus, and action
state without direct effect calls.
