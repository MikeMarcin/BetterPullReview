# Release updater component contract

RFC-BPR-0035 milestones 1 and 2 supply the deterministic policy and a desktop-wide
check/download/staging composition. The application menu and General Settings
share one service, including when the last review window closes on macOS.
[[BPR-0947]] implements discovery and verified staging; [[BPR-0948]] still owns
installation, shutdown coordination, replacement and recovery. Install and Restart
is disabled even after a successful native staging result. No package has been
published or upgraded as evidence for this milestone.

## Distribution and authority

The release source is always private `MikeMarcin/BetterPullReview` on GitHub.com.
It is independent of the repository and GitHub/GHES profile being reviewed. The
native adapter acquires an explicitly selected release-access account
through the existing native authentication/vault boundary. It supplies only a
`release_update_access_port` capability; the host/core see no token, general
provider handle, CLI token extraction or remote URL. Persist an opaque account
binding only. The live lease identity is separate, nonzero, never reused during
its application session, and revoked atomically when the account loses authority.

The host observes that lease before and after effect execution and again on the
serialized completion lane. Replacement/revocation cancels old jobs and advances
core generation before replacement results may be accepted. A staged candidate
is discarded on replacement; it cannot become installable by selecting another
account. Milestone 3 must additionally refresh release/asset identity and revalidate
the exact staged bundle immediately before an install gesture can take effect.

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
| Metadata / assets / plain-text notes | 1 MiB / 128 / 32 KiB |
| Archive / expanded data / archive entries | 512 MiB / 2 GiB / 50,000 |
| Metadata / download / verification time | 30 seconds / 15 minutes / 5 minutes |

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
these checks on this Apple Silicon host; an Intel execution and packaged upgrade
remain later acceptance, not inferred from universal headers.
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
exceptions become typed failures. The absent installer remains explicitly disabled,
including when native validation reaches `ready`. `BPRManagedUpdates=true` in
the application preference domain supplies the macOS managed-policy refusal.
Read-only volumes (including mounted read-only disk images), App Translocation,
untrusted installations and Windows/Linux receive manual fallback. Automated
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

Native quiescing, helper handoff, journal recovery and healthy-start receipt states
will be added with their actual effects in milestone 3. This component does not
make application-level shutdown, installation or relaunch claims.
