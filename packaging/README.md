# Packaging

This root owns distribution metadata and platform packaging. RFC-BPR-0014 M6
adds tests-disabled Windows per-user x64 MSI and baseline Linux DEB/RPM
definitions, and extends the existing universal macOS release path with exact
SBOM, notices and corresponding source. See the
[packaging guide for maintainers](../docs/development/release-packaging-maintainers.md).
Native matrix qualification, distribution terms and release trust remain
separate gates; definitions and local artifacts do not certify a release.

Agent integrations and local automation are removable at build time
([[BPR-1179]]). The `-agent-free` and `-agent-free-automation-off` presets
produce packages without them. `Install.cmake` installs only the helpers
those families keep, and the package records name what was left out; see
[build packages without agent integrations](../docs/development/release-packaging-maintainers.md#build-packages-without-agent-integrations).

Packaging must preserve the core/application/adapters/composition dependency
graph and restrict Qt plugin discovery to trusted installation paths. No browser
runtime, network server, telemetry or general remote loader is permitted. The
existing owner-private AF_UNIX profile activation channel and the RFC-BPR-0045
tour command channel are the only declared listener exceptions, bounded by the [release security policy](../docs/security/release-policy.md).
RFC-BPR-0035
adds only its user-confirmed, qualified macOS updater to the earlier manual policy.

RFC-BPR-0035 development now bundles `Contents/MacOS/bpr_update_helper` on macOS.
The existing release tool signs nested Mach-O files and normalizes their rpaths,
including this helper. [[BPR-0948]] qualifies disposable signed/notarized native
upgrade, timeout and recovery on Apple Silicon; native composition now selects
the installer for eligible trusted macOS installations. These unpublished,
qualification-only packages do not authorize release publication or replace the
developer application. Owner amendment r3 defers Intel updater execution to
nonblocking [[BPR-0950]]; live private-service acceptance and dependency-ordered
RFC closeout remain on [[BPR-0911]] and [[BPR-0912]]. Windows/Linux and managed
packages retain manual distribution. See [Application updates and recovery](../docs/development/application-updates.md)
for bootstrap, operator qualification, retained data and exact helper-recovery
commands, and the [security boundary](../docs/security/release-updater.md) for
authority, network destinations, resource limits and process inventory.

## Application icon

The unchanged supplied vector is retained as
[`icons/better_pull_review.svg`](icons/better_pull_review.svg). The application
embeds fixed PNG sizes so its windows and notification tray use the product
icon without an SVG runtime plugin. Windows builds compile the tracked ICO
through a native resource file. Tests-disabled macOS builds are application
bundles whose `Info.plist` names the tracked ICNS. The Linux SVG, PNG family,
and desktop entry supply the hicolor/application directories installed by
`Install.cmake` on Linux.

These assets and bindings are application identity, not a package-readiness
claim. Clean-machine installation and release trust remain RFC-BPR-0014 work.
Regeneration and asset
ownership are documented in [`icons/README.md`](icons/README.md).

## Release asset ordering

Build assets retain their `BetterPullReview-<version>-<target>` names.
Corresponding-source archives use
`Source-BetterPullReview-<version>-<target>-corresponding-source.zip`, with a
matching `.sha256` file, so filename ordering puts builds before source.
The package producer and publisher must agree with the exact name and URL in
installed source offers and notices. Existing releases retain their original
asset names and URLs for recipients of those builds.

## Dif asset notice boundary

RFC-BPR-0030's accepted ten-clip production provenance records
`notice_required: false` and no additional attribution or third-party notice for
the exact derivative set. The tests-enabled and tests-disabled application
targets embed only the ten committed generated strips. Editable source frames,
the manifest, provenance and digest records, generator, raw inputs, review
media, and other maintenance tools are not runtime resources.

The selected-graph boundary check confirms that inventory and keeps every Dif
resource and Qt dependency out of `core-only`. It does not produce or certify an
installable package. RFC-BPR-0014 remains the authority for final SBOM
generation, installation paths, dependency collection, signing, notarization,
and release evidence. Any later production input or rights change must update
the provenance and notice disposition before packaging.

## About page logo

Help › About shows the Game Core Tech logo
([`src/adapters/qt/assets/about`](../src/adapters/qt/assets/about/README.md)).
The mark is the owner's own, copied from their AllTheThings project, and is a
first-party asset under this project's licence and copyright: it has no
attribution, SBOM component or third-party notice. Every graph embeds the one
640-pixel PNG as a Qt resource (`/bpr/about`), and a tests-disabled Windows
image carries it in its resource bundle with the others.

The page opens the installed `THIRD-PARTY-NOTICES.txt` and `SOURCE-OFFERS.txt`
from `legal_root` (`packaging/package_runtime.py`). Moving those files means
moving `installed_legal_folder` in `src/app/src/about_desktop.cpp` with them.

## RFC-BPR-0002 dependency record

This section retains historical foundation candidates and their original
evidence limits. It is not the current runtime or credential lifecycle inventory.
Use the [corporate security overview](../docs/security/corporate-security-overview.md)
and [M8/M9 ledger](../docs/verification/RFC-BPR-0014.md) for current candidates,
the separate corresponding-source asset, unresolved vulnerability findings and
native/trust gaps.

The effect-backed GitHub adapter adds private Qt Core and Qt Network 6.8+
dependencies plus the platform credential-vault dependency. Its native HTTPS
implementation uses the TLS backend and system CA/proxy sources selected by
the target's Qt Network runtime; the application does not package an alternate
CA bundle or proxy implementation in this slice.

| Target | GitHub adapter dependency facts |
|---|---|
| macOS arm64 development host | Qt Network 6.11.1; active TLS backend reported as OpenSSL; the public tests-off Release self-test reports native platform-vault selection; the binary links Security/CoreFoundation and imports `SecItemAdd`, `SecItemCopyMatching`, `SecItemUpdate`, and `SecItemDelete`; current binary is ad-hoc linker-signed |
| Linux x86_64 full-system QEMU guest | Historical v1 Secret Service and internal native-HTTPS evidence: Ubuntu 26.04, Linux 7.0.0-28, GCC 15.2, Qt 6.10.2; adapter build includes Threads and `libsecret-1>=0.20.0`; native HTTPS test ELF directly needs Qt6Network and Qt6Core; runtime selected OpenSSL and one direct proxy candidate. The pinned BPR-0029 v2 recorded public-composition path, idle runtime sample, exact native cleanup, tests-disabled seam absence, and dependency/runtime/ELF audits passed on that snapshot |
| Windows x64 (tested on Windows 11; other versions untested, MSI floor Windows 10) | Qt Core/Network, Threads, and `Advapi32` for Credential Manager are selected by the build; native runtime, TLS/proxy, and PE dependency inspection remain BPR-0036 work |

The pinned BPR-0029 v2 Linux run completed fresh tests-on and tests-off
Release builds at 81/81 and 36/36 targets. All 40 non-native registered test
identities passed on that snapshot after the repaired gates; the
Secret Service setup/lifecycle/cleanup cases passed 3/3 plus independent exact
cleanup and hostile/unavailable/locked cases; and both public fixtures passed
with exact host and rendered state through their substituted recorded
transport/vault. The tests-off self-test reported
`PUBLIC_PATH_OK` and `NATIVE_PLATFORM_VAULT_SELECTED:true`; its executable
SHA-256 is
`78a85e15319473e39baaa6b6c8723ff7f9adbade13b48475a3f94a5622719b01`.

Linux verification froze `native_https_transport.cpp` at SHA-256
`fd7976b48241795302ded4d1a73c851064edce66364a3b50b990294ca95ab134`
and its harness at
`bd0f771cd261a7fb6b4f1f4691af93c12adc7d4fe7ced96cb1182ee8bf2369dd`.
The focused contract/fixture and architecture matrices passed 2/2, and the
architecture matrix covered all 92 hostile mutations. The uninstrumented
focused suite passed repeatedly. A controlled trace of one
trusted-success case under `strace` opened
`qt6/plugins/tls/libqopensslbackend.so`, `libssl.so.3`, and `libcrypto.so.3`.
A full-suite `strace` is not evidence because instrumentation perturbs
cancellation timing; the single-success trace is dependency evidence only.
Tracked and on-disk scans found no GitHub Actions workflow file, and the
source/build configuration selected no GitHub Actions integration.

The macOS production identity is `com.gamecoretech.betterpullreview`, registered
under App ID prefix/team `YTQKP2V2A8` without optional capabilities or managed
capability requests. `tools/release.sh --version X.Y.Z` is the narrow private
macOS distribution path: it builds the tests-disabled universal product,
deploys the allow-listed Qt runtime, signs with Developer ID and hardened
runtime, notarizes and staples the app and DMG, and now emits DMG/app ZIP/SPDX/
separate version-bound source ZIP and four checksums.
`tools/publish-release.sh` separately guards GitHub visibility as `PRIVATE`
before any tag push or release creation. Signing credentials remain outside the
repository. This slice does not satisfy the full RFC-BPR-0014 cross-platform,
SBOM, license, install/upgrade/uninstall, or clean-target acceptance matrix.

The one-time notarization setup is interactive so the app-specific password is
entered into Keychain rather than a shell argument or repository file:

```sh
xcrun notarytool store-credentials BetterPullReview-notary \
  --apple-id YOUR_APPLE_ID \
  --team-id YTQKP2V2A8
```

After that profile exists, `tools/release.sh --version X.Y.Z` performs the
Apple submissions. `--skip-notarize` exists only for local package-path
verification and does not produce a distribution-ready artifact.

For a profile stored in a specific existing file Keychain, pass
`--notary-profile NAME --notary-keychain /absolute/path/to/login.keychain-db`
to `tools/release.sh`. `APPLE_NOTARY_KEYCHAIN` supplies the optional default.
Both app and DMG submissions use that Keychain; omitting it keeps the ordinary
Keychain lookup. Verify access with `xcrun notarytool history --keychain-profile
NAME --keychain /absolute/path/to/login.keychain-db` before the release build.

The Linux observations remain development build/runtime evidence, not a signed,
bundled, or installable package claim. RFC-BPR-0014 still owns complete
transitive dependency inventory, SBOM, other-platform signing and installers,
launch, upgrade, uninstall, and clean-machine evidence.
Windows target-native execution is nonblocking for current implementation and
documentation work but remains required for the affected Windows claim and
RFC-BPR-0002 closeout. Those original credential fixtures proved exact deletion
for their recorded session-only policy. The current device-grant policy retains
a settled vault pair across orderly quit and revalidates its persisted inert
binding on restart; manual-token sessions remain session-only. Sign out and
profile purge use exact keys, never vault-wide enumeration. These changes do not
retroactively qualify signed-package native continuity. Follow the
[storage and removal guide](../docs/development/storage-and-diagnostics.md),
including shared/updater/export residuals after purge or uninstall.

`BUILD_TESTING=ON` adds the adapter-private native HTTPS seam and the
GitHub.com/GHES public composition fixture. A directly inspected
`BUILD_TESTING=OFF` macOS build exported no
`make_native_https_*_transport_for_test` symbols, omitted the fixture
translation unit and test definitions from its compile graph, and contained
none of the fixture arguments, status strings, labels, identities, paths, or
access symbols in its raw application bytes. Release/package builds must keep
tests disabled and repeat the graph, byte, symbol, and dependency audits on
the packaged target.

GitHub Actions has no role in configuring, building, testing, packaging,
releasing, signing, or verifying BetterPullReview. Packaging evidence comes
from committed source on local release-operator or corporate-managed native
machines.
