# RFC-0018 macOS Settings inventory and native entry paths

Captured 2026-09-20 on macOS 26.6.2 (25G83), arm64, Qt 6.10.3. This is
agent-driven native evidence, not a new human accessibility or visual verdict.
The accepted BPR-0683, Accounts and Appearance decisions remain unchanged.

## Candidates

| Candidate | Product source | Executable SHA-256 |
|---|---|---|
| Current dev, Debug, tests ON | `19a3dfd71324d59c3f0f6e4c23ff3ffc3c69faf8` | `b6ae3791a26e6278e92b492c519148c33b1ee546b46e2f69564b511e0d59ade9` |
| Current app-release, Release, tests OFF | same source | `df25f694275a20397b090896e5a2f3237d3d08c4b234d3474676701a8a101a78` |
| Comparable app-release, Release, tests OFF | `e93d9215045f06063b7cbdcbeadd61a25bda3e76` | `e7c15865cf7636dda67ee9337ac60e7ac7cb707c404a082ae93e84cb49a0566c` |

All three native courses passed and native Quit exited 0. Product source and
packaging remain unchanged from `19a3dfd7`; this closeout changes tests and
documentation. The comparable source is an unmodified `git archive` of the exact
Windows capture revision under ignored `build/`, not a checkout change. The
Linux capture is `9bbe3604`; its Settings shell, workbench, server card and
Appearance page are byte-identical to `e93d9215`. Both release bundles pass
`codesign --verify --deep --strict`. These locally signed arm64 bundles do not
establish notarized/universal distribution acceptance for RFC-0014.

Captures:

- [Current macOS release](BPR-0593-macos-settings-inventory/macos-app-release.json)
- [Current macOS developer build](BPR-0593-macos-settings-inventory/macos-dev.json)
- [Comparable macOS release](BPR-0593-macos-settings-inventory/macos-app-release-e93d9215.json)
- Existing [Windows evidence](BPR-0730-windows-settings-inventory.md) and
  [Linux evidence](BPR-0731-linux-settings-inventory.md)

## Native public course

[`macos_settings_inventory.swift`](../../tests/scripts/macos_settings_inventory.swift)
verifies the exact PID's executable path, hashes the image and checks native
accessibility permission. The app launches normally, with no fixture, test-root
flag or special Settings constructor. The course:

1. Invokes the real macOS `Preferences...` menu item, checks its Command-comma
   equivalent and requires `Settings — Default profile`.
2. Focuses the workbench with Settings visible, proving it is modeless. Native
   Command-comma must focus the same visible Settings identity, not a duplicate.
3. Sends Escape, requires Settings to hide, then reopens one labelled Settings
   window with Command-comma.
4. Invokes `Profiles > New Window`. From the same-profile sibling, Preferences
   reuses the visible Settings identity; native close and Command-comma reopen
   exactly one Settings. Closing the non-owning first workbench preserves it.
5. Uses Home/Down in the sidebar to reach all seven categories. Each heading
   must match the selected category before its native AX subtree is captured.
6. Closes/reopens Settings, invokes native Quit and requires process termination.

Qt recreates AX and WindowServer wrappers on hide/show. Native identity is
therefore compared while visible, not across hiding. `qt_settings_window`
checks the retained Qt object; `application_profile_multi_window_public_path`
checks the actual shared profile composition (current source passed, 78.15 s).
Retiring the page-owning workbench deliberately hides Settings before unmounting
its controls; retiring its sibling does not. Early capture-script assertions
were corrected to these existing contracts without changing the product. The
observer also pumps its run loop so `NSRunningApplication` refreshes exit state.

BPR-0730/0731 opened Settings through its toolbar button. Their captures do not
prove Windows/Linux native menu/shortcut/two-window behavior. That AC-01
target-native evidence remains pending; a Mac course is not a substitute.

## Isolation and reproduction

No sign-in, setting, update or maintenance action was invoked. Each candidate
used an empty mode-0700 home. A same-Qt probe verified `CFFIXED_USER_HOME`
redirects `QStandardPaths::AppLocalDataLocation` while `QDir::homePath` still
reports the real home. The sandbox independently denied all access to the
personal BetterPullReview data subtree. No `HOME` override was used.

All network inbound/outbound/bind operations were denied except Unix sockets
in one disposable mode-0700 directory, needed by profile activation. An initial
deny-all attempt reported `bind-refused`, opened no workbench and was terminated;
it is not passing evidence. No TCP endpoint was allowed. The socket directory
was empty after final Quit and removed with `rmdir`.

Recorded current release commands (run the capture from a second terminal):

```sh
env CFFIXED_USER_HOME=/Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018-macos-release-upmLRH TMPDIR=/private/tmp/bpr-settings-U2SPvd QT_QPA_PLATFORM=cocoa /usr/bin/sandbox-exec -p '(version 1) (allow default) (deny network-outbound network-inbound network-bind) (allow network-outbound network-inbound network-bind (subpath "/private/tmp/bpr-settings-U2SPvd")) (deny file-read* file-write* (subpath "/Users/jaeger/Library/Application Support/BetterPullReview"))' /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview
swift /Users/jaeger/.codex/worktrees/0152/BetterPullReview/tests/scripts/macos_settings_inventory.swift 42601 /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview macos-app-release 19a3dfd71324d59c3f0f6e4c23ff3ffc3c69faf8 /Users/jaeger/.codex/worktrees/0152/BetterPullReview/docs/verification/BPR-0593-macos-settings-inventory/macos-app-release.json
```

