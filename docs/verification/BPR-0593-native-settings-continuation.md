# RFC-0018 native Settings continuation, 2026-09-21

## Owner-amended closeout r2 — 2026-09-21

The owner explicitly directs that the Windows Defender/security disposition
and Linux native focus/lifetime limitation become follow-up cards, and that
RFC-BPR-0018 close against the amended accepted scope. The
[bounded mandate](../rfcs/plans/ux-redesign-pack-0028/owner-closeout-r2.md) and
[current criterion ledger](../issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist)
record this acceptance change. It does not supply any missing observation.

| Transferred obligation | Live disposition | Preserved result |
|---|---|---|
| Windows security disposition, full native AC-01 course and current AC-11 inventory | [BPR-0974](../issues/60-done/BPR-0974-dispose-windows-defender-finding-and-complete-native-settings.md), Ready (was Blocked, `security-owner-disposition`) | Defender quarantined the exact candidate. Native prefix only; no complete course/current inventory and no false-positive verdict. 2026-09-25: the owner chose to lower the image's static signals and go ahead, with Authenticode signing later (decision `5c30287a`, on the card). |
| Linux full native focus/shortcut/window-lifetime AC-01 course | [BPR-0975](../issues/60-done/BPR-0975-complete-linux-native-settings-focus-and-lifetime-course.md), Done by owner amendment (was Parked, `human-review`; Blocked, `window-manager-activation`; and before that `native-focus-surface`) | Failure reproduced in a separate bare-Qt control. Current native inventory-only pass remains narrower evidence, not a lifetime pass. 2026-09-23: the focus failure is attributed to WSLg's window manager and the controls pass through native RAIL activation. The full course now stops where that window manager refuses the app's own activation of Settings ([evidence](BPR-0975-linux-native-settings.md)). 2026-09-24: WSLg's compositor grants no activation request an application can make; the owner chooses between a WSLg-only remap in the product and waiting for the upstream change ([diagnosis](BPR-0975-linux-native-settings.md#why-wslg-refuses-activation-2026-09-24)). 2026-09-25: the owner accepted WSLg's refusal with no product change; AC2 and AC3 are amended to the diagnosis course, and the recheck is [BPR-1122](../issues/50-blocked/BPR-1122-wslg-refuses-application-window-activation-so-a-reused-settings-window-cant-take-focus.md), Blocked on `upstream-wslg`. |

These standalone cards (`rfcs: []`, explicit origin and r2 metadata) are not
dependencies of the amended closeout. The original RFC native requirements,
candidate hashes, failed courses and exact ready commands remain unchanged
below. Other acceptance rows and existing human verdicts are not amended.
Full RFC-0038 release qualification is neither required anew nor accepted here.

The r2 slice has no product/test changes. It reuses the exact corrected product
`2d3bdbdf` and test/observer upkeep `e7843405`. Amendment/follow-up commit
`eadf48af` precedes the shared claim `95e64b4c`. The canonical main reservation
`97067055` is incorporated as `bbba8973`, preserving counter and receipt.

| R2 executed check | Result and exact limit |
|---|---|
| `ctest --preset dev --output-on-failure -j 1 -R '^(qt_settings_window(_scale_(125\|150\|200))?\|qt_settings_effect_boundary\|application_appearance_theme_customization_contract)$'` | 6/6, 27.78 s at bbba8973; full appearance contract 25.10 s. Unchanged product/test inputs, not a new Windows/Linux native course. |
| `ctest --preset dev --output-on-failure -j 1 -R '^application_workbench_composition_contract$'` | 1/1, 21.61 s at 95e64b4c. |
| `python3 tests/scripts/settings_inventory_comparison_tests.py` | 11 tests pass at bbba8973, including negative controls and explicit inventory-only scope. |
| `python3 scripts/verify_documentation.py --root .` and `python3 tests/scripts/verify_documentation_tests.py` | DOCUMENTATION_OK and 64 tests pass. The tests' intentional invalid-fixture diagnostics are expected; final terminal-state gate is repeated after finish. |
| Shared board validation, explicit `verify_closeout_card` and `git diff --check` | Valid 967-card board, all 14 canonical criteria/base present, clean diff; final post-finish receipts retained. |
| Read-only native station/lease probes | No owned apps/tasks/processes; both leases free. Windows image remains absent/quarantined; live roots and pre-existing dirty MASK locks unchanged. No application relaunched. |

