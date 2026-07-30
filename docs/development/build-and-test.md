# Build and Test

## Prerequisites

- CMake 3.25 or newer
- Ninja
- A C++23 compiler
- Qt 6.8 or newer for the Qt adapter and application
- Python 3 for structural and runtime verification scripts
- On Linux, stable `libsecret-1>=0.20.0` and its development metadata

The validated local foundation environment uses CMake 4.3, AppleClang 16,
Ninja, Python 3.14, and Qt 6.11.1 on macOS arm64.

## Presets

`dev` configures the complete foundation, including Qt and all tests:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

`core-only` excludes the Qt target graph and proves that the core and
application host build without Qt:

```sh
cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only --output-on-failure
```

Both presets use out-of-source directories under ignored `build/`.

Windows developers and release operators use the conditional
multi-configuration preset that selects the Visual Studio 2022 x64 generator:

```powershell
cmake --preset windows-dev
cmake --build --preset windows-dev
ctest --preset windows-dev --output-on-failure
```

## Targets

| Target | Responsibility |
|---|---|
| `better_pull_review_core` | Qt/effect-free semantic library |
| `better_pull_review_application` | Host, effect/decision ports, serialized completion, manual executor |
| `better_pull_review_adapter_{github,git,sqlite,ai}` | Capability boundaries established for later RFCs |
| `better_pull_review_adapter_qt` | Native Qt presentation adapter |
| `better_pull_review_composition` | Concrete foundation wiring |
| `better_pull_review` | Native application executable |

## Focused evidence

```sh
cmake --build build/dev --target check_boundaries
python3 scripts/verify_documentation.py --root .
python3 scripts/verify_binary_dependencies.py \
  build/dev/src/app/better_pull_review
python3 scripts/verify_runtime_surface.py \
  build/dev/src/app/better_pull_review
build/dev/src/app/better_pull_review --self-test
build/dev/src/app/better_pull_review \
  --render-evidence build/dev/foundation-window.png
```

On macOS, the fake-backed contract test is non-mutating. The native fixture
creates, replaces, retrieves, and deletes one disposable generic-password item
under the dedicated
`org.betterpullreview.github-native-credential.file-keychain.integration-test.v1`
service in the exact default Keychain:

```sh
cmake --build build/dev --target \
  better_pull_review_macos_keychain_contract_tests \
  better_pull_review_macos_keychain_native_tests
ctest --test-dir build/dev --output-on-failure \
  -R '^macos_keychain_(contract|native|native_setup|native_cleanup)$'
```

The setup, lifecycle, and cleanup tests share one CTest resource lock. The
native parent performs same-binary cleanup after every reaped child outcome,
and the independent cleanup fixture verifies the exact item is absent. Run
this only on a designated macOS test account where use of the default
Keychain is acceptable. Do not treat it as proof that an item orphaned by a
crash remains removable after rebuilding the current ad hoc-signed test
binary; stable package identity and cross-build lifecycle verification belong
to RFC-BPR-0014.

On a designated Windows 10/11 x64 test account, the Windows target builds the
fake-backed contract and native Credential Manager fixture:

```powershell
cmake --preset windows-dev
cmake --build --preset windows-dev
ctest --preset windows-dev --output-on-failure `
  -R '^windows_credential_manager_(contract|native|native_setup|native_cleanup)$'
```

The native setup, lifecycle, and cleanup tests share one CTest resource lock
and use only the fixed integration-test target prefix. Runtime canaries enter
the child through anonymous standard input, not arguments or environment.
The parent requires the child to stop, deletes the exact integration item,
verifies absence, scans fixed output and the isolated scratch tree, and
verifies successful scratch removal. The independent cleanup fixture repeats
exact deletion and absence.

Run the complete `windows-dev` CTest matrix as well as the focused command.
Inspect the resulting PE imports and dependencies on that same Windows
machine. A MinGW cross-compile, a non-Windows build that excludes the targets,
or a mock-backed pass is useful preparatory evidence but is not a native
Credential Manager result. Do not run the fixture on an account where a
disposable test credential is unacceptable.

On Linux, configure the regular `dev` preset and build the fake-backed
contract plus native Secret Service fixture:

```sh
cmake --preset dev
cmake --build build/dev --target \
  better_pull_review_linux_secret_service_contract_tests \
  better_pull_review_linux_secret_service_native_tests
ctest --test-dir build/dev --output-on-failure \
  -R '^linux_secret_service_(contract|native|native_setup|native_cleanup)$'
