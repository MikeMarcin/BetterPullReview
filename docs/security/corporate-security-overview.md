# Corporate security overview

BetterPullReview is a local desktop client for GitHub and explicitly configured
GitHub Enterprise Server (GHES) pull requests. GitHub remains the authority for
shared reviews. Private notes, receipts, retained revisions and working state
remain local unless the user deliberately publishes review content or supplies
context to an external agent. There is no BetterPullReview hosted account,
telemetry service or automatic diagnostic/crash upload.

This packet describes the implementation inspected at `59226c62` for
RFC-BPR-0014 M10, mandate r3. It is a security-review input, not release approval.
The [M8/M9 evidence ledger](../verification/RFC-BPR-0014.md) retains exact
artifacts, failures and native gaps. R4 closes only the implemented foundation;
[RFC-BPR-0038](../rfcs/RFC-BPR-0038-native-package-release-security-qualification.md)
retains complete release qualification. Current Qt 6.11.2 macOS producer
`92c4e159` and Windows producer `67f5e514` both use separate version-bound
source assets. Earlier records remain candidate-specific history.

## Release qualification

The declared matrix is Windows 10/11 x64, macOS 13+ on native Intel and Apple
Silicon, and selected Linux x86_64 on Ubuntu 26.04 with a glibc 2.43 ceiling,
under the owner's 2026-10-11 next-release scope amendment. WSL2 may produce the
DEB/per-user archive; clean Ubuntu desktop, Secret Service and final installer
acceptance remain pending. Ubuntu 24.04 and RHEL are no longer selected targets;
retained optional RPM mechanics do not add a release requirement. These are qualification targets, not
completed support certifications. The [package guide](../development/release-packages.md)
names toolchains, runtime prerequisites and installation routes.

At the Qt 6.11.2 hand-back 478 (2026-09-22 UTC), retained by owner amendment r4:

- macOS has an offline ad-hoc candidate, not final Developer ID/notarization/
  Gatekeeper acceptance. The Windows MSI is unsigned; there is no final Linux
  package. Native Intel, declared Linux baselines, clean ordinary-user Windows
  desktops and the complete clean-account/vault/removal courses remain pending.
- Grype 0.119.0 with the recorded v6.1.9 database returned exit 2: 32 macOS and
  30 Windows matches. Critical/High and all lower-severity
  findings remain undisposed. A match is not proof of exploitability; the
  [release policy](release-policy.md) still blocks distribution until remediation
  or a named independent, exact-version applicability disposition. Moving the
  source ZIP did not resolve findings. Qt 6.11.2 removes the old libpng match,
  while twelve Tika MIME-data matches per platform and source-backed applicability
  proposals still require independent disposition; historical 21/19 failures remain.
- The owner accepted only the exact Qt 6.11.2 macOS licensing/source-delivery
  v003 criterion at event 477, hand-back 478. Windows v002 remains unanswered.
  That accepts no vulnerability, trust,
  native-matrix or publication claim. The source URL remains unpublished;
  recipient access and retention must still be fulfilled during distribution.
- Native active-operation network/vault checks, foreign-user IPC, external
  dependency hardening, complete PE CFG coverage and crashes with resident
  synthetic source/comment/token material remain pending. The recorded
  environment-canary crash course is narrower evidence.

## Data flow and authorities

```text
Reviewer gesture -> Qt intent -> application host -> core decision
                                      |
                              typed effect request
                                      v
                       designated adapter / OS capability
                                      |
                          identity-bearing completion
                                      v
                     serialized decision lane -> core acceptance
                                      |
                           bounded Qt projection
```

Core state contains compact semantic identities and policy, not credentials or
effect APIs. Hosts own bounded observations, jobs, capability leases and durable
state. Concrete adapters perform network, vault, file and process effects.
Composition selects those adapters and keeps them alive through shutdown. This
in-process division reduces accidental authority flow; it is not an OS sandbox.
Request/session/profile/account/revision/generation checks reject stale work.
The [system architecture](../architecture/system-architecture.md),
[maintenance contract](../architecture/release-maintenance.md) and
[conformance matrix](../verification/architecture-conformance.md) describe the
typed path and its direct checks.

## Network destinations

