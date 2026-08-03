# Build and Test

## Prerequisites

- CMake 4.4.2
- Ninja
- A C++23 compiler
- Qt 6.8 or newer for the Qt adapter and application
- Python 3.10 or newer for structural and runtime verification scripts
- Git 2.39 or newer for installed-Git contract tests
- Clang-format and clang-tidy 22.1.8 for coding-standard verification
- On Linux, stable `libsecret-1>=0.20.0` and its development metadata

The validated local foundation environment uses CMake 4.4.2, AppleClang 16,
Ninja, Python 3.14, and Qt 6.11.1 on macOS arm64.

## Dependency boundary

The root resolves dependencies before product subdirectories. Qt uses the
caller-installed CMake package and is never fetched. Python and Git use the
standard CMake find modules, while Linux libsecret uses `pkg-config`; each is
discovered only when its selected target graph needs it. The centralized
provider requires `Qt6::*`, `Python3::Interpreter`, `Git::Git`, and
`LibSecret::LibSecret` before product targets are declared, so adapters and
tests never repeat package discovery. Those target names must not be
predeclared by a parent project: the provider rejects collisions before
discovery so its target-provenance audit cannot describe an injected target as
an installed package. Python, Git, Threads, and `PkgConfig` are selected
explicitly from CMake's built-in module providers regardless of
`CMAKE_FIND_PACKAGE_PREFER_CONFIG`; package redirects for provider-owned names
are rejected before any redirect script can execute.
BetterPullReview must be the top-level project until RFC-BPR-0014 adds its
install/export development package. `CMAKE_PROJECT_TOP_LEVEL_INCLUDES` is
rejected so a dependency provider cannot intercept audited discovery. The
repository modules, CTest, and CMake's built-in find modules are loaded through
trusted absolute paths rather than caller-controlled `CMAKE_MODULE_PATH`.

CPM.cmake 0.43.1 is the source-dependency provider. Its loader and SQLite
archive are pinned by cryptographic digests, and concurrent configurations
serialize initialization of the shared loader and SQLite source caches. SQLite
remains `DOWNLOAD_ONLY`, is validated by archive, source-file, version, and
source-ID checks, and copies only the verified `sqlite3.c` and `sqlite3.h` into
a controlled build-tree staging directory. The static
`BetterPullReview::SQLite3` target never includes the broader CPM extraction
directory. CPM is not used to fetch Qt or to replace caller-installed platform
packages.

KSyntaxHighlighting 6.28.1 is acquired the same way, with
extra-cmake-modules 6.28.0 as a pinned transitive build input. Caller-installed
discovery was investigated first, because the library is Qt-dependent and Qt is
never fetched, but no Homebrew formula exists for it or for the KDE Frameworks,
which would have made RFC-BPR-0006 unimplementable rather than deferred. Each
input carries an archive digest and an extracted-source digest computed over a
sorted manifest of every file's own digest, so a changed, added, or removed file
is detected rather than only a tampered archive; a symbolic link anywhere in the
extracted tree is rejected. The provider states its own Qt 6.9 minimum and fails
naming both versions rather than letting the nested configure fail less legibly.
The Qt-free `core-only` preset does not acquire it. Provenance is recorded in
`third_party/ksyntaxhighlighting/PROVENANCE.md`.

Producer hints are `Qt6_ROOT`/`Qt6_DIR`, `Python3_ROOT_DIR`, `Git_ROOT`,
`CMAKE_PREFIX_PATH`, and `PKG_CONFIG_PATH`. `CPM_SOURCE_CACHE` selects the
shared source cache used by the exact
CPM 0.43.1 loader and SQLite 3.53.4 download-only provider. Presets use
`build/.cpm-cache` so `dev` and `core-only` reuse the same verified source.

For a fail-closed cached configure, reuse an already generated build tree and
cache:

```sh
cmake --preset dev -DBPR_DEPENDENCY_OFFLINE=ON
```

Offline mode validates the pinned CPM script and uses only existing SQLite
cache content. A missing CPM script, archive extraction, or changed source is
fatal before any download is attempted. Caller-installed package discovery is
local and remains available in offline mode.

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
Both also have schema-12 workflow presets, for example
`cmake --workflow --preset core-only`.

Run the RFC-BPR-0004 core/host diff seam and its Qt-free architecture checks
with:

```sh
cmake --build --preset core-only --target \
  test_better_pull_review_diff_transition_core_tests \
  test_better_pull_review_application_diff_host_tests
ctest --preset core-only --output-on-failure \
  -R '^(core_dependency_boundary|core_boundary_contract|architecture_boundaries|architecture_boundary_contract|binary_dependency_audit_contract|core_diff_transition|application_diff_host)$'
```

`application_diff_host` uses separate deterministic effect and decision lanes;
it does not contact GitHub, launch Git, or require Qt.

