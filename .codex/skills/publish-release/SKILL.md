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

## Select and prepare

For a coordinated release or a resumed run, read
[execution and recovery](references/execution.md) before dispatching native work.
Use one durable phase record to reconcile existing jobs and schedule the shared
Windows/WSL host. The [qualification procedure](references/optimized-qualification.md)
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
   its full commit and use that commit on every producer. Run the
   relevant normal native test gate before the tests-disabled distribution build;
   the macOS script runs its own normal gate. Record failures and configured
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
- Preserve M8/M9 native, vault, crash/privacy and vulnerability gaps in claims and
  release notes. Check current findings and exact owner/security dispositions;
  do not manufacture approval or report failed gates as passed. Offline audits
  do not close qualification. Profile purge is not uninstall and excludes shared
  updater/registry state.

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

## Publish once

`tools/publish-release.sh` runs on clean `main`. With no manifest it selects the
standard macOS eight-asset set under `build/releases/VERSION/`. Repeat
`--manifest /absolute/path/to/INSTALLER.release.json` to select signed Windows
and Linux outputs; add `--macos` to include the standard macOS set too.
It verifies final products, complete runtime archive/SPDX, source binding,
producer commit, variant/baseline names and signature receipts before remote
operations, deduplicating only identical native assets and refusing collisions.
Native signatures, installer extraction and qualification must already have
passed on the producers; this offline preflight is not release trust approval.

Review notes/artifacts and preflight the selected set:

```sh
tools/publish-release.sh --version X.Y.Z --notes-file /absolute/notes.md \
  --macos --manifest /absolute/windows/INSTALLER.msi.release.json \
  --manifest /absolute/ubuntu/INSTALLER.deb.release.json --dry-run
```

Omit unrequested platforms. For an already-public repository, add `--public`
to both preflight and publication. The default continues to require private
visibility; neither mode admits the other state. The publisher preserves
exact repository identity and selected visibility guards,
exact version/commit checks, checksum/source binding and atomic
`main`/annotated-tag push. macOS variants still require a separate publisher
extension; do not treat native manifests as arbitrary asset upload lists.
`origin` must have one canonical GitHub HTTPS or SSH push URL for
`MikeMarcin/BetterPullReview`. A wrong, aliased or multiple push destination is
refused. Visibility and destination are rechecked before each remote mutation.

For a coordinated release from the Mac host, use its existing macOS producer
and publisher plus the signing PC's native Windows station and Ubuntu 26.04
WSL2 producer. The owner selected this Linux route for the next release on
2026-10-11; Ubuntu 24.04 and RHEL are not required release targets. Ordinary
Linux binaries may be produced under WSL2. The tested environments are Windows
11 and Ubuntu 26.04 under WSL2/WSLg; release notes say other Windows and Linux
versions may work but are untested ([[BPR-1743]]). Final installation
qualification remains separate. Keep signing credentials and
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
downloads. The standard macOS route needs all eight assets. Other routes need
their declared package/archive/source/SPDX/checksum/provenance/signature set,
not a fixed count borrowed from macOS.

The owner-authorized incremental normal/Debug-residual and explicit heavy profile
contract is [release-test-profiles](../../../docs/development/release-test-profiles.md).
Normal PASS covers selected rows only; known resource failures and separate
artifact/source/legal/native trust obligations remain explicit.
