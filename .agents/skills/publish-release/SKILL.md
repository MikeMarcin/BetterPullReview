---
name: publish-release
description: Prepare, package, sign, verify, and publish BetterPullReview releases for macOS, Windows, and Linux through the repository's native release tools and approved GitHub repository. Use for release preparation or publication, not ordinary development builds.
---

# Publish a BetterPullReview release

Build the requested platform set from one committed source revision and SemVer.
Prepare and verify every selected artifact before creating a tag or GitHub
release. Publication defaults to requiring a private repository. Use `--public`
only for an authorized release after the repository has separately become
public; the publisher never changes visibility. Use local or target-native
tools; GitHub Actions stays disabled and has no role in the release workflow.

## Release candidate build identity

Use the BPR-only [cohort procedure](../../../docs/development/release-build-identity.md).
Ordinary local configure/builds allocate nothing. After committing the source,
collect `_bpr/release-observed.json` from each selected native graph into one
external `bpr.release-inputs.v1` configuration, with named producers, complete
observed flags/settings/toolchains and the previous actual Windows MSI identity.
The clean controller, on `main` or that version's `release/X.Y.Z` branch,
fetches/reconciles its authoritative branch and allocates:

```sh
tools/publish-release.sh --prepare-candidate --candidate-inputs /absolute/inputs.json \
  --cohort-output /absolute/allocation.json
```

Commit exactly its counter and numbered receipt, then freeze without another
source change:

```sh
tools/publish-release.sh --freeze-candidate N --cohort-output /absolute/cohort.json
```

Give identical cohort bytes to every producer. Configure with
`-DBPR_RELEASE_COHORT=/absolute/cohort.json -DBPR_RELEASE_PRODUCER=declared-name`;
`tools/release.sh` takes `--cohort /absolute/cohort.json --producer declared-name`.
A numbered Windows package also requires `--previous-msi /absolute/previous.msi`
so the native producer checks real per-machine upgrade ordering and the exact
installer digest in the committed authoritative Windows floor before building
or signing. Unchanged retries reuse N; changed source/configuration gets another
identity. Never remove the durable common-Git reservations to reuse a number.

Initial numbered RC publication requires `--cohort /absolute/cohort.json` and
`--prerelease`. It derives the distinct `vVERSION-build.N` tag; macOS short
versions keep the base marketing version. Repeated 0.4.0 RCs need no patch bump.
Default artifact directories and filenames include `VERSION-build.N`; an existing
numbered output directory is refused without replacement. Build-tag stable
promotion is refused; a compatible stable route is a separate owner decision.
The macOS ZIP also has an exact byte-identical legacy base-version alias and
checksum for installed updaters. The publisher verifies that alias and refuses
mixed source/configuration/cohorts. Existing unnumbered published releases retain
the separate guarded append/promote flow; they are never retroactively numbered.
Version selection, native qualification, security and publication authority stay
separate. In particular v0.4.0 and its preview assets remain immutable.

## Work from a release branch