Run the selected RFC-BPR-0004 rendered/composed checks headlessly and one at a
time. Do not parallelize this group: each case owns an application or Qt
instance and its evidence files.

```sh
CTEST_PARALLEL_LEVEL=1 QT_QPA_PLATFORM=offscreen \
  ctest --preset dev --output-on-failure \
  -R '^(qt_diff_(workbench|link_map)_scale_(100|125|200)|application_diff_workbench(_shutdown)?_public_path)$'
```

The six scale cases are deterministic layout, interaction, and raster
evidence. The two public-path cases traverse the selected production
composition and shutdown lane. The normal public path must emit
`PUBLIC_PATH_OK`, `GITHUB_FIXTURE_OK:github.com`, `DIFF_WORKBENCH_OK`, and
`DIFF_INTERACTION_EVIDENCE_OK`, followed by `RENDER_OK`. It writes the final
full-window raster to
`build/dev/tests/end_to_end/diff-workbench-public-path.png` and an 18-step
rendered interaction sequence plus `interaction-trace.jsonl` below
`build/dev/tests/end_to_end/bpr-0083-diff-interaction/`. The trace records the
retained comparison position, active segment, projected coordinates,
requested and applied pane origins, clamp state, selected identities,
link-map dimensions, visible-set work, and selection-callback counts, geometry
generation, and semantic generation for every rendered frame. Scale rasters are written below
`build/dev/tests/adapters_qt/`. Offscreen output is not target-native
System-theme or final presentation-polish evidence.

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
| `better_pull_review_adapter_github` | Normalized provider, native-HTTPS, credential-vault, and recorded-test adapters |
| `better_pull_review_adapter_git` | Concrete Qt Core/`QProcess` installed-Git local pack/object/ref adapter |
| `better_pull_review_sqlite3` | Verified CPM-acquired static SQLite 3.53.4 provider, exposed as `BetterPullReview::SQLite3` |
| `better_pull_review_adapter_sqlite` | Concrete BPEC v1 epoch-catalog storage and operation-state adapter |
| `better_pull_review_adapter_ai` | Capability boundary established for a later RFC |
| `better_pull_review_adapter_qt` | Native Qt presentation adapter |
| `better_pull_review_composition` | Explicit profile, transport, vault, provider, worker, host, and Qt wiring |
| `app_better_pull_review` | Native read-only GitHub application target; emits `better_pull_review` |

## Coding standard

`_coding-standard/STANDARD.md` is the canonical first-party C++ standard. The
root `.clang-format`, `.clang-tidy`, and `.editorconfig` files are its local
enforcement configuration. The default scripts scan `src/` and `tests/` while
excluding build output, generated sources, CMake dependency trees, and vendored
code.

After configuring `dev`, run the non-default CMake targets:

```sh
cmake --build --preset dev --target coding-format-check
cmake --build --preset dev --target coding-tidy
```

`coding-format` rewrites the complete first-party tree. Direct invocation is
available when explicit tool paths or a narrower path set are needed:

```sh
python3 _coding-standard/scripts/format_check.py --root .
python3 _coding-standard/scripts/format.py --root .
python3 _coding-standard/scripts/tidy.py \
  --root . --build-dir build/dev --warnings-as-errors
```

Coding targets default to the value of `BUILD_TESTING`. A normal
tests-disabled product graph therefore has no Python or coding-tool dependency;
set `-DBPR_ENABLE_CODING_TARGETS=ON` to add the targets deliberately. The tidy
profile's narrow compatibility exclusions and compilation-database adaptation
are documented in `_coding-standard/README.md`.

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