```

Run the resource-locked setup/lifecycle/cleanup sequence only inside a
designated desktop D-Bus session with a Secret Service implementation and an
unlocked default collection. A controlled Xvfb, D-Bus, and test-only
gnome-keyring session under a dedicated user is suitable. Never place the
keyring password or a credential canary in arguments, environment, command
output, or checked-in configuration. The fixture uses only the disjoint
integration-test schema and confirms exact cleanup.

The unavailable, locked, and dependency-level redirection cases are
controlled direct checks rather than registered CTest cases. Run the first
outside a desktop session with no session-bus address, the second in a fresh
test session whose real default collection is locked, and the third while an
ordinary canonical Secret Service remains available:

```sh
bpr_linux_vault_test=build/dev/tests/github_contract/\
better_pull_review_linux_secret_service_native_tests

env \
  -u DBUS_SESSION_BUS_ADDRESS \
  -u DBUS_STARTER_ADDRESS \
  -u DBUS_STARTER_BUS_TYPE \
  -u XDG_RUNTIME_DIR \
  -u DISPLAY \
  -u WAYLAND_DISPLAY \
  -u SECRET_SERVICE_BUS_NAME \
  "${bpr_linux_vault_test}" --expect-unavailable

"${bpr_linux_vault_test}" --expect-locked

SECRET_SERVICE_BUS_NAME=org.betterpullreview.Hostile.Secret \
  "${bpr_linux_vault_test}" \
  --expect-bus-redirect-rejected
```

The production adapter does not read or mutate
`SECRET_SERVICE_BUS_NAME`; the last command proves that a dependency-level
redirect is detected after opening without a session and before explicit
session establishment or item work. In the live hostile-owner audit, only
proxy-property inspection reached that owner; `OpenSession` and every
collection or credential-item method remained absent. These commands are
valid evidence only when their stated session preconditions are independently
established.

The BPR-0024 Linux vault matrix passed in an Ubuntu 26.04 x86_64 full-system
QEMU VM with an emulated CPU, Linux 7.0, Qt 6.10.2, libsecret 0.21.7, and
gnome-keyring 50. Under Xvfb, a desktop D-Bus session, and a dedicated real
user, the four focused CTest cases passed; the lifecycle stored and retrieved
73 bytes, replaced them with 2,560 bytes, deleted the item, and proved
absence. Runtime canary probes, exact integration-schema cleanup, hostile
service-name rejection, a truly locked collection, and no-session-bus
unavailability also passed. This is direct Linux vault evidence from a
full-system VM, not bare metal and not proof of the pending native HTTPS or
public composition surfaces.

For the BPR-0025 GitHub CLI process boundary, build and run the two focused
contract executables:

```sh
cmake --build build/dev --target \
  better_pull_review_gh_cli_transport_tests \
  better_pull_review_github_cli_process_runner_tests
ctest --test-dir build/dev --output-on-failure \
  -R '^(gh_cli_transport|github_cli_process_runner)$'
```

The transport test checks the closed operation-to-argument translation,
complete requested child-environment allowlist, canonical directory currently
outside repository ancestry, request correlation, four typed trust blockers,
and the fixed zero-process `cli_unavailable` gate. The runner test uses
same-executable child modes to exercise standard input, retained-output
limits, both output streams, deadline, cancellation, process failure, and
reaping without a shell. On Unix, direct child observation proves closed
inherited descriptors, disabled core dumps, reset signal handlers, and that
`CreateNewSession` makes the child the new session and initial process-group
leader. Lifecycle cases prove clean leader exit, nonzero failure, timeout, and
cancellation signal descendants that remain in that initial process group;
an escape case proves a descendant that moves to another group survives until
the fixture removes it. Queue-expiry cases prove no late launch.

The runner owns one serialized process worker. A compiled seam test proves
completion-watchdog quarantine, rejection of another request with zero
launch, and recovery after the worker resumes. Source and hostile architecture
gates separately require an actual failed bounded reap to retain the live
`QProcess` on the quarantined worker for continued reaping without spawning
unbounded workers or invoking Qt's destructor wait inside the completed call;
the compiled suite does not force that live-process path.
Allocation-free shape validation and pre-cancellation precede work-item
allocation and request copying; every checked N+1 dimension must leave the
marker-backed helper unlaunched.

On macOS, raw child-environment inspection observes only the requested
allowlist plus Darwin's deterministic `__CF_USER_TEXT_ENCODING` addition. On
Linux, the checked request includes its explicit locale fact and raw child
inspection must find no addition. These checks bound retained bytes and every
read requested through the public Qt API; Qt does not expose a cap for its
private in-flight pipe buffers, so do not describe those internal allocations
as hard-bounded. Windows Job Object descendant containment, private pipe
allocation, and target-native runner behavior remain BPR-0036 work.

The selected-binary trust probe is separate and loopback-only:

```sh
python3 tests/scripts/verify_gh_cli_trust.py \
  --gh /opt/homebrew/bin/gh \
  --expect-version 2.95.0 \
  --expect-sha256 \
  798882434e7f6ae5846194191263ecc59d56bc201f13f016270f44cb4f34499e
