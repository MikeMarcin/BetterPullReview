# ICU 73.2 supplied with the Linux Qt 6.12.0 provider

The Linux Qt6.12.0 `linux_gcc_64` essentials install includes ICU73.2 from
Qt's official `6.12.0-0-202609280346icu-linux-Rhel8.6-x86_64.7z` archive.
This is an existing Qt runtime dependency, not a core library adoption.
Only QtCore's required data, uc and i18n libraries are redistributed. The io,
test and tool libraries in the upstream archive are excluded.

- Binary archive: https://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt6_6120/qt6_6120/qt.qt6.6120.linux_gcc_64/6.12.0-0-202609280346icu-linux-Rhel8.6-x86_64.7z
- Its published SHA-256: `1fdbcd67b1760f3a3f031c64637a9f19992aed2f35aa615e12a468f8c655d4da`.
- Source: https://github.com/unicode-org/icu/releases/download/release-73-2/icu4c-73_2-src.tgz
- Source SHA-256: `818a80712ed3caacd9b652305e01afc7fa167e6f2e94996da44b90c2ab604ce1`.
- Source published SHA-512 (release `SHASUM512.txt`): `76dd782db6205833f289d7eb68b60860dddfa3f614f0ba03fe7ec13117077f82109f0dc1becabcdf4c8a9c628b94478ab0a46134bdb06f4302be55f74027ce62`.
- `LICENSE` is the complete `icu/LICENSE` from that exact source archive,
  SHA-256 `f3005e195ff74d8812cc1f182a1c446fab678d70a10e3dada497585befee5416`.

The exact binary hashes in `packaging/dependencies.json` were checked against
the SHA-256-verified official Qt6.12 archive. The three files are byte-identical
to the previously verified Qt6.11.2 provider; Qt6.12's actual Linux QtCore
NEEDED entries and SPDX still name this exact ICU73.2 trio. Native Linux
execution of the upgraded application remains unobserved:

| Runtime SONAME | Upstream file | SHA-256 |
|---|---|---|
| libicudata.so.73 | libicudata.so.73.2 | `4f182a855296cf96d39f75c9028b12765a78fe6afff3d3781d633110525b7a8b` |
| libicui18n.so.73 | libicui18n.so.73.2 | `a18e48266e59efff3ec872d9dc5cc99b776e180b8ede57269d354e6bc6595778` |
| libicuuc.so.73 | libicuuc.so.73.2 | `b76380b2c15c8ff8b3c94576ac98b40d34fc0a091db4a89324d72e271c7699f0` |

Packaging validates these exact official Qt ABI inputs and their selected-provider
origin. The shipped trio is rebuilt from the pinned ICU source, rather than
redistributing those prebuilt files: the upstream binaries lack immediate symbol
binding. `packaging/linux_icu.py` uses the configured native C/C++ compilers,
explicit PIC/stack-protector/frame-pointer/FORTIFY3 flags, and RELRO/NOW/noexecstack
link flags in a private out-of-source build. No shared Qt cache is modified.

The source archive includes that exact helper and `rebuild/README.md`, pinned as
ICU build inputs, so a recipient can repeat the build and replace the three
SONAME73 files without the application checkout or signing credentials.
`ThirdParty/icu-runtime-provenance.json` records the actual source-built producer,
source/recipe/compiler/make/flags binding, extracted-source hash, commands and
three output hashes. A cache must still match all of those inputs and bytes.
`runtime-closure.json` distinguishes the admitted Qt input hashes, hardened
source-built producer hashes and final relocated hashes. Audits reproduce the
existing patchelf loader-path operation on disposable source-built copies and
compare the complete delivered files. No other Qt-directory library gains admission.

Qt's own SPDX calls ICU73.2 a system third party and supplies no binary hashes,
source URL or licence conclusion. Preserve that producer record unchanged.
The explicit Linux ICU source package and full retained notices supplement it.
The primary Unicode permission and all included older ICU, BSD, dictionary/data
and build-tool exception texts travel together; no single aggregate licence or
prebuilt Qt hardening conclusion is asserted. Exact final Linux scan, native
trust, material review and independent applicability acceptance remain separate.
