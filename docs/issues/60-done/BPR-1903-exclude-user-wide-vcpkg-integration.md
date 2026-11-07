---
schema: "implement-rfc.card.v1"
id: "BPR-1903"
title: "Exclude user-wide vcpkg integration from BPR builds"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"discovered_by":"BPR-1901","mandate":"Owner confirmed on 2026-11-06 that BPR does not use vcpkg and its user-wide integration must not affect this project.","milestone_plan":{"id":"windows-vcpkg-exclusion-v1","path":"docs/rfcs/plans/windows-vcpkg-exclusion-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"39ee4ea4a45387515d72103799228839644cf628","evidence":{"kind":"observation","detail":"Native audit: 939 generated projects opt out; all 408 current CL/link/Lib command logs exclude vcpkg.","artifact":"docs/verification/windows-build-latency.md"}},{"criterion":"AC2","outcome":"pass","revision":"39ee4ea4a45387515d72103799228839644cf628","evidence":{"kind":"observation","detail":"Unrelated VS globals and 355-test core-only inventories preserved; four native registered functional/policy courses and assertion controls pass in both Debug and Release.","artifact":"docs/verification/windows-build-latency.md"}},{"criterion":"AC3","outcome":"pass","revision":"39ee4ea4a45387515d72103799228839644cf628","evidence":{"kind":"observation","detail":"Native refusal control rejects reenabled Abseil project; portable overrides/forwarding controls and documentation pass. Source, completed card, journal and evidence are ready for the authorized main fast-forward.","artifact":"docs/verification/windows-build-latency.md"}}]
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

## Evidence

The [native investigation](../../verification/windows-build-latency.md) records
source `39ee4ea4a`, 939 opted-out projects, 408 current command logs without
vcpkg, preserved unrelated globals and Debug/Release selection, passing native
functional checks and an intentional generated-property refusal. Warm retry
times did not improve; this correction removes an undeclared dependency source.
