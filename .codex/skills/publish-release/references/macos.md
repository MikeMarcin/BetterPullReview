# macOS release

Run on macOS from the exact committed source:

For a coordinated release, first complete the
[all-platform compilation barrier](execution.md#compile-every-selected-platform-before-long-suites).
The macOS script's long tests must not start while another selected platform's
required compilation is still unverified.

```sh
tools/release.sh --version X.Y.Z
```

This authoritative path runs the normal test gate, builds a tests-disabled
universal x86_64/arm64 app (macOS 14.4 minimum), deploys restricted Qt, signs with
Developer ID and hardened runtime, submits/staples app and DMG, runs Gatekeeper,
and writes DMG/ZIP/SPDX/checksums and separate source under
`build/releases/X.Y.Z/`.

The normal test gate uses tests-on Release plus the native Debug residual. Pass
`--test-scope incremental --test-regressions REGEX` for a bounded patch; the
baseline remains selected. Named heavy owners are added with `--test-heavy-group`.
`--test-profile comprehensive`, major/Qt/toolchain/platform/broad/unknown scope,
requires an explicit comprehensive request; omitted scope/regressions refuse before builds or credential lookup. See the
[profile contract](../../../../docs/development/release-test-profiles.md).
Every run retains configured/selected/omitted rows and actual child results.
Normal PASS does not clear an omitted heavy obligation or the known Ubuntu RSS
release blocker. Do not run another build or suite alongside timed rows.

Expected bundle: `com.gamecoretech.betterpullreview`; team: `YTQKP2V2A8`;
identity: `Developer ID Application: Michael Marcin (YTQKP2V2A8)`.
Inspect actual signatures. Bundle IDs are not legal publisher names.
Add no entitlements unless behavior requires them; the current App ID has no
optional capabilities.

Do not publish with `--skip-tests`, `--skip-notarize` or `--allow-dirty` without
explicit acceptance of the limitation. A missing `BetterPullReview-notary`
profile is a setup stop, not a reason to skip notarization. Have the owner enter
credentials securely; never put an app-specific password on the command line:

```sh
xcrun notarytool store-credentials BetterPullReview-notary \
  --apple-id YOUR_APPLE_ID --team-id YTQKP2V2A8
```

For an existing file Keychain, pass `--notary-profile NAME --notary-keychain
/absolute/path/to/login.keychain-db` to `tools/release.sh`.
`APPLE_NOTARY_KEYCHAIN` supplies the optional default. Both app and DMG use
that selector. Verify with `xcrun notarytool history --keychain-profile NAME
--keychain /absolute/path/to/login.keychain-db` before building.

`--prepare-only` needs no Apple credentials; its ad-hoc seal does not supply
Developer ID/notarization/Gatekeeper trust. For requested variants use
`--variant agent-free` or `--variant agent-free-automation-off` with a separate
output directory. The publisher currently supports neither variants nor local
candidate names.

Require `codesign --verify --deep --strict --verbose=2`, and inspect
`codesign -d --verbose=4` for identity/team and hardened runtime. Require app
and DMG `xcrun stapler validate`; app
`spctl --assess --type execute --verbose=4`; DMG
`spctl --assess --type open --context context:primary-signature --verbose=4`.
Use actual absolute artifact paths. Never clear quarantine or weaken trust
checks to force a pass.

Standard assets: DMG, app ZIP, SPDX JSON,
`Source-BetterPullReview-X.Y.Z-macos-universal-corresponding-source.zip`,
and one `.sha256` for each: eight assets. Preserve the `Source-` name bound
to installed notices/offers. Publisher preflight invokes
`packaging/verify_release_sources.py` against actual app ZIP/SPDX/source before
remote operations. Return to the shared publication procedure.
