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
| `better_pull_review_composition` | Explicit profile, transport, vault, provider, worker, host, and Qt wiring |
| `better_pull_review` | Native read-only GitHub application executable |

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

The public application provides built-in GitHub.com native-HTTPS and
fail-closed CLI choices. Add explicit GHES 3.18 choices with:

```sh
build/dev/src/app/better_pull_review --ghes-host ghe.example.com
```

Activating native HTTPS opens the Qt-owned password prompt, then enrolls the
credential through the selected platform vault and authenticated `/user`
observation. Activating CLI shows the bounded unavailable state and starts
zero processes. `--self-test` enumerates every visible CLI choice, requires the
same bounded unavailable presentation for each, and reports
`CLI_UNAVAILABLE_COUNT:1` for the default profile set or
`CLI_UNAVAILABLE_COUNT:2` with `--ghes-host`. It then reports whether
composition selected the native platform vault. On macOS, Windows, and Linux
its successful fixed output includes `NATIVE_PLATFORM_VAULT_SELECTED:true`.

The test-enabled application also exposes two deterministic recorded public
paths:

```sh
QT_QPA_PLATFORM=offscreen \
  build/dev/src/app/better_pull_review \
  --github-fixture github-com \
  --render-evidence build/dev/github-public-path-github-com.png

QT_QPA_PLATFORM=offscreen \
  build/dev/src/app/better_pull_review \
  --github-fixture ghes-3.18 \
  --render-evidence build/dev/github-public-path-ghes-3.18.png
```

On headless Linux, also set `QT_STYLE_OVERRIDE=Windows`. Each run must report
`PUBLIC_PATH_OK`, the matching `GITHUB_FIXTURE_OK` marker, and `RENDER_OK`.
The fixture drives a real QAction through the concrete provider, application
host, core acceptance, Qt projection, and orderly exact credential deletion.
The test-enabled composition substitutes a recorded transport and fixture
vault, so the delete assertion means no credential remains in fixture-vault
state. This is deterministic structural integration/layout evidence, not a
real-network native-HTTPS run, native-vault lifecycle, final presentation
polish, or native system-theme certification. Tests-disabled composition
inspection separately proves selection of the native factory, and the native
transport/vault fixtures below exercise those implementations directly.

