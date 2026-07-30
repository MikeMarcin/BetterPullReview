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

`verify_runtime_surface.py` runs on macOS, Linux, and Windows. The
binary-dependency script also supports all three platforms, using `dumpbin` on
Windows when available, followed by `llvm-readobj` and `objdump` fallbacks.

## Build ownership

GitHub Actions does not build, test, package, or release BetterPullReview.
Developer builds run through the checked-in CMake presets on the current
machine. Clean Windows, macOS, and Linux package verification belongs to
RFC-BPR-0014 and runs on release-operator or corporate-managed environments.

Platform support is never inferred from Qt availability or a configuration
file. RFC-BPR-0014 requires direct install, launch, runtime, and uninstall
evidence from each supported native environment.
