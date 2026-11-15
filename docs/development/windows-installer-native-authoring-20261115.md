# Windows installer native authoring, 2026-11-15

Mandate `release-prep-20261115-r2`, bounded BPR-1788 native authoring slice.
Source candidate `bf995128013b34dfa456468e2d2a832e00c3feb4` on
`codex/release-platform-prep-20261115`. This validates production installer
settings and authoring with an inert payload; it does not qualify the application
MSI or complete BPR-1788 AC2.

## Native result

Native Windows 11 build 26200, principal `mikem`, through `aito-main-win`:
CMake/CPack 4.4.2, WiX `5.0.2+aa65968c` and matching
`WixToolset.UI.wixext` 5.0.2. An isolated scratch project copied the exact
production Windows CPack block, adapted template and patch. No product graph
was built, no MSI was installed and no signing or release identity was allocated.
The existing Windows station and WSL runtimes were left untouched.

The first native build failed with WIX0091: duplicate `INSTALL_ROOT` and
`MSIINSTALLPERUSER` properties. CPack already declares secure `INSTALL_ROOT`;
WiX's `perUserOrMachine` scope supplies the standard `MSIINSTALLPERUSER`
default. Removing the duplicate patch declarations fixes compilation.
Explicit `SecureCustomProperties` authoring was also tried and rejected by
WIX0070; that attempt is absent from the candidate. No policy or elevation
boundary was weakened to obtain a pass.

The maintained native regression command is:

```powershell
py -3 tests\scripts\package_runtime_tests.py PackageRuntimeTests.test_windows_dual_scope_compiles_and_passes_ice105
```

It compiles the actual production Windows settings and patch with an inert file
named `better_pull_review.exe`, runs explicit `wix msi validate -ice ICE105`,
and reads the MSI database through Windows Installer. It installs nothing.
On this Windows host it passes with no skip. Restoring the original two
properties makes the same test fail with WIX0091; restoring the committed
correction passes again. Off Windows, or without the native packaging tools,
the test reports its explicit skip instead of supplying native credit.

## Observed MSI tables

The representative MSI's SHA-256 is
`3aed93ff0969cc97e88cce1e22c1989c3ac33d3b304bb9973019bc49b937531b`.
It contains only the inert file; do not offer it as an application installer.
The external evidence retains its generated WiX inputs and fourteen MSI tables.

| Surface | Native observation |
|---|---|
| Property | `ALLUSERS=2`, `MSIINSTALLPERUSER=1`, `BPR_INSTALL_SCOPE=user`; `INSTALL_ROOT` is in `SecureCustomProperties` |
| Summary | Installer version 500, `x64;1033`, WordCount 2; the per-user-only no-elevation bit is clear |
| Registry | Both package-owned markers have Root -1 (`HKMU`) |
| UI sequence | AppSearch 50, scope/root restores 51–56, related products 58, scope dialog 799, CostInitialize 800 |
| Execute sequence | AppSearch 50, scope/root restores 51–56, related products 57, CostInitialize 800 |
| Custom actions | Scope/root restore actions have type 307, including the first-sequence bit |
| Validation | CPack/WiX compilation and explicit ICE105 exit 0 |

The local package-runtime suite passes 35 tests with two explicit native-only
skips (Linux producer and this Windows check). Native execution supplies the
Windows result separately. These checks do not replace final package tables,
ordinary-user, credential elevation or installed-product behavior.

## Pending acceptance and continuation

BPR-1788 stays In Progress and AC2 stays pending. The complete application
candidate still requires the feature/common-candidate composition audit before
its product build. Then validate the final actual MSI and exercise both
ordinary-user routes, Everyone with different administrator credentials,
installing-user HKCU identity, old same-scope per-user/per-machine upgrades,
custom-directory repair/removal, both-marker refusal, scope-change
uninstall/reinstall and preservation of profile data. No review-ready product
candidate exists, so no attended session or manual acceptance is claimed here.

External continuation and raw evidence:
`/Users/jaeger/.codex/release-preparation/v0.5.0-20261115/windows-installer-native`.
The existing station's canonical checkout remains protected by its actual
supervisor lease; this slice uses only a separate owned scratch root.
