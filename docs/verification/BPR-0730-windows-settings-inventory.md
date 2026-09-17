# BPR-0730 Windows packaged Settings inventory (RFC-BPR-0018 AC-11)

**Station and task.** Station `bpr-win` (JaegAW, `windows-native`, branch
`windows-lane`) ran task `task-20260917-win-evidence` in run
`run-20260916-stations`.

**Revision.** The producing revision is
`e93d9215045f06063b7cbdcbeadd61a25bda3e76`. It is at or after `d1ba791f`, and
it carries the [[BPR-0806]] storage fix without which the Windows image could
not start.

## Builds

Both builds were configured and built from a `vcvars64` shell at that revision.

| Build | Preset and configuration | Executable | SHA-256 |
|---|---|---|---|
| Packaged, tests disabled | `windows-app-release` (Ninja, `CMAKE_BUILD_TYPE=Release`, `BUILD_TESTING=OFF`) | `build/windows-app-release/src/app/better_pull_review.exe` | `46619a531e8ffd51d6cd25b3c3431f7d4e15e43cc728b3986d781bc9919eca51` |
| Developer, tests enabled | `windows-dev` (Visual Studio 18 2026, `Debug`, `BUILD_TESTING=ON`) | `build/windows-dev/src/app/Debug/better_pull_review.exe` | `0be2793135918e59f691604366c1d99eaf9caa8f634a493cd1d6854948cdb7ce` |

## How the inventory was taken

The captures were taken by an agent from the UI Automation tree, not read by
eye.

1. Each image was launched on the interactive desktop on an empty default data
   root, `%LOCALAPPDATA%\BetterPullReview\BetterPullReview`, cleared before
   each build's run. The Qt runtime directory was prepended to its `PATH`.
2. The toolbar button named `Settings` was invoked.
3. For each item of the `Settings pages` list, the item was clicked at its
   bounding rectangle. The capture then checked that the page's first group
   carries the category's name.
4. The raw UI Automation subtree under the `Settings page` pane was recorded.
   Each node records depth, control type, accessible name, enabled state and
   toggle state. Control values are omitted, so the capture records what exists
   rather than what a profile holds.
5. Both windows were closed through `WindowPattern.Close`, and each process
   exited 0.

The normalized captures are
[`windows-app-release.json`](BPR-0730-windows-settings-inventory/windows-app-release.json)
and [`windows-dev.json`](BPR-0730-windows-settings-inventory/windows-dev.json).

## Result

Both builds expose the seven categories in this order: General, Accounts & hosts, Reviews & queue, Appearance, Notifications, Agents & context, Storage & privacy.

| Category | Nodes in the packaged build | Nodes in the developer build | Differing nodes |
|---|---|---|---|
| General | 6 | 6 | 0 |
| Accounts & hosts | 28 | 28 | 0 |
| Reviews & queue | 66 | 66 | 0 |
| Appearance | 334 | 334 | 0 |
| Notifications | 12 | 12 | 0 |
| Agents & context | 15 | 15 | 0 |
| Storage & privacy | 13 | 13 | 0 |

There is **no difference** between the packaged inventory and the developer
build's. The two capture files differ only in their `build` field. So there is
no platform capability row to explain, and no defect to report.

## Rows

Named controls and row labels, from the packaged build, in tree order. Unnamed
layout groups and structural panes are omitted.

### General

- Text: General

### Accounts & hosts

- Text: Accounts & hosts
- Group: Accounts and hosts
- Text: One account per server. Credentials are never shown here.
- Group: github.com
- Text: Host
- Text: Server kind and transport
- Text: Connection state
- Text: ?
- Text: Account
- Text: Sign-in terms
- Button: Sign in to github.com
- Button: Connect github.com with GitHub CLI
- Text: GitHub CLI trust warning
- CheckBox: Show details for github.com (toggle=Off)
- Text: Capabilities, rate limits and the last problem.
- Button: Add enterprise server

### Reviews & queue

