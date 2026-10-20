# Install and verify BetterPullReview packages

This guide is for people who download, install or redistribute BetterPullReview.
It covers which package to choose, how to check a download before you install
it, what each package needs from the system, how to remove it, and what the
licences of the bundled components ask of you.

If you build or publish packages, see the
[packaging guide for maintainers](release-packaging-maintainers.md).

## Choose a package

Each release offers one native package per platform, plus checksums, an SPDX
inventory and a separate corresponding-source archive.

| Platform | Package | Notes |
|---|---|---|
| macOS 14.4 or later, Apple Silicon and Intel | `*-macos-universal.dmg` | Universal app (arm64 and x86_64). The app is also published as `*-macos-universal.zip`. |
| Windows 10/11 x64 | `*-windows-x64.msi` | Per-user installer. The `*-windows-x64-runtime.zip` beside it is an inventory for auditing, not a portable installation. |
| Ubuntu 26.04 LTS x86_64 | `*-linux-ubuntu-26.04-x64.deb` | Installed system-wide through the package manager. |
| Ubuntu 26.04 LTS x86_64 | `*-linux-ubuntu-26.04-x64-user.tar.gz` | Per-user archive with the same runtime as the DEB. |

> [!NOTE]
> v0.3.0 packages require macOS 13 or later. Packages built with Qt 6.12
> require macOS 14.4 or later.

Linux packages run on X11 through `qxcb`; on a Wayland desktop they use
XWayland. Native Wayland is not included or supported.

An RPM for RHEL 10 (`linux-rhel-10-x64`) can be built but is not part of the
selected release set.

