# Release updater component contract

RFC-BPR-0035 milestones 1 and 2 supply the deterministic policy and a desktop-wide
check/download/staging composition. The application menu and General Settings
share one service, including when the last review window closes on macOS. The
Qt adapter retains one native macOS application-menu item for that service's
lifetime, independent of window and empty-desktop menu bars. Repeated
attachment, focus changes and window teardown cannot accumulate check actions.
Other platforms expose the corresponding Help action on each window.
[[BPR-0947]] implements discovery and verified staging. [[BPR-0948]] adds the
helper, shutdown coordination, replacement and recovery protocol described below.
Disposable signed/notarized installations qualify that protocol on Apple Silicon.
Native composition selects the installer only for an eligible trusted macOS
installation; every verified candidate still requires an explicit Install and
Restart gesture. No release was published or developer application replaced.

## Distribution and authority

The release source is always `MikeMarcin/BetterPullReview` on GitHub.com, whose
releases are public.
It is independent of the repository and GitHub/GHES profile being reviewed. The
native composition offers a credential-free public lease first ([[BPR-1669]])
and any accepted native account through the existing authentication/vault
boundary. Nothing is picked by hand: the stored binding is kept while it is
still offered, and otherwise the first offer is selected ([[BPR-1671]]). Each
offer supplies only a `release_update_access_port` capability; the host/core
see no token, general provider handle, CLI token extraction or remote URL. Persist an opaque account
binding only: an account's is the lowercase hex SHA-256 of its authority, and the
public releases' is the SHA-256 of a fixed label, so the store keeps one form
([[BPR-1672]]). The live lease identity is separate, nonzero, never reused during
its application session, and revoked atomically when the account loses authority.

The host observes that lease before and after effect execution and again on the
serialized completion lane. Replacement/revocation cancels old jobs and advances
core generation before replacement results may be accepted. A staged candidate
is discarded on replacement; it cannot become installable by selecting another
account. Install refreshes release metadata through the same live authority and
requires equality of the complete staged candidate, including any digest. The
installer rechecks authority before publishing its journal and handing ownership
to the helper. A completed handoff is a local ownership transfer, not an installed
result; later credential revocation cannot falsely relabel it as an aborted
handoff. Stale host observations still cannot resurrect a discarded candidate.

The release-only transport contract is pinned in
[github_release_update_contract.hpp](../../src/adapters/github/src/github_release_update_contract.hpp).
The implementation stays in the designated native HTTPS effect owner; release
metadata normalization and the revocable lease have a separate private adapter.
Accounts are offered from accepted native GitHub.com sessions, labelled for an
explicit choice. No current review account is silently selected. The saved binding
is a hash of nonsecret authority identities, not a login or credential. Restoring
it requires the corresponding accepted account to be available again. Losing that
authority cancels its live lease; closing a review window alone does not sign out
the desktop's selected release account. Acquisition is bounded to one vault
request and one coalesced latest selection.

| Fact | Bound value |
|---|---|
| API origin | `https://api.github.com` |
| Metadata operation | `GET /repos/MikeMarcin/BetterPullReview/releases/latest` |
| Asset operation | `GET /repos/MikeMarcin/BetterPullReview/releases/assets/<selected numeric ID>` |
| API version | `2022-11-28`, explicitly pinned; independent of GitHub documentation's current default |
| Accept | `application/vnd.github+json` for metadata; `application/octet-stream` for asset bytes |
| Initial asset redirect origin | Exact HTTPS origin `https://release-assets.githubusercontent.com`, no subdomain wildcard; maximum three redirects |
| Production bundle | `BetterPullReview.app`, `com.gamecoretech.betterpullreview`, team `YTQKP2V2A8` |
| Required architectures | Both `arm64` and `x86_64` in the initial universal package |