- Text: Reviews & queue
- Group: Reviews and queue
- Group: Review behaviour
- Text: Modality rules
- Button: Reset modality rules
- Text: A pull request opens in
- CheckBox: A pull request opens in: Overview (toggle=On)
- CheckBox: A pull request opens in: Read (toggle=Off)
- CheckBox: A pull request opens in: Last used (toggle=Off)
- Text: Continue review enters
- CheckBox: Continue review enters: Read (toggle=On)
- CheckBox: Continue review enters: Review (toggle=Off)
- Text: Reading
- Button: Reset reading
- Text: Mark a file read
- CheckBox: Mark a file read: By hand (toggle=On)
- CheckBox: Mark a file read: When fully seen (toggle=Off)
- Text: On return, compare
- CheckBox: On return, compare: Whole PR (toggle=On)
- CheckBox: On return, compare: Since my last review (toggle=Off)
- CheckBox: On return, compare: Since last upload (toggle=Off)
- Text: Comments drawer opens on
- CheckBox: Comments drawer opens on: All (toggle=On)
- CheckBox: Comments drawer opens on: Unread (toggle=Off)
- CheckBox: Comments drawer opens on: Unresolved (toggle=Off)
- Text: Drawers
- Button: Reset drawers
- Text: Drawer controls
- CheckBox: Drawer controls: Hidden (toggle=On)
- CheckBox: Drawer controls: Shown (toggle=Off)
- Group: Queue defaults
- ComboBox: Queue role filter
- Text: Role
- ComboBox: Queue observer reason filter
- Text: Observer reason
- ComboBox: Queue attention bucket filter
- Text: Attention
- ComboBox: Queue repository filter
- Text: Repository
- ComboBox: Queue quiet or current filter
- Text: Quiet reviews
- ComboBox: Queue sort order
- Text: Sort
- Button: Reset queue defaults
- Text: Queue settings status
- Text: Observer rules
- Group: Observer rules
- ComboBox: Saved observer rules
- ComboBox: Observer rule kind
- Edit: Observer rule pattern
- CheckBox: Exclude matching rule (toggle=Off)
- Button: Add local observer rule (disabled)
- Button: Remove selected observer rule (disabled)
- Text: Saved repositories
- Group: Saved repositories
- ComboBox: Saved repositories list
- Edit: Repository to save
- Button: Save repository (disabled)
- Button: Remove saved repository (disabled)

### Appearance

