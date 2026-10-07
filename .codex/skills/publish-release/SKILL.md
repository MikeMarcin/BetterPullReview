---
name: publish-release
description: Build, sign, notarize, verify, and publish a BetterPullReview macOS release to its private GitHub repository. Use for BetterPullReview release preparation or publication; do not use for ordinary development builds.
---

# Publish a BetterPullReview release

Produce release artifacts from committed source before creating any tag or
GitHub release. The repository and every release must remain private.

## Prepare

1. Inspect `git status`, the current branch, recent tags, `CMakeLists.txt`, and
   the intended release notes. Do not discard unrelated changes.
2. Choose the SemVer with the user when it is not already explicit. Update the
   CMake project version and user-facing release notes, then commit that exact
   release source.
3. Confirm `gh repo view MikeMarcin/BetterPullReview --json visibility` reports
   `PRIVATE`. Stop if it does not.

## Package and verify

Run:

```sh
tools/release.sh --version X.Y.Z
```

This is the authoritative local macOS package path. It runs the normal test
gate, builds a tests-disabled universal app, deploys the restricted Qt runtime,
signs with Developer ID and hardened runtime, submits and staples the app and
DMG, performs Gatekeeper checks, and writes DMG/ZIP/SPDX/checksum artifacts and a
separate version-bound corresponding-source ZIP under `build/releases/X.Y.Z/`.

Do not use `--skip-tests`, `--skip-notarize`, or `--allow-dirty` for a published
release unless the user explicitly accepts the resulting limitation. A missing
`BetterPullReview-notary` Keychain profile is a setup stop, not a reason to skip
notarization. Do not add entitlements unless application behavior changes to
require one; the current App ID has no optional capabilities.

Create the missing profile once, using the secure password prompt rather than
putting an app-specific password on the command line:

```sh
xcrun notarytool store-credentials BetterPullReview-notary \
  --apple-id YOUR_APPLE_ID \
  --team-id YTQKP2V2A8
```

Review the package output and release notes. Then run the non-mutating remote
preflight:

```sh
tools/publish-release.sh --version X.Y.Z --notes-file /absolute/path/to/notes.md --dry-run
```

## Publish

Creating the annotated tag, pushing it, or creating the GitHub release is an
external mutation. Do it only when the user has asked to publish this prepared
version. Run the same command without `--dry-run`:

```sh
tools/publish-release.sh --version X.Y.Z --notes-file /absolute/path/to/notes.md
```

The script rechecks that `MikeMarcin/BetterPullReview` is `PRIVATE` immediately
before pushing and immediately before release creation. GitHub release access
inherits repository access; there is no separate public/private switch for an
individual release. Stop on any partial failure and report the tag, push, and
release state without automatically retrying a potentially completed mutation.

For a requested prerelease, add `--prerelease` to both preflight and publication.
This sets GitHub's prerelease flag and leaves the stable Latest release in place.

Verify the returned release URL, tag, all eight uploaded assets (DMG, app ZIP,
SPDX, corresponding-source ZIP, and one SHA-256 file for each), and repository
visibility. The source ZIP must be the exact versioned asset referenced by
SOURCE-OFFERS.txt and notices inside the app; the publication script verifies
that binding offline before any remote operation.

Every binary recipient must retain equivalent access to that source asset at
no additional charge. For corporate/offline redistribution or a recipient
who loses repository access, provide the exact asset through the original
distribution channel. Retain the asset immutably while the binaries remain
available and throughout every applicable source-offer/license retention
period. Retention is indefinite until the distribution owner documents that
all obligations have ended; do not replace the asset or rebind its tag.
Legal approval of terms, practical build/relink material, recipient access and
retention remains part of release readiness, not a result of the offline check.

Never include credentials, private repository content, build logs, or transient
test evidence in the release.
