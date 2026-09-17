---
schema: "implement-rfc.card.v1"
id: "BPR-0817"
title: "A Linux close exits 1 when the app created its data folder group-writable"
status: "Ready"
rfcs: ["RFC-BPR-0013", "RFC-BPR-0033"]
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux during BPR-0799","found_at":"17f495fea40f6985c0d2f6b0430c7a71062f8085","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0817: A Linux close exits 1 when the app created its data folder group-writable

## Defect

bpr-linux observed this while proving [[BPR-0799]]
([evidence](../../verification/BPR-0799-linux-native-empty-desktop-quit.md),
"Defect: a group-writable data-root parent makes every Linux close exit 1").
Under umask `0002`, the application creates `$XDG_DATA_HOME/BetterPullReview`
itself, as mode `775`. Startup recovery then calls
`recover_installed_agent_workspaces` → `prepare_workspace_neighbourhood`
(`src/adapters/git/src/installed_git_process.cpp:3003`), which refuses a parent
writable by group or other and returns `repair_required`. The agent workspace
neighbourhood sits beside the data root (`src/app/src/github_application.cpp:4796`).
Shutdown counts that cleanup failure, so the process exits **1**
(`src/app/src/profile_desktop.cpp:853`). The window still closes with no
chooser, the diagnostics line still reads `outcome=shutdown failure=none`, and
nothing tells the person why.

| Course | Umask | Data-root parent | Neighbourhood created | Exit |
|---|---|---|---|---|
| `scratch-0799-release-umask002` | `0002` | 775, created by the application | no | 1 |
| `scratch-0799-release-umask002-parent755` (control) | `0002` | 755, created before launch | yes | 0 |

With `USERGROUPS_ENAB yes`, `pam_umask` gives an Ubuntu user-private-group
login session umask `0002`, so an ordinary desktop login plausibly hits this.
The station's own shell used `0022`, so this is inferred, not observed on a
desktop session. `application_profile_management_public_path` also fails
"orderly exit code" under a stock `/tmp` for the same reason, and passes with a
private `TMPDIR`.

## Scope and direction

- **Folders the application creates.** Create the data root, its parent
  folder and the workspace neighbourhood with explicit owner-only modes,
  independent of umask. The refusal then never fires on a fresh install. This
  part is an ordinary defect fix.
- **Pre-existing group-writable parents** (earlier installs, or a folder a
  person made). Keep refusing to place agent workspaces there. Surface that
  refusal visibly: [[BPR-0808]] restored the agent panel's repair message.
  Decide separately whether a refused *workspace cleanup* should turn an
  otherwise orderly quit into exit 1. That is a product decision about exit-code
  meaning. If the answer is unclear, record the exact question for the owner and
  do not change the exit semantics silently.
- Make the course independent of the host's `/tmp` mode, or make it assert the
  intended behaviour there, with no private-`TMPDIR` dependency hidden in the
  harness.

## Acceptance

- On the Linux station under umask `0002`, a fresh data root is created
  owner-only, and closing the last window exits 0 with the neighbourhood
  created.
- With a pre-existing `775` parent, the refusal is visible to the person, and
  the exit code follows the recorded decision.
- `application_profile_management_public_path` passes under the stock `/tmp`
  on Linux, and still passes on macOS.
