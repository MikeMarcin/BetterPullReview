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

On Windows and Linux, BetterPullReview is tested on Windows 11 and Ubuntu
26.04 (WSL2). Other Windows and Linux versions, including Windows 10 and native
Linux desktops, may work but are untested: the packages do not refuse them,
and nothing in this overview has been observed there. Even on the tested
environments, the qualification above is incomplete.

## Network destinations

| Feature | Destination and what is sent |
|---|---|
| Reading and publishing reviews | HTTPS `api.github.com` and Git smart HTTP to `github.com`, or the configured GHES host only. Only text the user publishes is sent. |
| GitHub.com device sign-in | `github.com/login/device/code` and `/login/oauth/access_token`. |
| Public preview | Anonymous GitHub.com requests; no credential. |
| Images in descriptions and comments, and avatars | GitHub.com: HTTPS to GitHub's own image and avatar hosts (`*.githubusercontent.com`, `github.com`) and, after a redirect, GitHub's `github-production-*.s3.amazonaws.com` storage; never a credential. GHES: HTTPS to any host a description or comment names; the credential goes only to the GHES host itself and its `media.` host. **Settings > Appearance > People > Avatars > Coloured initials** fetches no avatar. |
| GitHub CLI profile (opt-in) | Whatever `gh` and its Git children reach; one short-lived credential per operation, no token stored. |
| Local PR draft Push (macOS and Linux) | The user's own Git pushes the draft's branch to the hosted repository's URL, through the user's Git configuration and credential helpers. Never forced; no tags. |
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

Each profile stores notes, drafts, local PR drafts, publication recovery,
retained Git objects, agent workspaces and conversations, MCP context
snapshots and a diagnostic log (and, from versions before 0.5.0, unread Review Cycles captures). Agent scratch folders
sit beside the profile folder or in the user's cache folder. These can
contain company source.

> [!IMPORTANT]
> BetterPullReview does not encrypt these stores; they rely on OS user
> permissions. Apply your own disk encryption, backup and endpoint controls.

**Export diagnostics** writes only codes and counts and uploads nothing; OS
crash reports and agent logs need your own handling. Uninstall leaves profile
data behind; secure erasure is not claimed. See
[storage and removal](../development/storage-and-diagnostics.md).

## Programs and local IPC

- **Git:** the app's own Git runs isolated: repository hooks, filters and
  external diff are not run. The exception is **Commit** and **Push** in a
  local PR draft (macOS and Linux; unavailable on Windows): after the user
  allows it for that draft, the user's installed Git runs in their own
  worktree with their Git configuration, the repository's hooks and their
  credential helpers. **Save** in a local PR draft writes files in that
  worktree.
- **`gh`** (optional): may start Git and its credential helper.
- **AI agents** (optional): Claude Code (`claude`), Codex (`codex`),
  `bpr-agent-terminal`, or a custom agent program the user names by absolute
  path. A custom agent's fixed arguments are passed exactly as written; no
  shell runs them.
- **Agent supervisor:** cleans up agent processes; it is not a sandbox.
- **MCP helper:** read-only context over stdio.
- **macOS updater:** `codesign`, `spctl`, `hdiutil` and its own helper; no
  elevation.

The app opens no TCP or UDP server. Its only listeners are two same-user local
IPC endpoints. One carries window activation. The other lets `bpr` send a fixed
set of commands to the app that owns the profile, which runs them against its
own local store: tour draft edits, writing-assist requests and `bpr draft
open`. It also answers one read, at most 200 lines of
pinned source for a pull request a window already has open, from that window's
Git cache. Neither endpoint carries credentials or publication authority, and
neither accepts a GitHub request, a process or a command of the caller's
choosing. There is no bundled
web engine or Node runtime.

## AI agents

- An agent starts only after the user asks and sees what will be shared.
- By default it works in an isolated copy of the pull-request head; private
  notes are included only when selected.
- Results never publish automatically. Agents' own tools may run code.
- Agents that speak Claude Code's or Codex's protocols start with a scratch
  folder by default, outside the repository (none on Windows). BetterPullReview
  adds permission for the agent to write there and run the scripts it writes
  there; the folder has a size limit, 256 MiB by default. When the agent's own
  arguments or Codex configuration set its permissions, BetterPullReview adds
  none.
- **Build and run tests** is off by default for each agent. When it is on,
  each pull request asks before the first build, and builds run only in a
  disposable checkout of the pull request, with the user's account rights.
- MCP servers are off by default: Claude Code gets an empty strict MCP
  configuration and Codex an empty server list. Per agent, the user may let
  the servers in the agent's own configuration load; a pull request's
  checkout's servers never load.

See the [AI boundary](../architecture/ai-agent-boundary.md).

## Review Cycles: retired

Review Cycles were removed in 0.5.0 ([[BPR-2308]]). A profile used with an
earlier version may still hold their captures in the unencrypted
`review-cycles.sqlite3` and a `publication-git-v1/` object store. Nothing
reads them; they are readable by any program running as the same user, as
before, until profile purge or **Reset review data** removes them.

## Further reading

[Security policy](../../SECURITY.md) ·
[Release policy](release-policy.md) ·
[Threat model](../architecture/security-threat-model.md) ·
[Package guide](../development/release-packages.md)