Run the durable public epoch path with:

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^application_epoch_persistence_(github-com|ghes-3_18)$'
```

Each case launches the real test-enabled BetterPullReview executable twice
against one disposable application-data root. The seed process accepts the
first revision, triggers the real refresh action, imports two complete local
protocol-v2 packs through installed Git, and selects Whole PR, Previous
upload, My last reviewed, arbitrary, reverse, and identical pairs through the
real Qt controls. The reuse process reopens the same SQLite catalog and bare
cache, explicitly loads retained history, repeats the selections, and must
make zero smart acquire, discovery, `ls-refs`, or fetch calls. Both authority
shapes use sanitized recorded GitHub responses plus a deterministic local Git
repository; no GitHub endpoint, credential, or private content is involved.

`application_composition_contract` creates a fresh tests-disabled Release
tree. It proves the public application header has no testing conditional and
the Release compile commands, build graph, symbols, and raw executable bytes
contain no fixture translation unit, macro, epoch fixture argument/marker,
label, path, or evidence string. It also requires production to compose only
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
  test_better_pull_review_macos_keychain_contract_tests \
  test_better_pull_review_macos_keychain_native_tests
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
  test_better_pull_review_linux_secret_service_contract_tests \
  test_better_pull_review_linux_secret_service_native_tests
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
  test_better_pull_review_gh_cli_transport_tests \
  test_better_pull_review_github_cli_process_runner_tests
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
  test_better_pull_review_native_https_transport_tests
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
  -DBUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
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
  test_better_pull_review_application_github_host_tests \
  test_better_pull_review_github_paged_read_tests \
  test_better_pull_review_github_provider_tests

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
  test_better_pull_review_application_github_host_tests \
  test_better_pull_review_qt_github_tests

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
  test_better_pull_review_qt_github_tests \
  test_better_pull_review_qt_github_conversation_model_property_tests
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

### Native appearance verification

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

Under Cocoa the probe refuses to collect initial evidence while the real
window is inactive or has no focus widget. It prints
`NATIVE_APPEARANCE_WAITING_FOR_OWNER_FOCUS` once and waits. The desktop owner
must click the BetterPullReview window to activate it and give a widget focus;
the probe never calls an activation API, synthesizes input, or relaxes a
post-focus assertion, so an unattended run reaches
`NATIVE_APPEARANCE_ACTIVE_FOCUS_FAILED`, writes no PNG, and is a clean
unverified attempt rather than evidence. Non-Cocoa probes enter the
fail-closed check immediately. The readiness loop below therefore does not
progress until the owner has focused the window.

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
  'NATIVE_APPEARANCE_ACTIVE_FOCUS_OK' \
  'NATIVE_APPEARANCE_SIGNAL_OK' \
  'NATIVE_APPEARANCE_STATE_OK' \
  'NATIVE_APPEARANCE_PIXELS_DIFFER_OK' \
  'NATIVE_APPEARANCE_DIFF_PALETTE_INHERITED_OK' \
  'NATIVE_APPEARANCE_DIFF_FONT_INHERITED_OK' \
  'NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_OK' \
  'NATIVE_APPEARANCE_DIFF_SELECTION_OK' \
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
collapsed conversation state and the same selected file comparison, with
native Cocoa/QMacStyle controls and readable contrast. Retain the serialized
`NATIVE_APPEARANCE_DIFF_PALETTE_BEFORE/AFTER`,
`NATIVE_APPEARANCE_DIFF_FONT_BEFORE/AFTER`, and
`NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_DELTA:1` lines with the marker
results. The probe's persisted-image check reloads the exact in-memory pixels.
Before persistence, it also requires equal raster geometry, equal
device-pixel ratio, and different pixels. Application-local
`QStyleHints` overrides, offscreen or forced-style renders, missing markers,
or a setting that was not restored are not acceptance evidence. This remains
a local target-native check; GitHub Actions has no role.

BPR-0028 recorded a direct adversarial model run. BPR-0030 preserves that
historical evidence and checks the corpus into a standalone, deterministic
CTest target:

```sh
cmake --build --preset dev --target \
  test_better_pull_review_qt_github_conversation_model_property_tests
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

For the BPR-0042 installed-Git local-process boundary, configure the `dev`
preset and run:

```sh
cmake --build --preset dev --target \
  test_better_pull_review_git_process_contract_tests
ctest --preset dev --output-on-failure \
  -R '^(git_process_contract|architecture_boundaries|architecture_boundary_contract)$'
python3 tests/scripts/architecture_boundary_tests.py
```

Git 2.39 is the minimum admitted version, but version text alone does not
select a usable installation. Adapter construction binds the absolute
executable and matching exec path and runs fixed SHA-1 and SHA-256
pack/object/ref capability probes under the production isolation policy. The
contract then exercises real installed Git plus a same-test fake child for
exact command/environment/working-directory inspection and process-terminal
faults.

This focused target proves the BPR-0042 credential-free local process and
completed-spool consumer on the machine where it runs. It does not prove the
BPR-0043 native smart-HTTPS/protocol-v2 path, cache authority or reuse,
BPR-0044 journal recovery, BPR-0047 public composition, target-native Linux
behavior when run on macOS, or the BPR-0053 Windows process/Job Object/path
surface.

Commit `b65b0a9` retains that boundary while binding cleanup decisions to
exact filesystem identities, serializing process-boundary mutation across
adapter instances, and adding hostile replacement/cleanup cases. It does not
turn process-only evidence into cache, transport, recovery, or Windows
evidence.

For the committed BPR-0043 protocol-v2 and native smart-HTTPS components, run:

```sh
cmake --build --preset dev --target \
  test_better_pull_review_application_git_protocol_v2_tests \
  test_better_pull_review_native_git_smart_transport_tests \
  test_better_pull_review_git_process_contract_tests
ctest --preset dev --output-on-failure \
  -R '^(application_git_protocol_v2|git_protocol_boundary|native_git_smart_transport_(contract|fixture)|git_process_contract)$'
```

