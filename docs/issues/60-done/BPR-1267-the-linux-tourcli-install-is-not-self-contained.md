---
schema: "implement-rfc.card.v1"
id: "BPR-1267"
title: "Linux: the TourCLI install is not self-contained; the installed bpr cannot find Qt"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux, BPR-1167's Linux run, 2026-10-02","reservation":"docs/issues/_reservations/BPR-1266--BPR-1267--82b4691d051e.json","related_cards":["BPR-1167","BPR-1266","BPR-0961"]}
acceptance: [{"criterion":"The installed bpr runs with no Qt on the system loader's path; readelf/ldd show no library outside the prefix except system libraries","outcome":"pass","revision":"68b81a88b","evidence":{"kind":"command","detail":"RUNPATH [$ORIGIN/../lib]. ldd resolves Qt Core, Network and ICU 73 to bin/../lib and everything else to /usr/lib/x86_64-linux-gnu, with no not-found and no Qt-cache path. ldconfig -p lists no libQt6. A probe on the installed lib/ under env -i loads the OpenSSL TLS backend (active=openssl)."}},{"criterion":"python3 tests/scripts/cycle_cli_tests.py build/tour-cli-install/bin/bpr passes on Linux, its pseudo-terminal part included","outcome":"pass","revision":"68b81a88b","evidence":{"kind":"command","detail":"cycle-cli-public-path:pass against a fresh TourCLI install, with no skipped line, so the pty part ran. Before the fix the script exited 127 on libQt6Network.so.6."}}]
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

## Fix (bpr-main-linux, 2026-10-02, `68b81a88b`)

**Policy.** Linux uses a private Qt, not the system's. `packaging/package_runtime.py`
deploys the private Qt shared objects and plugins beside the application with
an `$ORIGIN` runpath and a `qt.conf`, and its notice reads "replace the
matching private Qt … shared objects and plugins … no global search path is
needed". [[BPR-0961]] is blocked on declared baselines and says nothing about
using system Qt. So the Linux TourCLI install mirrors the Apple branch.

**Change (`src/app/CMakeLists.txt`, Linux branch only; the Apple and other
branches are byte-identical).**

- `app_bpr`'s `INSTALL_RPATH` is `$ORIGIN/../lib`.
- `TourCLI` installs `libQt6Core.so.6`, `libQt6Network.so.6` and
  `libicu{i18n,uc,data}.so.73` into `lib/`. Each is the real file, renamed to
  its soname and found by one soname glob per library, which refuses to
  configure if it does not find exactly one. Qt's libraries carry
  `$ORIGIN`, so they find each other there.
- The OpenSSL TLS plugin goes to `lib/bpr/tls`, with `bin/qt.conf` set to
  `Plugins=../lib/bpr`.

The plugin's own `$ORIGIN/../../lib` runpath does not reach `lib/` from
there. It takes Qt Core and Network from the process that loads it, which
was shown in-process below. Moving it to `lib/tls`, where that runpath would
reach `lib/`, would collide with glibc's legacy `tls` library directory
under a `/usr` prefix.

| Check (Linux, WSL2, GCC 15.2, `tour-cli` preset, fresh `build/tour-cli-install`) | Result |
|---|---|
| `readelf -d bin/bpr` | `RUNPATH [$ORIGIN/../lib]` (before: none) |
| `ldd bin/bpr` | Qt Core, Network and ICU 73 resolve to `bin/../lib/`. Everything else is `/usr/lib/x86_64-linux-gnu` (glibc, libstdc++, GLib, libsecret and their dependencies). No `not found`, no reference to the Qt cache. |
| System Qt | `ldconfig -p` lists no `libQt6` |
| TLS from the installed layout | A probe linked to the installed `lib/` and run from a copy of the prefix (`env -i`) reports `backends=openssl active=openssl ssl=1` |
| `python3 tests/scripts/cycle_cli_tests.py build/tour-cli-install/bin/bpr` | **`cycle-cli-public-path:pass`**, including the pseudo-terminal part (no "skipped" line) and its `ldd` no-GUI-library check |
| Before the fix | The same script exited 127 on `libQt6Network.so.6` |
| `tour-cli` configure | 40 CMake warnings with and without the change, all Qt's own `uninitialized variable` notices |
| `build/dev` build; `architecture_boundaries`, `test_network_isolation_{manifest,structure_contract}`; `DOCUMENTATION_OK`; `git diff --check` | Clean, 3 of 3 pass |

Not touched: the Apple and Windows install branches.
