---
schema: "implement-rfc.card.v1"
id: "BPR-0908"
title: "Prepare a private notarized macOS test release"
status: "In Progress"
rfcs: ["RFC-BPR-0014", "RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["mac"]
---

# Prepare a private notarized macOS test release

The owner supplied the production bundle identifier
`com.gamecoretech.betterpullreview` and App ID prefix `YTQKP2V2A8`. This is a
narrow distribution prerequisite for a private real-GHES test. It does not
close RFC-BPR-0014, authorize a live pull-request mutation, publish a GitHub
release, or make the repository public.

- [ ] The tests-disabled application uses the supplied production bundle ID;
      its Developer ID signature has the supplied team identifier, hardened
      runtime, a secure timestamp and no unneeded entitlements.
- [ ] One local release command builds the tests-disabled product, deploys only
      the required Qt runtime, signs nested code and the app, verifies the
      relocated bundle, creates DMG and ZIP artifacts with SHA-256 checksums,
      and normally notarizes, staples and Gatekeeper-assesses them.
- [ ] The publication procedure refuses a non-private GitHub repository before
      it can create a release. GitHub release creation remains a distinct,
      explicit final step after package verification.
- [ ] Focused local verification proves the package path without contacting
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
