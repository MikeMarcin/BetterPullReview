---
schema: "implement-rfc.card.v1"
id: "BPR-1179"
title: "Package genuinely removable agent and automation components"
status: "Done"
rfcs: ["RFC-BPR-0060"]
depends_on: ["BPR-1146", "BPR-1167", "BPR-1171"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "optional-adapters", "acceptance": ["RC60-01", "RC60-02", "RC60-03"], "pack_alias": "PACK-CYCLE-37", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC60-01","outcome":"pass","revision":"c3c7d6b3c473e4d3f3d4eb1910112ba1063a3572","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1179's share, macOS arm64. tools/release.sh --prepare-only --variant agent-free and --variant agent-free-automation-off: .app holds the application, the supervisor (kept: Git's guarded launches run inside it) and bpr_update_helper, no MCP helper; verify_release_package RELEASE_PACKAGE_OK with families recorded false; SPDX sourceInfo ends '; agent integrations absent' (and '; local automation absent'); verify_composed_hosts forbids the four agent hosts, launcher, protocol decoder and MCP supervisor (and the cycle owner service) and the full audit of the same images fails naming them; a planted MCP helper fails omitted-family-file-present. Started relocated with a fresh HOME and agent-named PATH canaries: composed probe reached, no child process, no canary ran, no vault selected. The tour-cli preset is agent-free: installed TourCLI is 12 files, bpr holds no agent anchor (the agents-ON build held nine), cycle_cli_tests passes. Windows/Linux packages owed to stations."}},{"criterion":"RC60-02","outcome":"pass","revision":"c3c7d6b3c473e4d3f3d4eb1910112ba1063a3572","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1179's share at composition level, macOS arm64 Release. Agent-free graph with automation and bpr: whole suite 555/558 (failures: the two pin rows, which pass after the reseal, and BPR-1236), including application_review_cycle_two_round_public_path, parity, cycle_cli_public_path, profile_cycle_handoff and review_cycle_publish_public_path (publication preparation, GitHub in memory). Full desktop course then reopened by the agent-free, automation-off reviewer (--reopen-application): R1-R3, F1/F2, ten notes, verifications and the reply read back; full bpr reads the same history. No PATH agent canary ran. Installed-package GUI course is BPR-1182's."}},{"criterion":"RC60-03","outcome":"pass","revision":"c3c7d6b3c473e4d3f3d4eb1910112ba1063a3572","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1179's share, macOS arm64. BPR_BUILD_LOCAL_AUTOMATION is its own switch and package variant, independent of agents. Agent-free, automation-off graph: whole suite 543/546 (pin rows before reseal; BPR-1236); application_review_cycle_automation_off_two_round runs the window-only loop over four launches with Copy, Save and bundle agreeing with the canonical payload, no owner command endpoint bound beside the bound activation endpoint, nothing started, nothing lost; negative control: the same course against the full reviewer fails 'expected absent'. bpr is removed by its install manifest; the desktop does not need it. The automation-off package forbids the cycle owner service anchor and passes its audit."}},{"criterion":"BPR-1179:boundary","outcome":"pass","revision":"c3c7d6b3c473e4d3f3d4eb1910112ba1063a3572","evidence":{"kind":"command","detail":"Full graph (both families, bpr) Release: 601/605, failures coding_format_check (other cards' files, cleared upstream by 12b7faa1c), the round-trip pin and composition contract (resealed: both pass after; mutation pin unchanged) and BPR-1236 (fails in every graph and in the pre-change Debug tree). architecture_boundaries passes in all three graphs; verify_composed_hosts_tests (56), verify_release_package_tests (7) and package_runtime_tests (17) pass; clang-tidy on seven changed TUs reports no finding on a changed line; DOCUMENTATION_OK. Landed as 192ffb56b after a rebase that changed no owned path."}}]
---

Pack alias: **PACK-CYCLE-37** (live `BPR-1179`, allocated at import).

# BPR-1179: Package genuinely removable agent and automation components

## Outcome and bounded work

Finish build/composition splits and generate desktop/CLI packages with agent integrations absent; make local automation independently removable. Update resource/helper/install/SBOM manifests. Uninstall adapters without deleting any cycle data.

## Acceptance and direct evidence

Link/dependency closure and installed-file inspection, clean startup with no providers, manual two-round course, automation-off GUI course, publication preparation and re-open old cycles after removal.

Criteria: RC60-01, RC60-02, RC60-03.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Result (2026-10-04)

Promoted at `f62213774`, claimed at `3691cc379`, implemented at `192ffb56b`
(the verified candidate was `c3c7d6b3c`; the rebase changed no owned path).
Owned paths, decisions, every command and its result are in the
[ledger](../../verification/RFC-BPR-0053-0061.md#bpr-1179-removable-agent-integrations-and-local-automation-2026-10-04).

- `BPR_BUILD_AGENT_INTEGRATIONS=OFF` compiles out the agent launch adapter
  and protocol loop, the agent hosts, the provider protocol decoders and the
  MCP helper and its supervisor. The window withdraws its agent controls.
- `BPR_BUILD_LOCAL_AUTOMATION=OFF` binds no owner command endpoint and
  builds no `bpr`.
- Presets `*-agent-free` and `*-agent-free-automation-off` exist for macOS,
  Windows and Linux. `tools/release.sh --variant` builds them on macOS. The
  `tour-cli` preset is agent-free.
- Package records name the families built, and the release audits enforce
  them.
- The supervisor stays in every package, because Git's guarded launches run
  inside it.

macOS arm64 evidence:

- Agent-free, automation-off and full packages, each `RELEASE_PACKAGE_OK`
  with its own families.
- An agent-free CLI install that passes the CLI course.
- Whole suites in three graphs, with the only failures being the resealed
  pins and [[BPR-1236]].
- The automation-off two-round course; the agent-free two-round, parity, CLI,
  handoff and publish courses; and a full desktop's cycle reopened by an
  agent-free, automation-off build.
- Negative controls for each.

Owed to stations, not run here: Windows and Linux agent-free builds and
packages, the full-graph regression on MSVC and GCC, and Windows replacement
of one MSI variant by another. The commands are in the ledger.
