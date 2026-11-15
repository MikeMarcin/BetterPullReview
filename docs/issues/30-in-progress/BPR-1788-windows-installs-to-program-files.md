---
schema: "implement-rfc.card.v1"
id: "BPR-1788"
title: "The Windows installer offers Just me or Everyone"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["windows"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1788--BPR-1788--bd4960d1d771.json","kind":"feature","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0014"],"milestone_plan":{"id":"release-preparation-20261115","path":"docs/rfcs/plans/release-preparation-20261115/milestone-plan.json"}}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "db73e634db9d43b9995ffac1b3b5e4f5140e6cbc", "evidence": {"kind": "command", "detail": "tests/scripts/package_runtime_tests.py (31 tests OK): perMachine scope, ProgramFiles64Folder, install directory BetterPullReview, no LocalAppDataFolder; the remembered directory is read and written under the same HKLM key; the HKCU per-user search and its launch condition are present."}}, {"criterion": "AC2", "outcome": "pending", "revision": "db73e634db9d43b9995ffac1b3b5e4f5140e6cbc", "evidence": {"kind": "command", "detail": "Not run: a native Windows MSI build (windows-app-release, package.py --generator WIX) and install as an ordinary user with UAC approval, landing in C:\\Program Files\\BetterPullReview, plus the refusal with an old per-user copy present. No Windows build host was reachable from this session."}}]
---

# BPR-1788: The Windows installer offers Just me or Everyone

## Owner amendment, 2026-11-15

The owner replaces the everyone-only choice with a choice at installation time:
current user or everyone. For the default: “Do whatever is the standard normal
behavior.” Microsoft recommends a per-user fresh default for a dual-purpose
MSI. Just me installs under `%LOCALAPPDATA%\Programs\BetterPullReview` without
elevation; Everyone installs under `%ProgramFiles%\BetterPullReview` with UAC.
Both choices must be available to a standard user. An existing installation's
scope and directory are preserved; changing scope requires uninstall/reinstall.
The [preparation inventory](../../development/release-preparation-20261115.md)
records the source/native boundary and outstanding platform work.

## Request

The owner, 2026-10-27, verbatim:

> The windows installer is trying to install BPR to users app data local
> programs. That's dumb it should install to program files like a normal
> window application

## Grounded behavior before

`packaging/Install.cmake` set `CPACK_WIX_INSTALL_SCOPE perUser`,
`CPACK_WIX_ROOT_FOLDER_ID LocalAppDataFolder` and the directory
`Programs/BetterPullReview`, so the MSI installed to
`%LOCALAPPDATA%\Programs\BetterPullReview`. `packaging/windows/InstallRoot.xml`
remembered a custom directory under HKCU. No Windows code depends on that
location; only macOS has an update helper.

## Previous implementation (2026-10-27)

- Per-machine scope, `ProgramFiles64Folder`, directory `BetterPullReview`:
  `C:\Program Files\BetterPullReview`. Installing asks for administrator
  approval, as a normal Windows application does.
- The remembered directory is read and written under HKLM, so repair and
  uninstall find a custom location.
- An MSI cannot upgrade a product across install scopes. When the installing
  user still has a per-user copy from an earlier package (its HKCU value is
  present), the installer stops and asks for it to be uninstalled from
  Settings, Apps, instead of leaving two installations. The upgrade GUID is
  unchanged.
- The maintainers' packaging guide and `packaging/README.md` say so.

## Acceptance

- AC1: package configuration, native scope dialog, directory restoration,
  registry and Start menu use the dual-context route. Focused checks cover
  fresh default, both scopes, preserving old scope/custom directory, refusal
  when both markers exist, and source/documentation consistency.
- AC2 (pending): build with native CMake/WiX and validate MSI tables/ICE105;
  install as an ordinary Windows user in each scope (Just me without UAC,
  Everyone with credential/consent approval); verify Start menu/registry,
  same-scope old per-user/per-machine upgrades, custom-directory repair and
  uninstall, scope-change uninstall/reinstall, and no user-data removal.

## Not verified

- Previous source-only acceptance evidence in the frontmatter describes the October
  everyone-only implementation. It is historical and does not accept this
  amendment. No native MSI build, ICE validation, UAC interaction or install
  has yet run for the dual-context candidate.
