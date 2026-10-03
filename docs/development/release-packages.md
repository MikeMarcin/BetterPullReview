# Local release packages

RFC-BPR-0014 supplies the package/maintenance foundation; RFC-BPR-0038 owns
remaining native and release-security qualification. Available-host composition
evidence is candidate-specific. The native matrix, release trust and vulnerability gates remain open;
the exact macOS source-distribution approval has the limits recorded below.
The existing project `publish-release` skill and
`tools/release.sh` remain the sole macOS release workflow. GitHub Actions is not
used. See the [release-readiness allocation](../architecture/release-readiness.md)
and [evidence ledger](../verification/RFC-BPR-0014.md) for exact candidates and
open native/security gates.

## Native inputs and outputs

All distribution graphs use Release, `BUILD_TESTING=OFF`,
`BPR_DISTRIBUTION_BUILD=ON`, pinned Qt 6.11.2 and the audited dependencies.
Build from committed, clean source. `app-release` is a development build;
`core-only` remains independently buildable/testable without Qt/providers.
There is no new core SDK or embedding API.

| Native build host | Definition and route | Required operator inputs |
|---|---|---|
| macOS | `macos-release`; universal x86_64/arm64, minimum macOS 13; app/ZIP, full release also DMG | Existing Qt/macOS toolchain; local prepare mode needs no Apple credential |
| Windows 10/11 x64 | `windows-app-release`; WiX 4 per-user MSI under `%LOCALAPPDATA%/Programs/BetterPullReview` | Native MSVC x64 developer environment, Ninja, WiX 4, Python 3.10+, Qt provider SBOM, `VCToolsRedistDir`, and `BPR_MSVC_REDISTRIBUTION_NOTICE` pointing to the licensed toolchain's applicable redistribution notice |
| Ubuntu 24.04 LTS x86_64 | `linux-package`; DEB and per-user archive | C++23 compiler/library meeting the build's checks, Qt provider SBOM, `patchelf`, `readelf`, `ldd`, Debian packaging tools and libsecret and libxcb development files |
| RHEL 10 x86_64 | `linux-package`; RPM and per-user archive | Native RHEL packaging/development tools and the same runtime requirements; CPU floor x86-64-v3 |

Build prerequisites include CMake 4.4.2, Ninja, Python 3.10+ and a C++23
compiler/library accepted by the root feature checks. The inspected macOS release
configuration records AppleClang `21.0.0.21000101`. M9's native Windows producer
records MSVC `19.51.36257.0` / toolset `14.51.36231`, Visual Studio 18 Community,
WiX 4.0.6 and Qt's `msvc2022_64` package. These identify observed producers, not
every compatible toolchain. No Linux compiler/libstdc++ package baseline has yet
completed native qualification. Installed Git 2.39+ is required for the Git path;
optional `gh` and agents are separately installed and explicitly selected.

Linux deliberately rejects Ubuntu 26.04 and other undeclared build baselines.
Both targets have a glibc 2.39 ceiling on required ELF symbol versions. The
staged runtime records actual GLIBCXX requirements; the native system package
dependency generator supplies corresponding system-library constraints. A newer
C++23 toolchain is not permission to raise the runtime baseline. Exact compiler,
libstdc++, CPU and installability evidence must be qualified on each native target.
Qt libraries/plugins are private; libstdc++/libgcc, OpenSSL, X11/XCB, libsecret and
other OS libraries are system dependencies, not silently bundled libraries.
Linux selects X11 (`qxcb`) with XWayland on Wayland desktops. Native Wayland
plugins are not included or claimed.

## Unpublished preparation

Run from the checkout. Choose a new output directory each time:

```sh
tools/release.sh --version 0.1.3 --prepare-only --skip-tests \
  --output-dir build/releases/local-candidate
```

This uses the existing universal build, restricted Qt deployment, both-slice
dependency/deployment-floor checks and relocated launch smoke. It includes
notices/source references, applies only an offline ad-hoc signature, and emits
an app, local ZIP, separate version-bound source ZIP, SPDX SBOM and checksums.
It does not read signing identities, change
Keychain, contact Apple, sign with Developer ID, tag, push or publish. Ad-hoc
sealing permits local execution; it is not Gatekeeper or distribution trust.
`--skip-notarize` is different: it still uses Developer ID signing.

Windows (inside the activated native x64 toolchain environment):

```sh
cmake --preset windows-app-release
python packaging/package.py --build build/windows-app-release \
  --generator WIX --output build/releases/windows-candidate
```

Linux on its exact baseline:

