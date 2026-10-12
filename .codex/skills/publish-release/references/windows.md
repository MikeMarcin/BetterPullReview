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
Owner identity. The initial setup Owner login lacked signer access. On
2026-10-11, the owner approved a separate dedicated user scoped to this profile;
its signer role and isolated Azure CLI login were observed, and one disposable
probe signed/timestamped and passed native SignTool/Authenticode trust checks
for Michael Marcin. This is a producer prerequisite observation, not qualification
of a BPR release. No service principal or broader access was provisioned.

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

Reuse the producer's nonsecret `windows-signing.prepared.json` outside source:
`tools.signTool`, `tools.azureDlib`, `azure.metadata`, `azure.publisher` and
`azure.preparedCliConfigDir` bind the existing installed tools, metadata and
isolated signer profile. Set `AZURE_CONFIG_DIR` to that approved directory only
for the signing process. Metadata's `ExcludeCredentials` permits the intended
Azure CLI credential and excludes other/admin fallback methods. Do not inspect
Azure token/cache files, change global login or silently widen the credential
chain. Recheck approved account/profile/identity through Azure's supported
commands; user sessions may expire or require login/MFA. Permanent unattended
Azure authentication has not been established.

## Build, sign payloads, then package

Run the normal native test gate, then:

```powershell
cmake --preset windows-app-release
cmake --build --preset windows-app-release
```

Use the integrated signing path with approved absolute paths:

```powershell
python packaging/package.py --build build/windows-app-release `
  --generator WIX --output build/releases/X.Y.Z/windows-x64 `
  --signtool 'C:\absolute\x64\signtool.exe' `
  --azure-dlib 'C:\absolute\x64\Azure.CodeSigning.Dlib.dll' `
  --azure-metadata 'C:\absolute\metadata.json' --publisher 'Michael Marcin'
```

The driver builds first, signs the `app` and `helpers` recorded in
`_bpr/package-inputs.json`, then lets CPack stage the signed bytes and generate
SPDX. It requires byte-identical staged images and verifies their Authenticode
publisher, chain and timestamp. It preserves upstream Qt/CRT bytes and signatures.
The finished MSI is signed last; final hashes/provenance are written afterward.
Retain signed producer images and matching linker maps for the PE audit.

Outputs include MSI, runtime ZIP for inventory auditing, SPDX, separate source
ZIP, checksums, provenance and `*.msi.release.json`. Unsigned preparation remains
available by omitting all four signing options; it emits no release manifest.
The runtime ZIP is evidence, not a qualified portable Windows installation.

Extract the final MSI in an isolated root and audit its actual runtime using
`scripts/verify_release_package.py` with the matching build/SPDX/source/commit
and `--windows-redist-root`. Check installed first-party signatures using
`signtool verify /pa /all /v` and `Get-AuthenticodeSignature`; require publisher
`Michael Marcin` and a timestamp. MSI/runtime ZIP equivalence and native Windows
10/11 clean-account install/launch/auth/repair/upgrade/removal remain qualification
gates. The offline manifest verifier checks byte bindings and producer receipts;
it cannot independently establish Windows trust from a Mac.

The maintained verifier imports the native Windows PowerShell security module
by its absolute `$PSHOME` manifest before `Get-AuthenticodeSignature`.
An inherited PowerShell 7 `PSModulePath` cannot select its incompatible module;
do not use a parent-shell environment workaround as a replacement for this check.

Return to shared publication with the emitted manifest. Never hand-edit hashes
or flip `signed` to convert an unsigned candidate into a trusted release.

Primary references: [Microsoft signing integration](https://learn.microsoft.com/en-us/azure/artifact-signing/how-to-signing-integrations)
and [role assignment](https://learn.microsoft.com/en-us/azure/artifact-signing/tutorial-assign-roles).
