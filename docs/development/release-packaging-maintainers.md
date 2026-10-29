# Packaging guide for maintainers

This guide is for people who build, audit or publish BetterPullReview packages.
It covers the native build hosts, preparing unpublished candidates, the
dependency and licence evidence each package carries, publication, the
conditions that still stop a release, and the candidate audits.

For installing, verifying and removing a package, and for the licence and
source obligations that apply to recipients, see
[Install and verify BetterPullReview packages](release-packages.md).

> [!IMPORTANT]
> The project `publish-release` skill and `tools/release.sh` are the only
> macOS release workflow. GitHub Actions is not used and supplies no release
> or secret-scan gate. Do not run the publication script, submit
> notarization or apply release signatures without explicit authority for the
> prepared version.

Related: [release-readiness allocation](../architecture/release-readiness.md),
[packaging evidence ledger](../verification/RFC-BPR-0014.md),
[native qualification ledger](../verification/RFC-BPR-0038.md) and
[release security policy](../security/release-policy.md). Platform commands and
human prerequisites are in `.codex/skills/publish-release/references/`.

## Build hosts and toolchains

macOS builds with local automation also build `app_bpr`; `tools/release.sh`
deploys it at `Contents/MacOS/bpr` with `macdeployqt -executable`, restricts its
plugin/library paths to the bundle, signs it as nested executable code, and
runs `scripts/verify_bundled_cli.py` on the final app and a relocated copy.
The same notices, SPDX file inventory, security scan and version-bound source
asset cover the CLI. Exact native sign-in and Developer ID/notarization remain
additional acceptance for each changed release. The automation-off variant
excludes `bpr` and must not promise the `pr-tour` workflow.

Every distribution graph uses Release, `BUILD_TESTING=OFF`,
`BPR_DISTRIBUTION_BUILD=ON`, pinned Qt 6.12.0 and the audited dependencies.
Build from committed, clean source.

- `app-release` is a development build, not a distribution build.
- `core-only` stays buildable and testable without Qt or providers.
- There is no core SDK or embedding API.