```sh
cmake --preset linux-package
python3 packaging/package.py --build build/linux-package \
  --generator DEB --output build/releases/ubuntu-candidate
# On RHEL 10, use --generator RPM and a different new output directory.
```

The native driver builds the application and both helpers, runs CPack, requires
one native installer/inventory, and emits checksums, an SPDX sidecar, a separate
version-bound source ZIP and source revision provenance. Linux also emits
`*-user.tar.gz` from the same audited runtime. None of these Windows/Linux outputs is signed or published by this
driver. Installer signing and independent trust are later explicit gates.

## Installation and removal boundaries

Per-user installation is the default: copy the macOS app to the user's
Applications folder; use the Windows MSI's per-user route; or unpack the Linux
`*-user.tar.gz` into a dedicated versioned directory under the user's local
application directory and run its `better_pull_review` executable. Linux archive
dependencies must already be present; the matching DEB/RPM metadata records them.
Do not set global Qt or library paths. Managed DEB/RPM installation uses `/usr`
and the package manager's administrative route instead. Those packages install
the launcher, desktop entry and scalable hicolor icon, plus the private runtime
under `/usr/lib/betterpullreview`.

The installed app locates its MCP helper and agent supervisor beside its actual
executable, with the platform-emitted filenames. macOS additionally carries
`bpr_update_helper`. Plugin paths are restricted before QApplication, ambient
plugin-path overrides are removed, and arbitrary plugin-loading CLI options are
refused. The app never uses developer Qt paths as a fallback.

On Windows the tests-disabled image keeps its compressed Qt resources outside
itself ([[BPR-0974]]): the application icon, the Dif strips and the
KSyntaxHighlighting syntax definitions ship as `better_pull_review.rcc` beside
the executable, and the installer and runtime inventory carry it as declared
runtime data. The image registers that file before it reads any resource and
refuses to start, printing `INSTALLED_RESOURCES_REFUSED:better_pull_review.rcc:`
and `missing`, `unreadable`, `digest-mismatch` or `not-a-resource-file`, unless
the file is byte for byte the one it was built with. KSyntaxHighlighting's 54 KB
theme data stays inside the image, because the pinned framework initializes it
unconditionally. Each shipped Windows image (application and both helpers) has
a `VERSIONINFO` resource naming the vendor, product, description, version and
original file name, and a Release image names its program database by file name
only (`/DEBUG /PDBALTPATH:%_PDB%`). The database stays in the build tree and is
not installed. None of this is a signature: the images remain unsigned until
Authenticode signing, which the owner has deferred.

The Windows MSI remembers its selected per-user directory in its own HKCU
installer value so repair/uninstall resolve the same directory, including a
custom location. CPack supplies the normal installed-location metadata. Use
native backslash paths with `msiexec`; M8's forwarded-slash command was refused
before installation. The native M8 run used an elevated SSH token and therefore
does not prove the ordinary-user desktop route. M9 now audits stripped PE images
with byte-identical producers, full linker maps and exact upstream DLL evidence;
its 16-image Windows audit passes for the recorded unsigned artifact. The map is
trusted build evidence, not publisher trust or proof of complete CFG call-site
coverage. That artifact also predates the separate-source disposition: a revised
native MSI still needs production and qualification.

