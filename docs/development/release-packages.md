# Install and verify BetterPullReview packages

How to check, install and remove a package, and what redistributors owe.
Detail is in the [packaging reference](release-packaging-maintainers.md#recipient-package-reference).

## Choose a package

| Platform | Package |
|---|---|
| macOS 14.4+, Apple Silicon and Intel | `*-macos-universal.dmg` (or `.zip`) |
| Windows x64 | `*-windows-x64.msi`, per-user |
| Ubuntu 26.04 x86_64 | `*-linux-ubuntu-26.04-x64.deb`, or the per-user `*-user.tar.gz` |

Tested on Windows 11 and Ubuntu 26.04 (WSL2). Other Windows and Linux versions
may work but are untested; the packages do not refuse them. Qualification on
the tested environments is itself incomplete; see
[Known limitations](#known-limitations).

Linux runs on X11 or XWayland, not native Wayland. Install Git 2.39+ yourself;
optional GitHub CLI support is macOS-only and needs `gh` 2.96+ and Git 2.49+.

## Verify a download

Take the checksums and the publisher's identity from a channel you trust, such
as the GitHub release page, and keep the file names unchanged.

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

On Windows, compare
`Get-FileHash -Algorithm SHA256 -LiteralPath 'C:\absolute\candidate.msi'` with
the published digest.

### Check signatures

- **macOS:** `codesign --verify --deep --strict --verbose=2 /absolute/BetterPullReview.app`
  and `spctl --assess --type execute --verbose=4 /absolute/BetterPullReview.app`
  must pass; the signing team is `YTQKP2V2A8`.
- **Windows:** `signtool verify /pa /all /v C:\absolute\candidate.msi`, then
  confirm the signer is the expected publisher.
- **Linux:** `gpg --verify /absolute/candidate.deb.asc /absolute/candidate.deb`
  with the key from the release's **Linux signature verification** section.

> [!WARNING]
> Never disable OS trust checks, clear the macOS quarantine attribute or enrol
> an unknown certificate to make a package pass.

The full check list is in [signature checks](release-packaging-maintainers.md#signature-checks).

## Install and update

- **macOS:** open the DMG and copy the app to `~/Applications`.
- **Windows:** run the MSI; it installs into
  `%LOCALAPPDATA%\Programs\BetterPullReview` by default.
- **Ubuntu:** install the DEB with your package manager, or unpack the
  per-user archive into its own folder (install the DEB's declared system
  libraries first) and run `better_pull_review`.

Eligible signed macOS installs can use **Check for Updates…**
([Application updates](application-updates.md)). Everything else updates by
installing the new package.

## Remove BetterPullReview

> [!WARNING]
> Uninstalling removes program files only. Profiles, credentials and exports
> stay behind. To remove them, sign out and run the profile purge in
> **Settings > Storage & privacy** before uninstalling; see
> [Storage, diagnostics and removal](storage-and-diagnostics.md).

<a id="exact-dependency-and-license-evidence"></a>

## Licences and corresponding source

BetterPullReview's own code is [MIT](../../LICENSE); bundled components keep
their own licences, listed in `THIRD-PARTY-NOTICES.txt` in the installed
`ThirdParty` folder (`Contents/Resources/ThirdParty` on macOS). Each platform's
source is a separate release asset,
`Source-BetterPullReview-VERSION-PLATFORM-corresponding-source.zip`.

> [!IMPORTANT]
> Everyone who receives the binaries must be able to get the exact
> corresponding source at no charge. If you redistribute inside a company or
> offline, ship the source archive with the binaries and keep it unchanged for
> as long as you distribute them.

Qt and xdiff are LGPL and replaceable; `SOURCE-OFFERS.txt` explains how. See
[licence detail](release-packaging-maintainers.md#bundled-components-and-notices).
**Help > About BetterPullReview** opens both files, and its **Copy version
info** copies the version, commit, Qt and system for a bug report.

## Packages without agent integrations or local automation

Packages ending `-agent-free` remove every AI agent feature; those ending
`-agent-free-automation-off` also remove the `bpr` command endpoint. Review,
Review Cycles, notes, publication and stored data all still work, and
installing one variant over another keeps your data. See
[agent-free variants](release-packaging-maintainers.md#agent-free-variants-further-detail).

## Known limitations

Native installation, signing trust, vault, accessibility, removal, crash-privacy
and vulnerability qualification are incomplete; see the
[release stop conditions](release-packaging-maintainers.md#release-stop-conditions).
