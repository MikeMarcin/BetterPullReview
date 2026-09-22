# BPR-0731 Linux packaged Settings inventory (RFC-BPR-0018 AC-11)

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-001-r2`, task
  `task-20260917-lnx-evidence`. Station `bpr-linux`, branch `linux-lane`.
  BPR-0799's evidence is pushed at `6d649ecd`.
- **State.** Both inventories are captured, and they are identical.
  - This file and its two captures are the Linux AC-11 evidence.
  - The card stays in Backlog, unmoved. Moving it rewrites links in
    [BPR-0593](../issues/30-in-progress/BPR-0593-close-rfc-0018.md) and BPR-0683,
    and this dispatch forbids BPR-0593 edits.
  - Linking this inventory from BPR-0593's AC-11 ledger row is main's step, as
    `4b56435e` did for BPR-0730.

**Station and task.** Station `bpr-linux` (JaegAW, `wsl2`, Ubuntu 26.04 x86_64,
branch `linux-lane`) ran task `task-20260917-lnx-evidence` in run
`run-20260916-stations`.

**Revision.**

- The product source is `9bbe3604a19416033e0543523a54d3fabab27b4a`, which is at
  or after `d1ba791f`. It is dispatched base `17f495fe` plus a one-line Linux
  compile fix in the agent supervisor.
- Both builds use GCC 15.2 with four diagnostic classes demoted from error to
  warning. Without that, neither Linux preset compiles at this revision.
  [BPR-0622's Linux evidence](BPR-0622-linux-native-evidence.md) lists those
  failures and the flags.

## Builds

| Build | Preset and configuration | Executable | SHA-256 |
|---|---|---|---|
| Packaged, tests disabled | `app-release` (Ninja, `CMAKE_BUILD_TYPE=Release`, `BUILD_TESTING=OFF`) | `build/app-release/src/app/better_pull_review` | `da30fc69b465b1bff053bc29451082e4c28311c1d45ded1b9e3eb81a0e7bf058` |
| Developer, tests enabled | `dev` (Ninja, `CMAKE_BUILD_TYPE=Debug`, `BUILD_TESTING=ON`) | `build/dev/src/app/better_pull_review` | `09c815c4c79ab0dda715df4edfb13fbec6438568af7559c3b726a5997927a563` |

**BPR-0804.** The Linux `app-release` graph compiles
`src/app/src/github_application.cpp` with no diagnostic under GCC 15.2 and
`-Werror`. The file is unchanged from `17f495fe`. The limitation: at `17f495fe`
with unmodified flags the build never reaches that translation unit, because
upstream libraries fail first.

## How the inventory was taken

The captures were taken by an agent from the AT-SPI accessibility tree over
D-Bus, not read by eye. No screenshot was used.

1. **Launch.** Each image was launched directly on the WSLg display
   (`DISPLAY=:0`, `WAYLAND_DISPLAY=wayland-0`, `QT_QPA_PLATFORM` unset), so Qt
   used its Wayland platform. It was not offscreen.
   - Each launch got its own empty scratch root: `XDG_DATA_HOME`,
     `XDG_CONFIG_HOME`, `XDG_CACHE_HOME` and `XDG_STATE_HOME` under a new
     directory.
   - `QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1` was set on that process only, so Qt
     publishes its tree on the session accessibility bus.
2. **Open Settings.** The capture located the process's accessible application
   by D-Bus connection PID. It found the `push button` named `Settings` in the
   `BetterPullReview — Default profile` frame and invoked its `Press` action.
   That opened `Settings — Default profile`.
3. **Select each category.**
   - For each `list item` of the `Settings pages` list, the capture posted a
     left button down and up to that row through the Settings window's WSLg
     `RAIL_WINDOW`. The position came from the row's AT-SPI window extents plus
     the window decoration offset.
   - `WM_LBUTTONDOWN`/`WM_LBUTTONUP` went through `user32!PostMessage`, so the
     physical cursor did not move.
   - AT-SPI `Selection.SelectChild` and the row's `Toggle` action change the
     selection but not the current row. The window switches pages only on the
     current row, so they could not be used.
   - After each click the capture checked that exactly one child of the
     `Settings page` layered pane was showing and carried the category's name.
     Every category was reached on the first click in both builds.
4. **Walk the page.** The capture walked the showing page's subtree.
   - **Visible tree.** Each node gives role, accessible name, enabled state, and
     checked or unchecked where the node is checkable. Nodes without the
     `visible` state are pruned with their descendants, as UI Automation omits
     them on Windows.
   - **Full walk.** A second walk including hidden nodes was compared too. It
     was not committed.
5. **Close.** The Settings window and then the workbench window were closed
   through their caption close (`WM_SYSCOMMAND`/`SC_CLOSE` posted to each
   `RAIL_WINDOW`). Both processes exited 0.

The normalized captures are
[`linux-app-release.json`](BPR-0731-linux-settings-inventory/linux-app-release.json)
and [`linux-dev.json`](BPR-0731-linux-settings-inventory/linux-dev.json). They
use the same page and node layout as BPR-0730's Windows captures, with the
`Settings page` pane at depth 0.

- **Packaged capture.** 2026-09-17T06:32:40Z to 06:33:04Z.
- **Developer capture.** 06:33:15Z to 06:33:40Z.
- **Secret Service.** The `login` collection read `(<false>,)` before each
  capture. Nothing signed in, and no GitHub request was made.

## Result

Both builds expose the seven categories in this order: General, Accounts &
hosts, Reviews & queue, Appearance, Notifications, Agents & context, Storage &
privacy.

| Category | Visible nodes, packaged | Visible nodes, developer | Differing nodes | Hidden nodes below the page (both builds) |
|---|---|---|---|---|
| General | 6 | 6 | 0 | 0 |
| Accounts & hosts | 28 | 28 | 0 | 68 |
| Reviews & queue | 66 | 66 | 0 | 38 |
| Appearance | 334 | 334 | 0 | 82 |
| Notifications | 12 | 12 | 0 | 1 |
| Agents & context | 15 | 15 | 0 | 2 |
| Storage & privacy | 13 | 13 | 0 | 1 |

- **No difference.** The packaged inventory and the developer build's are the
  same. The two capture files differ only in their `build` field.
- **Hidden nodes.** The full walks, including every hidden node, are also
  identical between the builds.
- **Consequence.** There is no platform capability row to explain,
  `docs/development/settings-authority-inventory.md` needs no change, and there
  is no defect to report.

### Beside the Windows inventory

BPR-0730's Windows captures were taken at `e93d9215`. Against them, every page
has the same node count, the same depths, the same enabled states and the same
check states. The roles correspond consistently: for example `Button` to
`push button`, `Text` to `label`, and `Group` to `filler`, or once to `panel`.
No control is present on one
platform and absent on the other. The 61 nodes whose names differ fall into two
classes. Both are values, not inventory.

- **Combo box names (10 nodes).** Qt on Unix exposes a combo box's current text
  as its AT-SPI name, where Windows exposes the accessible name. Examples:
  `Role: any` for `Queue role filter`, `Sans Serif` for `Interface font family`,
  `System` for `Dif motion`.
- **The theme on screen (51 nodes).** The Appearance page follows the platform
  colour scheme. On this host it shows Light, where the Windows capture showed
  Dark. Four theme-named nodes differ: the `Colours for Light` panel and label,
  `Edits change Light only…`, and the summary line. So do the 47 colour hex
  labels.

## Rows

These are the accessible names in the packaged build, in tree order, with their
AT-SPI roles. Unnamed layout nodes and the `Settings page` pane itself are
omitted. The combo box names are their displayed values, as described above.

### General

- filler: General
- label: General
- label: New windows open Home. Window restoration is not available in this build. Review navigation defaults are in Reviews & Queue; colours and fonts are in Appearance.

### Accounts & hosts

- filler: Accounts & hosts
- label: Accounts & hosts
- filler: Accounts and hosts
- label: One account per server. Credentials are never shown here.
- filler: github.com
- label: Host
- label: Server kind and transport
- label: Connection state
- label: ?
- label: Account
- label: Sign-in terms
- push button: Sign in to github.com
- push button: Connect github.com with GitHub CLI
- label: GitHub CLI trust warning
- check box: Show details for github.com (unchecked)
- label: Capabilities, rate limits and the last problem.
- push button: Add enterprise server

### Reviews & queue

- filler: Reviews & queue
- label: Reviews & queue
- filler: Reviews and queue
- label: Local behaviour only. Nothing here changes assignments or subscriptions on GitHub.
- filler: Review behaviour
- label: Modality rules
- push button: Reset modality rules
- label: A pull request opens in
- check box: A pull request opens in: Overview (checked)
- check box: A pull request opens in: Read (unchecked)
- check box: A pull request opens in: Last used (unchecked)
- label: Continue review enters
- check box: Continue review enters: Read (checked)
- check box: Continue review enters: Review (unchecked)
- label: Reading
- push button: Reset reading
- label: Mark a file read
- check box: Mark a file read: By hand (checked)
- check box: Mark a file read: When fully seen (unchecked)
- label: On return, compare
- check box: On return, compare: Whole PR (checked)
- check box: On return, compare: Since my last review (unchecked)
- check box: On return, compare: Since last upload (unchecked)
- label: Comments drawer opens on
- check box: Comments drawer opens on: All (checked)
- check box: Comments drawer opens on: Unread (unchecked)
- check box: Comments drawer opens on: Unresolved (unchecked)
- label: Drawers
- push button: Reset drawers
- label: Drawer controls
- check box: Drawer controls: Hidden (checked)
- check box: Drawer controls: Shown (unchecked)
- panel: Queue defaults
- combo box: Role: any
- label: Role
- combo box: Observer: any reason
- label: Observer reason
- combo box: Bucket: any
- label: Attention
- combo box: Repository: any
- label: Repository
- combo box: State: any
- label: Quiet reviews
- combo box: Sort: core priority
- label: Sort
- push button: Reset queue defaults
- label: Queue settings status
- label: Observer rules
- filler: Observer rules
- combo box: Saved observer rules
- combo box: repository
- text: Observer rule pattern
- check box: Exclude matching rule (unchecked)
- push button: Add local observer rule (disabled)
- push button: Remove selected observer rule (disabled)
- label: Saved repositories
- filler: Saved repositories
- combo box: Saved repositories list
- text: Repository to save
- push button: Save repository (disabled)
- push button: Remove saved repository (disabled)

### Appearance

- filler: Appearance
- label: Appearance
- filler: Appearance settings
- label: Changes apply as you make them and show in the review window behind this one.
- panel: Theme
- label: Theme
- push button: Reset appearance mode
- filler: Theme
- panel: Type
- label: Type
- push button: Reset type
- label: Interface font
- push button: About interface font
- combo box: Sans Serif
- label: Interface size
- push button: About interface size
- spin box: Interface font size, in points
- label: Interface weight
- push button: About interface weight
- spin box: Interface font weight
- label: Line height
- push button: About line height
- spin box: Interface line height, percent
- label: Code font
- push button: About code font
- combo box: monospace
- label: Code size
- push button: About code size
- spin box: Code font size, in points
- label: Code weight
- push button: About code weight
- spin box: Code font weight
- label: Tab width
- push button: About tab width
- filler: Tab width, in spaces
- check box: Tab width 2 (unchecked)
- check box: Tab width 4 (checked)
- check box: Tab width 8 (unchecked)
- label: Ligatures
- push button: About ligatures
- check box: Ligatures (unchecked)
- label: Bold syntax emphasis
- push button: About bold syntax emphasis
- check box: Bold syntax emphasis (checked)
- label: Italic syntax emphasis
- push button: About italic syntax emphasis
- check box: Italic syntax emphasis (checked)
- filler: Preview
- label: Preview
- label: Files
- label: 12 of 40 read
- label: Engine/Source/Runtime/Renderer
- text: Code preview
- label: Interface 9 pt, weight 400, line height 100 %. Code 9 pt, weight 400, tabs of 4, ligatures off, bold and italic emphasis on. Diff, selection and current-line colours from Light.
- panel: Colours for Light
- label: Colours for Light
- push button: Import theme
- push button: Export saved theme
- push button: Reset all colours
- label: Edits change Light only. Each theme keeps its own colours.
- filler: Colours
- text: Filter roles
- filler: Colour roles
- filler: Surfaces and text
- label: Surfaces and text
- label: 7
- push button: Application surface
- label: Application surface
- label: #f6f7f9
- push button: Application text
- label: Application text
- label: #1f2328
- push button: Code background
- label: Code background
- label: #ffffff
- push button: Code gutter background
- label: Code gutter background
- label: #f0f2f5
- push button: Code line number text
- label: Code line number text
- label: #5b6570
- push button: Code selection background
- label: Code selection background
- label: #7fb0e8
- push button: Code current line background
- label: Code current line background
- label: #f2f6fc
- filler: Diff
- label: Diff
- label: 8
- push button: Diff added background
- label: Diff added background
- label: #e6ffec
- push button: Diff removed background
- label: Diff removed background
- label: #ffebe9
- push button: Diff in-line added background
- label: Diff in-line added background
- label: #9effb5
- push button: Diff in-line removed background
- label: Diff in-line removed background
- label: #f1b9b4
- push button: Diff moved background
- label: Diff moved background
- label: #fff1d6
- push button: Diff hunk header background
- label: Diff hunk header background
- label: #e7eef9
- push button: Diff hunk header text
- label: Diff hunk header text
- label: #3b5070
- push button: Diff group boundary rule
- label: Diff group boundary rule
- label: #8c8c8c
- filler: Review states
- label: Review states
- label: 6
- push button: State unread
- label: State unread
- label: #0969da
- push button: State accepted
- label: State accepted
- label: #1a7f37
- push button: State defect
- label: State defect
- label: #cf222e
- push button: State fixed
- label: State fixed
- label: #8250df
- push button: State verified
- label: State verified
- label: #0f7b6c
- push button: State stale
- label: State stale
- label: #5b6570
- filler: Syntax
- label: Syntax
- label: 26
- push button: Syntax normal
- label: Syntax normal
- label: #1f2328
- push button: Syntax keyword
- label: Syntax keyword
- label: #b51d27
- push button: Syntax control flow
- label: Syntax control flow
- label: #a40e26
- push button: Syntax operator
- label: Syntax operator
- label: #24292f
- push button: Syntax builtin
- label: Syntax builtin
- label: #953800
- push button: Syntax function
- label: Syntax function
- label: #6639ba
- push button: Syntax variable
- label: Syntax variable
- label: #24509b
- push button: Syntax type
- label: Syntax type
- label: #6e3ec8
- push button: Syntax constant
- label: Syntax constant
- label: #0a3069
- push button: Syntax number
- label: Syntax number
- label: #0550ae
- push button: Syntax string
- label: Syntax string
- label: #086a49
- push button: Syntax character
- label: Syntax character
- label: #116329
- push button: Syntax escape sequence
- label: Syntax escape sequence
- label: #944800
- push button: Syntax comment
- label: Syntax comment
- label: #555e69
- push button: Syntax documentation
- label: Syntax documentation
- label: #456351
- push button: Syntax annotation
- label: Syntax annotation
- label: #7d4e00
- push button: Syntax macro
- label: Syntax macro
- label: #7a3e9d
- push button: Syntax preprocessor
- label: Syntax preprocessor
- label: #7a3e9d
- push button: Syntax import
- label: Syntax import
- label: #156b2e
- push button: Syntax attribute
- label: Syntax attribute
- label: #7e5500
- push button: Syntax region marker
- label: Syntax region marker
- label: #555e68
- push button: Syntax information
- label: Syntax information
- label: #0759bc
- push button: Syntax warning
- label: Syntax warning
- label: #9f3f00
- push button: Syntax alert
- label: Syntax alert
- label: #b60a5b
- push button: Syntax error
- label: Syntax error
- label: #a40e11
- push button: Syntax other
- label: Syntax other
- label: #444d56
- panel: Dif
- label: Dif
- push button: Reset Dif preferences
- label: Show Dif mascot
- push button: About show dif mascot
- check box: Show Dif mascot (checked)
- label: Dif motion
- push button: About dif motion
- combo box: System
- label: Motion now
- label: Dif motion status
- label: Appearance settings status

### Notifications

- filler: Notifications
- label: Notifications
- check box: Enable review notifications (checked)
- check box: Pause notifications (unchecked)
- label: Notifications contain the repository, review number, attention category and unread count. They never include titles, source code or comment text. Choosing a notification selects that review in Home.

Your operating system controls permission and presentation. BetterPullReview cannot confirm every system permission setting.
- push button: Preview for selected Home review
- push button: Reset notification preferences
- label: Notification status

### Agents & context

- filler: Agents & context
- label: Agents & context
- label: The selected host determines which agent profiles are allowed. Choices here apply to the next explicit request in the current review; they are not saved as global defaults. Private notes, the current draft and uncommitted work are included only when selected. Start the request from the review's agent panel.
- combo box: Allowed agent profile for this request (disabled)
- list: Private notes included with this request
- check box: Include current private draft (disabled; unchecked)
- label: Allowed profile
- label: Private notes
- label: Private draft
- check box: Use existing working copy for this request (unchecked)
- label: Agent request scope

### Storage & privacy

- filler: Storage & privacy
- label: Storage & privacy
- label: Local data location
- text: Local application data location
- label: Local data includes repository caches, retained review history, drafts, notes, receipts and preferences. Retained epochs remain until explicitly pruned; maintenance can remove revisions needed for an older comparison. Moving this storage location and global purge are not available in this build.
- push button: &Prune selected epochs (disabled)
- push button: Delete repository &cache (disabled)
- push button: Cancel history &maintenance (disabled)
- label: Maintenance is limited to the selected review or repository cache and asks for confirmation. It does not change GitHub or your working copy. Basic diagnostics contain bounded operation metadata; advanced verbosity and export are not available here. Removing the application does not remove its local data folder or system credential entries. Use Accounts & Hosts to sign out of remembered device sessions.
## Limitations

- **What the tree exposes.** The inventory is what Qt exposes to AT-SPI on
  Linux under Wayland. Controls behind a closed disclosure are hidden in both
  builds alike. For example, the details under `Show details for github.com`
  are collapsed by default.
- **Default state only.** Both builds ran on an empty root with no account
  signed in, so the rows are the default-state rows. Rows that appear only after
  sign-in, or with saved servers, rules or repositories, were not exercised.
- **Row selection.** Rows were selected by posted mouse messages to the WSLg
  window, not by a pointer device. Each click was verified by the page it
  showed.
- **Display.** WSLg's compositor is Weston inside the WSL utility VM, not a
  GNOME or KDE session.
- **No person looked.** The acceptance asks for an inventory match, and the
  agent capture answers it. A visual check would be a separate attended
  candidate on this station; none was prepared.
- **Kept outside the checkout.** The capture tooling (`atspi.py`,
  `settings_inventory.py`, `rail_click.ps1`, `uia_rect.ps1`, `uia_close.ps1`),
  the raw captures and the scratch roots are under
  `/home/mmarcin/bpr-station-logs/task-20260917-lnx-evidence/`. They are not
  committed.