Documentation commands use
`PYTHONPATH=/Users/jaeger/.agents/skills/implement-rfc/scripts` because the
checkout's managed helper deployment is missing; no global MASK repair was
performed. Generated evidence is
`build/rfc0018-r2-{focused,inventory,integration,documentation-tests,documentation-final,board-final,closeout-final,diff-final}.json`
and the `windows-quiescence`, `windows-lease`, `linux-quiescence` JSON receipts.
The per-criterion BPR-0593 ledger retains actual producing commits and earlier
native/human evidence. No broad full-suite or release pass is asserted.

Final helper finish, r2 binding and quiescence are recorded on BPR-0593.
The native observations below belong to r1;
their former blocking effect is superseded only by the owner's explicit r2
transfer, not by a successful rerun.

## Prior native continuation r1

Run `rfc-0018-0026-closeout-20260920`, segment `rfc-0018`, mandate `r1`;
sole leaf owner `/root/rfc0018`, milestone `close-rfc-0018`. The owner resumed
the same assignment after merge `86fa923e`, explicitly confirmed both remote
stations available, and permitted stale planning reconciliation. This is
agent-driven evidence, not a new human verdict or release approval.

**Result: partial, blocked.** RFC-0014's owner-amended foundation is accepted
at `b7eb3722`, exact handoff `61907e20`; that dependency is satisfied. Its
remaining native/security qualification belongs to RFC-0038. The present
Settings obligations are not waived: Windows' current candidate is quarantined,
and Linux's full native focus/lifetime course did not pass.

## Corrected entry behavior

The actual Windows package at product `67f5e514` opened Settings through the
View menu but native Ctrl-comma did not focus it. This is a negative control,
not merely source inference. `QKeySequence::Preferences` has no default Windows
or GNOME binding in Qt's [standard shortcut table](https://doc.qt.io/qt-6/qkeysequence.html#standard-shortcuts).
The old `ApplicationShortcut` scope could also make two workbenches' actions
ambiguous. `2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273` now binds
Ctrl/Command-comma explicitly with `WindowShortcut`, preserving the invoking
workbench/profile. No host, credential, storage or provider authority changes.

The new two-workbench Qt key-event regression and exact binding/context checks
pass at four scales. The test uses `activateWindow()` instead of the deprecated
Qt 6.11 `QApplication::setActiveWindow`. The plan's bounded closeout scope now
explicitly includes correcting Settings entry defects exposed by its required
native courses; no exit criterion changed.

## Candidates and observations

All current product candidates below use exact source `2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273`
and Qt 6.11.2. Native observers are external to the tests-disabled application.

| Candidate | Executable SHA-256 | Actual result |
|---|---|---|
| macOS arm64 `app-release`, Release/tests OFF | `cff115f8c987c7a94f89667b59cd69b46ad833cd5500cfb06f8710cf13bedeb7` | All 11 native Preferences/Command-comma/modeless/singleton/sibling/close/reopen/category/Quit observations pass; process exit 0; strict deep signature verification passes. This is a locally signed app bundle, not notarized/universal release qualification. |
| Windows x64 MSVC 19.51, Release/tests OFF/distribution ON, staged runtime closure | `341e25b20111f48b7882d8c0a288de477cbcfd41eefc24b3bbbf861f1281e768` | Menu/Ctrl-comma/modeless/Escape/reopen prefix completed, then the course stopped at a missing Profiles menu. The following launch found no executable. No full course or inventory pass. Digest is from the already-generated SPDX file, not a post-quarantine executable read. |
| Linux x64 GCC 15.2, Release/tests OFF, native WSLg XCB | `8234abccb4141bbf52c91c568b25917abd2e3125f46f14e1a0073ee285b7a51e` | Separate inventory-only course passes: native View/Settings, all seven showing pages, native Settings/workbench close, process exit 0. This supplies the same tests-OFF release-graph inventory scope previously accepted on BPR-0731. Full keyboard/modeless/sibling course fails at native focus; no distribution qualification is claimed. |