GitHub documents the [latest release](https://docs.github.com/en/rest/releases/releases#get-the-latest-release)
and [binary asset](https://docs.github.com/en/rest/releases/assets#get-a-release-asset)
operations, including direct-byte and redirect responses for assets. The single
asset origin above is the initial packaged allowlist, not a claim that live
private-service routing was observed. Unexpected routes must fail closed pending
explicit contract revision and fixtures. Metadata redirects remain prohibited.
No credential or cookie may accompany a cross-origin asset redirect, signed query
strings remain transient secrets, and existing PR redirect prohibitions stay intact.
The adapter must validate scheme, exact authority/port, userinfo, path and hop
count before each request, using system TLS/proxy verification. HTTP 404 maps to
inaccessible/unavailable; 401, 403, rate limits, offline and malformed responses
remain distinguishable. None is an up-to-date result.

View Release is a host browser request constructed from the fixed GitHub.com
repository and the percent-encoded validated tag. Do not pass a service-provided
`html_url`, signed asset URL, HTML note or installer command through presentation.

## Stable versions and exact selection

The core accepts `[v]MAJOR.MINOR.PATCH[+BUILD]`, with decimal unsigned 64-bit
components, no leading zeroes except `0`, and nonempty dot-separated ASCII
alphanumeric/hyphen build identifiers. The entire tag is at most 128 bytes.
Prerelease suffixes, unknown formats, whitespace and numeric overflow refuse.
Comparison uses only the three numeric components. Equal build variants and
older releases never reinstall.

For a strictly newer stable release, require exactly one uploaded
`BetterPullReview-<tag without leading v>-macos-universal.zip`. Build metadata,
when present, stays in this exact filename. No source archive, other product,
single-architecture package, duplicate name/ID or DMG fallback is eligible.
Bind release ID, exact tag, parsed version, asset ID/name, declared size and
optional parsed `sha256:` digest into one candidate. A malformed provided digest
refuses; an absent digest does not weaken the later native trust requirement.

| Bound | Initial limit |
|---|---|
| Metadata / assets / release notes | 1 MiB / 128 / 32 KiB |
| Archive / expanded data / archive entries | 512 MiB / 2 GiB / 50,000 |
| Metadata / download / verification time | 30 seconds / 15 minutes / 5 minutes |

Release notes stay untrusted text up to the panel, which shows them in the
pull request overview's reader (`markdown_body_view`, as the overview composes
it) with that reader's own rules ([native Markdown](native-markdown.md)). The
panel binds no link or image dispatch, so a link opens nothing, an image stays
its text placeholder and nothing loads; the release page is one button away
([[BPR-1671]]).

Core enforces asset count and selected advertised archive size. The native
transport enforces headers, content types, actual streaming bounds, cancellation
and deadlines without buffering an entire ZIP. Credentials are supplied only to
the first API request. Every redirect uses a fresh manager without authentication
or cookies; the metadata path never follows redirects.

macOS staging uses pinned LibArchive 3.8.9 with only its ZIP reader enabled at the
call site. Optional codecs and archive tools are disabled; system zlib provides
deflate. The provider pins the published source SHA-256 and bundles the upstream
license notices. Extraction creates files relative to held directory descriptors
with no-follow/exclusive creation. It refuses traversal, external links, devices,
duplicates, excess entries/expansion and unexpected top-level content. AppleDouble
sidecars are counted but not materialized as application resources. Framework
links must resolve inside the application. Cleanup owns only its private
transaction, retains its lock through asynchronous disposal, and never changes
the running application.

Validation compares the declared SHA-256 when present, the exact tag version,
bundle identifier, both Mach-O slices and their deployment commands. It validates
all architectures and nested code against the installed production designated
requirement and pinned team, then asks system execution trust to assess the bundle.
The installed trust source is checked as well. No quarantine bypass, signing
identity or privilege elevation is used. A content/inode manifest detects changed
staging before and after validation and at revalidation. The signed fixture proves
these checks on this Apple Silicon host. Disposable packaged upgrades also pass;
Intel execution is the separate nonblocking BPR-0950 follow-up under owner
amendment r3; it is not inferred from universal headers.
The core also requires completed download bytes to equal the selected size.
Passing fake verification establishes orchestration only, not native validation.

## Policy and host lifecycle

The core owns command legality, scheduling decisions, preferences, one accepted
candidate, one pending operation and typed status. The host supplies explicit
wall/monotonic facts. A one-second desktop timer supplies opportunities; core
decides whether a request is due. The application service loads preferences before
admitting controls and serializes/coalesces their nonsecret snapshots through an
owner-private `state/v1/release-update.sqlite3` store. Automatic checks, the skipped
version, attempt/success times and opaque account binding survive orderly restart.
Foreign schemas, corruption and storage failures are refused and shown as
session-only state. No release notes, URLs, archives or credentials enter SQLite.

Automatic discovery defaults on for an eligible installation. A check records
attempt time before its effect; success time changes only for an accepted valid
release observation. Runtime spacing is monotonic and a failed/cancelled attempt
still waits 24 hours for the next automatic opportunity. Manual checks bypass
that interval and Skip, while retaining the current server retry delay. Invalid
persisted attempts are discarded; future attempts clamp to the startup wall time
and wait at most a day. Invalid/future success facts are removed. Runtime wall
clock changes do not accelerate a retry or suppress one indefinitely.

Later hides the current offer without recording Skip. Skip records exactly the
parsed semantic version for automatic offers, so a newer version is unaffected.
Disabled automatic discovery, managed policy and unsupported installations issue
no automatic request. Unsupported, untrusted, read-only, translocated and
mounted-image installations have distinct manual-fallback reasons.

Every request carries application session, request ID, generation, release-access
lease ID, operation and complete candidate where applicable. Bounded equality
rejects stale/duplicate observations before any candidate can be published.
Disable, cancel, authority replacement and shutdown invalidate pending policy.
Download and verification require exact accepted candidate identity; worker
exceptions become typed failures. An absent installer remains explicitly disabled,
including when native validation reaches `ready`. `BPRManagedUpdates=true` in
the application preference domain supplies the macOS managed-policy refusal.
Read-only volumes (including mounted read-only disk images), App Translocation,
untrusted installations and Windows/Linux receive manual fallback. Writable disk
images are identified by exact device in the bounded native `hdiutil info -plist`
inventory; malformed or unavailable inventory refuses eligibility. Automated
application fixtures force an unavailable native production composition; the
explicit fixture composition supplies fake capabilities and cannot use live
release transport or a system browser.

The host confines mutable state and projection to the decision executor and runs
blocking capability work on the effect executor. Four unfinished jobs are allowed,
including cancelled jobs waiting for terminal delivery. Reentrant user ingress is
bounded to 64 admissions per decision turn; replacement feedback beyond that bound
revokes authority. Shutdown and authority replacement have priority slots. Download
progress coalesces to one queued observation per job. Projection exceptions cannot
veto acceptance. Quiescence includes retained jobs, running effects, queued
deliveries and decision ingress. Executors and clock outlive that quiescence.
Opaque artifact destruction must be nonblocking; adapters schedule owned cleanup
on their effect service when the host releases a stale or replaced capability.

## Qualified installation protocol

The macOS build bundles `Contents/MacOS/bpr_update_helper`. Native installation
requires the trusted production bundle and its exact helper; the helper verifies
its enclosing bundle again. Fixed absolute executables are launched with argument
arrays, closed inherited descriptors and an empty environment, without a shell,
privilege elevation, remote command or forced termination. The application reaps
only helpers it launched; stopping that bounded monitor does not kill a child.
The release tool's existing nested-Mach-O signing and rpath normalization include
this helper. A local ad-hoc build is compile/link evidence, never production trust.

Before opening profile stores, a participating native process joins an
installation-scoped, owner-private sibling control directory. Admission,
transaction and shared running locks serialize startup and replacement. Each
participant has a uniquely named, locked inode; reusable process numbers grant no
authority. Native running and participant descriptors remain held until kernel
process exit, including after desktop teardown. New starts are refused while an
active transaction exists, except the exact target executable/version carrying
its matching startup transaction. Uncooperative older releases cannot be assumed
to participate; their upgrade eligibility remains a native qualification concern.

Preparation copies the opaque verified staging capability to a uniquely owned
target-volume transaction directory. It needs at least 4 GiB free space and uses
a five-minute copy bound. Native trust, complete candidate identity, content/inode
manifests and exact executable digest are checked again. The durable journal binds
the installation, parent and transaction inodes, old/new trees, candidate and up
to 64 live participant inodes. Journals are at most 64 KiB; the participant scan
is bounded to 4,096 entries. No path supplied by release metadata is executable.
The journal also binds the canonical owner-private application-state root and its
inode. The new process must accept that exact root before opening stores; an
absent, aliased or replaced root refuses startup instead of selecting default
state from the helper's empty environment.

Every participating desktop freezes new window/profile work and its editors,
captures all profiles' draft text, then requests the existing orderly shutdown.
Inline drafts, raw conflicting editor text and whole-review text are saved as
inert recovery records; publishing text is marked indeterminate. Existing
publication reconciliation remains authoritative and updating never retries a
write. Save failure refuses quit and re-enables the windows. Each process records
a durable acknowledgment only after profile workers, layouts, preferences,
restoration and updater state have drained without a persistence failure.
Disappearance without that acknowledgment cannot authorize replacement.

Recovery text is append-only in the profile's private
`state/v1/update-draft-recovery.sqlite3`, bounded to 4,096 records and 16 MiB total,
with 64 KiB per body. Existing versions are retained, not overwritten. Startup
opens a read-only, copyable recovery window; it grants no publication authority
and warns that indeterminate writes must be checked on GitHub. Overflow, corrupt
or foreign schemas and failed writes refuse the barrier. No existing store is
migrated irreversibly, and user state is never moved with the application bundle.

The helper waits at most 30 seconds for every captured acknowledgment and the
exclusive running lock. Timeout aborts the prepared transaction and preserves the
old app; it never signals a process. Immediately before replacement it repeats
native trust and exact-tree checks. Same-volume exclusive renames move the old
app to the journal-owned backup and the candidate into place. A flushed journal
records both sides of each rename and the single exact-executable launch.

A launch is not success. The new process waits for startup and state-store
readiness, then writes a receipt bound to transaction, executable digest, exact
version and participant inode. The helper waits at most 60 seconds for that
receipt. Failure retains the backup; rollback requires exclusive process
ownership and never runs alongside a live replacement. Recovery reconciles actual
old/new/backup identities and does not retry a launch. A terminated helper can be
resumed explicitly with its `--recover` operation using the absolute installation
path and exact journal transaction ID. If the installed path is absent, use the
helper inside that transaction's `backup.app`. This is a diagnostic recovery
interface. The [operator/recovery guide](../development/application-updates.md)
documents journal discovery, exact absolute arguments, ordinary cooperative Quit
and interpretation of helper exit/result. Its signed backup-helper operation is
qualified on Apple Silicon; there is no automatic recovery wizard or attended
self-service usability claim.

Only the matching healthy receipt permits backup cleanup. Before the first unlink,
a bounded 16 MiB inode inventory is durably written. Cleanup uses no-follow,
descriptor-relative traversal and removes only inventoried entries; absent entries
are already-cleaned successes. A changed inode or new file is retained and reports
recovery-required, so partial deletion can be retried without a recursive sweep.
Terminal journals and failed candidate copies are retained; no age/name sweep is
authorized. The next viable desktop reports a journal-proven terminal result
separately from current discovery state. Active interrupted transactions refuse
normal startup until explicit helper recovery; a user-facing recovery launcher is
not yet qualified.

The [verification ledger](../verification/RFC-BPR-0035.md) distinguishes fixture
fault courses, real peer processes and signed older-to-newer native upgrade,
timeout, missing-receipt and rollback courses. These use synthetic private state,
network denial and disposable installations. Owner amendment r3 defers Intel
updater execution to [[BPR-0950]] outside M4/RFC closeout. Live private-release
access and dependency-ordered closeout remain [[BPR-0911]]/[[BPR-0912]] obligations. The
[security boundary](../security/release-updater.md) inventories network, helper,
storage, cleanup and trust policy; neither
local signing authorization nor milestone acceptance authorizes publication.