Some releases may also offer packages built without agent integrations or local
automation. Their names end in `-agent-free` or `-agent-free-automation-off`; see
[Packages without agent integrations or local automation](#packages-without-agent-integrations-or-local-automation).

## Verify a download

Check each file before you install it. Get the package, its checksum file, the
SPDX inventory, the corresponding-source archive with its checksum, and the
publisher's identity through the channel you trust, such as the GitHub
release page. A checksum that came from an untrusted source proves
nothing about who made the file. Keep the published file names unchanged.

### Check checksums

On macOS or Linux, run the check from the folder that holds the downloads, so
the file names inside each checksum file resolve:

```sh
cd /absolute/path/to/downloaded-release
shasum -a 256 -c BetterPullReview-X.Y.Z-macos-universal.dmg.sha256
shasum -a 256 -c BetterPullReview-X.Y.Z-macos-universal.spdx.json.sha256
shasum -a 256 -c Source-BetterPullReview-X.Y.Z-macos-universal-corresponding-source.zip.sha256
```

Replace the version and platform with the names you actually downloaded.
Not every file has a `.sha256` beside it: where a release publishes only a
`.asc` signature (the Linux source archive, for example), verify that
signature instead, as described below.

On Windows, compare the output of
`Get-FileHash -Algorithm SHA256 -LiteralPath 'C:\absolute\candidate.msi'` with
the published digest. Do the same for the source archive and the SPDX file.

Checking a hash installs nothing.

### Check signatures

A signed package can also be checked with the platform's own tools.

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

```sh
signtool verify /pa /all /v C:\absolute\candidate.msi
```

Check each signed executable and DLL you received the same way. A zero exit
code is not enough: compare the signer, certificate chain and timestamp with
the expected publisher. An unsigned MSI has no signature to check.

**Linux**

For a DEB or per-user archive delivered with a detached `.asc` signature, use
the publisher's signing key, identified by its full fingerprint:

```sh
gpg --verify /absolute/candidate.deb.asc /absolute/candidate.deb
```

Each GitHub release explains how to obtain the key in its
**Linux signature verification** section. An RPM is checked with
`rpmkeys --checksig /absolute/candidate.rpm`, which needs the signing key
installed separately. If you install from a corporate repository, that
repository's signed metadata and package digest take the place of these
checks, under its own policy.

> [!WARNING]
> Never disable the operating system's trust checks, clear the macOS
> quarantine attribute or enrol an unknown certificate to make a package pass.
> An ad-hoc `codesign` signature only proves the local seal; it is not a
> Developer ID signature or notarization.

## Install

Install for your own account unless you need a managed system-wide install.

### macOS

Open the DMG and copy **BetterPullReview** into your Applications folder
(`~/Applications`).

### Windows

Run the MSI. It installs for the current user into
`%LOCALAPPDATA%\Programs\BetterPullReview` unless you choose another folder.
The installer remembers the folder you chose, so repair and uninstall find it.

If you run `msiexec` from a command line, give it native backslash paths.
Paths with forward slashes are refused before installation.

### Ubuntu: DEB package

Install the DEB with your package manager, which needs administrator rights.
It installs into `/usr`:

- the `better_pull_review` launcher,
- a desktop entry and a scalable icon in the `hicolor` theme,
- the private runtime under `/usr/lib/betterpullreview`.

The package declares the system libraries it needs, so the package manager
installs them.

### Ubuntu: per-user archive

1. Make sure the system libraries the DEB declares are installed. The archive
   does not install them.
2. Unpack `*-user.tar.gz` into its own versioned folder under your local
   application directory.
3. Run the `better_pull_review` executable inside it.

Do not set global Qt or library search paths for it.

### What the installed app contains

- **Helpers.** The MCP context helper and `better_pull_review_agent_supervisor`
  sit beside the main executable. On macOS the bundle also has
  `bpr_update_helper`. The supervisor ships in every package because Git runs
  inside it.
- **Plugins.** The app loads Qt plugins only from its own installation. It
  ignores plugin-path environment overrides, refuses command-line options that
  load arbitrary plugins, and never falls back to a developer Qt installation.
- **Licences.** A `ThirdParty` folder holds the notices and source offers (see
  [Licences and corresponding source](#exact-dependency-and-license-evidence)).
- **Windows resources.** The app icon, diff strips and syntax definitions ship
  as `better_pull_review.rcc` beside the executable. The app checks that this
  file is exactly the one it was built with. If it is not, the app does not
  start and prints `INSTALLED_RESOURCES_REFUSED:better_pull_review.rcc:`
  followed by `missing`, `unreadable`, `digest-mismatch` or
  `not-a-resource-file`. Reinstall to fix it.
- **Windows file properties.** The application and both helpers name the
  vendor, product, description, version and original file name. These
  properties are not a signature. Debug symbol files are not installed.

## System requirements

| Requirement | Detail |
|---|---|
| Git 2.39 or later | Needed for pull-request history and comparisons. Install it yourself. |
| GitHub CLI (optional) | macOS only. Needs `gh` 2.96+ and Git 2.49+. Install it yourself and select it in Settings. |
| AI agents (optional) | Installed separately and selected explicitly. |
| Linux system libraries | libstdc++/libgcc, OpenSSL, X11/XCB, libsecret and other OS libraries come from the system, not the package. |
| Linux C library | The package requires no glibc symbol version newer than 2.43. |

Qt's libraries and plugins are bundled privately with the app on every
platform. On Linux the package also carries its own build of ICU 73.2, the
Unicode library Qt uses.

## Updates

- **macOS:** an eligible signed installation can use **Check for Updates…**,
  which asks before it installs anything. See
  [Application updates](application-updates.md). Ad-hoc signed builds are not
  eligible.
- **Windows, Linux and managed installations:** update by installing the new
  package yourself.

## Remove BetterPullReview

> [!WARNING]
> Uninstalling removes program files only. Profiles, credentials, files you
> exported, desktop registration and update-recovery records stay behind, and
> reviews already published to GitHub are unaffected.

To remove your data as well, do it while the app is still installed:

1. Finish or reconcile any publication in progress, and any update that is
   being installed.
2. Sign out of each account.
3. Use the explicit profile purge in **Settings > Storage & privacy** for each
   profile. The purge retires that profile first, and cancelling does not
   restart it. It does not touch shared updater and registry state.

[Storage, diagnostics and removal](storage-and-diagnostics.md) lists exactly
what remains afterwards.

To remove the program, quit BetterPullReview, then:

- **macOS:** delete the app from its Applications folder.
- **Windows:** uninstall it from **Installed apps**, or uninstall the MSI.
- **Linux:** remove the per-user versioned folder, or remove the DEB with the
  package manager.

<a id="exact-dependency-and-license-evidence"></a>

## Licences and corresponding source

Original BetterPullReview code and documentation are under the
[MIT License](../../LICENSE). Bundled third-party components keep their own
licences. They include Qt (QtBase, QtSvg and QtImageFormats), SQLite,
KSyntaxHighlighting with its syntax and theme data, Quill, fmt, Glaze, zlib,
utf8proc, sha1collisiondetection, HACL*'s SHA-2, rapidfuzz-cpp, dtl, RE2 with
Abseil, Git's xdiff, the JetBrains Mono NL font (SIL OFL 1.1), LibArchive on
macOS and ICU 73.2 on Linux.

### What each package includes

The installed `ThirdParty` folder (`Contents/Resources/ThirdParty` on macOS)
contains:

- `THIRD-PARTY-NOTICES.txt`: every component's licence and notice text,
  including the full syntax and theme attributions, the font's licence and
  authors, ICU's mixed third-party text, and the full LGPL-2.1.
- `SOURCE-OFFERS.txt`: where to get the corresponding source and how to
  replace the LGPL libraries.
- Provenance records for the source and for the Qt build used.

Syntax and theme data keep their own licences (a mix of MIT, LGPL and GPL);
they are not relabelled MIT because the highlighting engine is MIT. Where the
SPDX inventory says `NOASSERTION`, no single licence has been concluded for
that component; it is not a waiver. On Windows the distributor must also
supply Microsoft's applicable runtime redistribution notice.

### Corresponding source

The corresponding source for each platform is a separate release asset:

`Source-BetterPullReview-VERSION-PLATFORM-corresponding-source.zip`

where PLATFORM is `macos-universal`, `windows-x64` or `linux-ubuntu-26.04-x64`
(`linux-rhel-10-x64` for the optional RPM; older Linux packages used
`linux-ubuntu-24.04-x64` or `linux-x64`). It is not embedded in the app,
installer or archive. The installed notices and source offers give its exact
download location (`releases/download/vVERSION/ASSET`) and SHA-256. It holds
the pinned upstream archives, including Qt's, with BetterPullReview's build
inputs. On Linux it also carries the ICU source with a standalone rebuild
recipe; start at `ICU-73.2-build/third_party/icu/rebuild/README.md`.

> [!IMPORTANT]
> Everyone who receives the binaries must be able to get the exact
> corresponding source at no additional charge.
>
> - The GitHub release holds binaries and source under the same access rules,
>   whether the repository is private or public.
> - If you redistribute the binaries inside a company or offline, deliver the
>   exact source archive with them.
> - If the repository's access changes or the download URL stops working, the
>   distributor must supply the source archive through the channel that
>   delivered the binaries.
> - Keep the source archive, its digest and the version tag unchanged for as
>   long as any matching binary is distributed, and for every retention period
>   that applies afterwards. Retention lasts until the accountable distributor
>   documents that every source-offer and licence obligation has ended.
>
> Package checks cannot prove future access or retention; the distributor has
> to arrange and maintain both.

### Replace Qt or xdiff with your own build

Qt and Git's xdiff are LGPL libraries linked dynamically, so you can replace
them with your own modified builds. `SOURCE-OFFERS.txt` describes how.

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

On macOS, a modified copy has to be resealed locally after any replacement.
Replacing a library gives that copy no update trust: it will not be eligible
for **Check for Updates…**.

## Packages without agent integrations or local automation

BetterPullReview can be built without its AI agent features, and optionally
without local automation as well. The code is removed from the build, not just
hidden in the window.

| Package name ends in | Removed | Still works |
|---|---|---|
| `-agent-free` | Agent launching, side chat, the finding queue, the comment workshop, tour writing assistance, the private **Ask agent**, agent-assisted composition, **Send to Agent**, the MCP context helper and the **Agents & context** settings page. No agent profile is offered. | Review, Review Cycles, notes and suggestions, **Copy**, **Save** and the bundle, capture, comparisons, responses, verification, publication, private notes and history, and every stored record, including ones an agent wrote earlier. |
| `-agent-free-automation-off` | All of the above, plus the local command endpoint that `bpr` talks to. There is no `bpr` for this package. | Everything above, single-instance activation, and tour routing inside the app. |

Things to know:

- `better_pull_review_agent_supervisor` still ships in every package, because
  guarded Git launches run inside it. It contains no agent protocol, provider
  knowledge or credentials.
- All variants share the same package identity (the macOS bundle identifier,
  the Linux package name and the Windows upgrade code). Installing one over
  another replaces program files only. Profiles, cycles, rounds, suggestions,
  responses, verification records, private history and credentials stay, and
  an agent-free build opens them. Replacing one variant with another on
  Windows and Linux has not yet been qualified natively.
- A disposable agent workspace left by a full install is still cleaned up.
- The `bpr` command-line tool is not a release package. If you installed it
  from a build with CMake, uninstall it by deleting the files listed in
  `build/tour-cli/install_manifest_TourCLI.txt`. The desktop app is unaffected.

To build these packages, see
[Build packages without agent integrations](release-packaging-maintainers.md#build-packages-without-agent-integrations).

## Known limitations

Native installation, signing trust, vault, accessibility, removal, crash-privacy
and vulnerability qualification are not complete on every platform. The
[release stop conditions](release-packaging-maintainers.md#release-stop-conditions)
list what is still open, and each release's notes record its own known
limitations.