The five-case focused replay after commits `d903290`, `edd762e`, and
`b65b0a9` passed 5/5: `git_protocol_boundary`,
`application_git_protocol_v2`, `native_git_smart_transport_contract`,
`native_git_smart_transport_fixture`, and `git_process_contract`. The
application tests cover the closed discovery/`ls-refs`/fetch sequence, exact
refs and OIDs, same-cache verified haves, object-format agreement, response
section and sideband grammar, arbitrary raw chunk fragmentation, bounds,
cancellation, and one-time pack-sink abort. The source boundary prevents
other production code from bypassing that complete acquisition through raw
smart-transport rounds.

The native loopback verifier runs 19 scenarios against the synthetic GHES
profile and the discovery, `ls-refs`, and fetch success subset against the
GitHub.com profile, for 22 isolated child runs. It directly checks one exact
CONNECT and TLS origin request, SNI/Host, route, media types,
`Git-Protocol: version=2`, and Basic authorization formed from
`x-access-token:<token>`. The proxy receives no authorization, and no GitHub
or GHES service is contacted. GitHub.com smart Git uses `github.com`, whereas
its REST/GraphQL authority remains `api.github.com`; GHES smart Git uses its
exact configured hostname.

Discovery and `ls-refs` control replies are read in chunks but retained only
within the 64-KiB whole-reply limit. Fetch is not retained as one body: native
reads are at most 16 KiB and are delivered to the incremental consumer. The
production smart total deadline is 15 minutes and its no-progress idle
deadline is 30 seconds. The contract asserts the total policy and the fixture
exercises a reduced test-only absolute deadline; it does not wait 30 seconds
to establish independent elapsed idle-time evidence. Do not describe the
fixture timeout case as such evidence.

For the committed BPR-0043 cache/ref adapter, build and run its direct contract
separately because the hostile filesystem/process matrix is intentionally
long-running:

```sh
cmake --build --preset dev --target \
  test_better_pull_review_git_cache_contract_tests
ctest --preset dev --output-on-failure -R '^git_cache_contract$'
```

At commit `2801241`, `git_cache_contract` passed 1/1 in 162.58 seconds. The
adjacent `git_process_contract`, application-protocol, native-transport, and
two architecture CTest cases passed 7/7 in 92.95 seconds. Directly running
`tests/scripts/git_protocol_boundary_tests.py` passed 3/3 and
`tests/scripts/architecture_boundary_tests.py` passed 104/104. A
tests-disabled Release build contained zero cache fault-injection markers.
The documentation validator and `git diff --check` passed, and tracked and
on-disk GitHub Actions workflow counts were 0/0.

The cache contract covers exact authority isolation/reuse, SHA-1 and SHA-256,
manifest/repository/lock/spool/pack/index identity checks, true cross-process
writer exclusion, bounded inventory and quota behavior, stop-aware public
operations, abort/finish linearization, import terminal reconciliation,
paired retained refs, exact prune/delete, and chronological old-head residency
after the fixture remote prunes that object. A corrected-snapshot independent
review completed cleanly. A conservative simplification review made no code
change: the repeated explicit guards and cleanup branches are the executable
fail-closed invariants rather than safely collapsible duplication.

These earlier commands verify committed application-protocol, native-wire,
installed-Git, and cache/ref component boundaries, not durable operation
facts. BPR-0044 catalog and BPR-0045 host evidence are recorded separately
below. Completed BPR-0047 supplies public composition, and BPR-0053 retains
target-native Windows execution. No BPR-0043 result above proves durable
authorization, host restart orchestration, selection by the BetterPullReview
executable, or Windows behavior.

For the BPR-0044 durable epoch-catalog component, build and run the exact core,
application-port, storage, and behavior contracts with:

```sh
cmake --build --preset dev --target \
  test_better_pull_review_epoch_core_tests \
  test_better_pull_review_application_epoch_catalog_contract_tests \
  test_better_pull_review_sqlite_storage_contract_tests \
  test_better_pull_review_sqlite_behavior_contract_tests
ctest --preset dev --output-on-failure \
  -R '^(core_epoch_policy|application_epoch_catalog_contract|sqlite_epoch_catalog_(storage|behavior)_contract)$'
ctest --preset dev --output-on-failure \
  -R '^sqlite_epoch_catalog_(storage|behavior)_contract$' \
  --repeat until-fail:5
```

A fresh macOS `dev` matrix passed 58/58. The focused
core/application/storage/behavior matrix passed 20/20 across five repetitions,
the AddressSanitizer/UndefinedBehaviorSanitizer matrix passed 4/4, and the
Qt-free `core-only` matrix passed 25/25. The storage executable verifies private
fixed-path bootstrap, no-replace publication, exact schema and integrity
admission, permissions/identity/symlink rejection, cancellation, and exact
bootstrap-orphan cleanup. The behavior executable verifies authority allocation
and restart, append/history/anchor/review facts, phase and evidence
compare-and-swap, revision exhaustion with zero mutation, exact quotas,
sustained WAL checkpointing, recovery paging, prune/cache-delete intent and
finalization, abort lifecycle restoration, repair provenance, and repeated
retention-rehydration cycles.