Run the focused composition group with:

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^application_(public_path(_ghes_cli_unavailable)?|worker_executor|composition_contract|github_public_path_(github_com|ghes_3_18))$'
```

`application_composition_contract` creates a fresh tests-disabled Release
tree. It proves the public application header has no testing conditional and
the Release compile commands, build graph, symbols, and raw executable bytes
contain no fixture translation unit, macro, argument, label, path, or evidence
string. It also requires production to compose only
`native_github_wire_transport_factory` and rejects CLI process-runner factory
symbols in the application. The tests-disabled `--self-test` enumerates every
visible CLI profile and requires `CLI_UNAVAILABLE_OK` plus the exact
`CLI_UNAVAILABLE_COUNT`; provider
counter tests prove the CLI viewer rejection makes zero transport-factory,
wire, capability, or CLI-specific effect calls, while the transport contract
proves a fake successful process port still receives zero calls. Public
clean-start selection itself performs one bounded noncredential
profile-resolution effect per row. Existing host lifecycle coverage separately
proves a native-to-CLI switch exact-deletes the predecessor credential binding
without enrolling or retrieving a credential for the CLI profile.
`application_public_path_ghes_cli_unavailable` supplies the explicit two-row
GitHub.com/GHES case.

On macOS, the fake-backed contract test is non-mutating. The native fixture
creates, replaces, retrieves, and deletes one disposable generic-password item
under the dedicated
`org.betterpullreview.github-native-credential.file-keychain.integration-test.v2`
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

BPR-0022's original target-native evidence used the historical v1 service.
The current fixture uses the disjoint hostname-bound v2 service shown above,
and the BPR-0029 macOS run repeated its setup, lifecycle, and exact cleanup.
Production never reads, enumerates, migrates, or falls back to v1.

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

BPR-0023's preparatory source and cross-PE evidence covered the historical v1
target format. The current source and fixture use the disjoint hostname-bound
v2 target. BPR-0036 retains every direct Windows 10/11 x64 current-format
lifecycle, cleanup, public-fixture, runtime, and dependency check.

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
unavailability also passed. That result is direct historical v1 Linux vault
evidence from a full-system VM, not bare metal. The current fixture uses the
disjoint hostname-bound v2 schema; its BPR-0029 result is separate and is not
inferred from the v1 pass.

The pinned BPR-0029 v2 run used an Ubuntu 26.04 x86_64 full-system QEMU guest.
A fresh tests-on Release build completed 81/81 targets, and all 40 non-native
registered test identities passed on that BPR-0029 snapshot after the repaired
dependency and composition gates. Secret Service
setup/lifecycle/cleanup passed 3/3, as did independent exact cleanup and the
hostile, unavailable, and locked cases. The GitHub.com and GHES public
fixtures passed with exact host and rendered state. A fresh tests-off Release
build completed 36/36 targets; the fixture seam was absent; the self-test
reported `PUBLIC_PATH_OK` and `NATIVE_PLATFORM_VAULT_SELECTED:true`; and
dependency, runtime, and ELF audits passed. The tests-off executable SHA-256
is `78a85e15319473e39baaa6b6c8723ff7f9adbade13b48475a3f94a5622719b01`.
The generated PNGs are deterministic layout/composition evidence only, not
presentation-polish, native system-theme, or light/dark-theme certification.

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
outside repository ancestry, request correlation, all eight typed
unavailability reasons, and the fixed zero-process `cli_unavailable` gate.
The first reason is the milestone-one product-scope deferral; the remaining
seven preserve the executable-image, cross-authority redirect, ambient
configuration, credential/account, working-directory, descendant-containment,
and numeric PID/process-group-reuse findings. The runner test uses
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
allocation, and target-native CLI runner behavior are deferred to
RFC-BPR-0015. BPR-0036 retains the Windows native-HTTPS, vault, and public-path
checks.

The selected-binary trust probe is separate and loopback-only:

```sh
bpr_gh_audit_root="$(mktemp -d "${TMPDIR:-/tmp}/bpr-gh-audit.XXXXXX")"
curl --fail --location \
  --output "$bpr_gh_audit_root/gh_2.96.0_macOS_arm64.zip" \
  https://github.com/cli/cli/releases/download/v2.96.0/gh_2.96.0_macOS_arm64.zip
(
  cd "$bpr_gh_audit_root"
  printf '%s  %s\n' \
    f23a0c37d963aacc3bed703ccbd59b41c5ca22101fab7f00eb2b7cad23aba463 \
    gh_2.96.0_macOS_arm64.zip |
    shasum -a 256 --check
)
ditto -x -k \
  "$bpr_gh_audit_root/gh_2.96.0_macOS_arm64.zip" \
  "$bpr_gh_audit_root/unpacked"
python3 tests/scripts/verify_gh_cli_trust.py \
  --gh "$bpr_gh_audit_root/unpacked/gh_2.96.0_macOS_arm64/bin/gh" \
  --expect-version 2.96.0 \
  --expect-sha256 \
  b1d6c442fde99ca27c04e1e74d624895abe37785f4a3e9e9b684bf7586ce4bc8
rm -r -- "$bpr_gh_audit_root"
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

The current audit selected the official `gh` 2.96.0 release dated 2026-07-02.
The downloaded macOS arm64 ZIP SHA-256 was
`f23a0c37d963aacc3bed703ccbd59b41c5ca22101fab7f00eb2b7cad23aba463`,
and the extracted binary SHA-256 was the value passed above. The expected
result is one fixed bad-certificate TLS alert with zero HTTP requests; a
followed cross-authority `302` whose origin receives the 47-byte body and
authorization while its sink receives neither; `307` and `308` origins
reached with sink behavior `not-observed-unproven`; and
`broker-availability: unavailable`. The installed `gh` 2.95.0 binary,
SHA-256
`798882434e7f6ae5846194191263ecc59d56bc201f13f016270f44cb4f34499e`,
was rerun and produced the same material outcomes. A different binary digest
requires a fresh review; never omit the expected digest and inherit a prior
result.