| Native build host | Preset and output | Required operator inputs |
|---|---|---|
| macOS | `macos-release`: universal x86_64/arm64, minimum macOS 14.4; app and ZIP, plus DMG for a full release | The existing Qt/macOS toolchain. Local prepare mode needs no Apple credential. |
| Windows x64 (tested on Windows 11) | `windows-app-release`: WiX per-machine MSI under `%ProgramFiles%\BetterPullReview` (installing asks for administrator approval) (CPack's WiX v4+ .NET-tool mode, `CPACK_WIX_VERSION 4`) | Native MSVC x64 developer environment, Ninja, the WiX .NET tool 4 or 5 with `WixToolset.UI.wixext`, Python 3.10+, the Qt provider SBOM, `VCToolsRedistDir`, and `BPR_MSVC_REDISTRIBUTION_NOTICE` pointing to the licensed toolchain's applicable redistribution notice |
| Ubuntu 26.04 LTS x86_64 (WSL2 can produce it) | `linux-package`: DEB and per-user archive | A C++23 compiler and library that pass the build's checks, the Qt provider SBOM, `patchelf`, `readelf`, `ldd`, Debian packaging tools, and libsecret and libxcb development files |

All hosts also need CMake 4.4.2, Ninja, Python 3.10+ and a C++23 compiler and
library accepted by the root feature checks.

macOS 14.4 is the minimum for Qt 6.12 builds
([upgrade evidence](../verification/RFC-BPR-0088.md)). The published v0.3.0
packages keep their recorded macOS 13 requirement. New packages need fresh
native, trust and vulnerability qualification for their exact bytes.

### Observed producers

These are toolchains that have produced packages, not the full set of
compatible ones:

| Host | Toolchain |
|---|---|
| macOS release configuration | AppleClang `21.0.0.21000101` |
| Windows producer | MSVC `19.51.36257.0`, toolset `14.51.36231`, Visual Studio 18 Community, WiX 4.0.6, Qt's `msvc2022_64` package |
| Windows agent-free MSI (`bpr-main-win`) | WiX .NET tool 5.0.2 and `WixToolset.UI.wixext` 5.0.2 (both MS-RL), with no CMake change |

Stay on WiX 5.0.2: WiX 6 and later ship under the Open Source Maintenance Fee
EULA, a term beyond their OSI licence.

No Linux compiler or libstdc++ baseline has completed native qualification yet.

### Linux baseline rules

- The selected release set is Ubuntu 26.04. Ubuntu 24.04 and RHEL are not
  selected targets.
- Ubuntu packaging rejects obsolete or undeclared Ubuntu versions and enforces
  a glibc 2.43 ceiling on required ELF symbol versions.
- The optional RHEL 10 RPM mechanics are retained with glibc 2.39 and
  x86-64-v3 constraints. The selected release needs no RPM producer.
- The staged runtime records its actual GLIBCXX requirements. The native
  package dependency generator adds the matching system-library constraints.
- A newer C++23 toolchain is not permission to raise the runtime baseline.
  Compiler, libstdc++, CPU and installability evidence must be qualified on
  each native target.
- Qt libraries and plugins are private to the package. libstdc++/libgcc,
  OpenSSL, X11/XCB, libsecret and other OS libraries are system dependencies,
  never silently bundled.
- Linux selects X11 (`qxcb`), with XWayland on Wayland desktops. Native Wayland
  plugins are not included or claimed.
- WSL2 may produce ordinary Linux binaries. The tested Linux environment is
  Ubuntu 26.04 under WSL2/WSLg (owner amendment 2026-10-16, confirmed
  2026-10-23 UTC). A WSL shell build does not qualify the rendered WSLg
  course, Secret Service or the installer. Native Linux desktops and other
  distributions are untested, not refused.

## Numbered release candidates

The [release build identity procedure](release-build-identity.md) allocates only
through publish-release preparation. One frozen external cohort binds the source,
declared native configurations and common sequential N. Local builds remain
unnumbered. Each producer receives the same cohort and its declared producer name;
CMake checks actual flags, compiler/Qt versions and architecture before accepting
that identity. A changed source or configuration requires a new cohort.

Canonical assets use `VERSION-build.N`. The macOS short version and stable tag
retain the base version for already installed updaters, with an exact byte alias
of the canonical ZIP under the legacy base-version name. A numbered Mac release
therefore has ten assets: the eight canonical assets and the alias/checksum.
Never replace an occupied base-version release or retroactively number it.

For initial publication, add `--cohort /absolute/cohort.json` to both dry-run and
publication commands. Pass `--cohort` and `--producer` to `tools/release.sh`, or
configure the native CPack graph with `BPR_RELEASE_COHORT` and
`BPR_RELEASE_PRODUCER`. Numbered Windows production also takes
`--previous-msi /absolute/previous.msi`: it reads the actual previous per-machine
MSI, preserves UpgradeCode and checks its native ProductVersion before any build
or signing. The package driver then inspects actual MSI/PE or DEB identities and
binds those observations to final hashes. Offline preflight is still separate
from native trust, install/upgrade qualification and publication approval.

## Prepare an unpublished candidate

Preparation builds packages locally without signing for distribution, tagging
or publishing. Choose a new output directory each time.

### macOS

Run from the checkout:

```sh
tools/release.sh --version 0.1.3 --prepare-only --skip-tests \
  --output-dir build/releases/local-candidate
```

This produces an app, a local ZIP, a separate version-bound source ZIP, an SPDX
SBOM and checksums. It uses the universal build, restricted Qt deployment,
dependency and deployment-floor checks on both slices, and a relocated launch
smoke. It includes the notices and source references, and applies only an
offline ad-hoc signature.

Prepare mode does not read signing identities, change the Keychain, contact
Apple, sign with Developer ID, tag, push or publish. Ad-hoc sealing lets the app
run locally; it is not Gatekeeper or distribution trust. `--skip-notarize` is
different: it still signs with Developer ID.

How the relocated launch smoke works:

1. It runs the self-test with `QT_QPA_PLATFORM=offscreen` and an isolated
   `HOME`, keeping the private-profile and production bundle-identity checks.
   The package itself still ships only the Cocoa platform plugin.
2. It copies the sealed app into a private assessment bundle and checks
   complete byte equality.
3. It adds the release build's exact `Qt6_DIR` offscreen plugin to that clone,
   checks universal slices and packaged Qt/system dependencies, and points the
   plugin's RPATH at the clone's frameworks.
4. It seals the plugin and clone with the selected identity (ad-hoc for
   prepare-only), verifies them, and logs plugin hashes and the signing team.
   Resealing changes the clone's signatures, so no equality is claimed after
   it.
5. The original app's complete fingerprint and executable hash must be
   unchanged afterwards, and its signature must still verify.

The offscreen plugin never enters the delivered app or release assets. External
plugin paths are excluded, so distribution startup's own plugin-path restriction
holds. This unattended smoke cannot qualify a native Cocoa window or desktop
interaction; those need their own isolated or attended courses.

### Windows

Inside the activated native x64 toolchain environment:

```sh
cmake --preset windows-app-release
python packaging/package.py --build build/windows-app-release \
  --generator WIX --output build/releases/windows-candidate
```

### Linux

On the exact baseline:

```sh
cmake --preset linux-package
python3 packaging/package.py --build build/linux-package \
  --generator DEB --output build/releases/ubuntu-candidate
# Ubuntu's CPack default is DEB; the per-user archive uses the same runtime.
```

### What the native driver produces

`packaging/package.py` builds the application and both helpers, runs CPack,
requires exactly one native installer and inventory, and emits checksums, an
SPDX sidecar, a separate version-bound source ZIP and source revision
provenance.

- Linux also emits `*-user.tar.gz` from the same audited runtime.
- Windows also emits `*-runtime.zip` for inventory auditing. It is not a
  qualified portable installation.

### Signing

Unsigned preparation is the default. Neither route provisions keys, discovers
credentials or publishes.

- **Windows:** `--signtool`, `--azure-dlib`, `--azure-metadata` and
  `--publisher` must be given together. The driver signs the first-party
  images, including the source-built xdiff DLL, after building and before
  CPack inventory generation, verifies the staged bytes, then signs the MSI
  before final hashing. Signing needs the approved Azure identity and client.
- **Linux:** `--gpg-key` takes an approved full fingerprint. An RPM is signed
  and verified first; then the final products and metadata get verified
  detached signatures.
- **Ubuntu on WSL:** `--gpg-vault-config /absolute/producer-local/config.json`
  unlocks one approved Windows Credential Manager credential before any output
  or build, then signs in its exact GPG home with pinentry disabled. Missing or
  wrong credentials stop production. One-time human enrolment uses the
  maintained hidden console helper. Private keys stay encrypted and no secret
  is transferred to the Mac.

## Windows audit inputs

### The reviewed `bcrypt.lib` specimen

The native Windows package and security auditors accept an optional explicit
`--windows-import-library C:/absolute/reviewed-sdk/bcrypt.lib`. It is a
reviewed metadata specimen, not proof of which library LINK selected. The
complete file must match SHA-256
`9fe31df255d17b0339391ff80a71bf2c5d442cc301bd2fe6eeea00b626f10785`; missing,
malformed or different bytes supply no exception.

The specimen's static `.idata$6` DLL-name marker is accepted only for an exact
same-owner, static-table, flag-free, non-executable, image-base address with
the exact 32-bit wrap observed after its name contribution was discarded. The
audit also requires a normally validated same-owner null descriptor, actual
native import absence, and the DLL name's absence in ASCII and wide form.
Every symbol stays in the fixture and composition scans, and the normal address
and identity checks still apply.

An `rc.exe` SDK path does not establish LINK's response-file or library-search
selection. A changed specimen needs a new review. Packaging, signing and offline
manifest preflight need no new argument: this input belongs only to the native
PE audit, and grants no installation, security or distribution qualification.

### Qt provenance and file names

Windows Qt provenance compares deployed file names case-insensitively, matching
PE producer inspection; Darwin and Linux keep exact file names. Qt provenance
and the native PE audit select the producer's exact `qtbase` SPDX, plus `qtsvg`
for the deployed Svg library (Windows `Qt6Svg.dll`, Darwin's `QtSvg` framework
image, Linux's versioned `libQt6Svg.so`) and `qtimageformats` for the WebP
plugin.

The audit refuses missing module documents, ambiguous file records, escaped
producer paths, changed bytes and invalid upstream checksum or signature
evidence. File names select which evidence is required; they cannot authorize a
DLL. Installed notices, source offers, Qt provenance and the final SPDX all use
the same selected closure.

### Image resources and symbols

- The tests-disabled image keeps its compressed Qt resources in
  `better_pull_review.rcc` beside the executable. The installer and runtime
  inventory carry it as declared runtime data. KSyntaxHighlighting's 54 KB
  theme data stays inside the image, because the pinned framework initializes
  it unconditionally.
- Each shipped image (application and both helpers) has a `VERSIONINFO`
  resource. A Release image names its program database by file name only
  (`/DEBUG /PDBALTPATH:%_PDB%`); the database stays in the build tree.
- Unsigned preparation produces unsigned images.

### The xdiff DLL and the private-library identity

Producer inputs bind the fixed private-library package identity to its exact
CMake target file. The source-built xdiff DLL needs a byte-identical producer,
a full public/static linker map, and the same signing and staged-byte receipts
as the application and helpers. Reconfigure older builds before production: an
unknown DLL, a foreign build path or a missing map is refused. This does not
change xdiff's separate LGPL library and source arrangement, or establish
package trust.

### MSI installation notes

The MSI installs per machine under Program Files ([[BPR-1788]]; earlier
packages installed per user under `%LOCALAPPDATA%\Programs`). It stores its
selected directory in its own HKLM installer value, so repair and uninstall
resolve the same directory, including a custom one. An MSI cannot upgrade a
product across install scopes, so when the installing user still has a per-user
copy from an earlier package, the installer stops and asks for it to be
uninstalled first rather than leaving two installations.
CPack supplies the normal installed-location metadata. Use native backslash
paths with `msiexec`: a forward-slash command was refused before installation.

The earlier native run used an elevated SSH token, so it does not prove the
ordinary-user desktop route. The PE audit checks stripped images against
byte-identical producers, full linker maps and exact upstream DLL evidence; its
16-image audit passes for the recorded unsigned artifact. The map is trusted
build evidence, not publisher trust or proof of complete CFG call-site
coverage. That artifact predates the separate-source arrangement, so a revised
native MSI still needs production and qualification.

## Linux ICU runtime

Linux Qt 6.12.0 needs ICU 73.2's data, uc and i18n companions. Packaging admits
only that exact trio from the selected Qt provider, with the official Qt archive
and raw file hashes pinned in `packaging/dependencies.json`.

Those prebuilt inputs lack immediate binding, so the delivered trio is rebuilt
from the exact pinned ICU source with the selected native compilers, PIC, stack
protection, frame pointers, FORTIFY3 and full RELRO/NOW/non-executable stack.
The shared Qt provider is not modified.

- `packaging/linux_icu.py` builds in a private out-of-source prefix. A cached
  producer must match source, recipe, compiler and make identities, flags, the
  extracted source and all three output hashes.
- `ThirdParty/icu-runtime-provenance.json` names that source build.
- `runtime-closure.json` keeps the official Qt input, source-built producer and
  relocated output hashes distinct.
- Native audits reproduce the loader-path operation on disposable producer
  copies and compare complete files.
- Wrong origins, unknown ICU names, changed producer or delivered bytes,
  incomplete trios and provenance drift fail closed. The ELF hardening gate is
  unchanged.

The Linux source asset carries 21 upstream archives and eight build inputs
(29 entries): the earlier 20 archives and six xdiff helpers, plus the ICU
source, its standalone Python recipe and a recipient README
(`ICU-73.2-build/third_party/icu/rebuild/README.md`). The installed notices
contain ICU's complete mixed third-party text. The ICU package keeps
`NOASSERTION` for a single aggregate licence conclusion, and the upstream Qt
SPDX's separate ICU 73.2 record is unchanged. macOS and Windows component and
notice sets are unchanged. Final native trust, material review, scan and
applicability decisions, and broad qualification remain separate.

Selected Linux names and installed source URLs include `linux-ubuntu-26.04-x64`;
reconfigure older distribution builds before production. Optional RPM names
remain `linux-rhel-10-x64` and are not required assets.

## Build packages without agent integrations

Two CMake switches, both `ON` by default, remove code from the build:

| Switch | `OFF` removes | Still present |
|---|---|---|
| `BPR_BUILD_AGENT_INTEGRATIONS` | The agent launch adapter and its protocol loop (`src/adapters/agent`), the application's invocation, chat, finding, comment-workshop and re-alignment hosts, the agent launch builder and provider protocol decoders in `agent_launch.cpp`, the MCP context helper executable and its supervisor, and their composition. The window withdraws the launch panel, side chat, finding queue, comment workshop, tour writing assistance, the private Ask agent and agent-assisted composition, and the Agents & context settings page. No agent profile is offered or resolved. | Review, local cycles, notes and suggestions, Copy, Save and the bundle, capture, comparisons, responses, verification, publication, private notes and private history, and every stored record, including those an agent wrote earlier |
| `BPR_BUILD_LOCAL_AUTOMATION` | The desktop's owner command endpoint (no listener is bound) and the forwarded cycle commands it serves. `BPR_BUILD_TOUR_CLI` requires it, so the graph cannot build `bpr`. | Everything above, the single-instance activation endpoint, and the desktop's own in-process tour routing |

The configure presets are `macos-release-agent-free`,
`windows-app-release-agent-free` and `linux-package-agent-free`, plus the same
names ending `-agent-free-automation-off`, which turn both switches off. Each
inherits its full preset and has its own build directory.

The `tour-cli` preset builds without agent integrations. Its `TourCLI` install
component contains `bpr`, the supervisor, xdiff, mimalloc and private Qt Core,
Network and TLS. A plain CMake component install is a development tree.
Prepare an unpublished standalone distribution from a clean committed source:

```sh
cmake --preset macos-release -DBPR_BUILD_TOUR_CLI=ON
python3 packaging/package_tour_cli.py --build build/macos-release \
  --output build/tour-cli-artifacts
```

Use `windows-app-release` or `linux-package` on their native producer, also
with `BPR_BUILD_TOUR_CLI=ON`. The existing distribution graph remains required;
the plain `tour-cli` developer preset is not a distribution graph.
The new output directory contains the installed `runtime/`, its SPDX and MIT
notice, a separately named versioned `*-tour-cli-corresponding-source.zip`, a
runtime archive and `preparation.json`. The source asset contains the CLI's
selected dependencies; desktop-only fonts, images, syntax definitions and the
Mac updater are excluded. Windows deploys the actual native CRT and requires
its licensed redistribution notice; Ubuntu replaces Qt's ICU companion with
the same pinned hardened producer used by Runtime. Mac preparation uses only
an offline ad-hoc seal. Nothing signs for release or publishes.

The distribution CLI uses the existing installed-runtime startup guard before
Qt construction. It clears ambient Qt plugin selectors, refuses code-loading
Qt arguments, requires the private plugin directory and restricts discovery to
that prefix (including Windows' existing DLL search restriction). Developer
and instrumented graphs retain their configured provider paths. `qt.conf`
alone does not establish this boundary.

Audit an expanded copy through `scripts/verify_release_package.py --tour-cli`
with `--build`, `--root`, `--sbom`, `--source-revision` and the separate
`--source-archive`. The auditor binds actual files, selected producers and
source references, and refuses missing or extra allocator files. On Windows the CLI preparer and auditor accept `--windows-import-library`
for the same independently reviewed SDK `bcrypt.lib` metadata specimen used
by the Runtime PE audit; it is not proof of linker selection. The auditor also
honors `--windows-redist-root`. For the CLI's discarded USER32 import-name
record, pass `--windows-user32-import-library` with the separately pinned
`User32.lib` specimen from that SDK. Unknown bytes/owners and live DLL
references remain refusals. Installed native loading, signed trust and
legal approval remain separate evidence.
`packaging/package.py` continues to build the desktop's Runtime component.

```sh
# macOS, unpublished local preparation of either variant
tools/release.sh --version 0.1.3 --prepare-only --skip-tests \
  --variant agent-free --output-dir build/releases/agent-free-candidate
tools/release.sh --version 0.1.3 --prepare-only --skip-tests \
  --variant agent-free-automation-off --output-dir build/releases/manual-candidate

# Windows (native x64 tools environment) and Linux (exact baseline)
cmake --preset windows-app-release-agent-free
python packaging/package.py --build build/windows-app-release-agent-free \
  --generator WIX --output build/releases/windows-agent-free-candidate
cmake --preset linux-package-agent-free
python3 packaging/package.py --build build/linux-package-agent-free \
  --generator DEB --output build/releases/ubuntu-agent-free-candidate
```

### What the package records say

- `_bpr/package-inputs.json` names the families built (`families`) and the
  artifact suffix (`variant`), and lists only the helpers that ship.
- Artifact names end `-agent-free` or `-agent-free-automation-off`.
- The product package's SPDX `sourceInfo` ends `; agent integrations absent`
  and, without automation, `; local automation absent`. A full package's text
  is unchanged.
- `runtime-closure.json` and the native provenance record `families`.
- The SPDX file list is computed from the installed bytes, so it never names a
  helper that is not there.

### Verifying a variant

`scripts/verify_release_package.py` reads the families from the build. It
requires the MCP helper only with agent integrations, and refuses an agent-free
package that still carries one. It audits the application image with
`scripts/verify_composed_hosts.py`: agent anchors are required in a full image
and forbidden in an agent-free one. The anchors are the agent hosts, the
launcher, the protocol decoder, the MCP supervisor and the Send to Agent round
sender. The cycle owner service is required with local automation and forbidden
without it. Run the verifier alone with `--without-agent-integrations` and
`--without-local-automation`.

`better_pull_review_agent_supervisor` ships in every package, agent-free ones
included. It is the process owner that guarded Git launches run inside, on
macOS, Linux and Windows, and holds no agent protocol, provider knowledge or
credential. Its file name is kept so the Git path, the installers and earlier
evidence all name the same file.

Every variant has the same package identity (macOS bundle identifier, Linux
package name, Windows upgrade code), so installing one over another replaces
program files only. On Windows and Linux that replacement still needs native
qualification.

### The installed cycle package course (macOS)

`tests/end_to_end/installed_cycle_package_course.py` checks the prepared
packages from outside, since nothing inside a tests-disabled package can be
driven. It installs the full, agent-free, agent-free automation-off and full
packages in turn into one place over one profile, starts each on Cocoa, and
requires that each:

- composes the profile,
- starts no child process and holds no network socket,
- runs none of the agent-named canaries placed first on `PATH`,
- binds its activation endpoint,
- binds the owner command endpoint exactly when it was built with local
  automation.

A cycle made with `bpr` before the first swap must read back byte for byte
after every package has run.

Prepare each variant into its own output folder, then name the folder holding
all three (the course expects `bpr1182-full`, `bpr1182-agent-free` and
`bpr1182-agent-free-automation-off`) and any `bpr`:

```sh
python3 /absolute/checkout/tests/end_to_end/installed_cycle_package_course.py \
  --package-root /absolute/package-root --bpr /absolute/install/root/bin/bpr
```

It prints `INSTALLED_CYCLE_PACKAGE_COURSE_OK`. Its root is created under the
account's home and removed afterwards. It does not cover Windows or Linux
installers, signing or the GUI round trip a person drives.

## Dependency and licence evidence

The recipient-facing summary is in
[Licences and corresponding source](release-packages.md#exact-dependency-and-license-evidence).

### Runtime inventory

`packaging/package_runtime.py` creates a closed runtime inventory and hashes the
deployed bytes after relocation (and after final macOS sealing and stapling). It
joins the selected Qt producer's SPDX graph with:

- statically compiled SQLite, KSyntaxHighlighting, Quill/fmt, Glaze and zlib,
- utf8proc,
- pinned fmt, separate from Quill's copy,
- sha1collisiondetection, for every SHA-1 Git identity,
- HACL*'s generated SHA-2, for the core SHA-256,
- header-only rapidfuzz-cpp, for the diff engine's line-pair similarity,
- header-only dtl, for intraline ranges and Review Cycle suggestion hunks,
- RE2 with its Abseil LTS,
- the macOS-only LibArchive input,
- the embedded JetBrains Mono NL faces,
- Git 2.56.0's xdiff, the diff engine's primary line matcher, shipped as its
  own shared library (see [xdiff](#xdiff-a-dynamically-linked-lgpl-library)).

`packaging/dependencies.json` pins their complete source archives; cached or
downloaded mismatches are refused. ECM is identified as build-only.

The selected product executables also use pinned mimalloc 3.5.3 (MIT). Linux
Runtime installs require the real `libmimalloc.so.3` beside the app and helpers;
the private producer map, SPDX and package audit bind it to the selected build.
TourCLI installs that runtime and xdiff under `lib/`, with the supervisor in
`bin/`. Normal startup requires no allocator preload environment. The native
allocator course and its limits are recorded in
[RFC-BPR-0093](../verification/RFC-BPR-0093.md); a local audit does not qualify
signatures, installation/removal, security or final all-platform source/legal
acceptance.

The font's OFL and authors list are kept under `third_party/jetbrains-mono/` and
carried whole into `THIRD-PARTY-NOTICES.txt` (`NOTICE_FILES` in
`packaging/package_runtime.py`, which the security audit checks too). Its SPDX
package is declared `OFL-1.1`.

### Notices, provenance and the source asset

The installed `ThirdParty` directory holds `THIRD-PARTY-NOTICES.txt`,
`SOURCE-OFFERS.txt`, source provenance and the selected Qt producer's
provenance. The corresponding source is a separate asset,
`Source-BetterPullReview-VERSION-PLATFORM-corresponding-source.zip`, never
embedded in the app, installer or per-user archive.

- Notices and source offers point to the asset's exact
  `releases/download/vVERSION/ASSET` location and SHA-256. The installed
  provenance also binds the producer commit and upstream input hashes.
- The SPDX hashes those installed references, and the package audits verify
  the asset's digest and complete source inventory.
- macOS preparation writes the asset beside the app ZIP. CPack stages it
  outside the installed tree, and the native driver copies it beside the
  MSI/DEB/RPM. The checksums and native provenance include it.
- The asset includes the pinned QtBase, QtSvg and QtImageFormats archives with
  their licence and replacement obligations. Qt stays dynamically linked.
- All embedded syntax and theme attribution and licensed data are kept, not
  mislabelled MIT.
- A correction to generated metadata needs fresh packages. Never update an old
  signed runtime or its inventory in place.
- Help › About opens `THIRD-PARTY-NOTICES.txt` and `SOURCE-OFFERS.txt` from
  this directory (`installed_legal_folder` in `src/app/src/about_desktop.cpp`)
  and shows the commit the build was made from, which
  `cmake/BprBuildIdentity.cmake` records. Packaging builds from a clean
  checkout, so a release's About names its producer commit with no local
  changes.

### xdiff, a dynamically linked LGPL library

Git's xdiff (LGPL-2.1-or-later) is accepted only as its own dynamically linked
shared library, the same arrangement as Qt:

- **Linkage.** `cmake/BprXdiffProvider.cmake` builds the pinned archive's
  `xdiff/` as the shared library `bpr_xdiff` and nothing else. Only the xdiff
  adapter links it; no BetterPullReview binary holds a copy. Its packaged
  location per platform is `PRIVATE_LIBRARIES` in
  `packaging/package_runtime.py`. Linux deployment accepts it only from the
  package itself, and the package audit refuses a package without it.
- **Licences and SPDX.** The `git-xdiff` entry in `packaging/dependencies.json`,
  and so the release SPDX package `SPDXRef-git-xdiff`, declares
  `LGPL-2.1-or-later AND BSD-3-Clause` (`xhistogram.c` is EDL 1.0).
  `third_party/git-xdiff/PROVENANCE.md` records the per-file licences
  confirmed against the pinned source.
- **Notices.** `third_party/git-xdiff/NOTICE` (copyright lines, licences, the
  library's location in each package and where its source is), the full
  `LGPL-2.1` and the `xhistogram.c` licence header are carried whole into
  `THIRD-PARTY-NOTICES.txt`; the security audit requires them.
- **Corresponding source.** The asset carries the unmodified
  `git-xdiff-2.56.0.tar.xz` and, under `git-xdiff-2.56.0-build/`,
  BetterPullReview's two compatibility headers that replace Git's
  `git-compat-util.h`, the original provider, the standalone recipient CMake
  project and README, and its self-contained hardening helper. These are the
  `buildInputs` of the `git-xdiff` entry, pinned by SHA-256. Preparing the
  asset refuses a build input whose bytes changed, and the audits verify each
  digest. Change a pinned file and its digest in the same commit;
  `package_runtime_tests` names the stale one. The original acquisition
  provider remains provenance, not the recipient entry point.
- **Replacement.** `SOURCE-OFFERS.txt` states that xdiff is separately
  replaceable, names the file on each platform and permits a modified build
  exporting the same C interface, with the macOS local resealing step used for
  Qt. Replacement grants no updater trust.

The 0.2.0 release approved this arrangement (separate LGPL-2.1 shared library,
notice, source offer, unmodified Git source with the build script and two build
headers). The [native qualification ledger](../verification/RFC-BPR-0038.md)
records the decision. It covers that arrangement only.

### Approval scope

- Original BetterPullReview code and documentation use the root MIT licence.
  Aggregate mixed MIT/LGPL/GPL syntax data and other dependency obligations
  stay separate and need review for each exact release.
- The revised macOS inventory from `1c429749` received a scoped source and
  distribution decision. It does not approve revised Windows or Linux
  artifacts, different bytes, vulnerabilities, trust or publication.
- First-party, syntax-data, toolchain and build/relink approval, and
  vulnerability, trust and publication decisions, are not covered by the xdiff
  approval.
- Metadata uses `NOASSERTION` where no single licence has been concluded; it
  is not a waiver.
- Windows also needs the native operator's applicable Microsoft runtime notice.
- Source access and retention are ongoing distribution obligations, and exact
  artifact licence and source completeness is an independent release gate.

## Publish a release

### Release manifests

Signed production emits `*.release.json`, binding the installer, runtime
archive, SPDX, separate source ZIP, final checksums and provenance, and the
signature receipts.

`tools/publish-release.sh --manifest PATH` accepts these manifests repeatedly;
`--macos` includes the standard macOS set. Native manifest preflight verifies
the complete archive inventory and byte and source bindings before any remote
change. It does not verify native trust or prove that installer payloads equal
the archive: extract and audit final installers and verify native signatures
separately.

The full macOS path emits DMG, app ZIP, SPDX and source ZIP plus four checksum
files. Numbered releases also emit the exact legacy ZIP alias and checksum.
The publication script requires that complete ten-asset set and checks the source
reference inside the actual app ZIP against the SPDX and the separate source
ZIP before any remote operation. Its offline, non-mutating sub-check is
`packaging/verify_release_sources.py`; `--help` lists the artifact and producer
arguments. It fetches no source URL.

### Repository visibility

Publication requires `MikeMarcin/BetterPullReview` to be private by default.
After a separately authorized visibility change, add `--public` to both the dry
run and the real publication command to require the repository to be public.

- The flag never changes visibility and never accepts both states.
- The publisher checks the selected visibility and exact repository identity
  at preflight, and again before the atomic push and release creation.
- `origin` must have exactly one canonical GitHub HTTPS or SSH push URL.
  Custom aliases, embedded credentials and multiple push destinations are
  refused without being printed.
- Use the same selection throughout one release.

> [!WARNING]
> A state change after the push can leave the tag pushed without a release.
> Inspect the existing state before continuing. Do not retag, force-push or
> blindly retry.

```sh
# Only after BPR is already public and this release is authorized:
tools/publish-release.sh --public --version X.Y.Z --cohort /absolute/cohort.json --notes-file /absolute/notes.md \
  --macos --manifest /absolute/windows/INSTALLER.msi.release.json --dry-run
```

Remove `--dry-run` only for the authorized publication. The script still checks
GitHub and the remote during preflight; the standalone artifact verifiers are
offline.

### Coordinated releases across producers

The Mac can publish the combined set after its own macOS producer and the
signing PC's native Windows and Ubuntu 26.04 WSL2 producers finish.

1. Every producer uses the same committed source and selected version.
2. Private signing credentials and keys stay on the approved producer PC.
3. Return the complete final asset directories and manifests to the Mac
   publisher, keeping source names, digests and signature receipts.
4. Use absolute manifest paths for the shared dry-run preflight, and verify
   native trust separately before publication.

This preparation changes no version and starts no remote station; the owner
requests the actual release. Reuse the approved producer-local non-secret
configs and the isolated Azure CLI profile; Azure may still require a fresh
human login or MFA. The vault helper binds one named generic credential to the
exact key and GPG home, validates before retrieval, and forwards the passphrase
only to the fixed preset command's stdin.

The dedicated Windows Azure profile signer, one signed disposable probe, and the
approved encrypted Linux key's local sign, public-only verify and tamper smoke
have been observed. Local synthetic vault and WSL evidence does not prove remote
station logon access, clean Windows or Ubuntu product vaults, final artifacts,
or native, security and legal release gates. See the `publish-release` skill's
platform guides and
[the signing-vault card](../issues/60-done/BPR-1402-reuse-release-signing-vault-and-native-verifier.md).

## Release stop conditions

A release stays blocked until each of these is settled:

- clean desktop qualification on each native target,
- owner and legal approval for the exact artifacts,
- recipient access and retention arrangements for the corresponding source,
- the recorded security gaps.

The [release security policy](../security/release-policy.md) defines the
emitted hardening, analysis, sanitizer and fuzz, secret and vulnerability gates.
Still unresolved: the recorded 32 macOS and 30 Windows Qt 6.11.2 Grype matches
(both raw gates exit 2; the earlier 21/19 reports are historical), the full
native vault, privacy and runtime matrix, and external dependency hardening. The
[corporate security packet](../security/corporate-security-overview.md) records
what they mean. Finishing documentation changes none of these.

When checking a candidate's trust, use the platform checks in
[Verify a download](release-packages.md#verify-a-download). Record source,
artifact and signature identities and results separately from package
production. An ad-hoc `codesign` verification cannot satisfy the Developer ID
and notarization checks. A missing approved Linux key or trust, or missing
final native qualification, is a release blocker.

## Audit a candidate

### Focused package audit

`scripts/verify_release_package.py` audits an expanded candidate without
building, signing or publishing. Give it the actual build directory, SPDX and
exact clean producer revision. A later revision that changes only the verifier
or documentation may reuse the package while product, build and package inputs
are unchanged.

The audit requires the separate source ZIP beside the SPDX (or an explicit
`--source-archive` path), all installed file hashes, helper and plugin
capability names, declared static provenance, the tests-disabled compile graph,
and native symbol and byte evidence. On macOS it also checks every packaged
native library reference and RPATH. Native Windows and Linux loaded origins and
full trust and licence review are separate qualification evidence.

Linux's pinned prebuilt Qt libraries and declared plugins may lack regular
symbol tables. Their audit uses actual defined dynamic symbols, but only after
the selected producer matches its upstream SPDX file and checksum and an
independently relocated copy matches the delivered bytes. Empty, malformed,
unowned or changed images are refused. The application, helpers, xdiff and the
source-built ICU keep the full-symbol requirement, and the production
application keeps its composition anchors. Byte, fixture, dependency and
hardening checks apply to every image. This covers one specific producer class;
it is not a general fallback for stripped binaries.

A separately pinned read-only verifier may audit an older signed candidate when
product, build, packaging and corresponding-source inputs are unchanged:

- Record the tool revision and hash separately from the artifact producer
  revision.
- Bind both verifier and runtime source context to the actual producer
  checkout, and keep its actual compilation database and all mandatory graph
  regions. Running from another checkout must not silently omit those
  compilation units.
- Keep the original failed audit results and actual artifact hashes.

Such a tool correction grants no native, security, material or publication
approval.

### macOS hostile course

Use an external canonical disposable root and the installed qualification
guard. Every path argument below expands absolutely; replace the checkout,
candidate and producer revision with the exact inputs:

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

The course alters only a copy and isolates the child's home. It observes the
shipped Cocoa plugin, sockets and processes, and positive packaged Qt loading,
then exercises missing helpers and plugins, unsafe loader overrides, identity,
hash, byte and RPATH controls, and real MCP snapshot read, traversal and
stale-binding refusals. It restores and re-audits the copy and the original,
reaps children and removes its internal temporary directory. Remove the empty
outer scratch root afterwards.

It does not certify install or uninstall, real authentication or vault use,
crash artifacts, signing trust or another native architecture.

### Native runtime surface probes

`verify_runtime_surface.py --native-platform /absolute/path/to/executable`
selects Cocoa, Windows or XCB instead of development's offscreen plugin. That
switch alone does not isolate the user's home or run the complete package
course; use the guarded course for an installed macOS candidate.

The manual `runtime_surface_native_probe.py` checks the inspector against its own
loopback listener, connected socket and child process. On remote stations, run
it under the station lease and qualification guard in a disposable root. A
passing WSL probe records only the operations it observed; it does not qualify a
clean Ubuntu desktop, Secret Service or the delivered installer.

## Recipient package reference

Detail moved from the recipient guide,
[Install and verify BetterPullReview packages](release-packages.md), for
auditors and redistributors who need it.

### Other platforms and signature cases

- An RPM for RHEL 10 (`linux-rhel-10-x64`) can be built but is not part of the
  selected release set. Check it with `rpmkeys --checksig /absolute/candidate.rpm`,
  which needs the signing key installed separately.
- Linux packages run on X11 through `qxcb`; on Wayland they use XWayland. Native Wayland is not included or supported.
- An unsigned MSI has no signature to check. Checking a hash installs nothing.
- Older Linux source assets used the platform names `linux-ubuntu-24.04-x64`
  or `linux-x64`; the optional RPM's source asset uses `linux-rhel-10-x64`.

### Installed package contents

- **Helpers.** The MCP context helper and `better_pull_review_agent_supervisor`
  sit beside the main executable. On macOS the bundle also has
  `bpr_update_helper`. The supervisor ships in every package, including the
  agent-free variants, because guarded Git launches run inside it. It contains
  no agent protocol, provider knowledge or credentials.
- **Plugins.** The app loads Qt plugins only from its own installation. It
  ignores plugin-path environment overrides, refuses command-line options that
  load arbitrary plugins, and never falls back to a developer Qt installation.
- **Windows resources.** The app icon, diff strips and syntax definitions ship
  as `better_pull_review.rcc` beside the executable. The app checks that this
  file is exactly the one it was built with. If it is not, the app does not
  start and prints `INSTALLED_RESOURCES_REFUSED:better_pull_review.rcc:`
  followed by `missing`, `unreadable`, `digest-mismatch` or
  `not-a-resource-file`.
- **Windows file properties.** The application and both helpers name the
  vendor, product, description, version and original file name. These
  properties are not a signature. Debug symbol files are not installed.
- **Windows MSI.** The installer remembers the folder chosen, so repair and
  uninstall find it.
- **Ubuntu DEB.** The desktop entry is accompanied by a scalable icon in the
  `hicolor` theme.

### Bundled components and notices

The full component list: Qt (QtBase, QtSvg and QtImageFormats), SQLite,
KSyntaxHighlighting with its syntax and theme data, Quill, fmt, Glaze, zlib,
utf8proc, sha1collisiondetection, HACL*'s SHA-2, rapidfuzz-cpp, dtl, RE2 with
Abseil, Git's xdiff, the JetBrains Mono NL font (SIL OFL 1.1), LibArchive on
macOS and ICU 73.2 on Linux.

`THIRD-PARTY-NOTICES.txt` includes the full syntax and theme attributions, the
font's licence and authors, ICU's mixed third-party text and the full
LGPL-2.1. Provenance records cover the source and the Qt build used. Syntax and
theme data are not relabelled MIT because the highlighting engine is MIT.

The source archive holds the pinned upstream archives, including Qt's, with
BetterPullReview's build inputs. On Linux it also carries the ICU source with a
standalone rebuild recipe; start at
`ICU-73.2-build/third_party/icu/rebuild/README.md`. The installed notices give
its exact location as `releases/download/vVERSION/ASSET`.

Source access obligations in full:

- The GitHub release holds binaries and source under the same access rules,
  whether the repository is private or public.
- If the repository's access changes or the download URL stops working, the
  distributor must supply the source archive through the channel that
  delivered the binaries.
- Retention lasts until the accountable distributor documents that every
  source-offer and licence obligation has ended.
- Package checks cannot prove future access or retention; the distributor has
  to arrange and maintain both.

### Replacing Qt or xdiff

- **Qt:** replace the libraries with ABI-compatible modified builds.
- **xdiff:** BetterPullReview's line matcher is Git 2.56.0's xdiff, shipped as
  its own shared library. You may replace it with a modified build that
  exports the same C interface (`xdiff/xdiff.h` of Git 2.56.0). It is:

  | Platform | File |
  |---|---|
  | macOS | `Contents/Frameworks/libbpr_xdiff.dylib` |
  | Windows | `bpr_xdiff.dll` beside `better_pull_review.exe` |
  | Linux | `libbpr_xdiff.so` beside the application in `lib/betterpullreview` |

  Its files are LGPL-2.1-or-later, except `xhistogram.c`, which comes from
  JGit under the Eclipse Distribution License 1.0 (SPDX `BSD-3-Clause`).

  To rebuild it, start at
  `git-xdiff-2.56.0-build/third_party/git-xdiff/rebuild/README.md` in the
  source archive. Verify and extract the supplied Git archive, then configure
  the standalone CMake project with the absolute path of the extracted source.
  It compiles the same seven C11 sources into the replaceable library using
  only the included compatibility headers and hardening helper. No
  BetterPullReview checkout, shared cache or network access is needed, and you
  may modify the extracted source before you build. The README covers each
  platform's architecture, output and import-library names, and resealing.

Replacing a library gives that copy no update trust.

### Agent-free variants: further detail

- Installing one variant over another keeps profiles, cycles, rounds,
  suggestions, responses, verification records, private history and
  credentials, and an agent-free build opens them. No agent profile is offered.
- Package identity shared across variants: the macOS bundle identifier, the
  Linux package name and the Windows upgrade code.
- A disposable agent workspace left by a full install is still cleaned up.
- An unpublished standalone CLI archive occupies its own prefix: remove that
  prefix to remove its runtime. If you installed the development component
  from CMake, delete the files listed in
  `build/tour-cli/install_manifest_TourCLI.txt`. The desktop app is unaffected.

### Removal detail

The profile purge retires that profile first, and cancelling does not restart
it. It does not touch shared updater and registry state. Reconcile any
publication in progress before purging.

### Signature checks

Get the package, its checksum file, the SPDX inventory, the corresponding-source
archive with its checksum, and the publisher's identity through a trusted
channel. A checksum that came from an untrusted source proves nothing about
who made the file. On Windows, hash the source archive and the SPDX file too.

**macOS**

```sh
codesign --verify --deep --strict --verbose=2 /absolute/BetterPullReview.app
codesign -d --verbose=4 /absolute/BetterPullReview.app
xcrun stapler validate /absolute/BetterPullReview.app
spctl --assess --type execute --verbose=4 /absolute/BetterPullReview.app
spctl --assess --type open --context context:primary-signature --verbose=4 /absolute/candidate.dmg
```

- The first command must pass.
- The `codesign -d` output must show bundle identifier
  `com.gamecoretech.betterpullreview`, team `YTQKP2V2A8` and the hardened
  runtime.
- The staple validation and both `spctl` assessments must pass. Validate the
  DMG's staple as well as the app's.

**Windows**

Check each signed executable and DLL you received with `signtool verify` as
well as the MSI. A zero exit code is not enough: compare the signer,
certificate chain and timestamp with the expected publisher.

**Linux**

Use the publisher's signing key, identified by its full fingerprint, for a DEB
or per-user archive delivered with a detached `.asc` signature. If you install
from a corporate repository, that repository's signed metadata and package
digest take the place of these checks, under its own policy.

An ad-hoc `codesign` signature only proves the local seal; it is not a
Developer ID signature or notarization.

### Install detail

- Install for your own account unless you need a managed system-wide install.
- **Windows:** if you run `msiexec` from a command line, give it native
  backslash paths. Paths with forward slashes are refused before installation.
  If `better_pull_review.rcc` is missing or altered the app refuses to start
  (see [Installed package contents](#installed-package-contents)); reinstall to
  fix it.
- **Ubuntu DEB:** needs administrator rights and installs into `/usr`: the
  `better_pull_review` launcher, a desktop entry and icon, and the private
  runtime under `/usr/lib/betterpullreview`. The package declares the system
  libraries it needs, so the package manager installs them.
- **Ubuntu per-user archive:** the archive does not install system libraries.
  Unpack it into its own versioned folder under your local application
  directory. Do not set global Qt or library search paths for it.
- **Windows runtime ZIP:** `*-windows-x64-runtime.zip` beside the MSI is an
  inventory for auditing, not a portable installation.
- **macOS:** v0.3.0 packages require macOS 13 or later; packages built with Qt
  6.12 require macOS 14.4 or later.

### System requirements detail

| Requirement | Detail |
|---|---|
| GitHub CLI (optional) | Select it in Settings after installing it yourself. |
| AI agents (optional) | Installed separately and selected explicitly. |
| Linux system libraries | libstdc++/libgcc, OpenSSL, X11/XCB, libsecret and other OS libraries come from the system, not the package. |
| Linux C library | The package requires no glibc symbol version newer than 2.43. |

Qt's libraries and plugins are bundled privately on every platform. On Linux
the package also carries its own build of ICU 73.2.

### Update detail

**Check for Updates…** asks before it installs anything. Ad-hoc signed builds
are not eligible. Windows, Linux and managed installations update by installing
the new package.

### Licence and removal detail

- `SOURCE-OFFERS.txt` says where to get the corresponding source and how to
  replace the LGPL libraries.
- Where the SPDX inventory says `NOASSERTION`, no single licence has been
  concluded; it is not a waiver.
- On Windows the distributor must also supply Microsoft's applicable runtime
  redistribution notice.
- Source PLATFORM names are `macos-universal`, `windows-x64` and
  `linux-ubuntu-26.04-x64`. The source is not embedded in the app, installer
  or archive; the installed notices give its download location and SHA-256.
- Keep the archive, its digest and the version tag unchanged for as long as any
  matching binary is distributed, and for every retention period that applies
  afterwards.
- On macOS a modified copy has to be resealed locally after replacing Qt or
  xdiff, and is no longer eligible for **Check for Updates…**.
- Uninstalling also leaves desktop registration and update-recovery records;
  reviews already published to GitHub are unaffected. Before purging, finish
  or reconcile any publication or update in progress and sign out of each
  account. To remove the program afterwards, quit the app, then delete it from
  Applications (macOS), uninstall it from **Installed apps** or uninstall the
  MSI (Windows), or remove the DEB or per-user folder (Linux).

### Agent-free variant contents

| Package name ends in | Removed | Still works |
|---|---|---|
| `-agent-free` | Agent launching, side chat, the finding queue, the comment workshop, tour writing assistance, the private **Ask agent**, agent-assisted composition, **Send to Agent**, the MCP context helper and the **Agents & context** settings page. | Review, Review Cycles, notes and suggestions, **Copy**, **Save** and the bundle, capture, comparisons, responses, verification, publication, private notes and history, and every stored record, including ones an agent wrote earlier. |
| `-agent-free-automation-off` | All of the above, plus the local command endpoint that `bpr` talks to. | Everything above, single-instance activation, and tour routing inside the app. |

The code is removed from the build, not just hidden. Replacing one variant with
another on Windows and Linux has not yet been qualified natively.
