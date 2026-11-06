---
schema: "implement-rfc.card.v1"
id: "BPR-1903"
title: "Exclude user-wide vcpkg integration from BPR builds"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"discovered_by":"BPR-1901","mandate":"Owner confirmed on 2026-11-06 that BPR does not use vcpkg and its user-wide integration must not affect this project.","milestone_plan":{"id":"windows-vcpkg-exclusion-v1","path":"docs/rfcs/plans/windows-vcpkg-exclusion-v1/milestone-plan.json"}}
---

# Exclude user-wide vcpkg integration from BPR builds

Native link commands acquire `vcpkg/installed/.../lib/*.lib` and additional
search paths through Visual Studio's user-wide integration. BPR declares its
dependencies through its own providers. Disable the implicit integration only
for generated BPR projects, preserving the user's installation and other projects.

- AC1: Native generated Visual Studio projects opt out, including dependency
  targets; actual compiler and linker inputs exclude ambient vcpkg paths.
- AC2: Retain caller-supplied unrelated Visual Studio globals, Debug/Release
  options and test selection; native affected builds and functional checks pass.
- AC3: Verify the generated property and refusal controls; document the setting
  and land on main with a card and one-line journal entry.