A repeat needs fresh `mktemp -d` directories, the new exact PID and a new output
filename; the capture refuses overwriting. Dev used PID 42518 and private root
`build/rfc0018-macos-dev-H39eqx`; comparable release used PID 45971 and
`build/rfc0018-macos-baseline-2Nvior`. Its bundle is under
`build/rfc0018-inventory-baseline-7BAJlH/build/app-release/src/app/BetterPullReview.app`.
These relative paths are within the assigned 0152 worktree.

After capture, the comparable bundle was moved unchanged to
`build/rfc0018-baseline-candidate/BetterPullReview.app`; its digest and strict
signature were rechecked. The task-owned extracted source/build directory was
removed because the architecture scanner also traversed that second source
tree. Its exact source remains recoverable from
`build/rfc0018-inventory-baseline.tar` or `git archive e93d9215`. The gate was
rerun after cleanup; no scanner exception or product change was introduced.

Build/check commands, from the applicable source root:

```sh
cmake --preset app-release -DBPR_DEPENDENCY_OFFLINE=ON
cmake --build --preset app-release --target app_better_pull_review -j 8
swiftc -typecheck tests/scripts/macos_settings_inventory.swift
python3 tests/scripts/settings_inventory_comparison_tests.py
```

The archive build also passed
`-DCPM_SOURCE_CACHE=/Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/.cpm-cache`.
Logs remain ignored as `build/rfc0018-{release,baseline}-{configure,build}.log`
and `build/rfc0018-native-{dev,release,baseline}-course.log`.

## Comparison

Current dev/release page trees are exactly equal by role/name/enabled/check
state, ordering and depth. Editable values and descriptions were not read.
Static text exposes its name through AXValue; native combo titles can contain
their default choice. No credentials or private repository data were present.

| Category | Current Mac nodes, both builds | Comparable Mac nodes | Comparable actionable controls, all three platforms |
|---|---:|---:|---:|
| General | 17 | 3 | 0 |
| Accounts & hosts | 14 | 15 | 4 |
| Reviews & queue | 58 | 58 | 35 |
| Appearance | 219 | 219 | 85 |
| Notifications | 8 | 8 | 4 |
| Agents & context | 12 | 12 | 3 |
| Storage & privacy | 9 | 9 | 4 |

AX flattens layout groups retained by UIA/AT-SPI; raw node counts do not prove
equality. The [comparison tests](../../tests/scripts/settings_inventory_comparison_tests.py)
map native button/check/combo/spin/edit roles, explicitly name observed combo
aliases and compare ordered control names, enabled states and check states.
All seven baseline pages match Windows/Linux; all five unchanged current pages
still match. Six tests pass, including negative controls removing a control and
flipping enabled state. Layout, headings and scrollbars are not counted as
adjustable settings; raw captures remain available for inspection.

The two later differences have explicit owners:

- RFC-0035 adds nine General updater controls after the Windows/Linux captures.
  Here eight are disabled under `untrusted_installation` with an explanation;
  only `View Release` is enabled. This is not trusted-install/updater acceptance.
  Windows/Linux use unsupported-platform capability; their older General
  captures predate the entire panel.
- BPR-0909/0910/0913/0915 add discovered CLI accounts and the owner's shared
  sign-in dialog. Per-server `Connect github.com with GitHub CLI` becomes
  page-level `Use GitHub CLI`. Both account inventories have four actionable
  controls; the new entry/order is explicitly checked. RFC-0015 still owns CLI
  platform policy and BPR-0933 the separate trust-dialog wording follow-up.

The missing comparable Mac capture and current Mac dev/release checks are now
complete. This is not a simultaneous current three-platform rerun of the
updater/shared-sign-in surfaces. Refreshing those Windows/Linux rows and their
native menu courses requires target-native surfaces outside this leaf's
exclusive macOS worktree assignment.

## Test-course diagnosis

The appearance restart contract is a required AC-03/05 check. Its first process
already passed. Its second used generic `--self-test`, entered the obsolete
inline CLI-card course and failed at stage 0. BPR-0915 already records that
non-fixture inventory limitation; BPR-0940 owns broader legacy composed-test
reconciliation. Restart now uses the existing composition-only probe with
`--network-denied`, composing the same stores and stopping before sign-in.
No production hook was added; every palette/persistence/zero-write assertion
remains. The full import/preview/cancel/commit/export/restart/FIFO contract
passes (26.42 s), not merely its restart process.

An extra empty-root composition control exits 0 and reports the non-imported
palette `#7fb0e8:#ffffff`, not the required persisted `#000000:#000000`. Its
CTest wrapper remains red: the separate CLI-label regex expects an old card
label, now empty. `qt_composed_github_scale_100` also remains red at the exact
BPR-0940 CLI-card assertion. These broad CLI presentation checks are not green
Settings evidence or regressions introduced by the test-only change. They are
bound to source `19a3dfd7` and the existing BPR-0915/BPR-0940 limitation, not an
untracked acceptance waiver.
