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

**Note: Linux run at `1d8b92642` (station `bpr-main-linux`, Ubuntu 26.04
under WSL2, GCC 15.2.0, offscreen).**

1. The reduced tree `build/bpr1179-off` (`dev` preset with
   `BPR_BUILD_AGENT_INTEGRATIONS=OFF` and `BPR_BUILD_LOCAL_AUTOMATION=OFF`)
   configured and built with no warnings, and 6 of 6 passed:
   `application_review_cycle_automation_off_two_round`,
   `application_review_cycle_publish_public_path`,
   `application_review_cycle_desktop_public_path`,
   `composed_application_hosts`, `architecture_boundaries` and
   `review_cycle_dependency_closure`. `dev` is a Debug preset, so this tree
   does not exercise `-O3`, and no warning needed a fix.
2. The full `dev` build succeeded. `ctest --preset dev` passed 573 of 580 in
   3,533 s. The 7 failures are the known ones:
   - [[BPR-1255]]: the six `qt_workbench_title_row_scale_{100,125,200}` and
     `qt_workbench_window_scale_{100,125,200}` rows, each failing on "no
     review-bar control folds into an overflow menu at any width".
   - [[BPR-1236]]: `application_profile_window_review_restoration_public_path`,
     failing on "failed reading restoration preserves exact recorded
     identities and sibling".

   Nothing new failed.
3. Not run: this station is Ubuntu 26.04, not the 24.04 package baseline, so
   the `linux-package-agent-free` DEB is owed by a 24.04 station.

Done is not reopened.

### Windows at `795d387cf` (bpr-main-win, 2026-10-04)

Windows 11 Home 10.0.26200, MSVC 14.51, Qt 6.11.2. This is MSVC's first
compile of the switched-off code.

- **`windows-dev` with both switches off** (`build/bpr1179-off`,
  `BPR_BUILD_AGENT_INTEGRATIONS=OFF`, `BPR_BUILD_LOCAL_AUTOMATION=OFF`). The
  build printed no `warning C` or `warning LNK` line. The station request's
  selection passes **6 of 6**: `review_cycle_dependency_closure`,
  `application_review_cycle_publish_public_path`,
  `application_review_cycle_automation_off_two_round`,
  `composed_application_hosts`, `application_review_cycle_desktop_public_path`
  and `architecture_boundaries`. The tree has no `bpr.exe` and no MCP context
  helper. It keeps `better_pull_review_agent_supervisor.exe`, which Git's
  guarded launches need.
- **A new pin.** `test_network_isolation_manifest` in that tree flagged
  `application_review_cycle_automation_off_two_round`: it is
  `WINDOWS_UNCONTAINED` but was not in the pinned set. Measured contained
  through the sandbox launcher, its first `git init` exits 128 before any
  desktop step, the [[BPR-1166]] two-round shape (`dos-volume-name`). It is now
  pinned with that line, and the manifest passes in both the off tree and the
  full tree.
- **Full `windows-dev` graph: 553 of 574** in 5,840.78 s (`-j6`). None of the
  21 failures comes from this card:
  - 13 are tracked or were in [[BPR-0905]]'s Windows record: the six Qt
    overflow rows ([[BPR-1255]]), [[BPR-1236]], `application_windows_title_row`
    ([[BPR-0653]]), `rfc0017_release_absence_negative_control`,
    `application_device_authorization_refresh_course`,
    `qt_agent_launch_canary` (it also fails serially),
    `application_large_review_cache_bytes` ([[BPR-1265]], also fails serially)
    and `application_agent_finding_dif_context_contract`.
  - 7 timed out under `-j6` and pass serially:
    `application_large_review_cache_entries`, `_loaded_close` and
    `_resources`, `application_private_review_native`,
    `composed_application_hosts`, `github_provider` and
    `application_diagnostic_subsystems`.
  - `documentation_structure` refused a local `NEXT_ID` behind origin's high
    water. That is checkout state, not a product failure.
- **Agent-free Release.** `cmake --preset windows-app-release-agent-free`
  configures, and `app_better_pull_review` builds with no `warning C` or
  `warning LNK` line. The tree holds `better_pull_review.exe` and the
  supervisor, and no MCP helper.
- **Owed.** `packaging/package.py --generator WIX` was not run: CPack is set
  to WiX 4 (`CPACK_WIX_VERSION 4`), and this station has no WiX toolset
  installed. Installing it is the owner's call, so the agent-free MSI,
  `scripts/verify_release_package.py` on it, and installing it over a full MSI
  and back are still owed on Windows.

### Windows agent-free MSI (bpr-main-win, 2026-10-05)