Prune facts admit only `available`, `unavailable`, or `corrupt` base/head
starting states and persist those expectations with the exact epoch, OIDs,
refs, and catalog revision. Begin/replay/finalize/restart drift fails closed.
Successful finalization marks both anchors and linked non-destructive reviews
`pruned` and releases retained quota once; replay neither decrements quota nor
mutates rows again. These tests feed exact absent-ref evidence into the catalog
and do not perform the BPR-0055 core-authorized Git ref deletion.

The source-observation fixtures use deterministic nonzero 256-bit values. They
prove exact persistence, restart round-trip, and rejection of a previously
published identity within the catalog incarnation. BPR-0045 supplies the
production operating-system CSPRNG, recovery-before-mint ordering, and fresh
retry after a catalog-reported collision; no BPR-0044 test claims entropy
quality.

The catalog builds verified, CPM-acquired SQLite 3.53.4 source as one static
library without a system fallback. Runtime admission verifies BPEC
application/schema identity,
the exact 18-table/16-index schema, integrity and foreign keys, prepared
allowlisted SQL, defensive/trusted-schema policy, 4,096-byte pages, the
262,144-page 1-GiB main-database limit, local WAL with `synchronous=FULL`, a
256-page automatic-checkpoint threshold, and disabled memory mapping. History,
recovery, exact-anchor, and prune batches are bounded to 100; a cache-delete
set is bounded to 10,000 PR scopes; retained epochs are bounded to 100,000 per
PR.

A tests-disabled Release build kept SQLite as the pinned static dependency and
contained no catalog fault-injection symbol or test-only classifier. The
component does not have a system-SQLite fallback. Tracked and on-disk GitHub
Actions workflow counts were 0/0.

A clean MinGW cross-build compiled and linked 47/47 targets. `ctest -N` listed
25 tests, exactly two labeled `BPR-0053` and `target-native-windows`. It did not
execute a Windows catalog test or any product binary. Only a real Windows x64
run may satisfy those native cases; the cross-build establishes source and link
compatibility only.

This component evidence proves durable catalog facts and recovery decisions
for exact host-supplied observations. The next section records the BPR-0045
host composition of those facts; completed BPR-0047 supplies public
composition, BPR-0055 supplies maintenance authorization/recovery, BPR-0056
supplies core/host history paging, BPR-0046 supplies its Qt consumption, and
BPR-0053 retains target-native Windows behavior. GitHub Actions has no role in
any of these commands or claims.

For the BPR-0045 epoch application-host component, build and run the source-
free handoff/runtime-port contract, manual-executor host contract, core policy,
and adjacent durable/physical contracts with:

```sh
cmake --build --preset dev --target \
  test_better_pull_review_epoch_core_tests \
  test_better_pull_review_application_epoch_catalog_contract_tests \
  test_better_pull_review_application_epoch_contract_tests \
  test_better_pull_review_application_epoch_host_tests \
  test_better_pull_review_sqlite_storage_contract_tests \
  test_better_pull_review_sqlite_behavior_contract_tests \
  test_better_pull_review_git_process_contract_tests \
  test_better_pull_review_git_cache_contract_tests
ctest --preset dev --output-on-failure \
  -R '^(core_epoch_policy|application_epoch_(catalog_contract|contract|host)|sqlite_epoch_catalog_(storage|behavior)_contract|git_(process|cache)_contract)$'
```

`application_epoch_contract` validates the exact
`accepted_pull_request_handoff`, matching/revoked smart authority, absence of a
caller-supplied source identity or time, bounded invalidation, nonzero entropy,
and the production runtime ports. The latter call `getentropy` on macOS,
`getrandom` on Linux, and system-preferred `BCryptGenRandom` on Windows, while
the clock rejects values outside the core UTC domain. A current-host native
run proves only that host; guarded Windows code remains BPR-0053 evidence.

