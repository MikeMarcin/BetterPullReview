# Ubuntu release readiness, 2026-10-16

The owner requested continuation of Ubuntu preparation and selected
**unversioned readiness for the next coordinated release**. This record covers
producer prerequisites and focused packaging contracts. A future release still
needs its selected common version/source and exact final-artifact evidence.
The existing Mac 0.2.1 and Windows 0.2.2 releases remain immutable.

The selected Linux route remains Ubuntu 26.04 x86_64 DEB plus the default
per-user archive. WSL2 is an admitted producer. The wider obligations on
[BPR-0961](../issues/50-blocked/BPR-0961-qualify-linux-x64-packages.md),
[BPR-0962](../issues/50-blocked/BPR-0962-verify-release-trust-and-security-boundaries.md)
and [RFC-BPR-0038](RFC-BPR-0038.md) remain open.

## Observed producer

The existing producer is `/home/mmarcin/bpr-v021` in distribution `Ubuntu` on
JaegAW, accessed from the Mac through SSH alias `aito-main-win`. Its clean
source is `aa6294c50f95b51d95f7c7861d7430f89cdb2fdc`, branch
`codex/bpr1527-wsl-v022-r51`. Neither its branch nor its source was rebound.
No competing CMake, CTest, Ninja or compiler workload was observed before the
focused course. Existing BPR-1527 r68b evidence records twelve affected
portability courses passing on this revision; that evidence retains its
original scope and is not a full Ubuntu release gate.

| Input | Observed value |
|---|---|
| OS and architecture | Ubuntu 26.04 LTS, x86_64, WSL2 Linux 6.18.33.2 |
| C runtime | glibc 2.43; selected package ceiling remains 2.43 |
| Compiler | GCC 15.2.0, Ubuntu package `15.2.0-16ubuntu1` |
| Build tools | CMake/CPack 4.4.2, Ninja 1.13.2, Python 3.13.15 |
| Qt | Existing provider 6.11.2 with producer SPDX files present |
| Native development libraries | libsecret 0.21.7, XCB 1.17.0 |
| Package tools | patchelf 0.18.0, readelf, ldd, dpkg-deb, dpkg-shlibdeps, dpkg-gencontrol, file, ar, tar and gzip available |
| Available producer disk | Approximately 710 GB at inspection |
| Public baseline CLI | `packaging/package_runtime.py baseline` returns `linux-ubuntu-26.04-x64` |

The existing login PATH finds the user-installed CMake, Ninja and Python.
patchelf needs the existing directory
`/home/mmarcin/.local/share/bpr-release-tools/patchelf-0.18/root/usr/bin`
appended to that PATH. No tools were installed. A generic `zip` CLI was absent;
the maintained driver uses Python's `zipfile`, so this is not a prerequisite.
The initial overbroad tool probe stopped there; the corrected required-tool
probe passed.

The QtBase producer SPDX SHA-256 is
`38a72dde5addf7a2e3ab0945e605d7ed614e4dac9106173481ca9fb5204f89ef`.
Presence of this build input supplies no final-runtime inventory qualification.

## Signing configuration

The approved native Windows `release_gpg_vault.py validate --config` command
passed through the existing producer-local configuration. It confirmed the
scoped GPG fingerprint/keygrip/home binding, passphrase protection and enabled
bounded preset configuration. Validation does not retrieve the Windows
credential, unlock the key or sign. The helper's observed producer SHA-256 is
`5db597dfa6240a50239d3286c44cf071dd47bfae0589b22bb45483ab5d1319f3`.

The approved public-key exports on Windows and WSL both match the recorded
SHA-256 `c271732f02b5f3c709ad1a61d7f7816200e96226c490fd77d88f6583437f9798`.
The prior successful enrollment and cold-agent courses remain historical
prerequisite evidence. Credential retrieval under the future actual producer
logon and signatures over final packages still require their own observations.
No passphrase or private key was requested or transferred.

## Focused verification

The original producer's four selected CTest rows ran inside its existing
network sandbox. Source assets passed 11 cases; native release passed 27 with
one Windows-only skip; package audit passed 7. Package runtime ran 21 cases
and failed its tiny native CPack fixture: `packaging/Install.cmake` requires
the separately replaceable `better_pull_review_xdiff` shared-library target,
which the fixture omitted. This is a test-graph regression; the production
graph already declares the library. The failed log and diagnostic child output
are preserved, including the exact CMake errors at the property and install
declarations. The fixture repair retains the existing target, baseline and
CPack-generator assertions and all production installation guards.

The [BPR-1561 repair](../issues/60-done/BPR-1561-restore-native-linux-cpack-baseline-fixture.md)
adds that one `SHARED` fixture target. The exact candidate file SHA-256
`ec369e426decff35b03f0f9da60521ebf2048f3dc786086d82fb618953ce3a96`
passed all 21 package-runtime cases in 0.858 seconds through the same producer
network sandbox. The course used a temporary detached ext4 worktree at
`aa6294c50f95b51d95f7c7861d7430f89cdb2fdc` plus only that line. The original
producer stayed clean; the temporary source was restored and removed after
verification. The affected installer, runtime implementation and test-support
inputs are byte-identical between that baseline and the local repair branch.

Across these four focused suites, 66 cases passed and one was explicitly
skipped. The unchanged three passing suites were reused. On macOS, the runtime
suite additionally passed its 20 available cases with the native-Linux fixture
explicitly skipped. The native release suite's explicit Linux skip is
`release_gpg_vault_tests.VaultTests.test_authenticode_ignores_poisoned_psmodulepath`,
which requires native Windows PowerShell. Synthetic packaging/signing fixtures
do not produce or qualify a distribution artifact.

Raw evidence and the resolved implement-rfc strategy are retained locally in
`.mask/sessions/ubuntu-preparation-20261016/`. The first native course is under
`ubuntu-readiness-20261016-evidence/`. Its failure remains part of the record.
The successful repair's receipt, patch and output are under
`ubuntu-readiness-20261016-repair/`; its receipt is dated
`2026-10-16T17:17:38.887613Z` with exit 0. The existing role preflight was restored
through MASK's reviewed directory rebind: the path and inode were unchanged,
and only the recorded device number changed. No skill version or role route
changed.

## Next release inputs

Select one version and committed source for the coordinated platform set, then
run the required normal native gate on that exact Ubuntu source before the
tests-disabled distribution build. Produce DEB/per-user archive, matching
corresponding source, installed notices/source offers, SPDX and provenance
through the maintained driver. Reuse the approved vault configuration on its
producer, verify every final signature and digest, and return only the complete
intended final asset directory and public verification material to the Mac
publisher. Native trust and the Mac publication preflight are separate checks.

Clean Ubuntu desktop/Secret Service authentication, exact delivered-package
installation/removal/residuals, native rendering/accessibility, runtime/ABI
qualification, security dispositions and distribution/source-access obligations
remain required within their existing cards. This preparation supplies no
acceptance of those criteria and requests no new owner action. No version was
changed and no production build, installation, unlock, signing, tag, push or
publication was performed.
