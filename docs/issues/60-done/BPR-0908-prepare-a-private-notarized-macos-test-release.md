---
schema: "implement-rfc.card.v1"
id: "BPR-0908"
title: "Prepare a private notarized macOS test release"
status: "Done"
rfcs: ["RFC-BPR-0014", "RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["mac"]
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"b7e92258bb6115388e1c837730bb5977cc6c4eb2","evidence":{"kind":"observation","detail":"The extracted tests-disabled app reported com.gamecoretech.betterpullreview, TeamIdentifier YTQKP2V2A8, hardened-runtime and secure-timestamp flags, no entitlement keys, and valid arm64 and x86_64 slices."}},{"criterion":"AC2","outcome":"pass","revision":"b7e92258bb6115388e1c837730bb5977cc6c4eb2","evidence":{"kind":"command","detail":"tools/release.sh --version 0.1.0 --skip-tests --skip-notarize --allow-dirty completed, including restricted Qt deployment, dependency and RPATH validation, relocated launch, Developer ID signing, DMG/ZIP creation and SHA-256 output. The normal branch contains app and DMG notarization, stapling and Gatekeeper assessment."}},{"criterion":"AC3","outcome":"pass","revision":"b7e92258bb6115388e1c837730bb5977cc6c4eb2","evidence":{"kind":"command","detail":"tools/publish-release.sh --version 0.1.0 --notes-file packaging/README.md --dry-run revalidated both checksums and passed its exact PRIVATE repository guard without creating a tag or GitHub release."}},{"criterion":"AC4","outcome":"pass","revision":"b7e92258bb6115388e1c837730bb5977cc6c4eb2","evidence":{"kind":"observation","detail":"Focused local package, extracted ZIP, read-only mounted DMG, shell/static, documentation and skill checks passed without Apple notarization submission or GitHub mutation. The missing BetterPullReview-notary profile and exact interactive store-credentials command are documented."}}]
---

# Prepare a private notarized macOS test release

The owner supplied the production bundle identifier
`com.gamecoretech.betterpullreview` and App ID prefix `YTQKP2V2A8`. This is a
narrow distribution prerequisite for a private real-GHES test. It does not
close RFC-BPR-0014, authorize a live pull-request mutation, publish a GitHub
release, or make the repository public.

- [x] The tests-disabled application uses the supplied production bundle ID;
      its Developer ID signature has the supplied team identifier, hardened
      runtime, a secure timestamp and no unneeded entitlements.
- [x] One local release command builds the tests-disabled product, deploys only
      the required Qt runtime, signs nested code and the app, verifies the
      relocated bundle, creates DMG and ZIP artifacts with SHA-256 checksums,
      and normally notarizes, staples and Gatekeeper-assesses them.
- [x] The publication procedure refuses a non-private GitHub repository before
      it can create a release. GitHub release creation remains a distinct,
      explicit final step after package verification.
- [x] Focused local verification proves the package path without contacting
      Apple's notarization service or creating, uploading or publishing a
      GitHub release. Any remaining credential/profile setup is named exactly.

## Implementation

- The production application bundle ID is
  `com.gamecoretech.betterpullreview`. The test application retains its
  separate test-only identifier.
- `macos-release` builds an `arm64;x86_64`, tests-disabled Release bundle.
- `tools/release.sh` deploys and allow-lists the required Qt runtime, rewrites
  build-cache RPATHs, verifies both slices and every dependency, signs nested
  code and the app, launches the relocated app with an isolated profile, and
  packages DMG/ZIP/checksum artifacts. Its normal path submits, staples and
  Gatekeeper-assesses the app and DMG.
- `tools/publish-release.sh` verifies the exact GitHub repository is `PRIVATE`
  before its atomic main/tag push and again before release creation.

## Remaining operator setup

`BetterPullReview-notary` is not present in this machine's Keychain. Create it
once with the secure interactive password prompt; do not put the app-specific
password in shell history:

```sh
xcrun notarytool store-credentials BetterPullReview-notary \
  --apple-id YOUR_APPLE_ID \
  --team-id YTQKP2V2A8
```

No App ID capabilities or managed capability requests are required for the
current application behavior.

## Verification

On revision `b7e92258bb6115388e1c837730bb5977cc6c4eb2`:

- `tools/release.sh --version 0.1.0 --skip-tests --skip-notarize
  --allow-dirty` completed. It built the universal tests-disabled bundle,
  validated the deployed runtime and relocated launch, signed with Developer
  ID, and emitted the DMG, ZIP and matching SHA-256 files.
- The ZIP was extracted beneath `/tmp`; bundle identity, deep strict signature,
  hardened runtime, secure timestamp, team identifier and both architectures
  were observed. The DMG was attached read-only and its enclosed app passed the
  same identity, signature and architecture checks.
- `tools/publish-release.sh --version 0.1.0 --notes-file
  packaging/README.md --dry-run` passed at that exact revision after reporting
  both checksums `OK` and the repository `PRIVATE` guard. It created no tag or
  release.
- `python3 scripts/verify_documentation.py --root .`, the 64 documentation
  verifier tests, JSON validation, skill validation, `shellcheck`, `bash -n`
  and `git diff --check` passed.

Notarization and GitHub publication were deliberately not exercised. The former
is the documented one-time Keychain setup plus a normal release command; the
latter still requires an explicit publish invocation.