`application_epoch_host` uses deterministic fake capability ports and separate
manual effect/decision executors. It proves recovery before source mint/core
acceptance, fresh collision retry, no-op and association-only Git avoidance,
exact journal-before-import and durable phase order, same-repository FIFO with
exact PR cache-session switching, stale/cancel rejection, pre-PONR abort,
post-PONR mandatory success, applied-then-error postcondition resolution,
restart continuation across every durable append phase, mismatch-to-repair,
decision-lane-only projection, projection-exception containment, deferred
reentrant ingress without recursive publication, and shutdown quiescence.
BPR-0068 adds injected `epoch_host_limits` and deterministic floods that hold
the manual effect/decision executors: one actor drain retains at most the job
capacity across queued closures and pending terminals, a request-specific
quota terminal is observed while blob materialization owns the same ledger,
effect-executor refusal rolls back before `unavailable`, and ordinary
reentrant ingress never exceeds its separate capacity. Matching invalidation
and shutdown use fixed terminal-priority state, rejected smart leases are
revoked, admitted ordinary work remains FIFO, projection depth remains one,
terminal retirement reserves capacity for core-emitted cleanup, and a saved
blob request cannot materialize the retained cache after matching authority
invalidation. Concurrent executor refusal preserves later admitted actor
work; shutdown cancels an entered synchronous blob read and counts it through
caller wakeup before reaching exact quiescence. Reentrant invalidation revokes
blob authority before returning, reentrant shutdown closes blob ingress before
returning, and post-read generation checking prevents stale-byte publication.
Shutdown cancellation also proves a synchronous stop callback can reenter
shutdown and the closed blob seam at callback depth one without deadlocking
the job ledger.
Replacement append cleanup is pre-admitted and must be successfully queued
before authority mutation or offer submission; quota and executor refusal
preserve the active authority. The real `git_process_contract`,
`git_cache_contract`, and SQLite contracts separately
exercise the installed-Git, physical recovery-inspection/cleanup, and durable
catalog operations used by that fake-port orchestration.
The BPR-0068 closeout replay passed the focused host under ASan/UBSan and a
12-target sequential, Qt-free component matrix. It launched no application
instance; the existing public persistence launch evidence was not repeated.

BPR-0056 extends the same focused core and host contracts with the public
history-page intent/request/terminal path. The table covers empty, one-row,
multi-page, exact-bound, invalid-cursor, malformed-page, catalog-revision,
identity-switch, failure, cancellation, duplicate, and reordered-completion
cases. It requires each host request to call the existing revision-bound
catalog page exactly once, publishes stable provider/Git/SQLite-free rows with
`Replace == false`, and proves core state retains no complete history.

The public BetterPullReview composition and two-process GitHub.com/GHES
persistence paths are complete in BPR-0047; completed BPR-0048 supplies the
broader combined failure/reordering matrix. BPR-0055 supplies the maintenance
product path; BPR-0056 supplies the bounded history-page core/host/projection
path, while BPR-0046 supplies its Qt trigger/model. No live private repository,
GitHub mutation, Windows-native execution, or GitHub Actions workflow is part
of this verification.

For the BPR-0048 focused epoch matrix, first build `dev`, then run the core,
host, catalog, Git, smart transport, Qt, public persistence, and boundary
targets selected in `docs/issues/done/BPR-0048-rfc-0003-focused-tests.md`.
The fixed public force-push graph has its own quick semantic contract:

```sh
ctest --preset dev --output-on-failure \
  -R '^epoch_history_fixture_contract$'
```

That contract reads
`tests/fixtures/git/epoch-history/v1/manifest.json`, validates the exact
test-only pack bytes embedded by the public fixture, and uses installed Git to
prove the sibling-head topology and omission of the old head from the
replacement pack. The public two-process cases then prove the real cache,
catalog, core, and Qt path retains and presents that omitted head with the
truthful force-push cause.

The native smart timeout case uses a 1.5-second test-only absolute deadline
against a two-second silent origin. Cancellation/revocation cases wait 1.5
seconds before interruption while their origin remains silent for three
seconds, allowing target-native proxy discovery and instrumented startup to
complete before the asserted interruption. Production's 15-minute total and
30-second no-progress deadlines are unchanged. The Git process/cache CTest
ceilings are 180/600 seconds to admit the complete hostile and cross-process
contracts on slower local hosts; those registration ceilings do not relax any
production operation, pack, output, cancellation, or idle bound.

For the BPR-0055 core-authorized maintenance slice, build and run the compact
core policy, manual-executor host matrix, real installed-Git cache contract,
and composed real Git/SQLite integration fixture with:

```sh
cmake --build --preset dev --target \
  test_better_pull_review_epoch_core_tests \
  test_better_pull_review_application_epoch_host_tests \
  test_better_pull_review_git_cache_contract_tests \
  test_better_pull_review_epoch_maintenance_integration_tests
ctest --test-dir build/dev --output-on-failure \
  -R '^(core_epoch_policy|application_epoch_host|git_cache_contract|epoch_maintenance_integration)$'
```

The core table proves exact authority/catalog/cache targeting, strictly
ordered unique batches of at most 100 epochs, digest-bound terminal identity,
single-flight policy, stale and duplicate rejection, pre/post-point-of-no-
return cancellation, incremental availability deltas, and compact trivially
copyable state. The host matrix covers every durable maintenance restart
phase plus injected lookup, transition, paired-ref, quarantine, catalog
finalization, and terminal-bookkeeping failures. It requires pre-point intent
to abort, post-point work to resume with one durable operation token, physical
ambiguity to become `repair_required`, and cache deletion to close the live
session before quarantine.

