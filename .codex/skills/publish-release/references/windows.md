# Windows release and manual Azure signing

Use native Windows 10/11 x64 with activated MSVC x64, Ninja and the repository's
Qt/CMake toolchain. Require WiX .NET tool 4/5 with matching
`WixToolset.UI.wixext`; the owner selected 5.0.2 rather than WiX 6's additional
EULA. Python 3.10+, Qt producer SPDX, `VCToolsRedistDir` and the applicable
Microsoft redistribution notice are required. Read the package guide for exact
inputs and native qualification limits.

## Azure prerequisite

The owner chose manual signing on the Windows PC. Recheck this configuration
and active profile/validated identity before use:

| Setting | Expected value |
|---|---|
| Region/endpoint | East US / `https://eus.codesigning.azure.net/` |
| Account | `gamecoretech` |
| Profile | `bprrelease`, Public Trust |
| Verified publisher | `Michael Marcin` |

Account names and bundle IDs are not certificate publisher names. This table is
configuration, not evidence of permanent readiness.

The identity needs **Artifact Signing Certificate Profile Signer**;
Owner/Contributor alone do not grant signing. Prefer a dedicated identity scoped
to this one profile. Microsoft documents profile-level assignment via Azure CLI;
the scope is the account's resource ID plus `/certificateProfiles/bprrelease`.
Verify effective roles: adding this role does not narrow an existing subscription
Owner identity. The setup session's Owner login lacked signer access; treat
assignment as pending until observed.

Keep authentication on the owner's signing PC; use the available wizard for
human-only login/MFA. Do not provision service-principal secrets or leave admin
Azure sessions on agent hosts merely to sign. Access changes need applicable
explicit approval. A signer-only identity cannot provision other services, but
signing overage can still cost money. Budgets are alerts, not hard caps; claim no
spending cap or deny policy without observing its enforcement.

Install Microsoft's official client:

```powershell
winget install -e --id Microsoft.Azure.ArtifactSigningClientTools
```

Verify compatible installed SignTool/.NET and matching x64
`Azure.CodeSigning.Dlib.dll` paths using the current Microsoft guide.
Keep metadata outside source:

```json
{
  "Endpoint": "https://eus.codesigning.azure.net/",
  "CodeSigningAccountName": "gamecoretech",
  "CertificateProfileName": "bprrelease"
}
```

The client uses its supported Azure credential chain. Select the intended
approved method with documented `ExcludeCredentials` when needed; do not
silently authenticate as another developer/admin account. No private key export
is needed.

## Build, sign payloads, then package

Run the normal native test gate, then:

```powershell
cmake --preset windows-app-release
cmake --build --preset windows-app-release
```

Read `build/windows-app-release/_bpr/package-inputs.json`. Its `app` and
`helpers` enumerate the first-party images shipped by the selected variant.
Sign these producer images **before CPack stages the runtime and generates
installed SPDX**. Retain matching linker maps and signed producer bytes for the
PE audit. Preserve upstream Qt/CRT signatures and byte evidence; do not resign
vendor files as this publisher.

For each approved first-party image, and later the finished MSI, use actual
absolute paths with this timestamped invocation:

```powershell
& 'C:\absolute\x64\signtool.exe' sign /v /fd SHA256 /td SHA256 `
  /tr 'http://timestamp.acs.microsoft.com' `
  /dlib 'C:\absolute\x64\Azure.CodeSigning.Dlib.dll' `
  /dmdf 'C:\absolute\metadata.json' 'C:\absolute\artifact.exe'
if ($LASTEXITCODE -ne 0) { throw 'Signing failed' }
```

Timestamping preserves verification after Azure's short-lived certificate
expires. Require each image's `signtool verify /pa /all /v` to pass with the
expected publisher, chain and timestamp.

Then package into a new output directory:

```powershell
python packaging/package.py --build build/windows-app-release `
  --generator WIX --output build/releases/X.Y.Z/windows-x64
```

The driver builds again before CPack. Require signed producer digests unchanged
through that build/staging step and valid signatures in installed images. If
a rebuild replaces an image, stop and correct ordering in the existing packaging
path. Do not distribute it unsigned or sign only an already-inventoried copy.
`package.py` has no signing hook and records `signed: false`; this manual order
requires exact-byte checks.

Sign/verify the finished per-user MSI. Extract that final MSI in an isolated root
and audit its actual runtime with `scripts/verify_release_package.py` using
matching build/SPDX/source/producer commit (`--windows-redist-root` supplies
original CRT inputs). MSI signing must leave delivered runtime unchanged.
Check every first-party payload signature, not just the installer wrapper.

Finalize MSI checksum/provenance after the outer signature. The driver's
`.msi.sha256` covers several products and `.msi.provenance.json` records a
pre-sign MSI hash. Both need final-byte finalization through reviewed existing
tooling: preserve source/SPDX hashes and producer commit, record signing evidence,
and validate every final product digest. Merely flipping `signed` is insufficient.
Until that finalizer exists and is verified, these are signed candidates, not
publication-ready outputs. Do not claim `package.py` implements it today.

Upload readiness includes final MSI, SPDX, emitted source ZIP, final checksums
and truthful provenance. Return to shared publication; the current macOS
publisher cannot upload this set. Signature validity does not complete native
Windows 10/11 clean-account install/launch/auth/repair/upgrade/removal criteria.

Primary references: [Microsoft signing integration](https://learn.microsoft.com/en-us/azure/artifact-signing/how-to-signing-integrations)
and [role assignment](https://learn.microsoft.com/en-us/azure/artifact-signing/tutorial-assign-roles).