The probe also writes a private `http_unix_socket` setting, verifies that the
CLI loaded it, and performs the exact authenticated `gh api` request against
an AF_UNIX sentinel. The required secret-free markers are:

```text
http-unix-socket-config-loaded: yes
http-unix-socket-api-accepts: 1
http-unix-socket-api-http-requests: 1
http-unix-socket-api-authorization-exact: yes
```

These markers demonstrate that persisted `http_unix_socket` actively reroutes
`gh api`; it is not telemetry-only. Canonical
`SSL_CERT_FILE`/`SSL_CERT_DIR` strings still do not bind CA identity or
contents through the CLI's later open. The current `gh api` exposes no
per-invocation `--user`; `gh auth token --user` reveals the token and is
prohibited, while `gh auth switch --user` mutates persisted active-account
state. Source review additionally records per-round-trip `ActiveToken`
selection, keyring-error fallback to a blank-host token, and the macOS keyring
launch of `/usr/bin/security`.

Milestone one does not positively enable CLI transport. Native HTTPS is the
sole production authentication transport, and visible CLI choices return
fixed `cli_unavailable` with zero process launches. The transport reports
eight reasons: product-scope deferral plus executable-image identity,
cross-authority redirect behavior, ambient configuration, credential/account
identity, working-directory identity, descendant containment, and numeric
PID/process-group identity reuse. RFC-BPR-0015 owns any future enablement and
must directly close all eight conditions without a privileged helper, weakened
security, or GitHub Actions.

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
- the response-body limit and one 8-KiB-plus-one response-header-field limit;
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
Neither alone proves the composed public provider; the BPR-0029 fixture above
supplies that separate recorded executable path on the directly exercised
platform, with a substituted fixture transport rather than a real native
network request. There is no separate 65-field-count or aggregate-header-only
loopback scenario; those adapter checks remain source/contract evidence.

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

Tracked and on-disk scans found no GitHub Actions workflow files, and the
source/build configuration selected no GitHub Actions integration. This
reinforces the build-ownership rule below: no remote workflow contributed to
configuration, compilation, tests, tracing, or verification.

Windows 10/11 x64 execution, TLS/proxy/dependency inspection, and fixture
registration remain BPR-0036 work. The open Windows task is nonblocking for
macOS/Linux implementation, tests, documentation, security review, and
simplification or later-RFC work; it blocks only affected Windows claims and
final RFC-BPR-0002 Auto-Verified closeout.

`verify_runtime_surface.py` supports macOS, Linux, and Windows. It launches the
idle offscreen application, waits approximately 0.5 seconds, samples its
network sockets and direct children once, checks captured output for a canary,
and terminates it. A pass is a point-in-time idle observation, not continuous
monitoring or evidence for active native-HTTPS mode, indirect descendants, or
listener absence on a platform where the script was not run. The Windows run
remains BPR-0036 work. The binary-dependency script also supports all three
platforms, using `dumpbin` on Windows when available, followed by
`llvm-readobj` and `objdump` fallbacks.

For the BPR-0027 application host and provider, build the focused targets and
run the complete local contract group:

```sh
cmake --build --preset dev --target \
  better_pull_review_application_github_host_tests \
  better_pull_review_github_paged_read_tests \
  better_pull_review_github_provider_tests

ctest --test-dir build/dev --output-on-failure \
  -R '^(application_github_(host|contract|capability_cache)|github_(paged_read|provider|read_contract|capability_probe)|native_https_transport_(contract|fixture))$'
```

The pager/provider tests use injected and loopback-only transports; the host
uses fake ports and the manual decision lane. They do not mutate GitHub or
contact a private repository. Run the host, pager, and provider executables
under AddressSanitizer and UndefinedBehaviorSanitizer before closing the
slice. Also run the complete clean `dev` and `core-only` matrices,
documentation and source-pack validators, architecture boundaries, platform
configuration, workflow-absence scan, tests-disabled Release symbol/dependency
inspection, and `git diff --check`.