| Capability and trigger | Destination and data boundary |
|---|---|
| Native GitHub reads and explicit review publication | HTTPS `api.github.com` REST/GraphQL; repository/review metadata and selected published text. Account-bound credentials stay in the designated transport. Native Git smart HTTP uses the accepted repository on `github.com`. |
| Review Cycles publication | HTTPS `api.github.com` REST only, after the reviewer confirms one plan's digest under the signed-in GitHub.com account: the candidate's blobs, one tree, one commit, a new branch (`POST git/refs`, never an update) and one pull request in the plan's repository, with reads before and after. No Git push, force, delete, merge or comment. GHES is refused. Notes, rounds and replies are never uploaded. |
| Explicit GHES profile | HTTPS on the configured canonical host: `/api/v3`, `/api/graphql` and that repository's smart-Git path. Provider content cannot add a host. |
| GitHub.com device sign-in | Outbound `github.com/login/device/code` and `/login/oauth/access_token`; the system browser opens the admitted verification address. No inbound OAuth callback listener. GHES device flow is unavailable. |
| Public preview | macOS-only anonymous GitHub.com REST and smart HTTP after an explicit read; no credential fallback from failed sign-in. Windows/Linux and GHES refuse this mode. |
| Opted-in GitHub CLI profile | Configured stock `gh` acquires the exact chosen account through a bounded sensitive path; one transient adapter-owned credential reaches that operation's API and gh-owned Git/helper children. BPR persists no token and changes no global gh state. Private configuration excludes ambient socket routing; executable, system trust, redirects and helper processes remain trusted surfaces. See [strict qualification](../verification/RFC-BPR-0078.md). |
| Eligible macOS updater | Fixed private `MikeMarcin/BetterPullReview` release API on `api.github.com`; exact asset download may redirect only to `release-assets.githubusercontent.com`, with fresh credential-free requests. Explicit native GitHub.com release access is separate from review authority. |
| Browser and external AI agent | User-initiated browser links use the system browser's own session. Explicit agent launches use the agent's own credentials, network, tools and approvals; there is no BPR-wide allow-list for their destinations. Corporate policy must govern those applications separately. |

Native HTTPS enforces system certificate/proxy policy and bounded responses,
timeouts and redirects. PR REST redirects stay within the validated authority
and API namespace; GraphQL and query-bearing PR redirects are refused. Updater
asset redirects have their own bounded contract. System proxies, DNS and OS
trust services remain platform dependencies. BetterPullReview offers no
certificate-error bypass. Source owners are `github_host_profile.cpp`,
`native_https_transport.cpp`, `gh_cli_git_smart_transport.cpp` and
`github_release_update_contract.hpp` under `src/adapters/github/src/`.

## Local data and credentials

Profile-private storage includes SQLite settings, notes/drafts/receipts and
mutation recovery, retained Git objects and epoch catalogs, managed agent
workspaces, MCP snapshots and a bounded local diagnostic log. These can contain
corporate source or review text. File permissions/ACLs isolate the OS user;
BetterPullReview does not encrypt these stores itself. Apply corporate disk,
backup, account and endpoint controls accordingly. The
[storage and removal guide](../development/storage-and-diagnostics.md) gives
locations, exact inventory, retention and residuals.

Native credentials use macOS Keychain, Windows Credential Manager or Linux
Secret Service. Manual-token sessions delete their exact slots on orderly quit.
Settled device grants retain access/refresh slots with inert binding/lifetime
metadata for restart revalidation. Sign out removes exact slots and disables
the binding. No token is serialized into SQLite, diagnostics or AI context.
Windows generic credentials are protected by the user's vault boundary, not
an executable-specific ACL. Incomplete/crashed enrollment can leave orphaned
material; there is no broad vault enumeration or erase operation. Stable signed
package continuity and real native purge still need the M8/M9 courses.

## Subprocesses, IPC and untrusted inputs

| Surface | Boundary |
|---|---|
| Installed Git | Bounded object plumbing under host authority in private caches/workspaces; hooks, filters, textconv, external diff and repository scripts are not an app execution feature. No BPR credential is sent in Git argv/environment. |
| Optional `gh` | Explicit macOS opt-in, bounded arguments/I/O/time and OS file-write denial with the owned temporary object-store exception. `gh` can internally launch Git and its shell credential helper. No equivalent Windows/Linux profile is claimed. |
| Optional agents | `codex`, approved `claude` or `bpr-agent-terminal` after an explicit gesture and context disclosure. GHES policy refuses Codex. The default workspace is an isolated exact-head copy; selecting an existing checkout deliberately grants broader context. Private notes/drafts enter the supplement only when selected. Agent results never automatically publish a review. |
| Bundled agent supervisor | Lifetime/descendant cleanup, not a general sandbox; deliberate POSIX process-group escape is outside that guarantee. Windows uses its native suspended-start/Job Object backend. Native package qualification is separate from component evidence. |
| Bundled MCP helper | Frozen, bounded read-only context over stdio, linked to the core-only helper graph. No HTTP endpoint or mutation tool. Its snapshots may contain selected source and must stay private. |
| macOS updater | Fixed native trust tools (`codesign`, `spctl`, read-only `hdiutil info`), the bundled `bpr_update_helper` and exact replacement app. No shell, privileged daemon or elevation. |
| Profile activation and tour commands | The only product listener exceptions are two bounded, same-user AF_UNIX IPC endpoints under the profile lease. Activation carries only activate/new-window intent. The owner command endpoint (RFC-BPR-0045, RFC-BPR-0058) carries a closed set of local draft, hook and assist commands and the finite `bpr cycle` command registry, which the owner runs against its own draft and store; neither carries HTTP, credentials, provider access, publication authority or arbitrary commands. On Windows both are owner-private local named pipes that reject remote clients. Agent lifetime channels are local connected IPC, not network services. |