```

It copies the selected executable into an owner-private temporary directory,
hashes those exact bytes, executes only that copy, and verifies the digest
again afterward. A private temporary CLI configuration holds a runtime-random
synthetic token solely to pass `gh api`'s startup authentication gate. The
probe runs from that canonical non-repository directory, never contacts
GitHub, retains no response, raw header, or TLS error text, and removes every
temporary input. `GH_CONFIG_DIR` is verification-only and is forbidden from
the production broker. Since `gh` rejects an ephemeral loopback `host:port`
as `--hostname`, the synthetic `github.com` startup hostname remains in this
fixture. Treat the loopback result as exact-binary, source-backed redirect
characterization, never as exact production-authority proof.

For the recorded binary, the expected result is one fixed bad-certificate TLS
alert with zero HTTP requests; a followed cross-authority `302` whose origin
receives the 47-byte body and authorization while its sink receives neither;
`307` and `308` origins reached with sink behavior
`not-observed-unproven`; and `broker-availability: unavailable`. A different
binary digest requires a fresh review; do not drop the expected digest and
inherit this result.

Redirect behavior is only one of four fixed production blockers. The direct
probe binds its result to exact copied bytes, while production's canonical
path/name validation cannot yet bind those bytes through replacement or
update races to the image actually launched. Official
CLI configuration permits persisted `http_unix_socket`, which remains
ambient when the child receives its selected home/application-data path.
Canonical Linux `SSL_CERT_FILE`/`SSL_CERT_DIR` strings do not bind CA
file/directory identity or contents through the CLI's later open. Also,
`gh api --hostname` has no per-invocation account selector and can use an
account changed independently by `gh auth switch`, so it cannot bind the child
to the request's credential identity generation. All four reasons must be
eliminated or directly bound under BPR-0037 before CLI mode can start a
process. BPR-0037 must also bind an owner-controlled working-directory
identity across launch, close directory permission/repository-creation and
replacement races, prove hostile CA-file/CA-directory replacement and content
mutation fail closed, and contain or fail closed for a Unix descendant that
moves out of the initial process group. It must also bind PID/process-group
identity through cleanup or otherwise eliminate numeric `kill(-pgid, ...)`
reuse risk after leader exit; the current group signaling is not claimed
race-free.

For the BPR-0026 native HTTPS internal wire transport, build the contract
executable and run the contract plus target-native loopback fixture:

```sh
cmake --build build/dev --target \
  better_pull_review_native_https_transport_tests
ctest --test-dir build/dev --output-on-failure \
  -R '^native_https_transport_(contract|fixture)$'

build/dev/tests/github_contract/\
better_pull_review_native_https_transport_tests --runtime
```

The runtime line has this fixed, secret-free shape:

```text
native-https-runtime:tls=BACKEND:proxy=KIND:candidates=COUNT:supported=BOOL
```

It reports Qt's active TLS backend category and support flag, then the first
raw system-proxy candidate category and raw candidate count for the canonical
synthetic GHES profile. It does not print proxy addresses, CA material,
credentials, URLs, or remote content. The runtime query is observation only,
not proof that every returned candidate is reachable. The request path
independently filters a maximum of 16 supported direct, HTTP, or SOCKS5
candidates and uses them in system order.

The Python fixture generates a disposable CA and leaf certificate, starts
only loopback CONNECT proxies, and passes one random bearer token plus the CA
to the compiled child through length-framed standard input. It verifies the
exact CONNECT authority, SNI, Host, request target, method/body, and
Authorization placement. The 15 scenarios are:

- trusted success;
- retryable first-proxy failure followed by ordered fallback;
- accepted same-authority REST redirect;
- rejected cross-authority redirect;
- rejected GraphQL redirect;
- untrusted certificate;
- response-body and response-header limits;
- in-flight cancellation, timeout, and pre-cancellation with no network
  effect;
- HTTP 401, 403, and 429 typed classification;
- content-encoding rejection.

The fixture replaces proxy discovery with a test-only ordered loopback list
and may add one disposable CA to the system root set. It otherwise uses the
production request, TLS, redirect, cancellation, response, and credential
binding path. This distinction matters: the fallback scenario proves
transport candidate ordering, while the `--runtime` observation and
target-native inspection characterize the selected system proxy/TLS backend.
Neither is the BPR-0027 public provider composition.

The default production deadline is 30 seconds, but synchronous Qt system/PAC
proxy discovery and system-root enumeration cannot be interrupted by the stop
token or deadline. Once discovery returns, the request uses the remaining
absolute deadline. Adapter limits cover accepted and retained bytes, not Qt's
private HTTP parser or socket/TLS buffers. Likewise, local mutable credential
buffers are overwritten, but Qt-owned Authorization copies do not expose an
exact wipe operation.

To prove the loopback proxy/CA seam is absent from a production build, use a
separate tests-disabled tree and inspect the GitHub adapter symbols:

```sh
cmake -S . -B build/native-https-production -G Ninja \
  -DBPR_BUILD_QT=ON \
  -DBPR_BUILD_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/native-https-production