Captures are immutable observations for these candidates:

- [macOS current capture](BPR-0593-native-settings-continuation/macos-app-release-2d3bdbdf.json)
- [Linux current inventory-only capture](BPR-0593-native-settings-continuation/linux-native-inventory-2d3bdbdf.json)
- [Earlier comparable three-platform baseline](BPR-0593-macos-settings-inventory.md),
  retained unchanged; it does not cover later rows.

## Native station checks and limits

### Windows

Configured SSH `aito-main-win` reached JaegAW and the native station
`stn-a5155427` / `bpr-win`, `C:/code/git/bpr-windows`, branch `windows-lane`,
HEAD `b9cf4edb316dceaf8c7ea75f4c7321ce353651e8`. The observer ran in interactive
session 1 on input desktop `Default` through an exact limited scheduled task.
This was actual connection and execution, not an unavailable-station assumption.

Windows Defender reported `Trojan:Win32/Bearfoos.A!ml`, threat ID `2147731250`,
on the task-owned staged executable at `2026-09-22T04:07:40.551Z` (the station
displayed September 21, 21:07:40). A subsequent read of the producer image was
also refused; its detection time is `2026-09-22T04:09:45.239Z`. Both actions
report success. `IsActive=false` and `DidThreatExecute=false` are Defender's
fields, not proof that no earlier application prefix ran. The staged image is
absent. Both affected paths are under
`C:/Users/mikem/bpr0018-native-280167ac`:

- `installed/better_pull_review.exe`
- `b/src/app/better_pull_review.exe`

No quarantine restoration, exclusion, Defender disablement, alternate-image
execution or rebuild-around was attempted. The missing-menu observation is
not classified as a product defect while the security intervention overlaps
the course. Security-owner disposition and a cleared exact candidate are
required before further Windows execution. This finding is relevant to existing
[[BPR-0960]]/[[BPR-0962]] under RFC-0038; this Settings task does not accept or
repair that broader release-security scope.

The Windows launch used the shipped `--render-evidence` flag **without an
output argument**. Its presence creates the existing owned temporary profile;
its absent value schedules no automated render loop. The ordinary public UI
was driven externally and `AUTOMATED_PROFILE_PRIVATE_OK` was checked. This is
not a claim of an ordinary no-argument launch or a newly introduced test hook.
No sign-in, update, export, maintenance or live-provider action was invoked.

### Linux

The configured `stn-3f7fcc92` / `bpr-linux` station is `/home/mmarcin/bpr-linux`,
branch `linux-lane`, HEAD `1aabc2f3bbd619a8e26938b8c2e909e4a7058f3e`, Ubuntu
26.04 x86_64 under WSL2. The owned source clone is clean at the exact candidate.

The unmodified Clang 21 build fails new missing-designated-initializer diagnostics;
demoting that diagnostic exposes an existing recursive JSON/std::pair incomplete
type error with libstdc++ 15. GCC 15.2's unchanged build fails
`release_maintenance_files.cpp:632` on `-Werror=ignored-attributes` for the
`closedir` function-pointer deleter. The native inventory candidate demotes
**only** that GCC diagnostic with `-Wno-error=ignored-attributes`; source is
unchanged and the warning is retained. This is not a clean compiler-policy pass.
The earlier accepted BPR-0731 inventory likewise used its `app-release`
Release/tests-OFF graph and documented diagnostic demotions, not a qualified
DEB/RPM. This continuation preserves that inventory meaning. It does not add
full RFC-0038 distribution qualification as a new RFC-0018 exit or dependency.

The complete course's last failure is reproducible: target workbench XID
`6291474` is mapped (`WM_STATE=[1,0]`), but X11 focus stays on Settings XID
`6291484`. The observer refuses to send keys to the wrong target. Bounded
XSetInputFocus/XSync, window-manager activation, exact X11 edge hit-testing and
an interactive native WSLg RAIL focus bridge were tried. The bridge verified
`Default` input desktop, matched only exact owned captions including WSLg's
`(Ubuntu)` suffix, and still could not establish foreground focus. No display,
security or window-manager policy was changed. This is a **failed native
course**, not a successful AC-01 course. macOS's pass cannot replace it.

