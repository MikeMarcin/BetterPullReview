# Local release packages

RFC-BPR-0014 M6 supplies package definitions and available-host composition
evidence. It does not approve distribution terms, certify the native matrix or
authorize publication. The existing project `publish-release` skill and
`tools/release.sh` remain the sole macOS release workflow. GitHub Actions is not
used. See the [release-readiness allocation](../architecture/release-readiness.md)
and [evidence ledger](../verification/RFC-BPR-0014.md) for exact candidates and
open native/security gates.

## Native inputs and outputs

All distribution graphs use Release, `BUILD_TESTING=OFF`,
`BPR_DISTRIBUTION_BUILD=ON`, pinned Qt 6.10.3 and the audited dependencies.
Build from committed, clean source. `app-release` is a development build;
`core-only` remains independently buildable/testable without Qt/providers.
There is no new core SDK or embedding API.

| Native build host | Definition and route | Required operator inputs |
|---|---|---|
| macOS | `macos-release`; universal x86_64/arm64, minimum macOS 13; app/ZIP, full release also DMG | Existing Qt/macOS toolchain; local prepare mode needs no Apple credential |
| Windows 10/11 x64 | `windows-app-release`; WiX 4 per-user MSI under `%LOCALAPPDATA%/Programs/BetterPullReview` | Native MSVC x64 developer environment, Ninja, WiX 4, Python 3.10+, Qt provider SBOM, `VCToolsRedistDir`, and `BPR_MSVC_REDISTRIBUTION_NOTICE` pointing to the licensed toolchain's applicable redistribution notice |
| Ubuntu 24.04 LTS x86_64 | `linux-package`; DEB and per-user archive | C++23 compiler/library meeting the build's checks, Qt provider SBOM, `patchelf`, `readelf`, `ldd`, Debian packaging tools and libsecret development files |
| RHEL 10 x86_64 | `linux-package`; RPM and per-user archive | Native RHEL packaging/development tools and the same runtime requirements; CPU floor x86-64-v3 |

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
notices/source, applies only an offline ad-hoc signature, and emits an app,
local ZIP, SPDX SBOM and checksums. It does not read signing identities, change
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
one native installer/inventory, and emits checksums, an SPDX sidecar and source
revision provenance. Linux also emits `*-user.tar.gz` from the same audited
runtime. None of these Windows/Linux outputs is signed or published by this
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

The Windows MSI remembers its selected per-user directory in its own HKCU
installer value so repair/uninstall resolve the same directory, including a
custom location. CPack supplies the normal installed-location metadata. Use
native backslash paths with `msiexec`; M8's forwarded-slash command was refused
before installation. The native M8 run used an elevated SSH token and therefore
does not prove the ordinary-user desktop route. The generic full package audit
currently refuses stripped installed PE symbol tables; do not turn a successful
MSI/file/graph/import or producer-map check into complete security acceptance.

Uninstalling program files does not purge profiles, credentials, exported user
files, desktop registry or update-recovery records. Storage & privacy's explicit
profile purge retires that profile first; cancellation does not restart it.
See the [maintenance ownership contract](../architecture/release-maintenance.md#m6-retained-lease-maintenance-composition).
Windows/Linux and managed installations retain manual updates. Only the existing
qualified, user-confirmed macOS updater exception applies to eligible trusted
bundles; an ad-hoc candidate does not qualify.

## Exact dependency and license evidence

`packaging/package_runtime.py` creates a closed runtime inventory and hashes
deployed bytes after relocation (and after final macOS sealing/stapling). It
joins the selected Qt producer's SPDX graph with statically compiled SQLite,
KSyntaxHighlighting, Quill/fmt, Glaze and the macOS-only LibArchive input.
`packaging/dependencies.json` pins their complete source archives; cached or
downloaded mismatches are refused. ECM is identified as build-only.

The installed `ThirdParty` directory (macOS: `Contents/Resources/ThirdParty`)
contains `THIRD-PARTY-NOTICES.txt`, `SOURCE-OFFERS.txt`, source provenance,
selected Qt producer provenance, and `corresponding-source.zip`. Qt stays
dynamically linked. The source-offer document describes replacement with
ABI-compatible modified libraries and local resealing of a macOS copy; it grants
no updater trust. All embedded syntax/theme attribution and complete licensed
data are retained, not mislabeled as MIT just because the engine is MIT.

First-party distribution terms and aggregate mixed MIT/LGPL/GPL syntax-data
obligations need owner/legal release approval. Metadata uses `NOASSERTION`
where no single license has been concluded; it is not a waiver. Windows also
requires the native operator's applicable Microsoft runtime notice. Exact
artifact license/source completeness remains an independent M9 release gate.

The full existing macOS path emits final DMG/ZIP/SBOM checksums, and the existing
private publication script now requires those SBOM assets too. Do not run that
script, submit notarization, or apply release signatures without explicit
authority for the prepared version.

## Focused candidate audit and hostile course

`scripts/verify_release_package.py` audits an expanded candidate without
building, signing or publishing. Supply its actual build directory, SPDX and
exact clean producer revision. A later verifier/documentation-only revision
may reuse the package only while product/build/package inputs are unchanged.
The audit requires all installed file hashes, helper/plugin capability names,
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