nm -g \
  build/native-https-production/src/adapters/github/\
libbetter_pull_review_adapter_github.a |
  rg 'make_native_https_(bootstrap|account)_transport_for_test'
```

The final `rg` must return no match. Inspect target dependencies and runtime
backend on the same native machine; a configure-time Qt discovery on another
platform is not evidence.

On the directly exercised macOS arm64 host, both focused CTest cases and all
15 fixture scenarios passed. The runtime line was:

```text
native-https-runtime:tls=openssl:proxy=direct:candidates=1:supported=true
```

The tests-disabled dependency/runtime audit selected Qt Network 6.11.1,
observed the OpenSSL runtime backend, retained direct Security and
CoreFoundation dependencies for the macOS vault in the GitHub adapter, found
no test-only native HTTPS symbols, and observed the current ad-hoc linker
signature. These are development build/runtime facts, not signed package or
distribution certification.

The Linux target-native run passed in an Ubuntu 26.04 x86_64 full-system QEMU
guest with Linux 7.0.0-28, GCC 15.2, and Qt 6.10.2. The copied inputs were
frozen before execution:

```text
native_https_transport.cpp
  sha256=fd7976b48241795302ded4d1a73c851064edce66364a3b50b990294ca95ab134
verify_native_https_transport.py
  sha256=bd0f771cd261a7fb6b4f1f4691af93c12adc7d4fe7ced96cb1182ee8bf2369dd
```

`native_https_transport_contract` and
`native_https_transport_fixture` passed 2/2 in 0.12 and 11.29 seconds,
respectively; the harness reported all 15 direct scenarios passing.
`architecture_boundaries` and `architecture_boundary_contract` passed 2/2 in
5.00 and 144.35 seconds with all 92 hostile mutations. The uninstrumented
focused transport suite passed repeatedly. Runtime observation was:

```text
native-https-runtime:tls=openssl:proxy=direct:candidates=1:supported=true
```

ELF inspection showed direct `NEEDED` entries for Qt6Network and Qt6Core. A
controlled `strace` of only the trusted-success case observed
`qt6/plugins/tls/libqopensslbackend.so`, `libssl.so.3`, and `libcrypto.so.3`
being opened. Do not use a full-suite `strace` as behavioral evidence:
instrumentation perturbs the fixture's cancellation timing. The dependency
trace intentionally runs one success, while the repeated uninstrumented suite
is the functional evidence.

Source and the generated Linux build contained no workflow files or GitHub
Actions references. This scan reinforces the build-ownership rule below; no
remote workflow contributed to configuration, compilation, tests, tracing, or
verification.

Windows 10/11 x64 execution, TLS/proxy/dependency inspection, and fixture
registration remain BPR-0036 work. The open Windows task is nonblocking for
macOS/Linux implementation, tests, documentation, security review, and
simplification; it blocks only affected Windows claims and RFC-BPR-0002
closeout.

`verify_runtime_surface.py` runs on macOS, Linux, and Windows. The
binary-dependency script also supports all three platforms, using `dumpbin` on
Windows when available, followed by `llvm-readobj` and `objdump` fallbacks.

## Build ownership

GitHub Actions does not configure, build, test, package, release, or verify
BetterPullReview.
Developer builds run through the checked-in CMake presets on the current
machine. Clean Windows, macOS, and Linux package verification belongs to
RFC-BPR-0014 and runs on release-operator or corporate-managed environments.

Platform support is never inferred from Qt availability or a configuration
file. RFC-BPR-0014 requires direct install, launch, runtime, and uninstall
evidence from each supported native environment.
