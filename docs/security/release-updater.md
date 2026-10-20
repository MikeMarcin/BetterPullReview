# Release updater security boundary

This document describes the implemented RFC-BPR-0035 macOS path, not final
cross-platform release certification. The [component contract](../architecture/release-updater.md)
defines the typed APIs and exact policies; the [verification ledger](../verification/RFC-BPR-0035.md)
binds actual evidence and gaps. Installation and recovery instructions are in
[Application updates](../development/application-updates.md).

## Authority and network

The updater is one desktop-wide application service, independent of the selected
review profile. The user explicitly chooses an accepted native GitHub.com account
with access to the private fixed distribution repository. The native adapter
retrieves its credential through the existing vault and exports only a revocable
release-read capability. No token reaches core, Qt presentation, SQLite, helper arguments or
logs. GHES and GitHub CLI credentials cannot be borrowed. Sign-out/replacement
revokes the lease, cancels work and invalidates pending generations; installation
refreshes the exact release and asset under the same authority.

| Destination | Purpose and credential rule |
|---|---|
| `https://api.github.com/repos/MikeMarcin/BetterPullReview/releases/latest` | Stable metadata GET, API `2022-11-28`, JSON Accept, selected credential on the API origin only; redirects refused |
| `https://api.github.com/repos/MikeMarcin/BetterPullReview/releases/assets/<id>` | Exact selected asset GET with binary Accept; direct bytes or bounded redirects |
| `https://release-assets.githubusercontent.com` | Exact allowlisted HTTPS asset origin, at most three hops; fresh credential/cookie-free requests |
| Fixed repository page on `https://github.com` | Explicit View Release through the system browser; browser sign-in is independent |

TLS certificates and system proxy policy remain enforced. Unexpected origins,
userinfo, ports and routing fail closed before another request. Signed redirect
queries are transient secrets and never appear in presentation or persistent
state. This asset-specific redirect rule changes no PR transport rule. HTTP 404
means unavailable/inaccessible; authentication, rate limits and offline failures
cannot become an up-to-date result. Live private-service routing/vault behavior
has not been observed under the current mandate; the exact allowlist is enforced
and fixture-tested, not inferred from successful access to a live release.

## Package and process boundary

Only a strictly newer stable version with one exact universal ZIP is selected.
Remote release notes are bounded text, shown as Markdown with HTML off and no
resource loading or link following; they cannot supply executable code, helper
arguments, update-source configuration, HTML or a fetch. The expected production
identity is `com.gamecoretech.betterpullreview`, team `YTQKP2V2A8`. Both arm64 and
x86_64 slices, deployment commands and nested code must validate. The installed
trusted designated requirement is the trust anchor; unsigned/ad-hoc installations
cannot bootstrap it. A release digest adds integrity checking and never replaces
native signature and notarization assessment. No quarantine is removed.

LibArchive 3.8.9 performs bounded ZIP extraction with system zlib. Absolute paths,
traversal, devices, duplicate entries and escaping links are refused; framework
links must stay inside the bundle. Held directories, no-follow operations and
content/inode manifests bind the validated tree through preparation and replacement.

The only updater process paths are fixed `/usr/bin/codesign`, `/usr/sbin/spctl`
and read-only `/usr/bin/hdiutil info -plist` observations, the exact bundled
`Contents/MacOS/bpr_update_helper`, and the exact replacement application.
Native Mach-O/header and filesystem inspection happens in the designated adapter.
There is no shell, archive-supplied installer, daemon, privileged service or
update listener. Helper/application launches use argument arrays, closed inherited
descriptors and an empty environment. A separate installation journal binds the
private state root before a replacement opens any store.

| Resource | Enforced bound |
|---|---|
| Metadata, assets, displayed notes | 1 MiB, 128 entries, 32 KiB |
| Archive, expansion, archive entries | 512 MiB, 2 GiB, 50,000 |
| Metadata/download/verification | 30 seconds / 15 minutes / 5 minutes |
| Target-volume preparation | At least 4 GiB available, five-minute copy bound |
| Helper shutdown/startup acknowledgment | 30 seconds / 60 seconds |
| Journal / captured participants / participant directory scan | 64 KiB / 64 / 4,096 |
| Backup cleanup inode inventory | 16 MiB |
| Draft recovery per profile | 4,096 records, 16 MiB total, 64 KiB per body |

## State, failure and cleanup

Core owns compact policy, command legality and request/candidate identity. The
application host owns scheduling, effects, cancellation and serialized completion;
Qt owns menus, controls and incremental presentation. Native adapters own network,
vault, files and processes. The updater introduces no Qt/core effect bypass.

The explicit Install gesture enters a desktop-wide barrier. Drafts and existing
publication-reconciliation records must be durable, all consumers must drain and
every captured participant must acknowledge and release its kernel lifetime lock.
A timeout never kills a process. Replacement is journaled as same-volume renames.
A helper exit or process launch is insufficient success: only the new process's
matching transaction/version/executable/participant receipt permits backup cleanup.
Rollback requires exclusive process ownership and preserves a recoverable old
copy. A later viable launch reports the durable outcome without retrying a
publication. Recovery text is read-only and grants no publication authority.

Cleanup removes only journal/inventory-proven owned entries and retains changed
or foreign files. Failed candidates and terminal journals may remain; there is no
name/age sweep. Preferences store an opaque binding and scheduling/skip facts;
journals store bounded nonsecret candidate/path/identity facts. Tokens, signed
URLs and raw private metadata/notes are excluded. Draft recovery intentionally
contains private review text in the profile's owner-private store; it must not be
included in diagnostic exports or public evidence.

Managed packages disable automatic access and replacement; unsupported and
ineligible installations give manual guidance without elevation. No irreversible
review-store migration is part of this updater. Same-user/admin tampering,
unreported media failure and power-loss durability beyond the tested journal
boundaries are not established by cooperative locking or disposable fault tests.
The ledger's native upgrade/recovery proof is Apple Silicon with fixture release
authority and synthetic state. Owner amendment r3 defers Intel updater execution
to [[BPR-0950]], outside M4 and RFC-BPR-0035 closeout. Live private-service/vault
qualification remains required on [[BPR-0911]]/[[BPR-0912]], as does the separate
RFC-BPR-0014 release matrix. The deferral is not evidence of Intel behavior.
