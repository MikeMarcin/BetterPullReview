---
id: "RFC-BPR-0035"
title: "GitHub Release Auto-Updater"
status: "Proposed"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0002","RFC-BPR-0014"]
pack_version: "post-v2"
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
---

# RFC-BPR-0035: GitHub Release Auto-Updater

## Owner amendment 2026-10-28 — release build identity compatibility

[[BPR-1799]] adds a separate positive release cohort number without changing
stable-channel marketing-version precedence or persisted skipped versions.
Stable tags retain `vMAJOR.MINOR.PATCH`. Canonical downloads include `build.N`;
the existing exact base-version ZIP remains a byte-identical compatibility
alias for shipped clients. A numbered staged bundle binds the base short
version and its native build number. No suffix is silently treated as newer,
and an occupied tag is never replaced. The 2026-10-29 UTC clarification permits
multiple RCs of marketing version 0.4.0 under `v0.4.0-build.N` tags, always
GitHub prereleases. Stable clients ignore them; the manual RC source offer binds
the actual RC tag. Build-tag stable promotion is refused until a separately
accepted compatible stable policy exists. See the
[release build identity design](../development/release-build-identity.md).

## Status and scope

Requested on 2026-09-18: add an RFC for an auto-updater using GitHub Releases,
with AllTheThings as implementation inspiration. This is a design and backlog
registration only. Implementation is [[BPR-0911]]; verification and closeout are
[[BPR-0912]]. Registration baseline: `1b4c24154bd1d4f1d4bea3d84c593e2fb4c3d47f`.
No updater is enabled by this RFC's registration.

Implementation mandate, 2026-09-20: implement this RFC through its four authored
milestones, now tracked by [[BPR-0946]] through [[BPR-0949]]. Local implementation
and disposable verification are authorized; live private-service access, release
publication and replacement of the developer's running application are not.
The RFC stays Proposed until its acceptance and canonical dependencies close.

Milestone 2 composes check/download controls, app-wide preferences, native private
release transport and signed macOS staging under [[BPR-0947]]. Installation was
gated on [[BPR-0948]]. The [verification ledger](../verification/RFC-BPR-0035.md)
records fixture coverage and exact signed-artifact evidence; it makes no live
private-service or Intel execution claim.

Milestone 3 implements the helper/journal protocol, cooperative lifetime locks,
multi-window draft barrier and matching healthy-start receipt. Mandate amendment
r2 authorizes local production signing/notarization for disposable qualification
only, without publication, tags, pushes or installed/developer-app replacement.
Signed native upgrade, timeout, missing-receipt and interrupted-rename recovery
courses pass on Apple Silicon. Eligible trusted macOS installations now select
the installer; [[BPR-0949]] retains the remaining native/security closeout scope.

Milestone 4 source `518c384a` fixes duplicate native Check for Updates entries
across two windows and the empty desktop, and supplies the
[operator/recovery guide](../development/application-updates.md) and
[security inventory](../security/release-updater.md). The r3 acceptance amendment
below closes [[BPR-0949]] using the accepted Apple Silicon course and current
documentation/native-menu checks. [[BPR-0911]] is Blocked on unavailable live
private-release/vault evidence; [[BPR-0912]] remains Backlog and
retains its explicit RFC-BPR-0014 dependency, which is now Auto-Verified.
The global canonical-order gate was removed by the owner on 2026-09-27.
The unchanged signed Apple Silicon helper evidence does not certify M4's menu
correction in a new signed package, Intel execution or live private-service access.

Owner acceptance amendment r3, 2026-09-20: Intel Mac updater validation is deferred
to [[BPR-0950]], outside this RFC's M4 and closeout acceptance. AC-07 and
BPR-0949:AC-01 now require the native Apple Silicon course. The follow-up must not
block M4, BPR-0949, BPR-0911, BPR-0912 or this RFC's closeout. This is an owner
decision to change acceptance, not Intel execution evidence. It waives no live
private-release/vault criterion or RFC-BPR-0014 dependency.

The first implementation targets the existing signed, notarized macOS universal
release. Windows and Linux expose manual update guidance until separately scoped
native installers and evidence exist. Corporate-managed installations retain
administrator-controlled updates. RFC-BPR-0014's initial manual-update policy
remains in force; this proposal amends it only for explicitly enabled, qualified
macOS packages when implemented. It preserves explicit RFC dependencies and
repository visibility, and does not authorize a release publication.

## Problem and outcome

Users currently download and replace BetterPullReview manually. The application
should detect a newer GitHub release, explain what is available, download and
validate the selected package, then install and relaunch after the user chooses
to restart. Automatic means discovery and the mechanical installation workflow;
it does not mean unattended replacement or forced interruption of review work.

