---
schema: "implement-rfc.card.v1"
id: "BPR-1267"
title: "Linux: the TourCLI install is not self-contained; the installed bpr cannot find Qt"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux, BPR-1167's Linux run, 2026-10-02","reservation":"docs/issues/_reservations/BPR-1266--BPR-1267--82b4691d051e.json","related_cards":["BPR-1167","BPR-1266","BPR-0961"]}
---

# BPR-1267: Linux: the TourCLI install is not self-contained; the installed bpr cannot find Qt

Found by bpr-main-linux on 2026-10-02, at `91f25b215` (see [[BPR-1167]]'s Linux note).

## Observation

`cmake --preset tour-cli && cmake --build --preset tour-cli && cmake --install
build/tour-cli --component TourCLI --prefix build/tour-cli-install` succeeds.
But the installed `bin/bpr` exits 127:

```
bpr: error while loading shared libraries: libQt6Network.so.6: cannot open shared object file: No such file or directory
```

The install log says it set the runpath of `bpr` to `""`, and `readelf -d`
shows no RPATH or RUNPATH. On non-Apple builds, `src/app/CMakeLists.txt` sets
only `INSTALL_RPATH_USE_LINK_PATH TRUE`, which adds link directories outside
the project. The station's Qt comes from the CPM cache under the source tree,
so its directory is left out. Nothing installs Qt with `TourCLI` on Linux.
The Apple branch installs QtCore and QtNetwork, the TLS plugin and a
`qt.conf`. So `cycle_cli_tests.py`, including its pseudo-terminal part,
never ran against an installed Linux `bpr`.

## Scope

- Unless packaging policy says Linux relies on system Qt, mirror the Apple
  branch on Linux: install QtCore, QtNetwork and the TLS plugins under the
  prefix's `lib/`, set `INSTALL_RPATH` to `$ORIGIN/../lib`, and add a
  `qt.conf` for the plugins.
- macOS and Windows are left untouched.

## Acceptance

- The installed `bpr` runs with no Qt on the system loader's path.
  `readelf`/`ldd` show no library outside the prefix except system libraries.
- `python3 tests/scripts/cycle_cli_tests.py build/tour-cli-install/bin/bpr`
  passes on Linux, its pseudo-terminal part included.