`epoch_maintenance_integration` is the closest deterministic composed runtime
surface for this non-UI lifecycle. It accepts a real handoff through the epoch
host, streams a fixed valid SHA-1 smart-protocol pack into the real installed-
Git cache, persists the initial epoch in the real SQLite catalog, dispatches
the actual core prune/delete intents, shuts down, and reopens durable state.
The prune case requires both catalog anchors to remain `pruned` while the exact
retained pair is absent. The cache-delete case requires both anchors to remain
`cache_deleted`, the exact repository directory to be absent, and the terminal
deletion tombstone to reject reuse of that cache scope. The fixture uses only
disposable local directories and no GitHub endpoint, credential, mutation, or
GitHub Actions workflow.

BPR-0067 adds the public Qt authorization path to that existing maintenance
protocol. Build the selected executable and run its core, catalog, Qt,
integration, and four-process public contract with:

```sh
cmake --build --preset dev -j 6
ctest --test-dir build/dev --output-on-failure \
  -R '^(core_epoch_policy|sqlite_epoch_catalog_behavior_contract|qt_epoch_comparison_scale_100|qt_epoch_comparison_scale_125|qt_epoch_comparison_scale_200|epoch_maintenance_integration|application_epoch_maintenance_public_path)$'
```

The public contract drives QAction, default-Cancel/destructive confirmation,
core acceptance, the application host, real installed Git and SQLite,
incremental Qt projection, shutdown, and restart. It requires exact target
identity, no extra GitHub or smart-transport traffic, no routine model reset,
exact retained-ref/cache effects, and durable `pruned` then `cache_deleted`
results across four processes.

For the BPR-0046 reusable Qt comparison control, build the actual adapter and
run its stable-model, interaction, accessibility, and rendered loaded-window
cases at all three deterministic scales:

```sh
cmake --build --preset dev --target \
  test_better_pull_review_qt_epoch_comparison_tests
ctest --test-dir build/dev --output-on-failure \
  -R '^qt_epoch_comparison_scale_(100|125|200)$'
```

The harness requires exact insert/remove/move/data-change signals, persistent
anchor indexes, zero routine resets, two catalog-bound selector intents,
named aliases, reverse/identical/review-anchor pairs, complete SHA-1/SHA-256
OID text, explicit retained/error/cancel/availability states, focus and
accessibility, deduplicated bounded announcements, and live font-event
handling. It then embeds the same control in the real loaded GitHub window and
saves scale-specific PNGs beneath the ignored build tree. Offscreen output is
deterministic interaction and geometry evidence, not target-native system
Light/Dark proof; BPR-0049 retains that gate. BPR-0047 selects the same
control through the production epoch-host composition and proves durable
persistence.

### Canonical diff records and deterministic text computation

BPR-0072 introduces the Qt/provider/effect-free full-authority comparison key,
retained normalized diff options and their digest, half-open semantic spans,
stable record identities, and public structural alignment validator.
BPR-0073 adds the pure byte-faithful decoder and bounded deterministic
line-diff, secondary-pairing, intraline, and exact-move computation. BPR-0074
adds the canonical mapping, group projection, navigation, semantic selection,
and lower-detail boundary. Build and run all focused targets without Qt:

```sh
cmake --preset core-only
cmake --build --preset core-only --target \
  test_better_pull_review_diff_core_tests \
  test_better_pull_review_diff_compute_core_tests \
  test_better_pull_review_diff_semantics_core_tests
ctest --test-dir build/core-only --output-on-failure \
  -R '^(core_diff_model|core_diff_compute|core_diff_semantics|core_dependency_boundary|core_boundary_contract|architecture_boundaries|architecture_boundary_contract)$'
```

`core_diff_model` contains exact empty/equal/insertion/deletion/replacement/
move tables, absent-side file identity, maximum line-boundary cases, malformed
invariant cases, stable identity replay, and a fixed-seed 512-case generated
valid/malformed corpus. This target validates the canonical publication
contract only. `core_diff_compute` directly covers empty and identical text,
exact start/middle/end one-sided spans, 4:7 and 7:4 replacements, internal
exact and similar anchors, deterministic repeated-line ties and weak-pair
rejection, whitespace and line-ending modes without display-byte mutation,
final-newline identity, strict malformed and multibyte UTF-8, valid intraline
boundaries, absent added/deleted sides, binary/unavailable diagnostics,
unambiguous/ambiguous moves, exact resource limits, stable file identity, and
generated replay. Its validator additionally rejects retained-text mutations
that would make equal segments, exact pairs, or exact move relations false.
`core_diff_semantics` covers every mapping kind, exact opposing boundaries,
unpaired replacement spans, separate move correspondence, canonical change
projection and traversal, stop/wrap endpoints, line/group selection and
reconciliation, lower-detail identity agreement, nontext/invalid/fabricated
inputs, 4:7 accepted anchors and tail pinning, and a generated cross-product
of small files and option policies.

### RFC-BPR-0005 workbench