Use `MikeMarcin/BetterPullReview` on GitHub.com as the fixed distribution source,
independent of the GitHub/GHES repository being reviewed. No custom update feed,
delta patches, background daemon, privileged installer, embedded browser,
telemetry upload or GitHub Actions workflow is introduced.

## Reference implementation

Read-only inspection of `/Users/jaeger/Documents/GitHub/AllTheThings` at commit
`2ba1b33c9886a3e20865801581a26b5ba73f2700`; its working tree was clean.

| Source | Behavior informing this design |
|---|---|
| `Sources/AllTheThings/ReleaseUpdater.swift` | Daily launch check, persisted automatic-check and skipped-tag settings, latest-release lookup, package staging, bundle/signature validation, post-quit replacement helper, install receipt and stale-mount cleanup. |
| `Sources/AllTheThings/AppDelegate.swift` | Launch maintenance and an application-menu Check for Updates action. |
| `Tests/AllTheThingsTests/ReleaseUpdaterTests.swift` | Reference test surface for updater behavior; this task did not execute its tests. |

Adapt the workflow, not its Swift singleton or AppKit effect ownership. In the
inspected implementation Later records the skipped tag; BPR gives Later and Skip
separate meanings. BPR also refuses malformed versions instead of treating any
unrecognized differing tag as newer. Its helper must not copy the reference's
forced SIGTERM/SIGKILL shutdown, blanket quarantine removal, or deletion of the
backup before successful relaunch. A generic first archive or first `.app` match
is insufficient asset identity.

BPR already emits `BetterPullReview-X.Y.Z-macos-universal.dmg` and `.zip` in
`tools/release.sh`; `tools/publish-release.sh` requires the distribution repository
to be private. The production bundle identifier is
`com.gamecoretech.betterpullreview`, with signing team `YTQKP2V2A8` as recorded in
[packaging](../../packaging/README.md). These facts govern the initial adapter.

## User experience and scheduling

Expose **Check for Updates…** in the macOS application menu, including when all
windows are closed, and the corresponding Help action on other platforms.
Settings offers **Automatically check for updates**, last successful check,
installed version, available version and last installation result. The same
application-owned service backs every window and Settings; it is not scoped to
the selected BPR profile. The feature can ship before the full Settings RFC.

On an eligible installation automatic checks default on, once after launch when
24 hours have elapsed and at most once per subsequent 24 hours while running.
Checks are quiet when current, offline or unavailable; show durable status without
repeated modal alerts. Manual checks show checking/current/unavailable/failed
results and bypass the daily interval and skipped version, but respect server
rate limits. Persist attempted and successful check times separately; failed
automatic checks wait until the next daily opportunity or explicit manual retry.
Use monotonic scheduling while running and clamp invalid/future persisted clock
facts so clock changes cause neither a request loop nor permanent suppression.

An available-update surface shows installed and target versions, a bounded plain
text summary, **View Release**, **Download Update**, **Later**, and **Skip This
Version**. Later dismisses this offer until a later scheduled check; Skip suppresses
that exact version for automatic offers. A newer version is still offered.
View Release uses the existing host-owned browser capability with a validated
fixed-repository URL. Remote notes cannot execute HTML or supply installer commands.

Download requires a gesture, shows bounded progress and supports cancellation.
Once verified, offer **Install and Restart** and **Later**. Installation requires
that explicit gesture for the exact verified candidate. No unattended install on
quit. Cancellation before replacement leaves the running app usable. During the
short replacement phase the helper finishes or recovers its transaction; the UI
must not promise cancellation once the old process has exited.

Managed policy overrides user preferences and disables automatic network access
and self-replacement. Unsupported platforms, unsigned development builds,
read-only or translocated bundles, packages running from a mounted image, and
unwritable destinations report why self-update is unavailable and offer manual
release guidance. Never elevate privileges to overcome these states.

## Release discovery and private access

Use GitHub's [latest release endpoint](https://docs.github.com/en/rest/releases/releases#get-the-latest-release)
for the stable channel. Reject drafts and prereleases even when returned by a
fixture or malformed service. Parse `vMAJOR.MINOR.PATCH` or `MAJOR.MINOR.PATCH`
with optional SemVer build metadata, compare numeric components, and reject
unknown or prerelease formats in this first channel. Equal versions, including
differing build metadata, do not reinstall; older releases never downgrade.
Do not infer ordering from publication dates, lexicographic strings or tag inequality.