- **WiX.** The owner chose WiX 5 over the current release. Installed for the
  user as a global .NET tool on .NET SDK 10.0.401: `wix` **5.0.2**
  (`5.0.2+aa65968c`) and `WixToolset.UI.wixext` **5.0.2**. Their NuGet
  metadata names the MS-RL, an OSI-approved license with no further terms. WiX
  7.0.0, the current release, ships `OSMFEULA.txt` (the Open Source
  Maintenance Fee EULA), which asks revenue-generating users for a fee beyond
  the OSI license; that is why it was not used. CPack's existing
  `CPACK_WIX_VERSION 4` (.NET-tool) mode drove 5.0.2 with no CMake change.
- **Redistribution notice.** The packager requires
  `BPR_MSVC_REDISTRIBUTION_NOTICE`. The owner chose Visual Studio 18
  Community's own installed `Licenses\1033\Redist.txt`. That file is a
  190-byte stub naming the distributable-code list and pointing to
  https://aka.ms/vs/18/redistribution, and the package's
  `THIRD-PARTY-NOTICES.txt` carries it as installed.
- **Package.** `packaging/package.py --build build/windows-app-release-agent-free
  --generator WIX` at `d31ebf1f5` (clean tree) produced
  `BetterPullReview-0.1.3-windows-x64-agent-free.msi` (21,925,888 bytes,
  SHA-256 `b8656d6d75bcf218baf9bbeddae39d1afca333433c9ba77a8cf57c5d7fc3cc31`), with its SPDX, provenance, checksums and
  `BetterPullReview-0.1.3-windows-x64-corresponding-source.zip`. It is
  unsigned and unpublished.
- **Audit.** `scripts/verify_release_package.py` on the expanded runtime prints
  `RELEASE_PACKAGE_OK`: 15 binaries, no MCP context helper, families
  `agent_integrations: false` and `local_automation: true`, and the source
  asset bound by name and SHA-256. `trust` is `not-assessed`, so this is not
  signature or license approval.
- **Owed.** Installing this MSI over a full one and back is a desktop-attended
  step, and stays owed until the owner clears the desktop.

**Note: the owed install, Windows (bpr-main-win, 2026-10-05).** The full record is on [[BPR-1182]].

**Install over a full MSI and back.** The owner cleared the JaegAW desktop on
2026-10-05 and approved the installs on the station. The packages were built
at `bd2f41995` (clean tree, WiX 5.0.2), and each passed the release-package
audit with `RELEASE_PACKAGE_OK`:

- full `BetterPullReview-0.1.3-windows-x64.msi`: 22,134,784 bytes, SHA-256
  `7b3ee013429b2e1cac68ab873f7165a92b2394c1689042c36b461a45dae3e043`, with
  the MCP helper, `agent_integrations: true`;
- agent-free `BetterPullReview-0.1.3-windows-x64-agent-free.msi`: 21,929,984
  bytes, SHA-256
  `0b11c09b6b881ef5e602848937ce93796f869578d1f456fc0b9b49c0217bab4a`, with
  no MCP helper, `agent_integrations: false`, `local_automation: true`.

Neither is signed or published.

A station driver mirrored `installed_cycle_package_course.py`. Before
starting it found no BetterPullReview product, so there was no prior install
to restore. It moved the account's existing `%LOCALAPPDATA%\BetterPullReview`
aside, so no installed build opened it, and moved it back at the end (10
files, 389,120 bytes, as before). Each package was installed per user with
`msiexec /i … /qn` into `%LOCALAPPDATA%\Programs\BetterPullReview` and
started natively. After each install Windows Installer listed exactly one
product with the package's upgrade code.

| State | Product code | Files | MCP helper | Desktop start |
|---|---|---|---|---|
| 1. full | `{0BB017D9-…}` | 24 | present | composed in 0.97 s |
| 2. agent-free over it | `{BBA74A4A-…}` | 23 | gone | composed in 1.68 s |
| 3. full again | `{0BB017D9-…}` | 24, the same bytes as state 1 | back | composed in 0.72 s |

`better_pull_review_agent_supervisor.exe` is in every state, as the
packaging guide's compatibility decision says.

Every start passed the same checks:

- it composed the profile, with a window titled "BetterPullReview — Default
  profile";
- it started no child process (sampled every 50 ms) and held no socket;
- it bound both its activation pipe and its owner command pipe;
- it closed on `WM_CLOSE` with exit 0 and left no pipe behind;
- none of the agent-named PATH canaries ran.

A cycle made after the first start, with `bpr cycle open`, a note on line 3
and a frozen F1, read back byte for byte after the agent-free start and
after the second full start: `show` and F1's export, using a
tests-on Release `bpr` from the tour-cli tree.

`msiexec /x` then removed the program files and the installer's
`HKCU\Software\GameCoreTech\BetterPullReview` key and kept the profile.