The host-owned credential binding catalog supports same-process host
recreation only. The public application selects exact deletion on orderly
shutdown; its delete terminal is part of host quiescence, and composition
closes and joins provider/vault workers only afterward. There is no durable
binding rehydration, enumeration, v1 fallback, or crash recovery. An abrupt
process or host crash can therefore leave a vault item that the next process
cannot discover. Query-bearing REST redirects continue to fail closed under
the completed BPR-0038 design decision. Any future relaxation requires new
direct public-contract evidence and separately scoped work.

For the BPR-0028 provider-free Qt projection and presentation, build the host
and Qt targets and run the atomic projection, architecture-boundary, and three
scale-specific presentation cases:

```sh
cmake --build --preset dev --target \
  better_pull_review_application_github_host_tests \
  better_pull_review_qt_github_tests

ctest --test-dir build/dev --output-on-failure \
  -R '^(application_github_host|qt_github_window_scale_(100|125|200)|architecture_boundaries|architecture_boundary_contract)$'
```

The presentation path receives only synthetic normalized projections and
issues no network, process, vault, or GitHub operation. The test harness saves
minimum-top, minimum-lower, and normal-size PNGs under the ignored build tree
and verifies the exact device-pixel ratio and raster dimensions. Inspect those
images in addition to the assertions; they are transient evidence and are not
checked in.

BPR-0031 adds explicit presentation-state, native-inheritance, wrapped-row,
state-preservation, and announcement checks to the same three scale cases.
Run the complete focused presentation group with:

```sh
cmake --build --preset dev --target \
  better_pull_review_qt_github_tests \
  better_pull_review_qt_github_conversation_model_property_tests
ctest --test-dir build/dev --output-on-failure \
  -R '^(qt_github_window_scale_(100|125|200)|qt_github_conversation_model_property)$'
```

Each scale also saves representative disconnected, loading, first-load error,
session cancellation, first-load cancellation, retained refresh error, and
retained refresh cancellation rasters beside the existing minimum and loaded
images. The assertions cover exact presentation properties and stack pages,
visible wording, retained identity/content, disclosures, responsive layout,
literal wrapped rows, focus, selection, collapsed expansion, inherited
palette/font changes, and combined deduplicated polite announcements.

On headless Linux, CTest selects Qt's built-in basic Windows style. Direct
Ubuntu 26.04 testing found Qt 6.10.2's offscreen Fusion cached painter could
segfault while repainting a combo box at 1.25x; the basic style passed 20
consecutive fractional-scale runs and retains the scale, geometry, content,
focus, and accessibility checks. This is deterministic headless raster
evidence, not native-desktop style certification.

For a native macOS startup check, first record the current System Appearance,
remove Qt platform/style overrides, then drive the test-enabled public fixture
through the real application:

```sh
defaults read -g AppleInterfaceStyle
env -u QT_QPA_PLATFORM -u QT_STYLE_OVERRIDE \
  build/dev/src/app/better_pull_review \
  --github-fixture github-com \
  --render-evidence build/dev/github-native-system.png
```

The run must report `PUBLIC_PATH_OK`, `GITHUB_FIXTURE_OK:github.com`, and
`RENDER_OK`; inspect the loaded style/plugin and rendered palette rather than
inferring appearance from the command. A controlled application-local color
scheme, palette, font, or style change can prove that Qt appearance events
refresh derived presentation state, but it cannot substitute for launching
under a real system Light appearance and changing the operating-system
appearance while the same loaded process remains alive. Record and restore the
original desktop setting for that manual native gate. Never force a palette or
QSS merely to make the evidence pass.

Only run the live Light-to-Dark gate after the desktop owner explicitly
authorizes changing the global appearance. The test-build-only probe reads the
global preference as independent evidence but never writes it. Use System
Events for the actual user-visible change, retain both captures in memory
until the application has shut down cleanly, and restore the original setting
through a shell trap:

```sh
set -eu

bpr_original_dark=$(
  osascript -e \
    'tell application "System Events" to tell appearance preferences to get dark mode'
)
case "$bpr_original_dark" in
  true|false) ;;
  *) exit 1 ;;
esac

bpr_probe_pid=
bpr_restore_appearance() {
  if test -n "$bpr_probe_pid" && kill -0 "$bpr_probe_pid" 2>/dev/null; then
    kill "$bpr_probe_pid" 2>/dev/null || true
    wait "$bpr_probe_pid" 2>/dev/null || true
  fi
  osascript -e \
    "tell application \"System Events\" to tell appearance preferences to set dark mode to $bpr_original_dark" \
    >/dev/null
}
trap bpr_restore_appearance EXIT HUP INT TERM

osascript -e \
  'tell application "System Events" to tell appearance preferences to set dark mode to false'
for bpr_attempt in $(seq 1 100); do
  bpr_dark_now=$(
    osascript -e \
      'tell application "System Events" to tell appearance preferences to get dark mode'
  )
  test "$bpr_dark_now" = false && break
  sleep 0.1
done
test "$bpr_dark_now" = false
sleep 1

bpr_evidence_dir=$(
  mktemp -d "${TMPDIR:-/tmp}/bpr-native-appearance.XXXXXX"
)
bpr_before="$bpr_evidence_dir/system-light.png"
bpr_after="$bpr_evidence_dir/system-dark.png"
bpr_log="$bpr_evidence_dir/probe.log"

env -u QT_QPA_PLATFORM -u QT_STYLE_OVERRIDE \
  build/dev/src/app/better_pull_review \
  --github-fixture github-com \
  --native-appearance-probe \
  --render-evidence "$bpr_before" \
  --changed-render-evidence "$bpr_after" \
  >"$bpr_log" 2>&1 &
bpr_probe_pid=$!

bpr_ready=false
for bpr_attempt in $(seq 1 300); do
  if \
    grep -qx 'NATIVE_APPEARANCE_READY:Light' "$bpr_log" \
    && grep -qx 'NATIVE_APPEARANCE_GLOBAL_READY:Light' "$bpr_log"
  then
    bpr_ready=true
    break
  fi
  kill -0 "$bpr_probe_pid" 2>/dev/null || {
    wait "$bpr_probe_pid" 2>/dev/null || true
    bpr_probe_pid=
    cat "$bpr_log"
    exit 1
  }
  sleep 0.1
done
test "$bpr_ready" = true
test ! -e "$bpr_before"
test ! -e "$bpr_after"

osascript -e \
  'tell application "System Events" to tell appearance preferences to set dark mode to true'
if wait "$bpr_probe_pid"; then
  bpr_probe_status=0
else
  bpr_probe_status=$?
fi
bpr_probe_pid=
test "$bpr_probe_status" -eq 0 || {
  cat "$bpr_log"
  exit "$bpr_probe_status"
}

for bpr_marker in \
  'PUBLIC_PATH_OK' \
  'GITHUB_FIXTURE_OK:github.com' \
  'RENDER_OK' \
  'NATIVE_APPEARANCE_READY:Light' \
  'NATIVE_APPEARANCE_GLOBAL_READY:Light' \
  'NATIVE_APPEARANCE_TRANSITION:Light->Dark' \
  'NATIVE_APPEARANCE_GLOBAL_TRANSITION:Light->Dark' \
  'NATIVE_APPEARANCE_PLATFORM:cocoa' \
  'NATIVE_APPEARANCE_STYLE:QMacStyle' \
  'NATIVE_APPEARANCE_SIGNAL_OK' \
  'NATIVE_APPEARANCE_STATE_OK' \
  'NATIVE_APPEARANCE_PIXELS_DIFFER_OK' \
  'NATIVE_APPEARANCE_CHANGED_RENDER_OK'
do
  grep -qx "$bpr_marker" "$bpr_log"
done
test -s "$bpr_before"
test -s "$bpr_after"

bpr_restore_appearance
trap - EXIT HUP INT TERM
cat "$bpr_log"
```

