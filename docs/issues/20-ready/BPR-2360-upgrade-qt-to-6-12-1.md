---
schema: "implement-rfc.card.v1"
id: "BPR-2360"
title: "Upgrade the pinned Qt runtime to 6.12.1"
status: "Ready"
rfcs: []
depends_on: ["BPR-1665"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner, 2026-11-23","mandate_revision":"qt-6-12-1-r1","assigned_implementer":"/root/qt_6_12_1","reservation":"docs/issues/_reservations/BPR-2360--BPR-2360--2ce2f43e9d3b.json"}
---

# BPR-2360: Upgrade the pinned Qt runtime to 6.12.1

The owner requested “Upgrade to Qt 6.12.1”. This patch upgrade follows
[RFC-BPR-0088](../../rfcs/RFC-BPR-0088-qt-6-12.md), which qualified 6.12.0.

## Acceptance

- AC1. The provider selects official Qt 6.12.1 and the existing modules.
  The Windows MSVC, Linux GCC and macOS Clang package layouts resolve.
- AC2. The corresponding-source archives and Linux ICU companion archive
  have verified SHA-256 digests, including the three redistributed ICU
  binaries. Focused packaging and source checks pass.
- AC3. The public configure, build and offscreen application path uses
  Qt 6.12.1 on the available Mac. Active documentation reflects the new
  pins, and the documentation gate passes.

## Plan

Query the official repositories with the pinned acquisition tools. Check
the source and companion archive bytes before updating the CMake pin, release
source inventory, ICU provenance and current build documentation. Configure
with the default provider, run a focused build and app self-test, and record
the platform coverage. Native Windows and Linux release qualification and
publication belong to subsequent release work.