- Text: Appearance
- Group: Appearance settings
- Text: Changes apply as you make them and show in the review window behind this one.
- Custom: Theme
- Text: Theme
- Button: Reset appearance mode
- Group: Theme
- Custom: Type
- Text: Type
- Button: Reset type
- Text: Interface font
- Button: About interface font
- ComboBox: Interface font family
- Text: Interface size
- Button: About interface size
- Spinner: Interface font size, in points
- Text: Interface weight
- Button: About interface weight
- Spinner: Interface font weight
- Text: Line height
- Button: About line height
- Spinner: Interface line height, percent
- Text: Code font
- Button: About code font
- ComboBox: Code font family
- Text: Code size
- Button: About code size
- Spinner: Code font size, in points
- Text: Code weight
- Button: About code weight
- Spinner: Code font weight
- Text: Tab width
- Button: About tab width
- Group: Tab width, in spaces
- CheckBox: Tab width 2 (toggle=Off)
- CheckBox: Tab width 4 (toggle=On)
- CheckBox: Tab width 8 (toggle=Off)
- Text: Ligatures
- Button: About ligatures
- CheckBox: Ligatures (toggle=Off)
- Text: Bold syntax emphasis
- Button: About bold syntax emphasis
- CheckBox: Bold syntax emphasis (toggle=On)
- Text: Italic syntax emphasis
- Button: About italic syntax emphasis
- CheckBox: Italic syntax emphasis (toggle=On)
- Group: Preview
- Text: Preview
- Text: Files
- Text: 12 of 40 read
- Text: Engine/Source/Runtime/Renderer
- Edit: Code preview
- Custom: Colours for Dark
- Text: Colours for Dark
- Button: Import theme
- Button: Export saved theme
- Button: Reset all colours
- Text: Edits change Dark only. Each theme keeps its own colours.
- Group: Colours
- Edit: Filter roles
- Group: Colour roles
- Group: Surfaces and text
- Text: Surfaces and text
- Text: 7
- Button: Application surface
- Text: Application surface
- Text: #1c2026
- Button: Application text
- Text: Application text
- Text: #e6edf3
- Button: Code background
- Text: Code background
- Text: #0d1117
- Button: Code gutter background
- Text: Code gutter background
- Text: #141921
- Button: Code line number text
- Text: Code line number text
- Text: #8b949e
- Button: Code selection background
- Text: Code selection background
- Text: #28518c
- Button: Code current line background
- Text: Code current line background
- Text: #161c24
- Group: Diff
- Text: Diff
- Text: 8
- Button: Diff added background
- Text: Diff added background
- Text: #153622
- Button: Diff removed background
- Text: Diff removed background
- Text: #3e1f1f
- Button: Diff in-line added background
- Text: Diff in-line added background
- Text: #145e20
- Button: Diff in-line removed background
- Text: Diff in-line removed background
- Text: #933030
- Button: Diff moved background
- Text: Diff moved background
- Text: #2d1900
- Button: Diff hunk header background
- Text: Diff hunk header background
- Text: #18212f
- Button: Diff hunk header text
- Text: Diff hunk header text
- Text: #9fb6d4
- Button: Diff group boundary rule
- Text: Diff group boundary rule
- Text: #808080
- Group: Review states
- Text: Review states
- Text: 6
- Button: State unread
- Text: State unread
- Text: #58a6ff
- Button: State accepted
- Text: State accepted
- Text: #3fb950
- Button: State defect
- Text: State defect
- Text: #ff7b72
- Button: State fixed
- Text: State fixed
- Text: #bc8cff
- Button: State verified
- Text: State verified
- Text: #39c5b0
- Button: State stale
- Text: State stale
- Text: #8b949e
- Group: Syntax
- Text: Syntax
- Text: 26
- Button: Syntax normal
- Text: Syntax normal
- Text: #dcdcdc
- Button: Syntax keyword
- Text: Syntax keyword
- Text: #6da9db
- Button: Syntax control flow
- Text: Syntax control flow
- Text: #d8a0df
- Button: Syntax operator
- Text: Syntax operator
- Text: #b4b4b4
- Button: Syntax builtin
- Text: Syntax builtin
- Text: #6da9db
- Button: Syntax function
- Text: Syntax function
- Text: #ff8000
- Button: Syntax variable
- Text: Syntax variable
- Text: #bdb76b
- Button: Syntax type
- Text: Syntax type
- Text: #ffd700
- Button: Syntax constant
- Text: Syntax constant
- Text: #b8d7a3
- Button: Syntax number
- Text: Syntax number
- Text: #b5cea8
- Button: Syntax string
- Text: Syntax string
- Text: #d69d85
- Button: Syntax character
- Text: Syntax character
- Text: #d69d85
- Button: Syntax escape sequence
- Text: Syntax escape sequence
- Text: #ffd68f
- Button: Syntax comment
- Text: Syntax comment
- Text: #66b559
- Button: Syntax documentation
- Text: Syntax documentation
- Text: #66b559
- Button: Syntax annotation
- Text: Syntax annotation
- Text: #8fbf72
- Button: Syntax macro
- Text: Syntax macro
- Text: #ce8ad4
- Button: Syntax preprocessor
- Text: Syntax preprocessor
- Text: #a3a3a3
- Button: Syntax import
- Text: Syntax import
- Text: #a3a3a3
- Button: Syntax attribute
- Text: Syntax attribute
- Text: #4ec9b0
- Button: Syntax region marker
- Text: Syntax region marker
- Text: #a3a3a3
- Button: Syntax information
- Text: Syntax information
- Text: #60b5ff
- Button: Syntax warning
- Text: Syntax warning
- Text: #f0a03c
- Button: Syntax alert
- Text: Syntax alert
- Text: #ff8ac7
- Button: Syntax error
- Text: Syntax error
- Text: #f39b73
- Button: Syntax other
- Text: Syntax other
- Text: #b1bac4
- Custom: Dif
- Text: Dif
- Button: Reset Dif preferences
- Text: Show Dif mascot
- Button: About show dif mascot
- CheckBox: Show Dif mascot (toggle=On)
- Text: Dif motion
- Button: About dif motion
- ComboBox: Dif motion
- Text: Motion now
- Text: Dif motion status
- Text: Appearance settings status

### Notifications

- Text: Notifications
- CheckBox: Enable review notifications (toggle=On)
- CheckBox: Pause notifications (toggle=Off)
- Button: Preview for selected Home review
- Button: Reset notification preferences
- Text: Notification status

### Agents & context

- Text: Agents & context
- ComboBox: Allowed agent profile for this request (disabled)
- List: Private notes included with this request
- CheckBox: Include current private draft (disabled; toggle=Off)
- Text: Allowed profile
- Text: Private notes
- Text: Private draft
- CheckBox: Use existing working copy for this request (toggle=Off)
- Text: Agent request scope

### Storage & privacy

- Text: Storage & privacy
- Text: Local data location
- Edit: Local application data location
- Button: &Prune selected epochs (disabled)
- Button: Delete repository &cache (disabled)
- Button: Cancel history &maintenance (disabled)

## Limitations

- **What the tree exposes.** The inventory is what Qt exposes to UI Automation
  on Windows. Controls behind a closed disclosure are absent from the tree in
  both builds alike. For example, the details under `Show details for
  github.com` are collapsed by default.
- **Default state only.** Both builds ran on an empty root with no account
  signed in, so the rows are the default-state rows. Rows that appear only after
  sign-in, or with saved servers, rules or repositories, were not exercised.
- **No person looked.** The acceptance asks for an inventory match. If a visual
  check is wanted, it would be a separate attended candidate on this station.
