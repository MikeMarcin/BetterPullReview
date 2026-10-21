# Corporate security overview

What BetterPullReview sends, stores and runs, for IT and security reviewers.

## Summary

- A local desktop client for GitHub.com and explicitly added GitHub Enterprise
  Server (GHES) hosts. No BetterPullReview account, server or telemetry, and no
  automatic diagnostic or crash upload.
- External AI agents run only after an explicit action, with their own
  credentials and network access. BetterPullReview does not restrict where
  they connect.

> [!WARNING]
> Release qualification is incomplete: native installation, authentication and
> vault, accessibility, upgrade and removal, crash privacy, external hardening
> and full vulnerability and fuzz qualification remain open. See the
> [qualification ledger](../verification/RFC-BPR-0038.md).

## Network destinations

| Feature | Destination and what is sent |
|---|---|
| Reading and publishing reviews | HTTPS `api.github.com` and Git smart HTTP to `github.com`, or the configured GHES host only. Only text the user publishes is sent. |
| GitHub.com device sign-in | `github.com/login/device/code` and `/login/oauth/access_token`. |
| Public preview | Anonymous GitHub.com requests; no credential. |
| GitHub CLI profile (macOS, opt-in) | Whatever `gh` and its Git children reach; one short-lived credential per operation, no token stored. |
| Review Cycles publication | `api.github.com` only, after confirmation: one new branch, commit and pull request. No push, force or delete. |
| macOS updater (eligible signed installs) | `api.github.com` and `release-assets.githubusercontent.com`. |
| AI agents the user starts | Whatever the agent connects to; no allow-list. |

All HTTPS uses system certificate trust and proxy settings; certificate errors
cannot be bypassed.

## Credentials

Credentials live in Keychain (macOS), Credential Manager (Windows; protected by
the user's vault, not a per-executable ACL) or Secret Service (Linux), never in
the app's databases, logs, Git arguments, environment or AI context.
Manual tokens are deleted on quit; device sign-in tokens persist until
**Sign out**. An interrupted sign-in can leave an orphaned entry.

## Local data

Each profile stores notes, drafts, publication recovery, retained Git objects,
agent workspaces, MCP context snapshots, Review Cycles captures and a
diagnostic log. These can contain company source.

> [!IMPORTANT]
> BetterPullReview does not encrypt these stores; they rely on OS user
> permissions. Apply your own disk encryption, backup and endpoint controls.

**Export diagnostics** writes only codes and counts and uploads nothing; OS
crash reports and agent logs need your own handling. Uninstall leaves profile
data behind; secure erasure is not claimed. See
[storage and removal](../development/storage-and-diagnostics.md).

## Programs and local IPC

- **Git:** repository hooks, filters and external diff are not run as an app
  feature.
- **`gh`** (optional, macOS): may start Git and its credential helper.
- **AI agents** (optional): `codex`, approved `claude` or `bpr-agent-terminal`.
- **Agent supervisor:** cleans up agent processes; it is not a sandbox.
- **MCP helper:** read-only context over stdio.
- **macOS updater:** `codesign`, `spctl`, `hdiutil` and its own helper; no
  elevation.

The app opens no TCP or UDP server. Its only listeners are two same-user local
IPC endpoints (window activation, and local commands used by `bpr`); neither
carries credentials, GitHub access or arbitrary commands. There is no bundled
web engine or Node runtime.

## AI agents

- An agent starts only after the user asks and sees what will be shared.
- By default it works in an isolated copy of the pull-request head; private
  notes are included only when selected.
- Results never publish automatically. Agents' own tools may run code.

See the [AI boundary](../architecture/ai-agent-boundary.md).

## Review Cycles: retained source and disclosure

A Review Cycle copies the chosen folder, file or checkout, including untracked
files not excluded by its ignore files, into the unencrypted
`review-cycles.sqlite3`. Capture never runs Git or anything the checkout
configures. A round leaves the machine only through clipboard, a saved file,
terminal output, a `bpr` harness or **Send to Agent**, and is then outside
BetterPullReview's control. Any program running as the same user can read the
store. [Agent-free packages](../development/release-packages.md#packages-without-agent-integrations-or-local-automation)
remove **Send to Agent**, or `bpr` as well. See the
[threat model](../architecture/security-threat-model.md#review-cycles).

## Further reading

[Security policy](../../SECURITY.md) ·
[Release policy](release-policy.md) ·
[Threat model](../architecture/security-threat-model.md) ·
[Package guide](../development/release-packages.md)