Cut `release/X.Y.Z` from `main` when release work starts and set `main` to the
next version, so the next release's work does not move this candidate. Fixes
reach the branch by `git cherry-pick -x` from `main`. Allocate, freeze, qualify
and publish from the branch, then merge it into `main` after the stable
release. The [branch rules](../../../docs/development/release-build-identity.md#release-branches)
cover the shared build-number sequence and the single Windows upgrade line:
carry each newly recorded Windows floor to `main` before `main` allocates.
Releasing straight from `main` still works when nothing else is landing there.

## Select and prepare

For a coordinated release or a resumed run, read
[execution and recovery](references/execution.md) before dispatching native work.
Use one durable phase record to reconcile existing jobs and schedule the shared
Windows/WSL host. The execution reference records the two-hour clean-release
target, source-only request admission and separate machine/wait accounting;
the target changes no qualification obligation. The [qualification procedure](references/optimized-qualification.md)
uses the owner-authorized normal Release baseline, named patch regressions and
native Debug residual for bounded incremental releases. Major, broad, unknown,
Qt/toolchain/platform or ownership changes require comprehensive native Release
and Debug. Read the exact profile scope and omitted-heavy inventory before work.
Cross-request hostile-tool or native verdict reuse is unready. Only the existing
suite runner's exact same-invocation bootstrap receipts may avoid duplicate rows.

1. Inspect the working tree, branch, recent tags, `CMakeLists.txt`, release notes
   and available native hosts. Preserve unrelated changes. Choose the SemVer,
   platform set and variants with the user when they are not explicit. An
   explicitly macOS-only release does not acquire Windows/Linux requirements.
2. Read only the selected guides:
   - [macOS](references/macos.md): universal Developer ID app, notarization,
     DMG and ZIP through `tools/release.sh`.
   - [Windows](references/windows.md): native x64 MSI and manual Azure Artifact
     Signing for application/helpers and installer.
   - [Linux](references/linux.md): Ubuntu 26.04 x86_64 DEB, per-user
     archives and approved OpenPGP signing.
3. Read `docs/development/release-packaging-maintainers.md`,
   `docs/development/release-packages.md` and
   `docs/security/release-policy.md`. Check the current
   `docs/verification/RFC-BPR-0038.md` ledger and applicable native cards;
   historical approvals do not qualify changed artifacts or another platform.
4. Complete blocker fixes and run the small changed public-interaction courses
   on relevant native hosts before the expensive release gates. Update the CMake
   version and user-facing notes, then commit the common release source. Record
   its full commit and use that commit on every producer. Before starting any
   long test suite, complete the [physical-host compilation prerequisites](references/execution.md#compile-each-physical-host-before-its-long-suites):
   configure and build the required Release and Debug test graphs and the
   tests-disabled distribution graph on that physical host. Independent hosts may advance separately; Windows and
   WSL share a host and finish their builds before long tests. Run the short
   [native risk preflight](references/execution.md#run-native-risk-checks-before-long-suites)
   in Release and Debug before the long suites. Prefer verified independent Linux;
   otherwise perform early WSL discovery before extended Windows work, without
   overlapping or interrupting jobs on the shared host. Fix compilation and
   functional preflight failures and retain every selected platform's required
   final receipts. Then run the relevant
   native test gates and proceed with packaging; the macOS script owns its
   normal gate. Record failures and configured
   skips. Do not publish dirty-source or skipped-test candidates without the
   user's explicit acceptance of the limitation.
5. Confirm `gh repo view MikeMarcin/BetterPullReview --json visibility` reports
   `PRIVATE` by default, or `PUBLIC` for an explicitly selected `--public`
   release. Do not change visibility as part of release publication. A missing
   native host, identity, signing key, credential or required
   decision is a prerequisite, not permission to substitute an unsigned build or
   different OS baseline. Complete agent-owned work before handing human-only
   setup to the available `wizard` skill.

Full packages are the default. For requested `agent-free` or
`agent-free-automation-off`, use matching native presets and artifact suffixes.
Variants retain the same installation identity. Keep producer outputs separate;
do not combine different variants or Linux baselines under colliding asset names.

## Verify the complete set

Record platform/baseline, architecture, variant, commit, version, artifact names,
final digests and native trust results. Preparation does not authorize signing
or publication of that version.

- Audit the actual packaged runtime with `scripts/verify_release_package.py`
  using its matching native build, expanded runtime, SPDX, separate source ZIP
  and exact producer commit. Inspect `--help` for current arguments. Preserve
  Windows linker maps and byte-identical producer images for the PE audit.
- Bind corresponding source to installed notices, `SOURCE-OFFERS.txt`,
  provenance and SPDX. macOS uses
  `Source-BetterPullReview-VERSION-macos-universal-corresponding-source.zip`;
  CPack uses its emitted names. Never rename an asset already named in installed
  source references.
- Verify native signatures/timestamps, then SHA-256 of final delivered bytes.
  Signing an executable, MSI or RPM changes bytes. Keep installed inventories,
  sidecars and final artifact provenance consistent with those bytes.
- Preserve M8/M9 native, vault, crash/privacy and vulnerability gaps in the
  qualification and security records. Check current findings and exact
  owner/security dispositions; do not manufacture approval or report failed
  gates as passed. Surface actionable release-specific limitations using the
  compact description guidance below. Offline audits do not close qualification.
  Profile purge is not uninstall and excludes shared updater/registry state.

Every binary recipient must retain equivalent access to the exact source asset
at no additional charge. Supply it through the original distribution channel
for corporate/offline redistribution or lost repository access. Retain source,
digest and tag immutably while matching binaries remain available and through
applicable retention periods. Retention is indefinite until the distribution
owner documents that all obligations have ended. Legal terms, practical
build/relink material, recipient access and retention remain release readiness
obligations; offline checks do not approve them.

Assets contain intended binary/source archives, notices, SPDX, checksums and
approved public verification material. Exclude credentials, unrelated private
data, private keys, build logs and transient test evidence. Corresponding source
is the deliberate exception to excluding private repository content; distribute
it only through the approved release channel. Public source and binary access
must be equivalent after an authorized public visibility change.

## Write a compact release description

Every release body, stable or prerelease, has one shape. Read
[release notes](references/release-notes.md) before writing one and copy its
fill-in template, not a published body: those written before the standard
differ from one another and are not models. The reference holds the template,
what each part is for, how to word an entry for users and a worked example.
The parts, in order:

1. A labelled notice, only in the case described below.
2. The download table: columns `System` and `Download`, one row and one
   installer link for each system being published, in the order macOS,
   Windows, Ubuntu.
3. With an Ubuntu row, the one fixed sentence that links the signature
   verification instructions.
4. `## Changes since [X.Y.W](previous-stable-release-url)`, then the entries
   under `### New`, `### Changed` and `### Fixed`.

The body has no `#` title, because GitHub shows the release title directly
above it. The download table opens the body; only a labelled notice may come
before it. A prerelease body has the same shape, and its change list is the
one the stable release will carry. The description ends after the changes.

Use the selected version, actual minimum OS versions and exact uploaded asset
URLs; include only the platforms being published, and the Ubuntu line only
with an Ubuntu row. Label a universal macOS build **Universal**. The publisher
sets the GitHub release title to `BetterPullReview TAG`. Write
each change as one short, plain entry for one user-visible outcome, sorted
into New, Changed or Fixed, with enough detail to explain what changed. Omit
empty groups, internal issue/RFC IDs, process narration, marketing filler and
a concluding summary.

Keep qualification reports, source/build identities, test counts and failures,
timing policies, waivers, vulnerability inventories and dispositions in their
release records. Keep source/licence/retention explanations and installation or
signature-verification procedures in linked, versioned documentation or approved
public release assets. Publish the approved Linux public key and full fingerprint
through that channel; do not paste a key block or verification tutorial into the
description. Make any needed links short and point to the exact release's
records or instructions, not a moving branch or private evidence.

Add a brief warning or instruction only when a release-specific security issue,
breaking change, known limitation or required installation/migration action
materially affects the user's decision or next step. Put it before the download
table as the reference's labelled notice: state the impact/action and link to details.
Do not append routine signing assurances, a general verification/limitations
section or standing policy disclaimers. Compact notes do not relax
qualification, disclosure, source access or signing requirements.

The publisher refuses a notes file in four cases: it opens with anything but
the download table or a labelled notice, it has no download table before
`## Changes since`, it has no `## Changes since` heading, or it still holds a
`{{` template slot. The publisher does not check wording or links.

## Publish once

`tools/publish-release.sh` runs on clean `main` or clean `release/X.Y.Z`. With no manifest it selects the
numbered macOS set under `build/releases/VERSION-build.N/`: eight canonical
assets plus the exact legacy ZIP alias and checksum. Repeat
`--manifest /absolute/path/to/INSTALLER.release.json` to select signed Windows
and Linux outputs; add `--macos` to include the standard macOS set too.
It verifies final products, complete runtime archive/SPDX, source binding,
producer commit, variant/baseline names and signature receipts before remote
operations, deduplicating only identical native assets and refusing collisions.
Native signatures, installer extraction and qualification must already have
passed on the producers; this offline preflight is not release trust approval.

Review notes/artifacts and preflight the selected set:

```sh
tools/publish-release.sh --version X.Y.Z --cohort /absolute/cohort.json --notes-file /absolute/notes.md \
  --macos --manifest /absolute/windows/INSTALLER.msi.release.json \
  --manifest /absolute/ubuntu/INSTALLER.deb.release.json --dry-run
```

Omit unrequested platforms. For an already-public repository, add `--public`
to both preflight and publication. The default continues to require private
visibility; neither mode admits the other state. The publisher preserves
exact repository identity and selected visibility guards,
exact version/commit checks, checksum/source binding and atomic
branch/annotated-tag push; from a release branch it pushes that branch, never `main`. macOS variants still require a separate publisher
extension; do not treat native manifests as arbitrary asset upload lists.
`origin` must have one canonical GitHub HTTPS or SSH push URL for
`MikeMarcin/BetterPullReview`. A wrong, aliased or multiple push destination is
refused. Visibility and destination are rechecked before each remote mutation.

For a coordinated release from the Mac host, use its existing macOS producer
and publisher plus the signing PC's native Windows station and Ubuntu 26.04
WSL2 producer. The owner selected this Linux route for the next release on
2026-10-11; Ubuntu 24.04 and RHEL are not required release targets. Ordinary
Linux binaries may be produced under WSL2. The tested environments are Windows
11 and Ubuntu 26.04 under WSL2/WSLg; the versioned package guide records that
other Windows and Linux versions may work but are untested ([[BPR-1743]]).
Final installation qualification remains separate. Keep signing credentials and
private keys on their approved producer PC. Return each producer's complete
final artifact directory, including `.release.json`, runtime archive, SPDX,
separate source asset, checksums, provenance and public signatures, to the Mac
publisher. Preserve names and bytes; use absolute manifest paths there for the
shared preflight. Its offline checks do not verify native trust independently.
Remote station dispatch uses the configured station workflow and requires
available toolchains and signing identities; documentation is not observed
remote readiness. No version change, station launch or release follows merely
from preparation; select version/platforms when the owner requests the release.

Reuse each producer's existing nonsecret signing configuration outside source.
The Windows preparation records approved tool/metadata paths and an isolated
`AZURE_CONFIG_DIR` for the dedicated profile signer. Read that configuration,
validate its approved identity/profile and use those inputs; never inspect token
caches or switch to an admin credential. Azure user authentication may expire
or require MFA, so use a wizard only when renewed human login is actually needed.
For the Ubuntu WSL producer, use the existing approved
`bpr.release-gpg-vault.v1` configuration with `package.py --gpg-vault-config
/absolute/producer-config.json`. Packaging unlocks its exact key automatically
through Windows Credential Manager before any build/output and signs in the
bound GPG home with pinentry disabled. Do not ask for the passphrase again after
one successful enrollment. A missing credential/logon session or failed key
check stops production; the [Linux guide](references/linux.md) describes recovery.
Remote invocation must run as the same local Windows signing account with its
credential set and working WSL interop. v0.3.1 exercised that approved producer
context; another logon or station still needs its own readiness check. No
secret/configuration is transferred to the Mac.

Create/push the tag and release only when the user requests publication of the
prepared version and platform set. Run the same
command without `--dry-run`. Add `--prerelease` to both commands when requested;
it preserves stable Latest. Use one tag/release for a coordinated platform set.
Inspect existing tag/release state read-only before retrying; do not retag,
overwrite immutable source or retry an uncertain mutation. On partial failure,
report which tag, push, release and assets actually exist.

Verify URL, peeled tag commit, draft/prerelease/Latest state, exact selected
assets and the selected repository visibility. Download uploaded products and compare SHA-256
with final producer bytes; verify source binding and native trust again on the
downloads. The numbered macOS route needs its eight canonical assets plus the exact legacy ZIP alias and checksum. Other routes need
their declared package/archive/source/SPDX/checksum/provenance/signature set,
not a fixed count borrowed from macOS.

The owner-authorized incremental normal/Debug-residual and explicit heavy profile
contract is [release-test-profiles](../../../docs/development/release-test-profiles.md).
Normal PASS covers selected rows only; known resource failures and separate
artifact/source/legal/native trust obligations remain explicit.
