---
schema: "implement-rfc.card.v1"
id: "BPR-1789"
title: "The Windows desktop app opens no console window"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["windows"]
metadata: {"reservation": "docs/issues/_reservations/BPR-1789--BPR-1790--55a3d3d1a1c6.json", "kind": "defect", "found_by": "owner chat, 2026-10-27", "related_rfcs": ["RFC-BPR-0014"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "9e4830bc93c9323c74fbec4e5c319f99e0638478", "evidence": {"kind": "command", "detail": "tests/scripts/package_runtime_tests.py test_windows_desktop_app_opens_no_console: app_better_pull_review is WIN32_EXECUTABLE with /ENTRY:mainCRTStartup under MSVC; app_bpr is not. architecture_boundary_tests.py (159) pass."}}, {"criterion": "AC2", "outcome": "pending", "revision": "9e4830bc93c9323c74fbec4e5c319f99e0638478", "evidence": {"kind": "command", "detail": "Not run: a native Windows build launched from Explorer or the Start menu shows the window and no console; a course that redirects stderr still reads its markers. No Windows build host was reachable from this session."}}]
---

# BPR-1789: The Windows desktop app opens no console window

## Request

The owner, 2026-10-27, with a photograph of a console window titled with the
install path and showing `AGENT_WORKSPACE_RECOVERY_REFUSED:path=omitted:repair-required`:

> When I launch the app on Windows I also get this console window I don't
> think that should happen

## Cause

No target set `WIN32_EXECUTABLE`, so `app_better_pull_review` linked with the
console subsystem and Windows opened a console for it.

## Built

- `src/app/CMakeLists.txt`: the desktop app is `WIN32_EXECUTABLE`; under MSVC
  it links `/ENTRY:mainCRTStartup`, so the CRT still calls `main()` and
  startup is unchanged. `bpr` (`app_bpr`), the command-line tool, stays a
  console program.
- The app's terminal diagnostics already write through
  `GetStdHandle(STD_ERROR_HANDLE)` and drop a record they cannot deliver, so
  with no console they are dropped quietly, and a parent that redirects
  stderr still receives them.

## Not verified

- No native Windows build or launch ran here.
- The printed refusal is its own problem: BPR-1790.
