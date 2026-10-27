---
schema: "implement-rfc.card.v1"
id: "BPR-1788"
title: "The Windows installer installs to Program Files, per machine"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["windows"]
metadata: {"reservation": "docs/issues/_reservations/BPR-1788--BPR-1788--bd4960d1d771.json", "kind": "feature", "found_by": "owner chat, 2026-10-27", "related_rfcs": ["RFC-BPR-0014"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "db73e634db9d43b9995ffac1b3b5e4f5140e6cbc", "evidence": {"kind": "command", "detail": "tests/scripts/package_runtime_tests.py (31 tests OK): perMachine scope, ProgramFiles64Folder, install directory BetterPullReview, no LocalAppDataFolder; the remembered directory is read and written under the same HKLM key; the HKCU per-user search and its launch condition are present."}}, {"criterion": "AC2", "outcome": "pending", "revision": "db73e634db9d43b9995ffac1b3b5e4f5140e6cbc", "evidence": {"kind": "command", "detail": "Not run: a native Windows MSI build (windows-app-release, package.py --generator WIX) and install as an ordinary user with UAC approval, landing in C:\\Program Files\\BetterPullReview, plus the refusal with an old per-user copy present. No Windows build host was reachable from this session."}}]
---

# BPR-1788: The Windows installer installs to Program Files, per machine

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

## Built

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

- AC1: the package configuration and patch carry the per-machine route
  (script tests).
- AC2 (pending): a native MSI build installs to Program Files and refuses
  over an old per-user copy.

## Not verified

- No native Windows build or install ran: the WiX patch's `Launch` element
  and the HKLM registry value were not compiled by WiX here.