The private distribution source requires explicit selection of a GitHub.com
release-access account through the existing native authentication/vault boundary.
This selection is separate from the current review profile; never silently borrow
its credential, widen its permissions or send a GHES credential to GitHub.com.
The updater receives a narrow read capability for the fixed release repository,
not a token or general provider handle. When access is absent, explain how to
select an authorized account or use manual download. This proposal does not
require a new credential system or extracting `gh` tokens.

Release access selection is persisted only as an opaque binding. Revocation,
sign-out or replacement of that binding cancels pending discovery/download and
invalidates their generations. An already downloaded candidate must reacquire a
valid release-access binding and refresh its release/asset identity before it
becomes installable. A 404 means unavailable or inaccessible, not proof of no
release; distinguish 401/403, rate limiting, offline and malformed responses.

Use the [release asset API](https://docs.github.com/en/rest/releases/assets#get-a-release-asset)
with binary Accept semantics for private assets. Handle both direct bytes and a
redirect response. Bind release ID, tag, asset ID, exact asset name, size and any
provided SHA-256 digest into one candidate. Prefer exactly one matching universal
ZIP for the initial implementation; the existing DMG remains the manual artifact.
Do not select source archives, another product, duplicate matches or a DMG merely
because ZIP validation failed.

The release transport is a separate narrow capability from PR transports. Keep
system TLS/proxy verification. Credentials go only to the fixed GitHub API origin.
Any asset redirect is HTTPS, bounded to three hops and an explicit packaged
GitHub asset-origin allowlist; strip authorization/cookies across origins and
reject all other origins before issuing requests. Signed redirect query strings
are transient secrets: never persist, log or expose them in Qt. This scoped asset
rule does not relax existing PR transport redirect prohibitions. Pin and fixture
the actual API version and asset origins during implementation; unexpected routing
fails closed, with manual guidance.

## Verification and installation transaction

Stream into an owner-only staging directory with a unique transaction identity.
Suggested initial hard limits: 1 MiB release metadata, 128 assets, 32 KiB rendered
notes, 512 MiB archive, 2 GiB expanded data, 50,000 entries, 30 seconds per metadata
request, 15 minutes per download and five minutes for unpack/validation. Enforce
actual bytes and entry counts, not only advertised lengths. One candidate may be
staged per installation; disk-space checks include both staged and backup copies.
All subprocesses have output/time bounds and host cancellation.

Reject absolute archive paths, traversal, devices, escaping links and unexpected
bundle layout. Framework symlinks are allowed only when their resolved targets
remain inside the staged bundle. Require exactly `BetterPullReview.app`, the
expected bundle identifier, candidate version equal to the offered release,
supported minimum OS, and the complete advertised universal architecture set.
Verify provided digests; missing digest metadata does not replace or weaken native
signature validation. A checksum fetched from the same release is integrity
metadata, not an independent publisher trust root.

Validate all nested code against the installed trusted production application's
designated requirement and expected signing team, with native platform trust and
notarization assessment. Unsigned/ad-hoc installations cannot bootstrap this trust.
Do not clear quarantine to bypass validation. Signing identity rotation needs a
separately reviewed trust migration; do not trust a key supplied by release notes.
Revalidate the exact staged bundle immediately before replacement and protect it
against path/link replacement. Invalid packages never become executable candidates.

Install and Restart enters an application-wide shutdown barrier. Persist drafts
and local state, stop admitting new work, cancel safe reads, and finish or durably
record indeterminate publications for existing reconciliation. Never retry a
publication as part of updating. A failed flush, canceled quit or inability to
quiesce aborts installation. All processes using the same installation must exit
cooperatively under an installation-scoped lock; do not identify them solely by
reusable PID. Timeout preserves the current app and reports failure; never kill
an application to make the update succeed.

Use a bundled, signed helper behind a narrow platform port. No script from the
release is executed. Pass a bounded transaction descriptor through an owner-only
channel, containing validated paths/identities rather than shell expressions.
The helper owns a durable journal and a uniquely owned backup. Stage on the target
filesystem before swapping, then rename the old bundle to backup and the verified
bundle into place. Record each boundary so restart recovery can reconcile crashes
between either rename without deleting the only launchable copy.

Relaunch the exact installation and require an acknowledgment from the new process
bound to transaction ID, executable identity and target version after startup and
state stores are ready. Merely starting a process or seeing a helper exit is not
success. On failed replacement restore the old bundle. On missing/failed startup
acknowledgment keep the backup and report recovery on the next viable launch;
restore only once no process is using the replacement. Never launch both versions
against the same state store. Installation must not run irreversible data migration
before this recovery boundary: use existing backward-compatible state formats or
an explicit separately verified migration/restore plan.

Delete the backup only after the matching healthy-start receipt. Cleanup is
idempotent and limited to journal-proven owned artifacts; names and age alone do
not authorize recursive deletion or mount detachment. Preserve recoverable files
when cleanup fails, with truthful local status. The first ZIP-only adapter needs
no disk-image mount helper.

## Architectural allocation

| Layer | Responsibility |
|---|---|
| Core | Typed updater state, version/asset policy, command legality, request/candidate/generation identity, stale rejection, progress/terminal deltas. Time and validation are supplied facts. |
| Application host | One app-wide updater lifetime, scheduling, release-access lease, bounded jobs, cancellation, serialized observations, preferences, install barrier and transaction orchestration. |
| GitHub/native transport adapter | Fixed-repository wire parsing and download capability, auth boundary, redirect policy and rate-limit observations. No provider types cross inward. |
| Platform update adapter/helper | Filesystem staging, archive checks, code-signature/trust assessment, exclusive installation ownership, journaled replacement and relaunch. |
| Qt | Render accepted status, progress and choices; translate gestures. No network, filesystem, installer process or policy decisions. |
| Composition and packaging | Bind fixed distribution identity, trust/managed policy, platform capability and helper; keep core-only builds free of these dependencies. |

State distinguishes idle, checking, available, downloading, verifying, ready,
quiescing, handed-off, recovered and failed. Every asynchronous observation carries
operation, generation and candidate identity where applicable. Disabling checks,
account replacement, cancellation and shutdown invalidate obsolete work. Progress
updates are coalesced and never trigger whole-workbench resets.

Persist only preferences, opaque account binding, candidate identity and bounded
transaction/receipt facts. Tokens, signed URLs, raw response bodies and private
release notes stay out of logs and receipts. Logs describe phases and typed errors
through the existing diagnostic host. User review data and credentials are never
moved with the application bundle.

## Delivery milestones

1. Bind the release/auth/version/asset contract and implement core policy with
   fake host ports, including failures and stale completion rejection.
2. Compose real check/download controls, native private-asset transport and signed
   macOS staging. Demonstrate the menu-to-host-to-projection path with fixtures.
3. Add the signed helper, shutdown barrier, journal, recovery and relaunch receipt;
   qualify on disposable signed installations before enabling production packages.
4. Record native upgrade evidence, package/security documentation and dependency-
   ordered closeout. Windows/Linux implementation remains a separate future scope.

[[BPR-0911]] is the implementation entry; split these milestones into allocated
cards when implementation is requested. This registration starts no implementation
plan or native installation. [[BPR-0912]] retains every unverified criterion.

## Acceptance criteria

- AC-01: Manual and daily automatic checks, Later, Skip This Version and persisted preferences work through the shipped UI; managed and disabled installations make no automatic requests.
- AC-02: Only a strictly newer stable release with one exact compatible asset is offered; malformed versions, drafts, prereleases, duplicate assets and stale completions are refused.
- AC-03: Private release access uses an explicitly selected GitHub.com authority; revocation cancels work, credentials never cross redirect origins, and inaccessible releases are not reported as up to date.
- AC-04: Downloads and staging enforce declared bounds and reject wrong identity, version, architecture, signer, digest, unsafe archive entries and changed staged candidates before replacement.
- AC-05: Installation quiesces every window and process using the target, preserves drafts and pending publication reconciliation, and refuses timeout without forced termination.
- AC-06: Interrupted replacement and failed relaunch retain a recoverable old application; success requires a matching new-process receipt, and cleanup touches only transaction-owned resources.
- AC-07: The packaged macOS update path on Apple Silicon upgrades an older signed version to the offered signed version, relaunches with retained user state, and reports truthful progress and terminal status.
- AC-08: Core, host, Qt and platform boundaries hold; unsupported platforms and managed packages expose truthful manual fallback; release and security documentation describe the shipped authority and network paths.

## Documentation impact and verification limits

Update installation/update instructions, release operator checks, application-wide
Settings ownership, authentication authority, network destinations and redirects,
process/helper inventory, signing/notarization policy, storage/cleanup and recovery
instructions when the feature ships. RFC-BPR-0014 and packaging's current no-updater
statements require an explicit scoped amendment then; registration changes no
current capability or security claim. RFC-BPR-0018 and RFC-BPR-0024 consume the same
application-wide controls without making update authority profile-local.

Use fixture releases and disposable installation roots for automated checks.
Native tests must not replace the developer's running application or mutate live
review repositories. Real private-service access and release publication require
their own authorized target and are not implied by this authoring task. Local
release tools produce evidence; GitHub Actions remains excluded. Record exact
artifact identities and native platforms, and leave unavailable evidence open.
