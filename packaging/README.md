# Packaging

This root owns distribution metadata and platform packaging. RFC-BPR-0001
establishes the boundary; RFC-BPR-0014 will add signed Windows, macOS, and Linux
artifacts and their release evidence.

Packaging must preserve the core/application/adapters/composition dependency
graph, restrict Qt plugin discovery to trusted installation paths, and must not
introduce an updater, browser runtime, listener, telemetry, or remote loader.

## RFC-BPR-0002 dependency record

The effect-backed GitHub adapter adds private Qt Core and Qt Network 6.8+
dependencies plus the platform credential-vault dependency. Its native HTTPS
implementation uses the TLS backend and system CA/proxy sources selected by
the target's Qt Network runtime; the application does not package an alternate
CA bundle or proxy implementation in this slice.

| Target | GitHub adapter dependency facts |
|---|---|
| macOS arm64 development host | Qt Network 6.11.1; active TLS backend reported as OpenSSL; the public tests-off Release self-test reports native platform-vault selection; the binary links Security/CoreFoundation and imports `SecItemAdd`, `SecItemCopyMatching`, `SecItemUpdate`, and `SecItemDelete`; current binary is ad-hoc linker-signed |
| Linux x86_64 full-system QEMU guest | Historical v1 Secret Service and internal native-HTTPS evidence: Ubuntu 26.04, Linux 7.0.0-28, GCC 15.2, Qt 6.10.2; adapter build includes Threads and `libsecret-1>=0.20.0`; native HTTPS test ELF directly needs Qt6Network and Qt6Core; runtime selected OpenSSL and one direct proxy candidate. The current v2 public selection/runtime, exact cleanup, tests-disabled seam absence, and dependency/runtime/ELF audits passed under BPR-0029 |
| Windows 10/11 x64 | Qt Core/Network, Threads, and `Advapi32` for Credential Manager are selected by the build; native runtime, TLS/proxy, and PE dependency inspection remain BPR-0036 work |

The BPR-0029 current-v2 Linux run completed fresh tests-on and tests-off
Release builds at 81/81 and 36/36 targets. All 40 non-native registered test
identities have passing current-tree evidence after the repaired gates; the
Secret Service setup/lifecycle/cleanup cases passed 3/3 plus independent exact
cleanup and hostile/unavailable/locked cases; and both public fixtures passed
with exact host and rendered state. The tests-off self-test reported
`PUBLIC_PATH_OK` and `NATIVE_PLATFORM_VAULT_SELECTED:true`; its executable
SHA-256 is
`78a85e15319473e39baaa6b6c8723ff7f9adbade13b48475a3f94a5622719b01`.

Linux verification froze `native_https_transport.cpp` at SHA-256
`fd7976b48241795302ded4d1a73c851064edce66364a3b50b990294ca95ab134`
and its harness at
`bd0f771cd261a7fb6b4f1f4691af93c12adc7d4fe7ced96cb1182ee8bf2369dd`.
The focused contract/fixture and architecture matrices passed 2/2, and the
architecture matrix covered all 92 hostile mutations. The uninstrumented
focused suite passed repeatedly. A controlled trace of one
trusted-success case under `strace` opened
`qt6/plugins/tls/libqopensslbackend.so`, `libssl.so.3`, and `libcrypto.so.3`.
A full-suite `strace` is not evidence because instrumentation perturbs
cancellation timing; the single-success trace is dependency evidence only.
Source and the generated build contained no workflow files or GitHub Actions
references.

The macOS and Linux observations are development build/runtime evidence, not
a signed, notarized, bundled, or installable package claim. RFC-BPR-0014 still
owns trusted Qt plugin placement, complete transitive dependency inventory,
SBOM, signing/notarization, installers, launch, upgrade, and uninstall evidence.
Windows target-native execution is nonblocking for current implementation and
documentation work but remains required for the affected Windows claim and
RFC-BPR-0002 closeout. The public application exact-deletes its retained v2
credential on orderly shutdown; it has no durable selector or enumeration
path, so no crash/restart recovery is claimed.

`BPR_BUILD_TESTS=ON` adds the adapter-private native HTTPS seam and the
GitHub.com/GHES public composition fixture. A directly inspected
`BPR_BUILD_TESTS=OFF` macOS build exported no
`make_native_https_*_transport_for_test` symbols, omitted the fixture
translation unit and test definitions from its compile graph, and contained
none of the fixture arguments, status strings, labels, identities, paths, or
access symbols in its raw application bytes. Release/package builds must keep
tests disabled and repeat the graph, byte, symbol, and dependency audits on
the packaged target.

GitHub Actions has no role in configuring, building, testing, packaging,
releasing, signing, or verifying BetterPullReview. Packaging evidence comes
from committed source on local release-operator or corporate-managed native
machines.