A separate [two-QWidget control](../../tests/scripts/linux_settings_focus_control.cpp),
compiled against the exact same Qt SDK and run on the same native XCB display,
reproduces the focus failure with **no BetterPullReview code**: A stays mapped
but focus remains on B. After native close of B, focus of A passes; native close
of A exits 0. The same observer and owned-PID checks are used. This isolates a
station/Qt/observer focus limitation from Settings' application composition;
which of those layers owns it is still unresolved. Its output is
`BARE_QT_TWO_WINDOW_FOCUS:FAIL`, `BARE_QT_POST_CLOSE_FOCUS:PASS`,
`BARE_QT_NATIVE_CLOSE_EXIT:0`, not a passing Settings claim. All control children
are waited and the guard reports unchanged live files.

The independently run `--inventory-only` mode does not exercise or claim
modeless/shortcut/sibling acceptance. Every page switch requires exactly one
showing page with the requested category before capture. It passed with
`[21,26,66,334,12,15,24]` visible nodes and native close exit 0. Each launch used
a fresh mode-0700 owned directory and separate XDG data/config/cache/state/TMP
paths, with XCB and AT-SPI enabled only for that child. No `HOME` override,
authentication, setting-change gesture or maintenance action was used.

The actual `packaging/package_runtime.py::linux_baseline()` gate refuses this
station: `not a supported package-build baseline: ('ubuntu', '26.04'); use Ubuntu
24.04 or RHEL 10`. An earlier distribution configure also refused its default
`/usr/local` prefix; correcting that prefix alone would not satisfy the baseline
gate. No configured Ubuntu 24.04/RHEL 10 desktop or container was found; the
existing image inventory includes Ubuntu 22.04 and Debian bookworm, not the
declared baselines. Nothing was provisioned or changed. This is the existing
[[BPR-0961]] qualification limit, not a reason to relabel this binary as a
qualified distribution, restore RFC-0014 as a canonical blocker, or deny its
direct release-graph inventory observation. Linux's current inventory refresh
is complete within the existing BPR-0731 scope; its AC-01 native lifetime
course remains unaccepted independently.

## Refreshed inventory comparison

| Category | Mac nodes | Linux nodes | Compared controls, Mac / Linux |
|---|---:|---:|---:|
| General | 17 | 21 | 9 / 9 |
| Accounts & hosts | 14 | 26 | 4 / 3 |
| Reviews & queue | 58 | 66 | 35 / 35 |
| Appearance | 219 | 334 | 85 / 85 |
| Notifications | 8 | 12 | 4 / 4 |
| Agents & context | 12 | 15 | 3 / 3 |
| Storage & privacy | 19 | 24 | 11 / 11 |

Native layout wrappers are not settings. The explicit role aliases include
GObject AT-SPI `button`/`spin button` alongside the older `push button`/`spin box`.
Names, enabled/check states and ordering compare directly. Six pages match;
Accounts has one documented capability difference: the Linux composition's
`cli_read_offered_here()` is false, so no `Use GitHub CLI` dispatch/button is
offered. The comparator removes only that exact expected Mac tuple for this
one comparison; its general normalizer never deletes a control.

General's nine updater controls match; its explanation differs between Mac's
untrusted installation and Linux's unsupported platform. Storage adds the seven
RFC-0014 diagnostic/removal buttons. Six Mac pages retain their previous
controls. The Settings guide and authority inventory now describe these owners,
exact preview/confirmation/commit boundaries and platform differences. No new
maintenance or updater acceptance is claimed from their visibility.

## Verification and reproducing the remaining work

At the corrected product plus the Qt 6.11 test-call upkeep:

- Four Settings scales plus effect boundary and full appearance
  import/preview/commit/export/restart/FIFO contract: **6/6, 27.18 s**.
