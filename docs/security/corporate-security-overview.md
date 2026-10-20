# Corporate security overview

This page is for a security or IT reviewer deciding whether to allow
BetterPullReview on company machines. It covers what the app sends over the
network and where, which credentials it holds, what it stores locally, which
programs it runs, what AI agents can see and do, and what is not yet qualified.

## Summary

- BetterPullReview is a local desktop client for pull requests on GitHub.com
  and on GitHub Enterprise Server (GHES) hosts the user adds explicitly.
- There is no BetterPullReview account, server, telemetry or automatic
  diagnostic or crash upload. The app talks only to GitHub (the hosts listed
  below) and to programs the user explicitly starts.
- GitHub stays the owner of shared reviews. Notes, drafts, receipts, retained
  revisions and review progress stay on the machine unless the user publishes
  review content or hands context to an external agent.
- Credentials live in the operating system's credential store, never in the
  app's databases, logs or AI context.
- Local stores can contain company source and review text. BetterPullReview
  relies on OS user permissions and does not encrypt them itself.
- External AI agents run only after an explicit action, under their own
  credentials, network access and tools. BetterPullReview does not restrict
  where they connect.

> [!WARNING]
> Release qualification is incomplete. Signed packages are published and their
> signatures checked, but native installation, authentication and vault,
> accessibility, upgrade and removal, crash privacy, external hardening and full
> vulnerability and fuzz qualification remain open. Scanner matches have
> recorded per-release applicability reviews; no applicable risk is waived. The
> [qualification ledger](../verification/RFC-BPR-0038.md) holds the current
> evidence, open gaps and the accepted Ubuntu memory-growth exception.

## Network destinations and data flows

| Feature and trigger | Destination | What is sent or received |
|---|---|---|
| Reading pull requests and publishing reviews the user submits | HTTPS `api.github.com` (REST and GraphQL); Git smart HTTP to the repository on `github.com` | Repository and review metadata; only the text the user chooses to publish. The account's credential stays in the app's own transport. |
| GHES profile the user adds | HTTPS on that configured host only: `/api/v3`, `/api/graphql` and the repository's smart-Git path | Same as above. Content from the server cannot add another host. |
| GitHub.com device sign-in | Outbound `github.com/login/device/code` and `/login/oauth/access_token`; the system browser opens the verification page | Device-flow sign-in. No inbound OAuth callback listener. Device sign-in is not available for GHES. |
| Public preview (macOS, Windows, Linux) | Anonymous GitHub.com REST and smart HTTP, after the user opens a public pull request | No credential. A failed sign-in never falls back to a credential. Not available for GHES. |
| GitHub CLI profile (macOS, opt-in) | Whatever the installed `gh` and its Git children reach for the chosen account | One short-lived credential for the chosen account is passed to that operation. BetterPullReview stores no token and changes no global `gh` state. See [GitHub CLI qualification](../verification/RFC-BPR-0078.md). |
| Review Cycles publication | HTTPS `api.github.com` REST only, signed-in GitHub.com account | After the reviewer confirms one plan: the candidate's blobs, one tree, one commit, one new branch (create only, never update) and one pull request in that plan's repository, with reads before and after. No push, force, delete, merge or comment. GHES is refused. Notes, rounds and replies are never uploaded. |
| macOS updater (eligible signed installs) | `api.github.com` for the fixed `MikeMarcin/BetterPullReview` releases; asset downloads may redirect only to `release-assets.githubusercontent.com` | Release metadata and the chosen installer. The releases are public, so no credential is used unless a stored GitHub.com account is still selected; redirected downloads never carry a credential or cookie. See [updater](release-updater.md). |
| Links the user opens | System browser | Uses the browser's own session. |
| External AI agents the user starts | Whatever the agent connects to | The agent uses its own credentials, network, tools and approvals. BetterPullReview has no allow-list for agent destinations; govern those applications with your own policy. |

Network handling applies to every HTTPS request the app makes:

- System certificate trust and proxy settings are enforced. There is no way to
  bypass a certificate error.
- Responses, timeouts and redirects are bounded. Pull-request REST redirects
  must stay on the validated host and API path; GraphQL redirects and
  redirects of requests with a query are refused. Updater downloads follow
  their own bounded redirect rule.
- DNS, proxies and OS trust services remain platform dependencies.

Source: `github_host_profile.cpp`, `native_https_transport.cpp`,
`gh_cli_git_smart_transport.cpp` and `github_release_update_contract.hpp` under
`src/adapters/github/src/`.