```sh
cmake --build --preset core-only --target test_better_pull_review_epoch_core_tests
ctest --test-dir build/core-only --output-on-failure -R '^core_workbench_interaction$'

cmake --build --preset dev --target \
  test_better_pull_review_application_workbench_host_tests \
  test_better_pull_review_qt_workbench_window_tests
ctest --test-dir build/dev --output-on-failure -R '^application_workbench_host$'
QT_QPA_PLATFORM=offscreen CTEST_PARALLEL_LEVEL=1 ctest --test-dir build/dev \
  --output-on-failure -R '^qt_workbench_window_scale_(100|125|200)$'
```

The Qt cases run offscreen and serial. The window suite includes the
large-file virtualization case, which presents sixty, six thousand, and sixty
thousand source lines through the same composed workbench and requires an
identical widget count at each. It prints a factual scale line naming the
platform, architecture, Qt version, counted subtrees, line counts, widget
counts, visible ribbons, and publish milliseconds. Those figures are context
for the machine that produced them; only the counts are asserted, never the
timing.

### RFC-BPR-0006 appearance and syntax

```sh
cmake --build --preset core-only --target \
  test_better_pull_review_appearance_core_tests \
  test_better_pull_review_theme_exchange_core_tests \
  test_better_pull_review_appearance_contrast_core_tests
ctest --test-dir build/core-only --output-on-failure \
  -R '^core_(appearance_policy|theme_exchange|appearance_contrast)$'

cmake --build --preset dev --target \
  test_better_pull_review_application_appearance_host_tests \
  test_better_pull_review_qt_appearance_profile_tests
ctest --test-dir build/dev --output-on-failure -R '^application_appearance_host$'
QT_QPA_PLATFORM=offscreen CTEST_PARALLEL_LEVEL=1 ctest --test-dir build/dev \
  --output-on-failure -R '^qt_appearance_profile$'

ctest --test-dir build/dev --output-on-failure -R '^syntax_classification_contract$'
```

The syntax contract exercises C++, CMake, Markdown, JSON, Python, and plain
text from `tests/fixtures/syntax/`, and pins one classification to one lexical
pass. Its sharpest case asserts that a hunk beginning inside a multiline
comment classifies exactly as the same region of the complete blob — the case a
per-hunk highlighter silently fails.

## Troubleshooting and safe verification

### GitHub and native credentials

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

### Retained Git history and epoch storage

- BetterPullReview admits Git 2.39 or newer only after the fixed production
  SHA-1 and SHA-256 pack/object/ref capability probes pass under the isolated
  child policy. A version string is insufficient. If admission fails, repair
  or replace the installed Git distribution; do not add inherited Git
  configuration, credential helpers, environment variables, or alternate
  executables to bypass the probe.
- The private bare repositories and epoch catalog use opaque app-owned
  identities beneath BetterPullReview's application-data root. They are not
  worktrees or operator databases. Do not run Git commands, Git maintenance,
  SQLite tools, filesystem cleanup, or manual ref edits against them: raw
  topology is not authority, and a mismatched identity or operation phase
  correctly fails closed.
- Restart recovery examines the exact durable operation and physical
  postconditions. It aborts only work still before its point of no return and
  completes or reports later work precisely. `repair_required` means the
  application could not prove a safe exact result; do not guess, delete a
  subset of files, or relabel the affected history as current.
- Cache, catalog, and retained-epoch limits are hard refusal boundaries. They
  never trigger automatic pruning, Git GC, or eviction of committed history.
  Epoch pruning releases logical retained-epoch quota but does not promise
  immediate byte reclamation; exact whole-cache deletion reclaims the
  repository bytes but leaves explicit cache-deleted history. Neither action
  claims secure media erasure.
- The public Qt composition exposes exact prune and whole-cache deletion under
  `Local history maintenance`. Each operation captures its exact target before
  a destructive confirmation whose default and Escape action are Cancel, then
  dispatches the existing core-authorized protocol. Do not emulate either
  command by deleting storage manually. Prune targets only the selected epoch
  IDs; repository-cache deletion affects every retained pull request in the
  current cache scope and leaves durable `cache_deleted` tombstones.
- An observed old head remains rooted by private refs after a force-push and
  may be selected after restart while its epoch remains retained rather than
  explicitly pruned or cache-deleted. BetterPullReview cannot recover commits
  from before its first successful observation, or any revision it never
  observed; present that case as `never-observed`, not as an empty or current
  comparison.

## Build ownership

GitHub Actions does not configure, build, test, package, release, or verify
BetterPullReview.
Developer builds run through the checked-in CMake presets on the current
machine. Clean Windows, macOS, and Linux package verification belongs to
RFC-BPR-0014 and runs on release-operator or corporate-managed environments.

Platform support is never inferred from Qt availability or a configuration
file. RFC-BPR-0014 requires direct install, launch, runtime, and uninstall
evidence from each supported native environment.