Uninstalling program files does not purge profiles, credentials, exported user
files, desktop registry or update-recovery records. Storage & privacy's explicit
profile purge retires that profile first; cancellation does not restart it.
See [Storage, diagnostics and removal](storage-and-diagnostics.md) for the actual
controls, migration/recovery, credentials and exact residuals, and the
[maintenance ownership contract](../architecture/release-maintenance.md#m6-retained-lease-maintenance-composition).
Windows/Linux and managed installations retain manual updates. Only the existing
qualified, user-confirmed macOS updater exception applies to eligible trusted
bundles; an ad-hoc candidate does not qualify.

## Exact dependency and license evidence

`packaging/package_runtime.py` creates a closed runtime inventory and hashes
deployed bytes after relocation (and after final macOS sealing/stapling). It
joins the selected Qt producer's SPDX graph with statically compiled SQLite,
KSyntaxHighlighting, Quill/fmt, Glaze, the macOS-only LibArchive input and the
embedded JetBrainsMonoNL Nerd Font faces ([[BPR-1270]]).
`packaging/dependencies.json` pins their complete source archives; cached or
downloaded mismatches are refused. ECM is identified as build-only. The font's
licence texts and its Nerd Fonts glyph-set licences and attributions are
retained under `third_party/jetbrains-mono-nerd-font/` and carried whole into
`THIRD-PARTY-NOTICES.txt` (`NOTICE_FILES` in `packaging/package_runtime.py`, which
the security audit checks too); its SPDX package is declared `NOASSERTION`
because it aggregates OFL-1.1, MIT, CC-BY-4.0 and Apache-2.0 material.

The installed `ThirdParty` directory (macOS: `Contents/Resources/ThirdParty`)
contains `THIRD-PARTY-NOTICES.txt`, `SOURCE-OFFERS.txt`, source provenance and
selected Qt producer provenance. Corresponding source is a separate asset:
`BetterPullReview-VERSION-PLATFORM-corresponding-source.zip`, where PLATFORM is
`macos-universal`, `windows-x64` or `linux-x64`. It is not embedded in the app,
installer or per-user archive. Both installed notices and source offers point
to its exact `releases/download/vVERSION/ASSET` location and SHA-256; the
installed provenance also binds the producer commit and upstream input hashes.
The SPDX hashes those installed references, and package audits verify the
separate asset's digest and complete source inventory. macOS preparation writes
it beside the app ZIP; CPack stages it outside the installed tree and the native
driver copies it beside the MSI/DEB/RPM. The corresponding checksum and native
provenance include it. Qt stays dynamically linked. The source-offer document
describes replacement with ABI-compatible modified libraries and local resealing of a macOS copy; it grants
no updater trust. All embedded syntax/theme attribution and complete licensed
data are retained, not mislabeled as MIT just because the engine is MIT.

Every binary recipient must have equivalent source access at no additional
charge. The private GitHub release holds binaries and source under the same
access policy. Corporate/offline redistribution must deliver the exact source
asset alongside its binaries; if repository access changes or the URL is
unavailable, the distributor must supply the asset through the original
distribution channel. Keep the asset, digest and version tag immutable while
any matching binary is distributed and throughout every applicable retention
period afterward. Retention is indefinite until the accountable distribution
owner documents that all source-offer/license obligations have ended. Package
checks cannot prove future retention or recipient access; release readiness
requires the owner to establish and maintain those arrangements.

First-party distribution terms and aggregate mixed MIT/LGPL/GPL syntax-data
obligations need owner/legal release approval. The exact revised macOS inventory
from `1c429749` received that scoped source/distribution decision at M9 event 469,
hand-back 470. It does not approve revised Windows/Linux artifacts, different
bytes, vulnerabilities, trust or publication. Actual source access and retention
remain ongoing distribution obligations. Metadata uses `NOASSERTION`
where no single license has been concluded; it is not a waiver. Windows also
requires the native operator's applicable Microsoft runtime notice. Exact
artifact license/source completeness remains an independent M9 release gate.

The full existing macOS path emits DMG, app ZIP, SPDX and source ZIP plus four
checksum files. The existing private publication script requires all eight
assets and verifies the source reference inside the actual app ZIP against
the SPDX and separate source ZIP before any remote operation. Its nonmutating
offline sub-check is `packaging/verify_release_sources.py`; `--help` lists the
explicit artifact and producer arguments. No source URL is fetched by it.
Do not run the publication script, submit notarization, or apply release signatures without explicit
authority for the prepared version.

## Recipient verification and release stop conditions

Obtain the package, its checksum, SPDX, corresponding-source ZIP/checksum and
publisher identity through the approved distribution channel. A checksum from an
untrusted source is not authentication. Verify the actual downloaded bytes before
installation and preserve the exact asset names. On macOS/Linux, run from the
asset directory so relative filenames inside checksum files resolve correctly:

```sh
cd /absolute/path/to/downloaded-release
shasum -a 256 -c BetterPullReview-X.Y.Z-macos-universal.zip.sha256
shasum -a 256 -c BetterPullReview-X.Y.Z-macos-universal.spdx.json.sha256
shasum -a 256 -c BetterPullReview-X.Y.Z-macos-universal-corresponding-source.zip.sha256
```

Replace the version/platform with the exact delivered names and verify the DMG
or installer checksum as well. On Windows, `Get-FileHash -Algorithm SHA256
-LiteralPath 'C:\absolute\candidate.msi'` must match the supplied digest; do the
same for the source asset and SPDX. Hash verification does not install anything.

Independent trust checks for a *signed* candidate use these native interfaces:

| Platform | Check and required result |
|---|---|
| macOS | `codesign --verify --deep --strict --verbose=2 /absolute/BetterPullReview.app`; inspect `codesign -d --verbose=4` for bundle `com.gamecoretech.betterpullreview`, expected team `YTQKP2V2A8` and hardened runtime. `xcrun stapler validate /absolute/BetterPullReview.app` and `spctl --assess --type execute --verbose=4 /absolute/BetterPullReview.app` must pass. Validate the DMG staple and assess it with `spctl --assess --type open --context context:primary-signature --verbose=4 /absolute/candidate.dmg`. |
| Windows | `signtool verify /pa /all /v C:\absolute\candidate.msi`, plus each signed delivered executable/DLL as applicable. Compare the signer/chain and timestamp with the approved publisher, not merely a zero exit. The current MSI is unsigned. |
| Linux | `rpmkeys --checksig /absolute/candidate.rpm` requires the independently provisioned approved signing key. For a DEB/archive delivered with a detached signature, `gpg --verify /absolute/candidate.deb.asc /absolute/candidate.deb` requires the approved key fingerprint. Corporate repository distribution instead validates its signed repository metadata and package digest under that repository's policy. The native driver does not yet produce these signatures; a missing signing route/artifact remains a release blocker. |

Do not disable OS trust checks, clear quarantine or enroll an unknown certificate
to make a candidate pass. Record source/artifact/signature identities and results
separately from package production. An ad-hoc `codesign` verification proves only
the local seal; it cannot satisfy the Developer ID/notarization checks.

The [release security policy](../security/release-policy.md) defines emitted
hardening, analysis, sanitizer/fuzz, secret and vulnerability gates. M9's recorded
32 macOS / 30 Windows Qt 6.11.2 Grype matches remain unresolved (both raw gates
exit 2; earlier 21/19 reports remain historical), as do the full native
vault/privacy/runtime matrix and external dependency hardening. The
[corporate security packet](../security/corporate-security-overview.md) records
their implications. A completed documentation milestone changes none of these
release stop conditions.

## Focused candidate audit and hostile course

`scripts/verify_release_package.py` audits an expanded candidate without
building, signing or publishing. Supply its actual build directory, SPDX and
exact clean producer revision. A later verifier/documentation-only revision
may reuse the package only while product/build/package inputs are unchanged.
The audit requires the separate source ZIP beside the supplied SPDX (or an
explicit `--source-archive` path), all installed file hashes, helper/plugin capability names,
declared static provenance, tests-disabled compile graph and native symbol/byte
evidence. macOS also checks every packaged native library reference and RPATH.
Native Windows/Linux loaded origins and full trust/license review are separate
qualification evidence, not implied by the static check.

For the macOS hostile course, use an external canonical disposable root and the
installed qualification guard. Every path argument below expands absolutely;
replace the checkout, candidate and producer revision with the exact inputs:

```sh
repo=/absolute/path/to/BetterPullReview
candidate="$repo/build/releases/local-candidate"
producer_revision=REPLACE_WITH_EXACT_40_CHARACTER_SOURCE_COMMIT
scratch=$(mktemp -d /Users/your-user/bpr-package-course.XXXXXX)
python3 "$repo/.agents/skills/implement-rfc/scripts/qualification_guard.py" \
  --allowed-root "$scratch" --target-root "$scratch" --live-root "$repo" \
  --live-file "$repo/.mask/skill-config/implement-rfc/project.json" \
  --live-file "$repo/.mask.lock.json" -- \
  python3 "$repo/tests/end_to_end/release_package_course.py" \
  --package "$candidate/BetterPullReview.app" \
  --build "$repo/build/macos-release" \
  --sbom "$candidate/BetterPullReview-0.1.3-macos-universal-local.spdx.json" \
  --source-revision "$producer_revision"
```

The course alters only a copy, isolates the child home, observes the shipped
Cocoa plugin, sockets/processes and positive packaged Qt loading, then exercises
missing helpers/plugins, unsafe loader overrides, identity/hash/byte/RPATH
controls and real MCP snapshot read/traversal/stale-binding refusals. It restores
and reaudits the copy and original, reaps children and removes its internal
temporary directory. Remove the now-empty outer scratch root after completion.
It does not certify install/uninstall, real authentication/vault, crash artifacts,
signing trust or another native architecture.

`verify_runtime_surface.py --native-platform /absolute/path/to/executable`
selects Cocoa, Windows or XCB instead of development's offscreen plugin. That
switch alone does not isolate the user's home or perform the complete package
course; use the guarded course for an installed macOS candidate. The manual
`runtime_surface_native_probe.py` checks the inspector against its own loopback
listener/connected socket and child process. Run it under the station lease and
qualification guard in a disposable root on remote stations. A passing WSL
probe does not qualify an undeclared Linux distribution or package baseline.
