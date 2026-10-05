---
schema: "implement-rfc.card.v1"
id: "BPR-1185"
title: "Finish documentation, examples and installed-package verification"
status: "Done"
rfcs: ["RFC-BPR-0053", "RFC-BPR-0060", "RFC-BPR-0061"]
depends_on: ["BPR-1182", "BPR-1183", "BPR-1184"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "qualification", "acceptance": ["RC53-10", "RC60-01", "RC60-02", "RC60-03", "RC61-01", "RC61-10"], "pack_alias": "PACK-CYCLE-43", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC53-10","outcome":"pass","revision":"88bc1a82f","evidence":{"kind":"observation","detail":"Product contract, system architecture, domain, Git, ledger, round-trip, AI, threat and corporate documents record the built local scope, disclosure channels, the one new GitHub write (new branch and pull request on github.com after a confirmed plan) and unchanged GitHub authority; every label read from the Qt source and every limit from the enforcing code; DOCUMENTATION_OK."}},{"criterion":"RC60-01","outcome":"pass","revision":"2f2eef6f5","evidence":{"kind":"command","detail":"macOS only. verify_release_package.py RELEASE_PACKAGE_OK for full (42 files), agent-free and agent-free automation-off (41 files, no MCP helper); SPDX sourceInfo names the absent families; installed_cycle_package_course.py: no child, socket or canary in any launch; negative control full-as-agent-free fails 'MCP helper present'."}},{"criterion":"RC60-02","outcome":"pass","revision":"2f2eef6f5","evidence":{"kind":"command","detail":"macOS only. INSTALLED_CYCLE_PACKAGE_COURSE_OK: a bpr-made cycle (R1, CRLF note, F1) reads back byte for byte after full, agent-free, automation-off and full packages ran; BPR-1182's native two-round course and owner GUI round trip reused, their product paths unchanged."}},{"criterion":"RC60-03","outcome":"pass","revision":"2f2eef6f5","evidence":{"kind":"command","detail":"macOS only. The automation-off package binds activate and no tourcmd endpoint and the stored cycle reads back unchanged after it and after the full package again; negative control agent-free-as-automation-off fails 'owner command endpoint bound'."}},{"criterion":"RC61-01","outcome":"pass","revision":"88bc1a82f","evidence":{"kind":"observation","detail":"This card's share: the guide and README document File > Open Project..., Home's Review Cycles section and the Local cycle title exactly as accepted in BPR-1157-home-section-v002 and BPR-1157-local-identity-v001 (reused)."}},{"criterion":"RC61-10","outcome":"pass","revision":"88bc1a82f","evidence":{"kind":"observation","detail":"This card's share: no new document claims a saving; BPR-1184's three platform reports and the owner's steering verdict (accepted without counts) stand as recorded."}},{"criterion":"examples","outcome":"pass","revision":"88bc1a82f","evidence":{"kind":"command","detail":"cycle_cli_documentation passes against the tests-disabled agent-free bpr (18 commands, 648 option probes, 13 provoked codes, 4 examples, 6 altered pages refused) and under ctest in a tests-enabled tour-cli graph with cycle_cli_public_path, skill_bpr_cycle_package, tour_cli_public_entry and test_network_isolation_manifest, 5/5; it failed on the pre-change bpr at the bare-bpr help claim."}}]
---

Pack alias: **PACK-CYCLE-43** (live `BPR-1185`, allocated at import).

# BPR-1185: Finish documentation, examples and installed-package verification

## Outcome and bounded work

Update current product/domain/git/ledger/round-trip/AI/security/storage/build docs and user help. Publish exact supported CLI/schema/skill/capability guidance and limitations. Re-run one integration-focused package course, not every historical project test indiscriminately.

## Acceptance and direct evidence

Native documentation structure check, runnable help/examples, installed agent-free/automation-off evidence, complete per-criterion ledger with open gates explicit and no accidental changes to settled predecessor status.

Criteria: RC53-10, RC60-01, RC60-02, RC60-03, RC61-01, RC61-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Input from BPR-1182 (2026-10-04): native runs owed, not gating

[[BPR-1182]] closed with these runs recorded as owed, at the integrator's
direction. They belong with this card's installed-package course:

- Windows (`bpr-main-win`): the six `dev` rows, the five agent-free rows,
  the native two-round course on the desktop, and the agent-free WiX MSI
  installed over a full one and back with the profile's cycles reopening.
  The exact commands are in the ledger's BPR-1182 section of
  [RFC-BPR-0053-0061.md](../../verification/RFC-BPR-0053-0061.md).
- Linux: the `xcb` two-round course once the owner clears the shared WSLg
  desktop, and the agent-free DEB on an Ubuntu 24.04 station.
- macOS: the desktop and editor rows on cocoa, which need the keyboard
  (nobody else using the machine).

## Result (2026-10-04)

Done at candidate `88bc1a82f`; the macOS packages at `2f2eef6f5`, whose
desktop inputs the candidate does not change. The full record, with the
per-criterion index and the station requests, is the BPR-1185 section of
[RFC-BPR-0053-0061.md](../../verification/RFC-BPR-0053-0061.md).

- Documentation: the product, architecture, domain, Git, ledger, round-trip,
  AI, threat, storage, build and corporate documents describe Review Cycles as
  built, and there is a new reviewer's guide,
  [Review Cycles workflow](../../ux/review-cycles-workflow.md).
- Examples: `cycle_cli_documentation` runs `cycle-cli.md` against the built
  `bpr`. It checks the commands, options, exit codes, limits, help and
  examples, and six altered pages must fail. `bpr` alone now names both
  command families.
- Installed packages: full, agent-free and agent-free automation-off each
  print `RELEASE_PACKAGE_OK`, and `installed_cycle_package_course.py` passes on
  Cocoa with two negative controls.
- Criteria: RC53-10 passes. RC60-01, RC60-02 and RC60-03 pass on macOS.
  RC61-01 and RC61-10 pass for this card's share. No attended session was
  needed.
- Finding for a follow-up card: profile purge refuses a profile holding
  `publication-git-v1/` (incomplete inventory). It is read from the code and
  documented in the storage guide. A card must be allocated on `main`.
- Owed, not gating: the Windows and Linux items above, the new row on both
  stations, and the macOS Cocoa desktop and editor rows.

**Note: Linux run at `15bb8c921` (holds `0eda62217`; station `bpr-main-linux`,
WSL2, GCC 15.2.0, the `tour-cli` preset, Release, `build/bpr1185-rows`; no
remote mutation).** Run exactly as written, `app_bpr` built but all 5 rows
were "Not Run": ctest could not find
`build/bpr1185-rows/better_pull_review_network_sandbox`, because the
`--target` list does not build `tool_better_pull_review_network_sandbox`
([[BPR-1180]]'s request had the same gap). With that target added, 5 of 5
passed: `cycle_cli_documentation` (3.7 s), `cycle_cli_public_path` (5.9 s),
`skill_bpr_cycle_package` (3.9 s), `tour_cli_public_entry` and
`test_network_isolation_manifest`. The build printed one
`-Wstringop-overread` warning from the vendored SQLite amalgamation
(`_bpr/sqlite/3.53.4/sqlite3.c:36482`), third-party code that does not fail
the build. `build/bpr1185-rows` was removed. Done is not reopened.

**Note: Windows run at `9c717f15a` (holds `0eda62217`; station `bpr-main-win`,
Windows 11 Home 10.0.26200, MSVC 14.51, the `tour-cli` preset with
`-DBUILD_TESTING=ON`, Release, `build/windows-bpr1185-rows`, targets `app_bpr`
and `tool_better_pull_review_network_sandbox`; no remote mutation).** The
first run passed 4 of 5: `skill_bpr_cycle_package` ([[BPR-1170]]) failed, for
two Windows reasons, both now fixed in the test and its registration.

- `bpr capabilities` exited `0xC0000135` (DLL not found) with empty output.
  The skill test builds its child environment without `PATH`, so `bpr.exe`
  could not load `Qt6Core.dll`. `cycle_cli_documentation_tests.py` already
  keeps `PATH` on Windows, and `bpr_cycle_skill_tests.py` now does the same.
- Contained, the row then failed at "`.claude/skills/bpr-cycle` differs from
  the package; import it again". On this station that copy is the MASK
  deployment, a link into the MASK store under the user profile. The
  container reads it as an empty tree. Uncontained, the store's copy matches
  the package byte for byte. The row is now pinned `WINDOWS_UNCONTAINED` as
  `repository-content-outside-checkout`, the reason [[BPR-0873]] measured
  for the same link, with this measurement.

`cycle_cli_documentation`, run contained through the sandbox launcher, stops
at its first `bpr cycle open` (prepare, line 331) with "profile_unavailable:
The application root is unavailable or is not owner-private". That is
`cycle_cli_public_path`'s refusal. The placeholder in
`verify_test_network_isolation.py` now holds this measurement. With these
changes 5 of 5 pass: `tour_cli_public_entry`, `cycle_cli_public_path`
(27.7 s), `cycle_cli_documentation` (22.2 s), `skill_bpr_cycle_package`
(13.6 s) and `test_network_isolation_manifest`. The build printed no
`warning C` or `warning LNK` line.
