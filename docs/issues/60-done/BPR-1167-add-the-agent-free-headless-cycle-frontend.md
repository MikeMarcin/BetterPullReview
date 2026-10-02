---
schema: "implement-rfc.card.v1"
id: "BPR-1167"
title: "Add the agent-free headless cycle frontend"
status: "Done"
rfcs: ["RFC-BPR-0058"]
depends_on: ["BPR-1146", "BPR-1158", "BPR-1159", "BPR-1164"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "external-consumers", "acceptance": ["RC58-01", "RC58-02"], "pack_alias": "PACK-CYCLE-25", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC58-01","outcome":"pass","revision":"2a79d08eb93ce5f04a5c3e692a0077b1ef73ccd7","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64: cycle_cli_public_path runs every bpr cycle command as a real process with no HOME, account, provider, credential or desktop and an impossible QT_QPA_PLATFORM (open/capture R1, notes, review, freeze, export to file/bundle/stdout, recapture, compare, response import); the same course passes against the tests-disabled TourCLI install (otool: QtCore and QtNetwork only) directly and under the network-deny sandbox; the terminal composes the desktop's runtime/store/workspace source/export destination; a profile another bpr holds is refused (capability_unavailable) and nothing is written. Negative controls fail (second writer, export ignoring policy)."}},{"criterion":"RC58-02","outcome":"pass","revision":"2a79d08eb93ce5f04a5c3e692a0077b1ef73ccd7","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Default output is prose (asserted not JSON); --json writes exactly one bpr.cycle.cli-result.v1 line and nothing on stderr; human refusals write only to stderr with a Next command; typed codes and exits asserted for unsupported_version, unsupported_option, unsupported_command, invalid_input, not_found, interaction_required, stale_revision, conflict, over_limit, capability_unavailable; without a terminal note/freeze/scripted review refuse and stray stdin is not read; pty: editor note saved, Enter freezes nothing (130), 'freeze' freezes. Negative controls fail (stdin read, stderr in JSON, freeze without revision, version 2 accepted, JSON by default)."}},{"criterion":"BPR-1167:boundary","outcome":"pass","revision":"2a79d08eb93ce5f04a5c3e692a0077b1ef73ccd7","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"59 of 59 rows in build/dev-tour-cli (dev + BPR_BUILD_TOUR_CLI=ON) incl. tour CLI, review cycle, desktop public path, architecture_boundaries and test_network_isolation_manifest; core-only 22 of 22; clang-format and clang-tidy clean on changed units; DOCUMENTATION_OK. Linux and Windows runs owed (commands in the ledger)."}}]
---

Pack alias: **PACK-CYCLE-25** (live `BPR-1167`, allocated at import).

# BPR-1167: Add the agent-free headless cycle frontend

## Outcome and bounded work

Extend the actual shared bpr runtime/command registry with local open/show/capture/note/review/freeze/export/compare/response operations. Preserve Qt Core if needed but no GUI/display. Implement useful human output, explicit versioned JSON and noninteractive behavior.

## Acceptance and direct evidence

Tests-disabled CLI install, empty environment with no desktop/provider, human versus JSON/stdout/stderr cases, stdin/editor handling, unsupported version and no credential or browser bootstrap for local operations.

Criteria: RC58-01, RC58-02.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Implemented — macOS, 2026-10-02

At `2a79d08eb` (tested as `0fcb6c4d1`, identical source). `bpr cycle` offers
`capabilities`, `list`, `open`, `show`, `capture`, `note`, `review`,
`freeze`, `export`, `compare` and `response import` from the same non-GUI
`bpr` executable as `bpr tour`, on the desktop's own Review Cycles runtime,
store, workspace source and export destination. Human prose by default; one
`bpr.cycle.cli-result.v1` document with `--json`; typed errors with fixed
exit codes and a next command; nothing interactive without a terminal; one
writer per profile. Owned paths, the compatibility decisions, the 59-row
pass, the tests-disabled installed run and ten negative controls are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under "BPR-1167:
the agent-free headless cycle CLI"; usage is in
[the developer page](../../development/cycle-cli.md).

RC58-01 and RC58-02 pass on that evidence. `round read`, grants and desktop
forwarding stay with BPR-1169 and BPR-1168.

## Owed platform runs

Not gating. Linux and Windows configure a second binary directory with
`-DBPR_BUILD_TOUR_CLI=ON`; the exact commands are in the ledger section. If
Windows contained refuses `cycle_cli_public_path` at the store, pin it
`WINDOWS_UNCONTAINED` with the measured line.

## Linux: bpr-main-linux, 2026-10-02

At `29b243f07` (includes `2a79d08eb`), on WSL2 with GCC 15.2 and Qt 6.11.2 from the
CPM cache `build/.cpm-cache`. Nothing was changed. No remote mutation.

**Command 1 (`build/dev-tour-cli`, `BPR_BUILD_TOUR_CLI=ON`).** The build exits 0
with 0 `warning:` lines; the configure prints only CMake's
`uninitialized variable 'QT_FEATURE_…'` notices from Qt's own config. The
selection passes **14 of 15** in 33.74 s:

- `tour_cli_*` (5 rows) and `skill_pr_tour_package`
- `core_review_cycle_{round,feedback,workspace,share}`
- `review_cycle_export` and `review_cycle_workbench` (31.48 s)
- `application_review_cycle_desktop_public_path` and `architecture_boundaries`

**`cycle_cli_public_path` fails** (0.05 s) at its first command:

```
cycle_cli_tests.py:78 human: AssertionError: success writes nothing to standard error:
'Detected locale "C" with character encoding "ANSI_X3.4-1968", which is not UTF-8.
Qt depends on a UTF-8 locale, and has switched to "C.UTF-8" instead. …'
```

`environment()` in `tests/scripts/cycle_cli_tests.py` starts from an empty
environment (`QT_QPA_PLATFORM` and `NO_COLOR` only, plus four variables on
Windows), so there is no `LANG`. On Linux, glibc then runs in the C locale,
and Qt's core start prints that notice to standard error. macOS has no
equivalent notice. `tour_cli_tests.py` inherits `os.environ` (`LANG=C.UTF-8`
here), so its rows pass. The station's own locale is `C.UTF-8`.

Either side could own the fix. The course could pass a UTF-8 `LC_ALL`/`LANG` on
POSIX. Or `bpr` could keep that notice off standard error: a user whose
locale is C would see it on every `bpr cycle` success, against the contract
that a success writes nothing to standard error.

**Command 2 (`tour-cli` preset, `TourCLI` install, script).**

- The configure and build exit 0. The one `warning:` line is the vendored
  `_bpr/sqlite/3.53.4/sqlite3.c` `-Wstringop-overread`, the known third-party
  line.
- `cmake --install … --component TourCLI` exits 0.
- The script then fails at its first command with exit **127**:

```
build/tour-cli-install/bin/bpr: error while loading shared libraries: libQt6Network.so.6: cannot open shared object file: No such file or directory
```

The install log says `Set non-toolchain portion of runtime path of
".../tour-cli-install/bin/bpr" to ""`, and `readelf -d` shows no
RPATH or RUNPATH on the installed `bpr`. The build-tree `bpr` has RUNPATH
`…/build/.cpm-cache/bprqt/6.11.2/6.11.2/gcc_64/lib`.

`src/app/CMakeLists.txt` gives non-Apple builds only
`INSTALL_RPATH_USE_LINK_PATH TRUE`, and installs no Qt libraries. The Apple
branch installs QtCore and QtNetwork frameworks, the TLS plugin and `qt.conf`.
CMake adds a link directory to the install runpath only when it lies outside
the project, and this station's Qt cache is under the source tree
(`build/.cpm-cache`). So the Linux install is not self-contained here. It
needs a Qt that is installed beside the binary, or one the system loader
finds. The script's pseudo-terminal part was therefore not reached on Linux.