## Credentials

| Platform | Store |
|---|---|
| macOS | Keychain |
| Windows | Credential Manager (generic credentials, protected by the user's vault, not by a per-executable ACL) |
| Linux | Secret Service |

- **Manual tokens** are deleted from the store when the app quits normally.
- **Device sign-in** keeps access and refresh tokens, plus non-secret binding
  and lifetime details, so the session can be checked again after a restart.
- **Sign out** removes the account's stored entries and disables its binding.
- No token is written to SQLite, diagnostics, Git arguments or environment, or
  AI context.
- An interrupted or crashed sign-in can leave an orphaned entry. The app does
  not enumerate or erase the credential store broadly.

## Local data

Each profile has private storage that can contain company source and review
text:

- SQLite settings, notes, drafts, receipts and publication recovery
- retained Git objects and revision catalogs
- managed agent workspaces
- MCP context snapshots
- Review Cycles captures (see [below](#review-cycles-retained-source-and-disclosure))
- a bounded local diagnostic log

> [!IMPORTANT]
> These stores are protected by OS user file permissions and ACLs only;
> BetterPullReview does not encrypt them. Apply your disk encryption, backup,
> account and endpoint controls accordingly.

The [storage and removal guide](../development/storage-and-diagnostics.md)
lists locations, the full inventory, retention and what remains after removal.

## Subprocesses and local IPC

| Program | What it does and its limits |
|---|---|
| Installed Git | Bounded object operations in private caches and workspaces. Repository hooks, filters, textconv, external diff and repository scripts are not run as an app feature. No BetterPullReview credential is passed in Git arguments or environment. |
| `gh` (optional, macOS only) | Explicit opt-in. Arguments, I/O and time are bounded, and file writes are denied by the OS except for an app-owned temporary object store. `gh` may itself start Git and its shell credential helper. No equivalent Windows or Linux profile is offered. |
| AI agents (optional) | `codex`, approved `claude` or `bpr-agent-terminal`, after an explicit action and a disclosure of the context to be shared. See [AI agents](#ai-agents). |
| Bundled agent supervisor | Cleans up the agent and its descendants when the session ends; it is not a sandbox. A process that deliberately leaves its POSIX process group escapes it. Windows uses suspended start and a Job Object. |
| Bundled MCP helper | Read-only, bounded context over stdio. No HTTP endpoint and no tool that changes anything. |
| macOS updater | `codesign`, `spctl`, read-only `hdiutil info`, the bundled `bpr_update_helper` and the exact replacement app. No shell, privileged daemon or elevation. |

The app opens no TCP or UDP server. Its only listeners are two same-user local
IPC endpoints (AF_UNIX sockets; owner-private named pipes on Windows that
reject remote clients), held under the profile lock:

- **Activation** carries only "activate" or "new window".
- **Local commands** carry a fixed set of draft, hook and assist commands and
  the `bpr cycle` commands, which act on the user's own drafts and store.

Neither endpoint carries HTTP, credentials, GitHub access, publication rights or
arbitrary commands. Agent lifetime channels are local connected IPC, not
network services.

Other hardening:

- No bundled web engine, browser engine or Node runtime, and no arbitrary
  plugin loader. Qt libraries and plugins load only from the admitted
  installation; hostile loader arguments and paths are refused.
- Repository content, Markdown, themes and agent output are treated as
  untrusted and bounded.
- The split between decision logic and code that performs effects reduces
  accidental privilege flow inside the process; it is not an OS sandbox. Stale
  results from an earlier request, session, profile, account or revision are
  rejected. See the [system architecture](../architecture/system-architecture.md),
  [maintenance contract](../architecture/release-maintenance.md) and
  [conformance matrix](../verification/architecture-conformance.md).

## AI agents

- An agent starts only after the user asks and sees what context will be
  shared.
- By default the agent works in an isolated copy of the exact pull-request
  head. Choosing an existing checkout deliberately gives it more context.
- Private notes and drafts are included only when the user selects them.
- Agent results never publish a review automatically.
- GHES profiles refuse Codex.
- Agents are separately trusted applications; their own tools may run code.

**Review Tour writing help** uses a narrower text-only mode: captured local
scope, no tools, MCP or hooks, bounded context, and one undo step. Enforcement
by installed agents and output quality are not yet qualified.

See the [AI boundary](../architecture/ai-agent-boundary.md) and the
[threat model](../architecture/security-threat-model.md) for residual risks.

## Review Tours

- Tour drafts and saved copies of the editable remote text are private
  authored content, kept in the profile store and the publication-recovery
  store with conflict versions and pending operations.
- **Publish** freezes one plan and issues single-use, journaled permissions for
  each write. Compatibility comments, maintenance edits and description edits
  are each visible in different places.
- The app reads before it writes, which reduces but does not prevent races with
  other writers; GitHub offers no compare-and-swap here.
- Production publishing stays unavailable until it is qualified live.
- Navigating or preparing a tour never publishes, resolves, approves or marks
  files viewed.
- Profile purge removes local tour files but never shared GitHub discussion.

See the [tour contract](../architecture/review-tours.md) and
[operator guide](../development/review-tours.md).

## Review Cycles: retained source and disclosure

A Review Cycle captures the bytes of a folder, file or checkout the reviewer
chooses into the profile-private `review-cycles.sqlite3`.

- The store is unencrypted, like the other stores, and holds source exactly as
  captured, including untracked files that the checkout's own ignore files do
  not exclude.
- Capture never runs Git or anything the checkout configures, and never writes
  into the checkout.
- A round leaves the machine only through a channel the profile's disclosure
  policy allows: clipboard, saved file or bundle, terminal output, a harness
  reading one granted round through `bpr`, or **Send to Agent**.
- Sources of unknown or enterprise origin are refused to the two channels that
  reach another program until the policy allows them. A denial set by the host
  applies everywhere. Every local folder is currently unclassified.
- Once a round is copied, saved or read by a harness, clipboard history, the
  saved file and the harness's transcripts and provider requests keep it
  outside BetterPullReview's control.
- `BPR_CYCLE_ROLE=consumer` and grants confine only a cooperating harness. Any
  program running as the same user can read the store.

Packages built without agent integrations or local automation remove **Send to
Agent**, or the local command endpoint and `bpr`. See the
[package guide](../development/release-packages.md#packages-without-agent-integrations-or-local-automation)
and the [threat model](../architecture/security-threat-model.md#review-cycles).

## Diagnostics

- The local log records fixed events with bounded, credential-sanitized
  detail. That text can still name a service or account.
- **Export diagnostics** writes only classified operation and outcome codes and
  numeric HTTP and drop counts. It omits free text, identities, paths, source,
  comments, notes, drafts and crash memory.
- The user previews the exact bytes before saving a new local file. There is no
  upload.
- OS crash reports and external-agent logs are outside this export and need
  your own handling. Their freedom from source and secrets is not yet
  qualified.

## Removal

- Retained history is removed only by explicit pruning, cache deletion or
  profile purge. Nothing expires accepted revisions silently.
- **Profile purge** stops the profile, inventories its files, deletes its known
  credential-store entries before their metadata, and journals recovery.
  Unknown or changed entries are not deleted.
- **Uninstall** removes program files only. Shared registry and updater state,
  updater draft recovery, exports, other profiles and documented empty folders
  and locks remain.
- Secure erasure from backups, SQLite write-ahead logs or storage media is not
  claimed.

## Platforms and distribution

| Platform | Current packages |
|---|---|
| macOS | Universal (Apple Silicon and Intel), macOS 14.4 or later; v0.3.0 and earlier require macOS 13 |
| Windows | Windows 10/11 x64, per-user MSI |
| Linux | Ubuntu 26.04 x86_64, DEB and per-user archive |

These are qualification targets, not completed support certifications. The
[package guide](../development/release-packages.md) lists toolchains, runtime
prerequisites and installation routes.

- Windows, Linux and managed deployments update manually. Only eligible signed
  macOS installs use the [updater](release-updater.md), and only with explicit
  download and install actions.
- Every binary comes with equal access to its separate corresponding-source
  asset; installed notices name its versioned file, URL and digest. Corporate
  or offline redistribution must include that asset, and source obligations
  continue after an app copy is removed.
- Releases are produced through the
  [publish-release skill](../../.codex/skills/publish-release/SKILL.md) and the
  package pipeline.

## Further reading

- [Security policy](../../SECURITY.md): how to report a vulnerability
- [Release policy](release-policy.md) and [qualification ledger](../verification/RFC-BPR-0038.md)
- [Hosted-content secret audit](github-content-audit-2026-10-19.md) and
  [publication readiness record](publication-readiness-2026-10-19.md); neither
  closes the open qualification work
- [Threat model](../architecture/security-threat-model.md)