- Application workbench composition: **1/1, 20.90 s**.
- Core/application architecture boundaries, changed C++ formatting, native
  Mac strict signature, Python observer compilation: pass.
- Inventory comparison: **11 tests pass**, including missing-control and
  changed-enabled-state negative controls and explicit inventory-only limits.
- The final Linux inventory-only rerun has byte-equivalent structured output
  to the committed capture; its process exits 0.
- Prior broad legacy CLI failures remain the precise BPR-0915/BPR-0940
  linkage in the earlier Mac report. They are not silently counted as green
  or newly re-run by this native continuation.

Exact local checks, from the assigned checkout:

```sh
ctest --preset dev --output-on-failure -j 1 -R '^(qt_settings_window(_scale_(125|150|200))?|qt_settings_effect_boundary|application_appearance_theme_customization_contract)$'
ctest --preset dev --output-on-failure -j 1 -R '^application_workbench_composition_contract$'
cmake --build --preset dev --target check_boundaries
python3 tests/scripts/settings_inventory_comparison_tests.py
codesign --verify --deep --strict build/app-release/src/app/BetterPullReview.app
```

The reusable external observers are
[`windows_settings_inventory.ps1`](../../tests/scripts/windows_settings_inventory.ps1)
and [`linux_settings_inventory.py`](../../tests/scripts/linux_settings_inventory.py).
They verify the supplied PID's image and refuse an existing output. The full
Linux mode stays failing; `--inventory-only` is explicitly weaker evidence.
Task-local launchers preserve isolation and bounded teardown. Ready commands
from this Mac, using their retained absolute paths:

```sh
# Linux: repeat the full course only on a usable native focus surface.
python3 /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_remote.py --linux /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_linux_guard.sh
# Optional native focus diagnostic: two bare Qt windows, no product/profile.
python3 /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_remote.py --linux /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_linux_control_guard.sh
# Windows: DO NOT run until security disposition supplies a cleared exact image.
python3 /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_remote.py /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_windows_guard.ps1
```

The Linux guard runs `/mnt/c/Users/mikem/bpr0018-native-280167ac/linux-course.sh`.
Its launcher creates a fresh private root and defaults to the full course.
For inventory-only diagnosis, invoke its `linux-launch.py --inventory-only`
under the same guard; never convert that output into lifetime acceptance.
For separate RFC-0038 Linux distribution qualification, first supply the declared native baseline and
produce/install the exact tests-OFF package with the `/usr` prefix and normal
diagnostic policy. Do not change `/etc/os-release` or suppress the baseline gate.

Ignored logs are `build/rfc0018-current-{focused,integration,boundaries}.log`,
`rfc0018-macos-corrected-course.log`, `rfc0018-windows-{build2,fixed-course,fixed-course2,defender}.log`,
`rfc0018-linux-{course21,inventory,inventory-final,focus-control,focus-control-final,baseline-gate,final-probe}.log` and
`rfc0018-windows-final-probe.json`. The diagnostic RAIL bridge's attempts are
`rfc0018-rail-bridge*.log`; bridge shutdown is not application acceptance.

## Ownership and preserved state

Both stations used `station_lease.py` and `qualification_guard.py` with owned
external roots; neither live branch was checked out, built in, fetched into or
committed. Before/after hashes of `.mask.lock.json`, `.mask.json`, station
identity and project config remain equal, including the pre-existing dirty
MASK locks. The guard is cooperative evidence, not an OS sandbox.

The sole owner retained inert candidate/source/build/evidence roots
`C:/Users/mikem/bpr0018-native-280167ac` and
`/home/mmarcin/bpr0018-native-280167ac`. The old RFC-0014 Windows package and
Qt root were read-only. All native app/observer children exited or were
terminated and waited after failed courses; both exact Windows scheduled tasks
were removed, and the RAIL bridge ended. The final probes report no owned apps
or tasks and free station leases. No review server, delegate, live GitHub/GHES
mutation, security-policy change or publication remains. No new attended review
was created: the existing Settings/Accounts/Appearance human verdicts stand;
the remaining Windows security and native-course failures are not new visual
approval questions.