Inspect both PNGs directly after the trap has restored the desktop. The Light
and Dark images must show the same loaded GitHub.com selection, focus, and
collapsed conversation state, with native Cocoa/QMacStyle controls and
readable contrast. The probe's persisted-image check reloads the exact
in-memory pixels. Before persistence, it also requires equal raster geometry,
equal device-pixel ratio, and different pixels. Application-local
`QStyleHints` overrides, offscreen or forced-style renders, missing markers,
or a setting that was not restored are not acceptance evidence. This remains
a local target-native check; GitHub Actions has no role.

BPR-0028 recorded a direct adversarial model run. BPR-0030 preserves that
historical evidence and checks the corpus into a standalone, deterministic
CTest target:

```sh
cmake --build --preset dev --target \
  better_pull_review_qt_github_conversation_model_property_tests
ctest --test-dir build/dev --output-on-failure \
  -R '^qt_github_conversation_model_property$'
```

The single property-test identity is intentionally independent of the three
scale-specific window cases. It executes 3,000 fixed-seed mixed projection
updates, 5,000 fixed-seed permutations of 100 rows, and left and right
50,000-row rotations. It requires exact tree/order and every public role,
deletion-aware display and accessible text/description, deterministic
variation of every mutable role, minimal move counts, exact changed-row
counts, correct insert/remove signals, persistent-index preservation or
invalidation, and zero model resets.

After building the `dev` preset, run the focused BPR-0030 evidence group with:

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(core_github_policy|github_read_contract|github_provider|github_capability_probe|application_github_host|application_github_credential_vault|qt_github_conversation_model_property|native_https_transport_contract|native_https_transport_fixture|github_contract_fixture_security)$'
```

This focused group does not replace either complete preset matrix, sanitizer
coverage, target-native platform evidence, documentation/source-pack gates,
or `git diff --check`. Windows verification remains open under BPR-0036 and
does not block running or completing the available-platform work.

## Troubleshooting and safe verification

- A GitHub CLI profile is visible but unavailable by design in milestone one.
  Use the corresponding native-HTTPS profile. Positive CLI enablement remains
  RFC-BPR-0015 work, and selecting the unavailable profile must launch no
  process.
- Supply GHES as a canonical hostname only, such as `ghe.example.com`, not as
  a URL, path, query, fragment, credential-bearing authority, or GitHub.com
  subdomain. Invalid host configuration fails before network or credential
  work.
- Native HTTPS uses the operating system and Qt network stack's authoritative
  CA and proxy configuration. Retryable connection or proxy failures may
  advance only to the next bounded system-provided proxy candidate. TLS,
  hostname, origin-authentication, malformed-response, and policy failures
  stop, and exhaustion fails closed. Cross-authority and every query-bearing
  redirect fail closed. Do not bypass trust validation or weaken system policy
  to make a fixture pass.
- A locked, denied, unavailable, or failed native vault operation fails
  closed. There is no plaintext, settings, SQLite, environment, or CLI
  fallback. An orderly shutdown exact-deletes only the retained credential
  binding; it never enumerates or broadly deletes credentials.
- Prefer the recorded public fixtures and loopback native-transport fixtures
  for operator-safe verification. They do not contact or mutate GitHub: the
  recorded paths substitute transport and vault state, while loopback cases
  exercise bounded read-only REST `GET` and GraphQL `query` admission.
- An abrupt crash can leave an unenumerable native-vault item because the
  application has no durable selector or recovery scan. Current macOS/Linux
  native lifecycle and shared fixture-lifecycle evidence prove exact cleanup
  after an orderly lifecycle, not crash recovery; Windows target-native
  cleanup remains BPR-0036. Do not compensate with a broad credential
  deletion.

## Build ownership

GitHub Actions does not configure, build, test, package, release, or verify
BetterPullReview.
Developer builds run through the checked-in CMake presets on the current
machine. Clean Windows, macOS, and Linux package verification belongs to
RFC-BPR-0014 and runs on release-operator or corporate-managed environments.

Platform support is never inferred from Qt availability or a configuration
file. RFC-BPR-0014 requires direct install, launch, runtime, and uninstall
evidence from each supported native environment.