No packaged WebEngine/browser engine, Node runtime, TCP/UDP server or arbitrary
plugin loader is permitted. Installed Qt libraries/plugins are selected from
the admitted installation; hostile loader arguments and paths are refused.
Repository/Markdown/theme/agent inputs remain untrusted and bounded. Agents
are separately trusted applications; their own tools may execute code. See
the [AI contract](../architecture/ai-agent-boundary.md) and
[threat model](../architecture/security-threat-model.md) for residual risks.

## Review-tour authority and retained text

Tour drafts and bounded editable remote baselines are private authored content,
not metadata-only ledger records. The profile store and publication-recovery
store retain conflict versions and exact pending operations; purge removes known
local files and never shared GitHub discussion. An explicit Publish freezes one
finite plan and issues separate one-use journaled grants. Compatibility comments,
maintenance and description-body edits have multiple visibility boundaries.
Read-before-write reduces concurrent-writer risk but is not server compare-and-swap.
Production operations remain unavailable until their exact live qualification.

Optional tour writing uses a separate restricted text-only contract on the existing
agent host: captured local scope, no tools/MCP/hooks, bounded context and one native
undo entry. Installed agent enforcement and model quality are still unqualified;
ordinary broader agent trust remains as described above. Tour navigation and
preparation never publish, resolve, approve or mark files viewed. See the
[tour contract](../architecture/review-tours.md) and
[operator guide](../development/review-tours.md).

## Review Cycles: retained source and disclosure

A Review Cycle (RFC-BPR-0053 through RFC-BPR-0061) captures the bytes of a
folder, file or checkout the reviewer chose into the profile-private
`review-cycles.sqlite3`, unencrypted like the other stores, so it holds
corporate source exactly as captured, including untracked files the checkout's
own ignore files do not exclude. Capture never runs Git or anything the checkout configures and
never writes there. A round leaves the machine only through a channel the
profile's disclosure policy allows: the clipboard, a saved file or bundle,
terminal output, a harness reading one granted round through `bpr`, or Send to
Agent. A source of unknown or enterprise origin is refused to the two channels
that reach another program until the policy allows them, and a host-imposed
denial holds everywhere. Every local folder is unclassified today. Once a round
is copied, saved or read by a harness, clipboard history, the file's location
and the harness's own transcripts and provider requests keep it beyond
BetterPullReview's reach. `BPR_CYCLE_ROLE=consumer` and grants confine a
cooperating harness only; a program running as the same user can read the store.
Builds without agent integrations or local automation remove Send to Agent, or
the owner command endpoint and `bpr`, from the package; see the
[package guide](../development/release-packages.md#packages-without-agent-integrations-or-local-automation)
and the [threat model](../architecture/security-threat-model.md#review-cycles).

## Diagnostics, removal and distribution

Local logs contain fixed events and bounded credential-owner-sanitized detail;
that prose can still identify a service/account. The default export instead
serializes only classified operation/outcome and numeric HTTP/drop metadata,
omitting prose, identities, paths, source, comments, notes, drafts and crash
memory. The user previews the exact frozen bytes before saving a new local
file; there is no upload action. OS crash reports and external-agent logs are
outside that export boundary and require separate corporate handling. Their
complete source/secret-free qualification is pending.

Retained history is removed only by explicit pruning/cache deletion or profile
purge; no TTL/LRU silently removes accepted epochs. Profile purge stops and
drains that profile, captures an exact bounded inventory, deletes known vault
bindings before their metadata, and journals recovery. Unknown/changed entries
refuse deletion. Uninstall removes program files; it does not erase all private
data. Shared registry/updater state, updater draft recovery, exports, sibling
profiles and documented empty roots/locks remain. Secure erasure from backups,
WAL or storage media is not claimed.

The existing [publish-release skill](../../.codex/skills/publish-release/SKILL.md)
and package pipeline remain authoritative. Every binary recipient must receive
equivalent access to the exact separate corresponding-source asset; installed
notices and offers bind its versioned name, URL and digest. Corporate/offline
redistribution includes that asset. Source retention and practical replacement/
relinking obligations survive removal of an application copy. Windows/Linux and
managed packages use manual distribution. Only eligible trusted macOS installs
use the [qualified updater exception](release-updater.md), with explicit
download/install gestures; this does not waive RFC-0038's matrix or release gates.
