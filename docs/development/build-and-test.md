# Build and Test

For RFC-BPR-0023, run the focused `qt_private_review_panel`, private store/host
rows, and `application_private_review_native`, `_file`, and `_retry` public
paths through CTest after building the dev preset. The public-path courses use
local safe fixtures and canary inputs; do not put credentials or private PR
contents in fixture logs. Run `python3 scripts/verify_documentation.py --root .`
and `python3 tests/scripts/verify_documentation_tests.py` with Python 3.10+
before closeout. [The verification ledger](../verification/RFC-BPR-0023.md)
contains the exact selectors, measured bounds, render digest manifest and
remaining designated live-server readback requirement.

## Prerequisites

- CMake 4.4.2
- Ninja
- A C++23 compiler
- Qt 6.8 or newer, including CoreTools (`rcc`), for the Qt adapter and application
- Python 3.10 or newer for structural and runtime verification scripts
- Git 2.39 or newer for installed-Git contract tests
- Clang-format and clang-tidy 22.1.8 for coding-standard verification
- On Linux, stable `libsecret-1>=0.20.0` and its development metadata
- On Linux, libxcb's development metadata (`xcb` in `pkg-config`), for the
  application's X11 activation host ([[BPR-1024]])
- On Linux, `dbus-daemon`, `dbus-send` and `gnome-keyring-daemon` in
  `/usr/bin` or `/bin` (on Ubuntu, the `dbus-daemon`, `dbus-bin` and
  `gnome-keyring` packages), for the native Secret Service rows
  ([[BPR-1117]]). Each of those rows starts its own private bus and test-only
  keyring from these tools and never uses the desktop session. Without them,
  `linux_secret_service_native_setup`, `linux_secret_service_native` and
  `linux_secret_service_native_cleanup` fail with
  `linux-secret-service-private-session:unavailable:<tool>-missing`, and
  `linux_secret_service_refusal_control`, which needs only `dbus-daemon`, fails
  the same way. The fake-backed `linux_secret_service_contract` needs none of
  them. See "Dependency boundary" below

The validated local foundation environment uses CMake 4.4.2, AppleClang 16,
Ninja, Python 3.14, and Qt 6.11.1 on macOS arm64.

## Run budgets and row timeouts

A run budget bounds how long an automated application course waits for its
condition. A CTest timeout also covers process startup, every child, teardown
and evidence output. They measure different intervals. Record platform,
architecture, Qt version, revision, course, host load and both figures when
adding or changing a timing bound; a green row alone does not establish margin.

Choose the existing budget class for the work a course actually performs:
self-test, single fixture render, or epoch seed/reuse. Record the measurement
and multiplier beside any new bound. Use three times an isolated measurement
as an initial sizing guide when there is no matching class, then check known
contended observations on every platform the row runs on. A measured contended
completion is stronger evidence than a quiet multiple; a timeout is only a
lower bound, and never a measured completion. Round upward and record the
remaining platform or load gaps. This guide does not automatically raise an
existing bound, and resource/performance budgets need their own measured
meaning rather than the same time multiplier.

Register automated application rows through `bpr_bound_automated_run` in
`tests/end_to_end/CMakeLists.txt`. Its invariant is
`RUNS * BUDGET_MS + RESERVE_MS <= 0.75 * TIMEOUT * 1000`. Count every possible
child, including failure/refusal phases. The reserve covers work outside the
run budgets; the remaining quarter belongs to the harness. A child-process
backstop must also exceed that child's run budget.

Every automated terminal prints `PUBLIC_PATH_BUDGET:expired=… budget_ms=…
elapsed_ms=… stage_ticks=…`, including successful runs. Elapsed time includes
shutdown; `expired=1` specifically means the course's own clock expired before
shutdown began. `RUN_BUDGET_EXPIRED` precedes the failed application marker and
names the stage and observed status. CMake drivers should use
`tests/end_to_end/bpr_automated_run_result.cmake` after `execute_process` to
forward successful timing and put expiry in the failure headline. A child
killed by its process backstop says `PROCESS_TIMEOUT`; a CTest row killed by
CTest reports `***Timeout`. Preserve those clock failures instead of reporting
only a failed domain assertion. No marker can be promised after a hard kill.

A sufficiently overloaded host can exceed any finite limit. Keep that run as
failed clock evidence, record its load, and diagnose or repeat under declared
conditions; do not silently retry until green or infer a product regression
from a clock alone. The [BPR-0905 survey](run-budget-survey-2026-09-25.md) records
current gaps and additional rows to inspect. [[BPR-0901]] shows a render budget
misapplied to epoch work. [[BPR-0893]] shows why a footprint budget and a live
heap cleanup budget answer different questions.

## Dependency boundary

The root resolves dependencies before product subdirectories. Qt 6.11.2 is
acquired by a pinned provider into the shared dependency cache and then
resolved through its ordinary CMake config package, so discovery does not
depend on how Qt arrived. `BPR_QT_ROOT` names an existing prefix to use
instead, and `BPR_QT_PROVIDER=OFF` restores the former requirement that the
caller install Qt themselves. Python and Git use the
standard CMake find modules, while Linux libsecret and libxcb use
`pkg-config`; each is discovered only when its selected target graph needs it.
The centralized provider requires the Qt libraries plus `Qt6::rcc`,
`Python3::Interpreter`, `Git::Git`, `LibSecret::LibSecret` and, on Linux,
`PkgConfig::BPR_XCB` before product targets are declared, so
adapters and tests never repeat package discovery. Those target names must not
be predeclared by a parent project: the provider rejects collisions before
discovery so its target-provenance audit cannot describe an injected target as
an installed package. Python, Git, Threads, and `PkgConfig` are selected
explicitly from CMake's built-in module providers regardless of
`CMAKE_FIND_PACKAGE_PREFER_CONFIG`; package redirects for provider-owned names
are rejected before any redirect script can execute.
BetterPullReview configures as the audited top-level project. RFC-BPR-0014 adds
application installation/package rules, not a separate core SDK or general
embedding API. `CMAKE_PROJECT_TOP_LEVEL_INCLUDES` is
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
packages. Qt 6.11.2 uses aqtinstall 3.3.0 on macOS/Linux. On Windows, that
released installer cannot locate Qt 6.11's changed repository layout, so the
provider pins upstream merge `8c3695d4a4e1ceabf6a74dc6c79681656dc6b74b`
([aqtinstall PR 1000](https://github.com/miurahr/aqtinstall/pull/1000)). This is
an explicitly unreleased, build-only installer revision, not a modified Qt
runtime. Its acquisition requires Git on `PATH`; the installed Qt version is
still checked exactly. A preinstalled official 6.11.2 prefix can instead be
supplied through `BPR_QT_ROOT`, including offline builds.

KSyntaxHighlighting 6.28.1 is acquired the same way, with
extra-cmake-modules 6.28.0 as a pinned transitive build input. Caller-installed
discovery was investigated first, because the library is Qt-dependent and Qt
was at that time never fetched, but no Homebrew formula exists for it or for
the KDE Frameworks, which would have made RFC-BPR-0006 unimplementable rather
than deferred. Qt has since moved to its own pinned provider, which does not
disturb that outcome: the Homebrew gap was the deciding constraint. Each
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

Distribution uses `macos-release`, `windows-app-release` or `linux-package`,
each with Release, tests disabled and distribution runtime restrictions. The
[package operator guide](release-packages.md) names the required native
baselines/toolchains and existing `publish-release` workflow. Ordinary
`app-release` builds are not distribution qualification. Exact package audits
bind source, compile/link graph, installed bytes, SPDX/notices and the separate
source asset; they do not supply publisher trust or native acceptance.

Three switches shape the graph for Review Cycles and its optional families,
all independent of the preset:

| Switch | Default | What it changes |
|---|---|---|
| `BPR_BUILD_TOUR_CLI` | `OFF` in `dev`, `ON` in `tour-cli` | Builds `bpr` (`app_bpr`) and registers the rows that run it: `cycle_cli_public_path`, `cycle_cli_documentation`, `skill_bpr_cycle_package`, and, in a graph with Qt, the two-round, parity and handoff courses. Requires `BPR_BUILD_LOCAL_AUTOMATION`. |
| `BPR_BUILD_AGENT_INTEGRATIONS` | `ON` (`OFF` in `tour-cli` and the `-agent-free` presets) | Compiles out the agent launch adapter, protocol decoders, agent hosts, MCP helper and Send to Agent. |
| `BPR_BUILD_LOCAL_AUTOMATION` | `ON` (`OFF` in the `-agent-free-automation-off` presets) | Without it no owner command endpoint is bound and `bpr` cannot be built; the automation-off two-round course is registered instead. |

A `dev` graph with `bpr` is `cmake --preset dev -B build/dev-tour-cli
-DBPR_BUILD_TOUR_CLI=ON`. The [terminal guide](cycle-cli.md) and
[release packages](release-packages.md#packages-without-agent-integrations-or-local-automation)
give the commands and what each package omits.

## Dif asset maintenance

The checked-in files below `src/adapters/qt/assets/dif/source/` and the manifest
are the editable authority. Generate proposed products into a new empty
directory; never write over committed products to inspect a candidate:

```sh
python3 scripts/dif_assets.py generate \
  --assets src/adapters/qt/assets/dif \
  --out <empty-output-directory>
```

Validate the admitted tree and reproduce the committed generated tree twice:

```sh
python3 scripts/dif_assets.py validate \
  --assets src/adapters/qt/assets/dif
python3 scripts/dif_assets.py verify \
  --assets src/adapters/qt/assets/dif
ctest --preset dev -R '^dif_assets_contract$' --output-on-failure
ctest --preset core-only -R '^dif_assets_contract$' --output-on-failure
```

The contract is network-isolated and uses only the configured C++ compiler and
Python standard library. Ordinary configure and build steps do not run the
generator or require ImageGen, ImageMagick, Pillow, a model, or network access.
Both the tests-enabled application and the tests-disabled `app-release`
application select the same ten accepted generated strips. On Windows the
tests-disabled image carries them, uncompressed, in its resource bundle
`better_pull_review.rcc` beside the executable rather than in itself
([[BPR-0974]]); the verifier then checks the bundle's collection and bytes, and
that the image no longer embeds a strip. `core-only` selects
neither those resources nor Qt, the platform motion observer, or the generator.

Before closing an asset or consuming application revision, build the affected
`dev`, `core-only`, and `app-release` targets and invoke:

```sh
python3 scripts/verify_dif_asset_boundaries.py \
  --source . \
  --dev-build build/dev \
  --core-build build/core-only \
  --app-build build/app-release
```

Any source, manifest, generator, generated-product, preview-renderer, or review
identity change invalidates the affected attended digest. Rebuild the offline
review page and obtain a new verdict. Unchanged accepted pixels may reuse their
bound verdict. RFC-BPR-0030 owns the ten-clip source and generated products;
RFC-BPR-0031 owns their fixed Qt resource selection and runtime composition.
Run focused acceptance and public-path checks for the changed boundary rather
than a full build/test matrix by default.

### The macOS menu bar status item

The status item's template images, `src/adapters/qt/assets/status-item/`
`dif-template.png` and `dif-template@2x.png`, are made from the accepted neutral
master by a standard-library script that no build runs ([[BPR-1250]]). It
refuses any other master. `check` regenerates both in memory, requires the
committed bytes, and requires each to be black with alpha:

```sh
python3 scripts/dif_status_item_template.py check
python3 scripts/dif_status_item_template.py generate   # only for a deliberate change
ctest --preset dev --output-on-failure -R \
  '^(dif_status_item_template_contract|qt_menu_bar_status_item(_cocoa)?|qt_system_tray_notification_sink|application_profile_empty_desktop_native_menu_bar)$'
```

`qt_menu_bar_status_item_cocoa` and the empty desktop course need a macOS
console session; the course observes the item in AppKit as an
`NSStatusBarWindow` before and after the last window closes. A changed image is
a new visual candidate for the owner.

## Attended native notification course

The deterministic `application_native_notification_attended_contract` replaces
only the operating-system display boundary. It preserves the real queue rule,
Refresh, system-tray sink, tray click signal, notifier activation, and exact
landing-row selection, and runs independent request and activation cuts. It
does not claim that the operating system displayed a banner. On Cocoa, the
testing-only course establishes the workbench as Qt's logical active window
before it emits the real tray signal; the selection path must still focus the
exact queue row. A separate testing-only signal observer must record exactly
one `messageClicked`, and its negative cut must fail closed before the course
can pass. This is deterministic Qt focus evidence, not proof that the unattended
process became the native foreground application.

BPR-0168's operator packet runs the shipped `better_pull_review` executable
with its native `QSystemTrayIcon::showMessage` boundary. The application first
reports a
quiet accepted head, drives the shipped observer-rule and Refresh gestures,
then stays visible for the owner to grant permission, inspect the exact
metadata-only banner, and click it. No automation may grant Notification Center
permission or substitute for the click.

## Live publication course (RC59-10)

RFC-BPR-0059's native publication is qualified live on one repository only,
github.com/MikeMarcin/BPR-Test, which the owner designated as a disposable
target on 2026-09-30 ([[BPR-1176]]). The course is not a test row and no
default graph builds it:

```sh
cmake -S . -B build/dev -DBPR_BUILD_LIVE_PUBLICATION_COURSE=ON
cmake --build build/dev --target tool_review_cycle_live_publication
python3 tests/scripts/run_live_publication_course.py \
    --tool build/dev/tests/workspace_contract/review_cycle_live_publication --git /usr/bin/git
cmake -S . -B build/dev -DBPR_BUILD_LIVE_PUBLICATION_COURSE=OFF
```

It needs a `gh` sign-in that can push to BPR-Test. The driver clones the
repository, pushes a base branch and a "foreign" branch under
`bpr-rc59-10/<UTC stamp>/`, and hands the credential to the tool on standard
input. The tool composes the shipped store, installed-Git catalog, publisher,
`github_publication_session` and production native HTTPS transport; the
owner and repository are constants in it. The driver then checks the result
through `git fetch` and `gh api`. Since [[BPR-1178]] it also cancels a branch
and a pull request write in flight, at three quarters of the same write's last
round trip, and reports whether GitHub took it. Branches and pull requests are left on
BPR-Test as evidence; nothing is deleted, closed or force-updated. GHES has no
designated target and no course.

## Automated-test network boundary

Every registered test uses `bpr_add_network_isolated_test`; raw `add_test` is
owned only by `cmake/BprTestIsolation.cmake`. The wrapper prefixes the complete
CTest command, so C++ executables, Python and CMake scripts, helper descendants,
and launches of `app_better_pull_review` inherit the same boundary. The
configured-manifest gate rejects a missing prefix or isolation label, while a
source gate rejects any second raw registration site. The macOS launcher runs
Python with isolated/no-site flags, and every registration unsets Python and
dynamic-loader injection variables before that launcher starts. The manifest
gate rejects a test that sets, overrides, or omits those scrubs.

On macOS, a trusted launcher reserves an exact set of free IPv4/IPv6 loopback
ports, creates a unique mode-0700 Unix-socket directory, derives a one-test
`/usr/bin/sandbox-exec` profile, and passes the endpoint facts only to that test.
Fixtures bind the selected ports before their clients run; a collision is a
test failure, not authority to contact the colliding listener. Tests without a
declared fixture receive no TCP port. Target-native counterexamples require
`EPERM` from direct HTTPS-port access, a Python descendant, the shipped native
HTTPS adapter, and unrelated loopback and Unix listeners kept open by the
unsandboxed runner. The unrelated listeners must receive zero connections. A
positive probe separately proves exact IPv4, IPv6, and private Unix fixtures.

On Linux, the checked-in launcher creates user and network namespaces, maps
only the invoking identity, brings up only `lo`, and fails the test before
`exec` when any step is unavailable. This implementation still requires
target-native Linux replay.

The Linux launcher also gives each test its own mode-0700 runtime directory
under `/tmp`, names it in `XDG_RUNTIME_DIR`, and removes it when the test
ends ([[BPR-0906]]). On Linux the product binds its profile activation
endpoint in `$XDG_RUNTIME_DIR` when that names an owner-private directory
short enough for the endpoint, so an owner that a test ends with a signal no
longer leaves its socket in the user's `/run/user/<uid>`. Two launches of one
profile meet only when they resolve the same directory. A test that starts a
second launch with a different `XDG_RUNTIME_DIR` gets `owner_busy` instead
of a handoff ([[BPR-1113]]).

The removal deletes a symbolic link the test left, never what the link
names. The directory is removed on every way the launcher ends once it has
made one. That includes a forwarded signal that arrives before there is a
child to pass it to, and a failure to install the signal handlers or to wait
for the child ([[BPR-1113]]). `test_network_isolation_runtime_directory`
holds both: it plants links to a file and a directory outside, and it drives
each of those failures through a preloaded fault shim. Three ways still leave
the directory in `/tmp`: a launcher killed outright, as by a CTest timeout,
because nothing runs in a process killed by SIGKILL; a signal the launcher
does not forward, such as SIGQUIT; and a crash of the launcher itself. The
directory then holds only what its test left, and stays until it is removed
by name or the host clears `/tmp`. A removal that fails prints
`BPR_TEST_RUNTIME_DIRECTORY_LEFT:` and does not change the verdict.

When `DBUS_SESSION_BUS_ADDRESS` is unset, the launcher first sets it to the
`$XDG_RUNTIME_DIR/bus` socket the test would otherwise have found, so the
test still reaches the same session bus. That can be the user's real bus.

The Linux Secret Service rows do not take that bus, or any bus the shell
names ([[BPR-1117]]). They refuse every session bus that is not designated
for tests, and each runs on a private bus with a test-only keyring that its
harness starts. The Secret Service section below says how. Before that card,
they reached the user's own Secret Service wherever an address was set or
pinned, and wrote and deleted their integration item there ([[BPR-1113]]).

On Windows, the checked-in launcher enters each test into one fixed
AppContainer with zero capabilities, so the Windows Filtering Platform refuses
every external connection and every descendant inherits the container token;
configure grants that container read access to the sources and the resolved
Qt prefix and write access to the build tree, and refuses when it cannot.
Some tests need something the container does not give them. Each case below
was measured on the Windows station:

- translating a volume device into a drive letter. `GetFinalPathNameByHandleW`
  in its DOS form, `Path.resolve(strict=True)` and Git's own `getcwd` all fail
  while the NT-form name resolves completely;
- running a `.bat` file through `cmd.exe`;
- the named pipes that `ninja` and `QProcess` use;
- the Windows Credential Manager;
- starting some child processes, which `CreateProcess` refuses with WinError 5
  ([[BPR-0872]]);
- reading content the checkout points at but does not contain (below).

The tests that need one of those are registered with
`WINDOWS_UNCONTAINED` and run uncontained on Windows only, labelled
`bpr_windows_uncontained`; `scripts/verify_test_network_isolation.py` pins
that set exactly in both directions, with each member's measured cause, so it
can neither grow quietly nor outlive its reason. On Windows the structural
symbol guard reads each automated executable's objects with `dumpbin`, since
an MSVC-linked image carries no symbol table, and the composed-host gate reads
the linker's map of the application (`link /MAP`, beside the executable) and
demangles it with `undname`; both fail closed when the tool is absent, which
outside the Visual Studio developer environment it is.

The documentation gate is the last case in that list. What it depends on is
not specific to Windows. `scripts/verify_documentation.py` imports
`board_model` and `project_config` from `.agents/skills/implement-rfc/scripts`,
and `project_config` asks Git for the repository root. The repository does not
track `.agents/`, which is the installed implement-rfc skill. Where skills are
installed from the MASK store, `.agents/` is a symlink out of the checkout into
the user's profile. A checkout without that installation cannot run the gate.
Neither can a sandbox that cannot read the store, which is why
`documentation_structure` and `documentation_issue_graph_contract` run
uncontained on Windows ([[BPR-0873]]).

The default graph registers no live-test exception. The earlier trusted runner,
gate, fork protocol, and harmless canary remain regression-only surfaces and
are never the live application. The sole opt-in exception starts the fixed
`/usr/bin/python3 -I -S` interpreter with a bounded preloader captured in the
generated CTest execution plan. The preloader captures the source launcher
once, matches those bytes to the producer-bound literal SHA-256, and executes
the captured bytes rather than reopening the launcher path. Stage zero then
mounts the exact digest-bound image read-only below a fresh owner-private mount
root and launches the signed-capsule supervisor and shipped application only
from that mount.

Do not prepare an approval from the invalidated v005 packet. After BPR-0256
regenerates a successor and capability-critical review accepts its exact bytes,
follow only that packet. It names the exact subject and stored image and tells
the operator to materialize an approval only after approving that exact build.
The file and its immediate parent must be owned by the invoking user,
non-symlink, and have no group or other permissions. The approval carries no
credential; any write-scoped credential is entered only through the
application's existing Qt sensitive prompt.

Before preparing a new approval packet, initialize the per-user replay
namespace once, or deliberately reset it, with the tests-disabled shipped app:

```sh
better_pull_review --initialize-or-reset-designated-live-replay-state
```

This mode accepts no path or other argument and reports only success or failure;
it does not print the namespace path or random identity. It is separate from an
attended launch. Reset creates a fresh identity, invalidates every approval
bound to the former identity, and therefore requires a newly generated and
attended approval packet. The attended application path only loads the fixed
namespace below its application-data root and fails closed if it is absent,
unsafe, substituted, full, or mismatched. It never initializes or repairs that
state implicitly.

Configure a separate opt-in test graph only after the approval file exists.
Supply the resolved image path, its literal SHA-256, an owner-private mount
parent, and the producer directory from a separate fresh `BUILD_TESTING=OFF`,
`BPR_BUILD_QT=ON`, `Release` capsule build. Configuration captures the bounded
producer `source-manifest.txt` once and uses that same byte sequence for both
its digest and its entries. It rejects a missing or symlinked approval/image,
plain supervisor or inventory selection, a digest or source mismatch, an
unsafe mount parent, an artifact outside the producer tree, a tests-enabled or
non-Release producer, a different source root, and an image older than product
source or build configuration. The registration is fixed to one test,
`live_review_mutation_designated_target`, with only `bpr_live_mutation`, a
30-minute timeout, no credential environment, and no pass/skip/disable or
exit-verdict overrides.

Stage zero retains the image descriptor, attaches the image read-only, verifies
the mounted inventory and runtime identity, and keeps the mount and capsule
descriptors open through child exit. It owns stage one as a separate process
group with a 1,500-second bound inside CTest's 1,800-second timeout. Timeout,
nonzero exit, `SIGTERM`, `SIGINT`, or `SIGHUP` triggers bounded process-group
`TERM`, then `KILL` if needed, complete reap, post-child image/runtime
revalidation, and ordinary non-force detach. Before startup the mounted
supervisor verifies the signed capsule, performs one
unauthenticated redirect-free public observation through the GitHub adapter,
creates the app suspended, compares its dynamic code identity with the
prevalidated static identity, and revalidates the retained executable identity
before resume. The shipped application then identifies and hashes its own
executable, validates the exact digest-bound approval through retained native
file/directory identity, loads the exact schema/path/identity-bound replay
namespace, exclusively records its random approval ID there behind an
`F_FULLFSYNC` barrier on both the record and the namespace directory, and
revalidates the retained marker name, inode, and single link before atomically
consuming the same approval inode. It then holds one private
15-minute authority. The first valid host binding seals that authority to the
freshly authenticated acting identity; the async host consumes it only after
session, generation, exact target and actor, comparison, file, right anchor,
resolved path, and current head all match. Invalid requests do not consume it;
cancellation, invalidation, expiry, and shutdown revoke it. Windows approval
bootstrap and target-native Linux/Windows evidence remain deferred and fail
closed.

The mounted filesystem is read-only, but the backing DMG remains owner-writable
during the child lifetime. Post-child identity and digest checks detect
persistent drift; they cannot prevent transient backing-byte influence while
the child runs. `hdiutil attach` also consumes an image pathname rather than a
retained descriptor. The generated CTest manifest and cache are therefore the
trusted same-UID execution plan, and the pathname attach window remains an
explicit platform boundary. DMG creation is not byte-reproducible; every
operator packet must bind the exact stored image it reviews.

While other images are being created, `hdiutil create` can exit 0 and leave
its write-once device attached, and a later attach of that image then fails.
After each create, the image build therefore ordinarily detaches, without
force, one whole disk attached from its own resolved image path per attempt.
It lists `hdiutil info -plist` again before each attempt, until none remains
or 60 seconds pass. If the image is still attached then, or the listing cannot
be read, the build fails and reports no digest (BPR-0836).

An attended course can select an existing isolated profile with
`BPR_DESIGNATED_LIVE_MUTATION_PROFILE=<canonical-positive-profile-id>`. Create
that profile through Profiles first. Its approval must bind its own exact
replay namespace; another profile's subject or a spent approval is never
reusable. The fixed stage-zero command carries the ID through the authenticated
supervisor to the ordinary `--bpr-profile` application entry. Probe mode cannot
select a profile. Omitting the setting retains Default. Profile selection does
not repair or reinterpret another profile's Git cache, initialize or reset its
replay namespace, or extend the one-comment grant.

Inspect the two manifest modes without running the live lane:

```sh
ctest --test-dir build/dev --show-only=json-v1

cmake -S . -B build/designated-live-structure -G Ninja \
  -DBUILD_TESTING=ON \
  -DBPR_BUILD_QT=ON \
  -DBPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST=ON \
  -DBPR_DESIGNATED_LIVE_MUTATION_APPROVAL=/absolute/resolved/path/to/approval \
  -DBPR_DESIGNATED_LIVE_MUTATION_IMAGE=/absolute/resolved/release/capsule.dmg \
  -DBPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256=<literal-image-sha256> \
  -DBPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT=/absolute/resolved/private/mount-parent \
  -DBPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR=/absolute/resolved/release/build
ctest --test-dir build/designated-live-structure --show-only=json-v1
```

```sh
ctest --preset dev --output-on-failure \
  -R '^(designated_live_mutation_(gate|runner_digest_mismatch)|test_network_isolation_(structure_contract|manifest|external|loopback|unrelated|child|application)|live_mutation_isolation|live_mutation_isolation_contract)$'
```

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
`build/dev/tests/end_to_end/diff-workbench-public-path.png` and a 19-step
rendered interaction sequence plus `interaction-trace.jsonl` below
`build/dev/tests/end_to_end/bpr-0083-diff-interaction/`. The trace records the
retained comparison position, active segment, projected coordinates,
requested and applied pane origins, clamp state, selected identities,
link-map dimensions, visible-set work, and selection-callback counts, geometry
generation, semantic generation, pane/detail viewport heights, and vertical
splitter sizes for every rendered frame. Before each attempt, the course
removes only its fixed trace, appearance stamp, and 19 frame filenames so a
failed run cannot leave later frames from an older success looking current.
It attempts all 21 fixed removals before reporting one bounded cleanup failure.
Its divider step chooses a changed target from the splitter's live legal
range, waits at most 500 ms for the queued geometry result, and requires one
viewport-attributed rebuild with the exact semantic selection retained. A
failure prints a fixed `DIFF_INTERACTION_FAILURE:<predicate>` marker without
paths or source content. Scale rasters are written below
`build/dev/tests/adapters_qt/`. Offscreen output is not target-native
System-theme or final presentation-polish evidence.

Windows developers and release operators use the conditional
multi-configuration preset that selects the Visual Studio 18 2026 x64
generator:

```powershell
cmake --preset windows-dev
cmake --build --preset windows-dev
ctest --preset windows-dev --output-on-failure
```

## RFC-BPR-0017 credential-free demonstration

RFC-BPR-0017 has to answer whether the native HTTPS transport can issue a
request with no authorization header while every RFC-BPR-0002 rule stays
intact. `rfc0017_credential_free_native_https` answers it and nothing else:

```sh
ctest --preset dev -R '^rfc0017_credential_free_native_https$' --output-on-failure
```

The course runs against the same disposable loopback CONNECT and TLS fixture
every other native-HTTPS scenario uses, and builds its transport from the same
configuration record, so trust, proxy, redirect, cancellation, timeout, and
response limits are the real mechanics rather than a copy. The child process is
handed an empty credential frame, so it never holds material to look up,
discover, or fall back to, and the fixture treats any authorization header on
the wire as a contract violation. The course performs two requests to prove the
state is a deliberate absence rather than a spent credential, then makes a
pull-request-scoped request that must be refused before reaching the wire —
which the fixture's exact two-request count is what proves.

Be precise about what that establishes. The absent authorization header is
proven by a fixture that fails when one appears, and the absence of
non-loopback traffic is enforced by the sandbox profile rather than asserted.
The claims of zero vault lookup, zero credential discovery, and no subprocess
are true by construction — the native HTTPS path contains no vault or process
call, and the process is handed nothing — but no command in this course would
fail if one were added. They are inherited properties, not demonstrated ones.
The same applies to redirect, cancellation, timeout, and limit fail-closed
behavior: those are the credentialed courses' evidence, exercised here through
shared code, not re-demonstrated credential-free.

`make_native_https_credential_free_transport_for_test` exists only under
`BPR_NATIVE_HTTPS_TESTING` and takes no credential parameter, so a missing,
wiped, or malformed credential cannot decay into it. Each of those still
produces its own typed refusal from both the bootstrap and account factories —
never a working transport — which `native_https_transport_contract` asserts
directly. The
credential-free binding correlates only to the exact session it was handed, so
a pull-request-scoped parent is refused rather than served under some other
account's correlation. The Git smart surface refuses the binding because
`credential_matches` requires an account credential; `can_acquire_smart`
restates that refusal where it can be read, so neither can open the surface
alone. This is a probe, not a product mode: it creates no anonymous profile and
changes no production factory.

Two guards keep it that way. The inventory half runs in the ordinary graph:

```sh
ctest --preset dev -R '^rfc0017_test_inventory$' --output-on-failure
```

The absence half needs a build the test graph does not contain, so run it at
closeout against a tests-disabled Release application:

```sh
python3 scripts/verify_rfc0017_test_inventory.py \
    --release-binary build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview
```

Both print `RFC0017_TEST_INVENTORY_OK`. Later RFC-BPR-0017 cards add their
selectors to the same script rather than starting a second checker.

The absence half forbids the `BPR_NATIVE_HTTPS_TESTING` transport factories
(`make_native_https_credential_free_transport_for_test` and its bootstrap
sibling) and the demonstration's marker strings. It deliberately does not
forbid the transport's credential-free binding itself: since BPR-0451 that
binding, `credential_free_request`, ships on purpose as RFC-BPR-0016's device
authorization transport, whose device-code and token-poll requests run before
any credential exists. The scan's own negative control,
`rfc0017_release_absence_negative_control`, runs the same scan against the
tests-enabled application in the ordinary graph and passes only when it fails
naming the test-seam factory, so a scan that could pass against anything is
caught in the same `ctest` run.

Two credential-free capabilities *are* compiled into a shipped build and are
neither regressions the absence scan should flag nor evidence for this RFC's
gate; the script records both in `KNOWN_PRODUCT_CREDENTIAL_FREE`.
`make_designated_review_public_observer` performs a credential-free GET,
predates RFC-BPR-0017, belongs to the attended designated-live lane,
duplicates the transport's TLS and redirect policy rather than exercising it,
and skips system-proxy discovery. `make_native_https_credential_free_transport`
is the RFC-BPR-0016 device authorization factory.

## The RFC-BPR-0016 device-flow guards

The device grant puts a client identifier, two service endpoints, a short-lived
device code, an access token, and a refresh token into the tree. Every one of
those can be shipped by accident, and every claim the RFC makes about them is
proved by a named CTest — so the guard has two halves, exactly as
RFC-BPR-0017's does.

The inventory half runs in every configured graph, including `core-only`:

```sh
ctest --preset dev -R '^rfc0016_test_inventory$' --output-on-failure
```

It requires each evidence selector exactly once, requires the commands behind
them to still carry the arguments that make them the right suite, and
cross-checks `docs/verification/RFC-BPR-0016.md` by compiling that document's
own `-R` patterns rather than grepping for literal names. It anchors on whether
the graph built Qt and the loopback fixture, so a preset that builds neither
must register none of those selectors rather than passing vacuously.

The shipped-artifact half needs a build the test graph does not contain, so run
it at closeout against a tests-disabled Release application:

```sh
python3 scripts/verify_rfc0016_test_inventory.py \
    --release-binary build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview
```

Both print `RFC0016_TEST_INVENTORY_OK`. The artifact scan looks for testing
seams, the recorded fixture roots, the captured user code, and
token-and-client-identifier-shaped material; it never prints a matched value,
because reporting a secret to prove a secret leaked puts it in the log. It also
carries a positive control: an artifact containing no device-flow route fails,
since an absence scan run against the wrong file passes trivially.

The service endpoints are deliberately not forbidden. The host owns them —
RFC-BPR-0016 says so — and a shipped binary that can run the flow must contain
them, so forbidding them would be a check that fails on correct code.

### The target-native platform course

`scripts/verify_rfc0016_platform_course.py` runs one platform's whole course
against isolated fresh Debug and Release trees:

```sh
python3 scripts/verify_rfc0016_platform_course.py \
    --platform macos \
    --dev-preset dev \
    --release-preset app-release \
    --build-root build/rfc0016-macos-unavailable \
    --expected-device-selection unavailable \
    --fresh
```

It refuses to run for a platform it is not on, runs the inventory *before* any
test so a missing registration fails loudly rather than being skipped by a
looser pattern, runs the focused matrix, then checks the Release artifact: this
platform's credential store present and the other two absent, no embedded
browser engine, no client secret, no private BPR-0276 verification identity,
and the runtime and dependency scans clean.

`--expected-device-selection` is stated rather than discovered. A course that
asked the binary what it selects and then asserted that it selected that would
pass on every build; the caller states what the platform is supposed to answer
and the run fails if the artifact disagrees, which is what lets the same script
run before and after a one-time enablement flip. It prints
`RFC0016_PLATFORM_COURSE_OK` with the selector count, the observed stage, the
request count, and the artifact path.

### The distributable identity

A production platform may only become available once a *distributable* GitHub
App identity exists — BPR-0276's private owner-only verification app is not one,
and must never be shipped as one.
`scripts/verify_rfc0016_production_identity.py` is what admits the record of
that second identity:

```sh
python3 scripts/verify_rfc0016_production_identity.py \
    --registration-observation docs/verification/reviews/rfc0016-production-app-registration-v001/registration-observation.json \
    --course-evidence-root docs/verification/reviews/rfc0016-production-app-registration-v001/read-only-course \
    --require-complete
```

It performs no network traffic at all; the registration and the designated
read-only course happen in attended sittings, and this reads what they left
behind. The checks are joins rather than fields, because each document is
plausible on its own: the course's registration record must name the client the
sanitized observation names and neither may be the verification client, the
execution record's digest must be the digest of the registration bytes beside
it, every probe URL is rebuilt from `MikeMarcin/BPR-Test` and pull request 1
rather than pattern-matched, the token projection must omit the token and report
the empty scope the approved permission set implies, and a byte scan re-reads
every evidence file for material that should never have been written down.

Four of its checks exist because an independent review found them missing, and
each is the kind a mutation sweep cannot find — there was no condition to
neutralize. The course must name the digest of
`run_rfc0016_production_course_attended.py`, which is what makes it attributable
and what freezes that runner once a course is recorded. The probe table is
recomputed by equality against `expected_route_probes()` rather than
spot-checked, because `request_contract_sha256` is the only field tying a record
to the exact request bytes and, for the GraphQL probe, is the entire selector.
The GraphQL document is pinned to the designated query, since a document can
name a repository in hardcoded literals and never consult the pinned variables.
And the course tree is bounded to its exact file set, so an undeclared file —
including one nested a level deeper than a non-recursive listing sees — is
refused rather than carried along.

Drop `--require-complete` to admit the identity alone, which is the state
between the registration and the course sitting. Add `--validate-existing` for
closeout, where every evidence file must also be tracked and unmodified, and
`--release-binary` to require a shipped artifact to select the recorded
production client and not the verification one. It prints
`RFC0016_PRODUCTION_IDENTITY_OK` with the gate, record commit, client, probe
count, and scanned file count.

`rfc0016_production_identity_security` is its isolated gate. Most of that suite
builds a valid course and then breaks exactly one join, and the mutations are
applied before the tree is digested where a field is under test — otherwise the
run would fail on a stale digest and the field check would never be reached,
which is a green test for a red reason.

### The production course, and why it is a second file

`scripts/run_rfc0016_production_course_attended.py` is what an attended sitting
runs to produce that evidence: one device grant for the distributable client,
one authorization, and the ten designated read-only probes.

```sh
python3 scripts/run_rfc0016_production_course_attended.py \
    --registration-observation docs/verification/reviews/rfc0016-production-app-registration-v001/registration-observation.json \
    --output-root docs/verification/reviews/rfc0016-production-app-registration-v001/read-only-course
```

It is separate from `run_rfc0016_device_capture_attended.py` for a reason worth
knowing before anyone tries to merge them: **that file is evidence, not just
code.** BPR-0276's admitted `course-execution.json` records
`capture_runner_sha256`, and that value is the hash of the runner. Editing it —
to add a flag, to widen a client — invalidates an attended capture the owner has
already accepted, and the accepted fixture stops validating. The same holds for
`capture_rfc0016_device_contracts.py`, whose provenance hardcodes BPR-0276's
authority digest. `rfc0016_production_course_security` pins the runner's digest
to the recorded one so that this fails immediately and legibly rather than as a
puzzling fixture error.

What the two tools share is everything that can be shared: the strict HTTPS
transport, the route-closed probe wire, the designated probe catalog, and the
probe reply validation are imported and run unchanged, so route closure has
exactly one definition. Only the thin protocol layer carrying the client pin is
reimplemented, and each tool refuses the other's identity.

The course writes nothing it has not first proved. Evidence is built in memory,
staged, and then admitted by `verify_rfc0016_production_identity.py` itself
before anything is renamed into place, so the producer and the gate are shown to
agree on every run rather than assumed to. That check earned its keep during
development by catching the verifier demanding HTTP 200 for
`authorization_pending`, which GitHub is entitled to send as 400.

### Platform selection and what each platform's gate expects

All three shipped platforms select the recorded distributable identity. Each
was flipped in its own slice against its own target-native course, and none
flipped another's; the constant in `src/app/src/github_application.cpp` is the
only thing that enables a platform, and building on a different operating
system is not.

| Platform | Enabled by | Dev-tree isolation | Device gate expectation |
|---|---|---|---|
| macOS | [[BPR-0318]], 2026-08-29 | `macos-sandbox-exec` profile | `DEVICE_FLOW_STAGE:failed`, `DEVICE_FLOW_REQUESTS:0` |
| Linux | [[BPR-0310]], 2026-09-02 | `linux-network-namespace` | `DEVICE_FLOW_STAGE:failed`, `DEVICE_FLOW_REQUESTS:0` |
| Windows x64 | [[BPR-0311]], 2026-09-02 | AppContainer, but these selectors are `WINDOWS_UNCONTAINED` | `DEVICE_FLOW_STAGE:cancelled`, `DEVICE_FLOW_REQUESTS:0` |

The expectation differs because the isolation does. On macOS and Linux the
boundary denies the network, so a registered platform's probe cannot reach
GitHub and lands on `failed`. On Windows the device selectors run outside the
AppContainer — [[BPR-0362]] measured why a lowbox token cannot give them what
they need — so the real flow starts, reaches `awaiting_authorization` against
GitHub, and the probe's own budget cancels it there. An offline Windows host
lands `failed` and fails that gate.

Read `DEVICE_FLOW_REQUESTS:0` exactly: it counts submissions that reached the
*refusing* transport, never HTTP requests. Zero proves the capability gate
held, and is entirely consistent with a real device-code request having gone
out. On macOS and Linux the isolation makes the two readings coincide; on
Windows they come apart. Whether an unattended Windows gate may make outbound
contact at all was adjudicated by [[BPR-0515]] on 2026-09-03: with the tests-enabled `--network-denied` override the dev-tree gate is denied locally on every platform and lands on `failed` with zero connections.

The course enumerates every selector by name rather than counting matches, so
a name that is not registered on a platform is a refusal by name unless the
per-platform exclusion table in `scripts/verify_rfc0016_platform_course.py`
lists it with its reason — and an exclusion for a name that is registered
after all is refused as stale. Windows excludes exactly two:
`github_device_authorization_refresh_contract` and
`native_https_device_authorization_fixture`, both registered under a POSIX
guard in `tests/github_contract/CMakeLists.txt` and not yet mirrored into the
Windows block. Mirroring them, or making one exclusion permanent with its
cause, is [[BPR-0525]] ([[BPR-0524]] named them).

### Running a target-native course

The Linux and Windows courses are the same script with that platform's
interpreter and presets, run on that platform:

```sh
python3 scripts/verify_rfc0016_platform_course.py \
    --platform linux \
    --dev-preset dev \
    --release-preset app-release \
    --build-root build/rfc0016-linux-enablement \
    --expected-device-selection enabled \
    --fresh
```

```text
py -3 scripts\verify_rfc0016_platform_course.py ^
    --platform windows ^
    --dev-preset windows-dev ^
    --release-preset windows-app-release ^
    --build-root build\rfc0016-windows-enablement ^
    --expected-device-selection enabled ^
    --fresh
```

Getting a station into the state where either of those runs — the hosts, the
lanes, the launcher, the writer lease, and the supported Windows procedure —
is not repeated here. It is in
[remote stations](remote-stations.md), which is the operator procedure of
record for both.

### What RFC-BPR-0016 does not deliver yet

- **The browser handoff has never been observed opening a real browser.** The
  opener's presence in a shipped graph is proven by linkage and construction,
  and no suite may launch one. That the action reaches a real browser at a
  real verification address is a target-native observation, and all three
  rows — macOS, Linux, and Windows x64 — are still pending on [[BPR-0516]].
- **The Windows gate's network posture is decided.** [[BPR-0515]] closed on 2026-09-03 with the fixture proxy override; the sentences that follow record the posture it replaced. [[BPR-0515]] was open
  and owns it.
- **Nothing here is closeout evidence.** Every result above is an ordinary
  run. The immutable final-head replay on each enabled platform, the complete
  local matrix, and the independent capability-critical review are
  [[BPR-0285]]'s, and RFC-BPR-0016 stays `Deferred` until BPR-0035/BPR-0036
  close RFC-BPR-0002 besides.

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

For clangd, configure the `dev` preset before opening the tree in an editor.
The root `.clangd` points to `build/dev/compile_commands.json`, so clangd uses
the same C++ standard, include paths, Qt headers and platform flags as the
build. It suppresses one Homebrew LLVM clangd warning on designated initializers
that the Apple clang build accepts; `-Werror` and the build's warning policy
stay in place. If editing against a different preset, change the local
`CompileFlags.CompilationDatabase` path to that preset's build tree (for
example `build/windows-dev` for `windows-dev`) and restore `build/dev` before
committing. Do not copy a machine's compile database into the repository.

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

### Warning classes every compiler treats as errors

`cmake/Warnings.cmake` is the one enforcement point: `bpr_enable_warnings`
gives every first-party target `/W4 /permissive- /WX` on MSVC and
`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror` elsewhere.
Two classes are worth naming because one compiler used to catch them alone.
A local that hides an earlier local or a parameter is MSVC's C4456, and since
BPR-0534 the same code fails on clang (`-Wshadow` together with
`-Wshadow-uncaptured-local`, because `-Wshadow` alone skips a lambda local
that hides an enclosing local the lambda does not capture, the commonest
shape in this tree) and on GCC (`-Wshadow=local`, the local-scope form; GCC's
plain `-Wshadow` would also reject members and globals, which MSVC does not). Narrowing conversions are `-Wconversion` and
`-Wsign-conversion` on the clang hosts and C4244/C4267 under `/W4`. A slice
that compiles clean on macOS can still fail the Windows station on a class
this list does not cover — C4189 (unreferenced local) and C4996 (a deprecated
CRT name) have both done so — and the fix is the rename or the portable call,
never a narrower flag.

### GCC 15 can destroy a braced aggregate member twice

GCC 15.2 miscompiles one aggregate-initialization shape ([[BPR-0822]]). Clang
and MSVC are correct. The shape:

- A member is itself an aggregate with more than one element and a
  non-trivially destructible subobject, such as `std::string`.
- That member is initialized from braces without naming its type. This covers
  named or positional elements, `{ }`, and leaving the member out.
- A later member's initializer can throw.

On a throw, GCC destroys the member's subobjects twice. AddressSanitizer
reports `attempting double-free`, and an optimized build aborts. It happens in
every context that aggregate-initializes: `return`, `new`, a function argument,
`push_back`, an initializer list, a lambda capture, a base class, and a default
member initializer used by an omitted member. A member initialized directly in
a constructor's member-initializer list, a single-element nested aggregate and
a non-aggregate member type are not affected. An aggregate braced inside a
member-initializer list is.

**The fix.** Initialize the member from a prvalue of its named type, for
example `.Receipt = application::review_mutation_receipt{ ... }` or
`.Anchor = review_mutation_anchor_binding{ }`. Values, order and behaviour stay
the same. For an omitted member, name it and its type explicitly. A named type
in the member's default initializer does not help.

**Warnings do not find it.** The `app-release` preset (`-O3 -Werror`) reported
two sites as `-Wmaybe-uninitialized` and then built clean with others still
present. Clang tooling sees Clang's lowering, not GCC's. On the Linux station,
run the survey that asks GCC itself:

```sh
python3 scripts/survey_gcc_aggregate_cleanups.py --build-dir build/dev --build-dir build/app-release --jobs 10
```

- **What it does.** It first compiles and runs an embedded reproducer of every
  known shape, then requires its detector to flag exactly the shapes that
  double-destroy. Next it recompiles each translation unit with its recorded
  flags plus GCC's GIMPLE and EH dumps. It reports the nested cleanups that one
  throw would run twice.
- **Which compiler it calibrates.** Each C++ compiler the databases record,
  chosen by each unit's language (`-x`, else its suffix), in any order of
  `--build-dir` and of entries ([[BPR-1025]]). It prints
  `compiler <path>, C++ units: <n>` before each calibration. A C++ unit is
  surveyed only after its compiler calibrated. C units, such as the vendored
  SQLite amalgamation that `cc` compiles, are recompiled with their own
  compiler and counted in `units=`; C has no destructors, so they cannot yield
  a finding. Units in another language, and those of a Clang compiler beside
  GCC, are skipped and named. The `app-release` database alone works.
- **Exit status.** 1 when a finding lies under `src/`. 2 when a calibration or
  a compile fails, a compiler is neither GCC nor Clang, the databases record no
  C++ unit, or a database was configured from another checkout. 0 when the
  tree is clean or no compiler is affected. Findings under `tests/` are listed
  but do not fail it.
- **Reading a finding.** It names the line GCC attributes to the overlap,
  which for an initializer list can be the closing brace of the enclosing
  function. It means GCC built overlapping exception cleanups there; the call
  that would throw may never throw in practice.
- **When to run it.** After the station's GCC changes, and on a Linux stock
  build of a base that adds aggregate initializers under `src/`. Both presets
  take about five minutes at `--jobs 10`.

### The format gate needs clang-format 22 or newer

The tree is rendered by **clang-format 22 or newer**, and both format scripts
now refuse an older tool rather than returning a verdict from it:

```
error: unsupported clang-format: /usr/bin/clang-format reports version 21.1.8
       This project's .clang-format requires clang-format 22 or newer. …
```

There are three tiers, and only the third can produce a verdict:

| clang-format | What happens |
|---|---|
| 17 and earlier | **Cannot read `.clang-format` at all**: `unknown key 'AlignFunctionPointers'`, an option added in 18. The tool falls back to a default style, so every file looks dirty for a reason that has nothing to do with the file. Verified directly with Apple clang-format 16.0.0. |
| 18 through 21 | Read the style file, but render an empty braced initializer as `{}` where this tree holds `{ }` |
| 22 and newer | Render the tree as it is committed |

The middle tier is the one that looks like a real result and is not. Measured on
Ubuntu 26.04, 21.1.8 reported 292 of 375 files needing formatting where 22.1.2
reported one; every one of the 151 files under `src/` containing `{ }` was in the
older tool's list, and every file it called clean contained none. So the failures
were never selective, and a passing run under an unrecorded version was never
proof. No key restores the space under `Cpp11BracedListStyle: false` in those
releases — `SpaceInEmptyBraces` is not an option they accept.

`format.py` refuses for the stronger reason: an older tool would not merely
misreport those files, it would rewrite them.

Pass `--allow-unsupported-clang-format` to run anyway. The scripts still print
the diagnostic and label the result advisory, because a verdict from a tool that
renders the style file differently is not a gate verdict.
`coding_standard_format_gate_contract` covers the refusal, the override, and the
accepted path.

### The sweep checks formatting: `coding_format_check`

The repo-wide format gate is also a CTest row, `coding_format_check`
([[BPR-0865]]). It checks every file under `src/` and `tests/`. Before this
row, a fully green sweep could hide an unformatted tree. That happened twice:
[[BPR-0862]] found seventeen unformatted files, and four more were found at
`95343750` when this row was added.

The row uses the clang-format that configure found. Configure does not simply
take the first one on `PATH`. It asks each candidate on CMake's program search
path for its version and passes over any older than 22. On `bpr-linux`,
`PATH` gives 21.1.8 from `~/.local/bin` first, and `/usr/bin/clang-format` is
22.1.8. Configure picks the second, and prints the result:

```
-- Coding standard clang-format: /usr/bin/clang-format
```

The `coding-format` and `coding-format-check` targets now use the same tool.
To choose one yourself, set `-DBPR_CLANG_FORMAT_EXECUTABLE=<path>`.

The row has three outcomes:

| Result | Meaning |
|---|---|
| Passed | Every file is formatted, as judged by a supported clang-format |
| Failed | The row lists the files needing formatting. Fix them with `coding-format` |
| Skipped | There was no supported clang-format, so there is no verdict. The row prints why |

A skip is not a pass. It appears in CTest's "The following tests did not run"
list and in the JUnit `skipped` count. When you report a sweep, report the
skip as well; a sweep where this row was skipped says nothing about whether the
source is formatted. The row exits 77, its `SKIP_RETURN_CODE`. On Linux and
macOS, the isolation launcher also prints `TEST_RUNNER_CHILD_EXIT_FAILED:77`.
CTest applies the skip code before the error expression, so the row is still
reported as skipped. This was measured on CMake 4.4.2.

On **Windows**, `clang-format` is not on `PATH` by default even with Visual
Studio 2022 installed, so the gate's first failure on a fresh checkout is
`Tool not found on PATH: clang-format` rather than a formatting complaint, and
configure finds no tool, so `coding_format_check` is skipped. Add
the Visual Studio LLVM directory before configuring:

```powershell
$env:PATH = "$env:PATH;${env:ProgramFiles}\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin"
```

Adjust the edition and version segments to the installed instance. You can
also give the executable directly: pass `--clang-format` to the scripts, or
`-DBPR_CLANG_FORMAT_EXECUTABLE` to configure. Check what the toolchain actually
supplies before trusting a run: Visual Studio's bundled LLVM has satisfied the
minimum since it moved to 22.x, and an instance still shipping 21 or earlier
gets the refusal above.

### A passing row can leave cases unchecked

CTest has no result for a row that passed but skipped some of its cases.
`dependency_modernization_contract` was such a row. Its whole output was a line
of dots with one `s` and `OK (skipped=1)`, so a green sweep could not say which
guarantee went unchecked, or why ([[BPR-0871]]).

Every Python unittest row that can skip a case now runs through
`tests/scripts/skip_report.py`. Before unittest's summary, the row prints one
line for each case it did not check, with the reason:

```
SKIPPED: dependency_modernization_tests.DependencyModernizationTests.test_syntax_prefix_installs_one_library_per_configuration: one installed KSyntaxHighlighting configuration, DEBUG, in …/prefix/lib/x86_64-linux-gnu/cmake/KF6SyntaxHighlighting: a single-configuration tree has nothing to compare
```

An expected failure prints `EXPECTED FAILURE: <case>`. To list every unchecked
case in a sweep, search its log. Copy or search `LastTest.log` before running
`ctest` again for any reason: every invocation rewrites it, `--show-only`
included, so take a `--show-only=json-v1` inventory before the sweep, not
after it ([[BPR-0905]]'s Linux run lost its sweep's lines that way):

```sh
grep -h -e '^SKIPPED: ' -e '^EXPECTED FAILURE: ' build/dev/Testing/Temporary/LastTest.log
```

The same lines appear in the JUnit report's `system-out`. CTest keeps only the
first KiB of a passing row's output there. So when the report has something to
name, it also prints `CTEST_FULL_OUTPUT`, and CTest keeps the whole output.

When you report a sweep, list these lines as well as the rows that did not run.
A skipped row, such as `coding_format_check` without a supported clang-format,
gave no verdict. A row with skipped cases gave a verdict on everything except
the named cases.

These case skips are deliberately not turned into row skips or failures. Most
are true for the tree or platform: the per-configuration KSyntaxHighlighting
check has nothing to compare on a single-configuration tree, and a Linux sweep
skips the macOS-only cases. A skipped row would report its other, checked
cases as not run. It would also put a row in "The following tests did not run"
on every Linux and macOS sweep, and readers would learn to ignore that list.

`skip_report_contract` pins the output. It also fails when a unittest script
under `tests/` can skip a case but does not use the runner. It reads each
script's own text, so it does not see a skip raised in an imported helper.

### A course's data root sits one owned level down

The application keeps its agent workspace neighbourhood beside its data root,
and startup recovery refuses a neighbourhood whose parent another principal
can write. A course that roots its data directly in a bare temporary
directory puts that parent in the system temporary directory: a shared `1777`
`/tmp` on Linux, and `%TEMP%` with its inherited entries on Windows. The
application then prints
`AGENT_WORKSPACE_RECOVERY_REFUSED:<root>/agent-workspace-ownership-v1:repair-required`
once and composes no disposable agent workspaces ([[BPR-1044]]).

So a course makes its root the `data` child of a temporary directory it has
narrowed to owner-private. Each language has one helper:

- C++: `bpr::e2e::private_data_root` in `tests/end_to_end/private_data_root.hpp`;
- CMake: `bpr_private_data_root` in `tests/end_to_end/bpr_narrow_owner_private.cmake`;
- Python: `contained_temporary_directory`, narrowed with `make_root_private`
  from `tests/end_to_end/bpr_narrow_owner_private.py`.

`bpr_add_network_isolated_test` fails every row whose output contains
`AGENT_WORKSPACE_RECOVERY_REFUSED` ([[BPR-1050]]). A row whose subject is the
refusal registers with `EXPECTS_WORKSPACE_RECOVERY_REFUSAL`, and
`scripts/verify_test_network_isolation.py` pins which rows may. The error
expression sees only what reaches CTest. It cannot see a driver that captures
its child's standard error and does not relay it, and it cannot fire in a
macOS run under CTest, whose `TMPDIR` is a private directory that recovery
accepts.

### On macOS the tests-enabled build is an application bundle

Both macOS graphs are bundles. The executable is at

```
build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
```

and every path in this document uses that form. On Linux and Windows the
executable is still flat at `build/dev/src/app/better_pull_review`; substitute
accordingly.

The reason is Notification Center, which registers applications by bundle
identity. An unbundled process cannot request authorization, never gains a
System Settings → Notifications entry, and its delivery requests go nowhere —
so the attended notification course, which binds the tests-enabled executable,
was asking a bare Mach-O to produce a banner nobody could have clicked.

The two bundles carry **deliberately different identities**:

| Graph | `OUTPUT_NAME` | `CFBundleName` | `CFBundleIdentifier` |
|---|---|---|---|
| tests-enabled | `better_pull_review` | `BetterPullReview (tests)` | `io.github.MikeMarcin.BetterPullReview.tests` |
| tests-disabled | `BetterPullReview` | `BetterPullReview` | `io.github.MikeMarcin.BetterPullReview` |

Notification Center keys authorization by identifier, so sharing one would let a
tests-enabled build grant or consume the shipped application's permission.
`OUTPUT_NAME` is left at the default for the tests-enabled graph on purpose: the
executable inside the bundle keeps its established name, so only the directory
portion of any recorded path changes and
`scripts/verify_dif_asset_boundaries.py` still finds it by name **in this graph**.
It cannot find the tests-disabled one, whose executable BPR-0328 renamed to
`BetterPullReview`; that is a pre-existing gap tracked by BPR-0395, not something
bundling introduced.

No registered test needed editing. They resolve the application through
`$<TARGET_FILE:app_better_pull_review>`, which for a bundle target names the
inner executable rather than the `.app` directory — the same distinction
`$<TARGET_BUNDLE_DIR:...>` relies on for the designated-live capsule.

The course refuses on its own when it has no identity, reporting
`NATIVE_NOTIFICATION_REFUSED:bundle_identity_absent` before it arms anything,
and prints `NATIVE_NOTIFICATION_BUNDLE_IDENTITY:<identifier>` so
`application_native_notification_attended_contract` can require the exact
expected one. A build that silently reused the shipped identity fails that
second check.

Two notes on what this does not do. The bundle is neither signed nor notarized,
so whether macOS ultimately surfaces a banner is still a question only a person
looking at the screen can answer — that remains BPR-0168's. And `macdeployqt`
does not run: Qt is still resolved through the build's rpath, exactly as for the
flat executable.

## Parser fuzzing

[[BPR-0970]]. `BPR_BUILD_FUZZERS=ON` adds the libFuzzer targets in
`tests/fuzz`, one per parser family, to a Clang or Apple clang build on macOS
or Linux. It instruments every first-party unit for coverage and for
AddressSanitizer and UndefinedBehaviorSanitizer, so use a tree of its own.
Apple clang instruments but ships no libFuzzer runtime; name the archive of an
LLVM installation instead. Linux Clang needs no runtime argument.

```sh
cmake --preset dev -B build/fuzz -DBPR_BUILD_FUZZERS=ON \
  -DBPR_LIBFUZZER_RUNTIME=/opt/homebrew/opt/llvm/lib/clang/23/lib/darwin/libclang_rt.fuzzer_osx.a
cmake --build build/fuzz --target parser-fuzzers
ctest --test-dir build/fuzz -L bpr_fuzz_corpus --output-on-failure
```

Each `fuzz_<family>_corpus` row replays `tests/fuzz/corpora/<family>` once.
A campaign runs a target on a copy of its corpus, never on the checked-in one,
with the target's `-max_len` from `tests/fuzz/CMakeLists.txt`:

```sh
cp -R tests/fuzz/corpora/github_api /tmp/github_api
build/fuzz/tests/fuzz/github_api -seed=970005 -max_total_time=900 \
  -max_len=262144 -timeout=10 -rss_limit_mb=2048 -malloc_limit_mb=512 \
  -print_final_stats=1 -artifact_prefix=/tmp/github_api- /tmp/github_api
```

A crash, sanitizer report, timeout or out-of-memory input is written beside
the artifact prefix. Fix the cause, then add the input to the corpus as a
regression seed. [docs/verification/BPR-0970-parser-fuzzing.md](../verification/BPR-0970-parser-fuzzing.md)
holds the inventory of entry points and the campaigns run so far.

## Focused evidence

```sh
cmake --build build/dev --target check_boundaries
python3 scripts/verify_documentation.py --root .
python3 scripts/verify_binary_dependencies.py \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
python3 scripts/verify_runtime_surface.py \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review --self-test
build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
  --render-evidence build/dev/foundation-window.png
```

## Current CLI milestone and historical checks

RFC-BPR-0015's amended first read milestone has a qualified macOS fixture
path. Native HTTPS remains the default. The tests-disabled macOS composition
has explicit
CLI read opt-in on the selected GitHub.com or GHES host, with a Connect warning,
observed `/user` identity, pre/post-request account checks and a read-only lease.
It invokes the configured `gh` directly with bounded arguments, input, output
and time; account discovery and API use require `gh` 2.x with minor >= 96
because the account inventory uses `gh auth status --active --json hosts`.
Older versions are refused before account status runs and the UI requires an
upgrade. BetterPullReview never passes `--show-token`.
This is a version check, not proof of an official executable's provenance.
The r2 trust-boundary amendment permits a bounded exact-repository `gh` Git
operation for objects; `gh` may internally launch Git and its shell credential
helper. Every macOS `gh` launch must run under OS-enforced file-write denial,
with a narrowly owned temporary object-store exception only for the bounded
clone; an unavailable boundary refuses the CLI profile. This covers `gh`'s
possible first-use global configuration migration, which a post-request check
cannot prevent. BetterPullReview must not read the helper's token exchange or
change global authentication. Full Git comparison currently requires
`gh` 2.100.0 or 2.101.0 and Apple Git 2.50.1 (Apple Git-155), with a 32 MiB per-file
ceiling and supervised 96 MiB/128-entry temporary-store abort threshold; the
aggregate threshold is not a hard disk quota. Local refs reflect the accepted
API snapshot, not fresh remote advertisement. A tests-disabled macOS fixture
now renders a full PR diff through the CLI profile; an isolated build with
the process adapter removed from composition visibly refuses the same healthy
fixture. A macOS native read-only probe now verifies one official `gh` HTTPS clone of a
small owner-controlled private repository through `gh auth git-credential`;
`gh` reports a keyring token source and a matching Keychain item exists.
The probe does not bind the precise Security API calls, credential generation,
or concurrent account selection. The private repository name and objects
were not retained. On macOS, the opted-in CLI profile routes fixture
whole-review and right-side inline-comment publication through a separate
exact-target confirmation and server reconciliation. No live PR was
written. Windows and Linux remain unavailable. BPR-0787 records native
qualification, and BPR-0788 closed RFC-BPR-0015 as Auto-Verified on 2026-09-23.

BetterPullReview does not extract or copy CLI tokens or change global CLI
authentication. Opt-in trusts the machine's configured executable, configuration,
socket routing, CA and redirect behavior, active account and helper processes.
Pre/post observations can detect some account changes but cannot bind a credential
generation or prevent a concurrent account switch. Executable/configuration/CA
replacement and process escape remain uncontained risks. Read opt-in grants no
publishing authority. RFC-BPR-0013 keeps a 250-record limit for commits returned by the pull-request
endpoint. A larger reported total is accepted only when that endpoint
returns exactly 250 records ending at the accepted head. The UI names the
reported total and returned count separately; this does not establish that
any particular private pull request can render.

The dev/test graph retains unavailable CLI fixtures; those results do not
qualify tests-disabled macOS CLI reads. The following zero-launch assertions
record RFC-BPR-0002 evidence and require reconciliation for RFC-BPR-0015.

The public application provides built-in GitHub.com native-HTTPS and
explicit CLI choices. Add explicit GHES 3.18 choices with:

```sh
build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review --ghes-host ghe.example.com
```

Activating native HTTPS opens the Qt-owned password prompt, then enrolls the
credential through the selected platform vault and authenticated `/user`
observation. Since [[BPR-0909]] no card carries a precomposed GitHub CLI
choice: GitHub CLI is the discovered Use GitHub CLI entry, wired only where
the composed run offers the read. `--self-test` checks once, before any
gesture and without pressing anything, that no server card carries a GitHub
CLI profile and that the entry is offered exactly when the read is
([[BPR-0982]]). Inside the automated test boundary it reports
`CLI_ENTRY_ABSENT_OK`; a tests-disabled macOS run reports
`CLI_ENTRY_OFFERED_OK`. `CLI_ENTRY_CARDS:1` counts the default server card,
`CLI_ENTRY_CARDS:2` the pair with `--ghes-host`. It then reports whether
composition selected the native platform vault. On macOS, Windows, and Linux
its successful fixed output includes `NATIVE_PLATFORM_VAULT_SELECTED:true`.

The test-enabled application also exposes two deterministic recorded public
paths:

```sh
QT_QPA_PLATFORM=offscreen \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
  --github-fixture github-com \
  --render-evidence build/dev/github-public-path-github-com.png

QT_QPA_PLATFORM=offscreen \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
  --github-fixture ghes-3.18 \
  --render-evidence build/dev/github-public-path-ghes-3.18.png

# The recorded public pull request google/UIforETW#74, served from
# tests/fixtures/github/pull-requests/uiforetw-74/. Serves the generated
# commits rather than GitHub's; see docs/verification/fixture-catalog.md.
QT_QPA_PLATFORM=offscreen \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
  --github-fixture uiforetw-74 \
  --render-evidence build/dev/github-public-path-uiforetw-74.png

# The constructed nested-folders pull request bpr-fixtures/nested-folders#21,
# from tests/fixtures/github/pull-requests/nested-folders-v1/ (BPR-1213).
QT_QPA_PLATFORM=offscreen \
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
  --github-fixture nested-folders-v1 \
  --render-evidence build/dev/github-public-path-nested-folders-v1.png
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
python3 -m unittest tests/scripts/verify_composed_hosts_tests.py
ctest --test-dir build/dev --output-on-failure \
  -R '^(composed_host_verifier_contract|composed_application_hosts|application_(public_path(_ghes_cli_unavailable)?|worker_executor|composition_contract|github_public_path_(github_com|ghes_3_18)))$'
```

`composed_application_hosts` inspects the real test-enabled executable with the
`development-fixture` profile. It requires exact demangled constructor or
decision anchors for the GitHub, epoch, diff, workbench, appearance, syntax,
review-ledger, defect-correlation, queue, notification, portable-agent, and Qt
window paths, plus the shared review-mutation host, coordinator, and generic
`bpr::application::reconcile` decision. That profile additionally requires the
exact test-only GitHub public-path fixture provider boundary and testing-grant
factory while forbidding the live provider type and factory. The `production`
profile additionally requires the live provider type and factory while
forbidding the fixture component type and testing-grant factory; the generic
reconciliation decision remains required because the tests-disabled live path
uses it for post-mutation projection. `application_composition_contract` runs
that profile against the fresh tests-disabled Release executable.

The inspector fails closed for a missing binary, missing tool chain, nonzero
tool, failed demangler, malformed inspector output, or table with zero defined
symbols. It selects demangling-capable `llvm-nm`, platform `nm` plus `c++filt`
or `llvm-cxxfilt`, then `objdump -t -C` as the portable fallback. The audit
parses only defined symbol records, discards `U`/`UND` records, and requires a
function's qualified name to begin the normalized symbol name; an undefined
reference or a name embedded in template text cannot satisfy an anchor.
Windows declarations may discard only documented access/member prefixes and
the declaration prefix through the first allowlisted calling-convention token
at top-level balanced-delimiter depth. Conventions inside return-function
parentheses or template arguments do not select the anchor boundary, and
unbalanced declarations fail closed. Exact angle-bearing C++ operator spellings,
including `operator->` and `operator->*`, are treated as operator tokens rather
than delimiters; arbitrary unmatched angle brackets still fail. The verifier
never searches arbitrary return or template text. Explicit symbol-kind and
section allowlists prevent absolute, common, debug, undefined, and unknown
records from counting as implementations. Forbidden provider types cover their
exact methods and ABI type metadata. Raw `dumpbin` or
`llvm-readobj` output is not accepted as a demangled symbol table. CTest uses
the verifier's process exit status as the verdict; no pass-output regular
expression can override a failing exit. These checks prove linkage only. The
executable contracts remain the separate evidence that a linked host is
constructed and that a real gesture reaches it at runtime.

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

The following describes the historical RFC-BPR-0002 composition contract;
its native-only symbol and zero-launch requirements are not RFC-BPR-0015
acceptance. The updated tests-disabled Mac public path still needs direct evidence.

`application_composition_contract` creates a fresh tests-disabled Release
tree. It proves the public application header has no testing conditional and
the Release compile commands, build graph, symbols, and raw executable bytes
contain no fixture translation unit, macro, epoch fixture argument/marker,
label, path, or evidence string. It also requires production to compose only
`native_github_wire_transport_factory` and rejects CLI process-runner factory
symbols in the application. Its production-profile composed-host audit also
requires every shared exact demangled anchor, including the generic
reconciliation decision, and the live mutation provider type and factory while
rejecting the fixture component type and testing-grant factory. The
tests-disabled `--self-test` requires `CLI_ENTRY_OFFERED_OK` on macOS
(`CLI_ENTRY_ABSENT_OK` elsewhere) plus the exact `CLI_ENTRY_CARDS`; provider
counter tests prove the CLI viewer rejection makes zero transport-factory,
wire, capability, or CLI-specific effect calls, while the transport contract
proves a fake successful process port still receives zero calls. Public
clean-start selection itself performs one bounded noncredential
profile-resolution effect per row. Existing host lifecycle coverage separately
proves a native-to-CLI switch exact-deletes the predecessor credential binding
without enrolling or retrieving a credential for the CLI profile.
`application_public_path_ghes_cli_unavailable` supplies the explicit two-card
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
  -R '^linux_secret_service_(contract|native|native_setup|native_cleanup|refusal_control)$'
```

The native rows write, replace and delete their integration item in the
collection the `default` alias names, so they run only on a session bus
designated for tests ([[BPR-1117]]). Every mode of the native executable
except `--expect-unavailable` checks this first. It refuses with
`linux-secret-service-bus:refused:<reason>` and a failing exit, before any
Secret Service call, unless all of these hold:

- `BPR_DESIGNATED_TEST_SESSION_BUS` names exactly one
  `unix:path=<absolute path>,guid=<32 hex digits>` address, the form
  `dbus-daemon --print-address` writes;
- `DBUS_SESSION_BUS_ADDRESS` is exactly that address;
- the server the connection reaches reports that GUID.

The reasons are `undesignated`, `designation-malformed`, `not-designated`,
`bus-unreachable` and `bus-identity-mismatch`. A bus the launcher pinned from
`$XDG_RUNTIME_DIR/bus`, or the user's own session bus, is never the
designated one.

Each registered native row runs through
`tests/scripts/linux_secret_service_private_session.py`. That harness:

- starts a private `dbus-daemon` on a path socket in an owner-private
  temporary root. Its configuration names no service directory, so it
  activates nothing;
- starts a test-only `gnome-keyring-daemon` in the foreground on that bus,
  under fresh directories, unlocked from standard input with a random
  password that is never printed or stored;
- waits until the `default` alias names an unlocked collection;
- runs the row with that bus designated;
- stops the keyring and the bus by the exact process it started, and removes
  the root.

The rows therefore need no desktop session and never touch the user's
keyring. They need `dbus-daemon`, `dbus-send` and `gnome-keyring-daemon` in
`/usr/bin` or `/bin`. Without them a row fails with
`linux-secret-service-private-session:unavailable:<tool>-missing`. Each row
has its own keyring, so the setup and cleanup rows cannot meet an item
another run left. The native row's own exact post-test absence check is what
proves cleanup. A harness killed outright takes both daemons with it
(`PR_SET_PDEATHSIG`), but leaves its `/tmp/bpr-secret-service-*` root for
removal by name.

`linux_secret_service_refusal_control` pins the refusals. It starts a
designated bus and a second one, with no keyring on either. It then requires
the native executable to refuse:

- the second bus, named by address or found through a runtime directory;
- a missing designation;
- a designation without its GUID;
- the designated path with another server's GUID;
- `--expect-unavailable` while a runtime directory holds a bus.

It also requires the designated bus to reach the vault.

To designate a controlled session explicitly, set both variables to the
address its `dbus-daemon` printed. An example is a dedicated user's Xvfb,
D-Bus and test-only gnome-keyring session in a VM. Never designate a bus
whose keyring holds anything real. Never place the keyring password or a
credential canary in arguments, environment, command output, or checked-in
configuration. The fixture uses only the disjoint integration-test schema and
confirms exact cleanup.

The unavailable, locked, and dependency-level redirection cases are
controlled direct checks rather than registered CTest cases.

- `--expect-unavailable` must find no bus at all. It refuses with
  `session-bus-named` while `DBUS_SESSION_BUS_ADDRESS`, `XDG_RUNTIME_DIR` or
  `DISPLAY` is set, because each is a route to a session bus.
- The other two need a designated bus. Run the second on a designated test
  session whose default collection is locked. Run the third while a
  designated test session's ordinary canonical Secret Service remains
  available.

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

# The address the designated test session's dbus-daemon printed, guid= included.
DBUS_SESSION_BUS_ADDRESS="${bpr_test_bus}" \
BPR_DESIGNATED_TEST_SESSION_BUS="${bpr_test_bus}" \
  "${bpr_linux_vault_test}" --expect-locked

DBUS_SESSION_BUS_ADDRESS="${bpr_test_bus}" \
BPR_DESIGNATED_TEST_SESSION_BUS="${bpr_test_bus}" \
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

For the historical BPR-0025 GitHub CLI process boundary, build and run the two focused
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

The historical RFC-BPR-0002 milestone did not positively enable CLI transport.
The following reports that original refusal policy, not the amended RFC-BPR-0015
macOS read policy. Native HTTPS was the
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
and terminates it. With `--private-data-root`, which the dev-preset
`runtime_process_surface` row passes, that launch gets an owner-private
temporary `--test-application-data-root` that the script removes afterwards, so
the idle application never composes the default profile (BPR-0820); a
tests-disabled artifact is inspected without it. A pass is a point-in-time idle
observation, not continuous
monitoring or evidence for active native-HTTPS mode, indirect descendants, or
listener absence on a platform where the script was not run. The Windows run
remains BPR-0036 work. The binary-dependency script also supports all three
platforms, using `dumpbin` on Windows when available, followed by
`llvm-readobj` and `objdump` fallbacks.

On macOS the direct-child sample calls `proc_listchildpids` through libproc
instead of launching the process-wide `ps` utility. The query admits at most
1,024 direct child PIDs, uses one extra native buffer slot to detect overflow,
normalizes duplicate results, and fails closed on library, API, return-count,
PID, or capacity errors. Linux and Windows keep their existing platform paths.

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

The host-owned catalog now rehydrates exact device-grant bindings through the
BPR-0541 private metadata store. The production policy retains only settled
device grants; manual sessions still exact-delete on orderly shutdown. Metadata
and delete terminals count through quiescence before composition joins their
workers. Startup validates the current profile, vault material, authenticated
account and stored lifetime. No enumeration, v1 fallback, or broad crash
recovery scan is available. Query-bearing REST redirects continue to fail
closed under the completed BPR-0038 decision.

For the BPR-0028 provider-free Qt projection and presentation, build the host
and Qt targets and run the atomic projection, architecture-boundary, and three
scale-specific presentation cases:

```sh
cmake --build --preset dev --target \
  test_better_pull_review_application_github_host_tests \
  test_better_pull_review_qt_github_tests

ctest --test-dir build/dev --output-on-failure \
  -R '^(application_github_host|qt_composed_github_scale_(100|125|200)|architecture_boundaries|architecture_boundary_contract)$'
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
  -R '^(qt_composed_github_scale_(100|125|200)|qt_github_conversation_model_property)$'
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
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
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
window is inactive or has no focus widget. Before reaching that checkpoint it
selects `review.cpp` through the shipped Files row and awaits the exact
authority/revision/file/blob/options-bound selected diff within the same
30-second run budget. A still-progressing host turn remains pending. A hard
failure or exhausted bound reports
`NATIVE_APPEARANCE_PRECONDITION_REFUSED:<typed-cause>:<selected-diff-cause>`;
neither case emits appearance verdict markers because no Light baseline was
captured. The registered executable contract also cuts the real selection wire
and requires that failure while the direct fixture diff helper's invocation
count remains zero. Its 1 ms startup-budget branch expires before the selection
course is initialized and must therefore report the truthful pending reason
`awaiting_workbench_observation`, helper-unused success, bounded shutdown, and
no render or appearance verdict.

Once the selected diff is ready, the Cocoa probe prints
`NATIVE_APPEARANCE_WAITING_FOR_OWNER_FOCUS` once and waits. The desktop owner
must click the BetterPullReview window to activate it and give a widget focus;
the probe never calls an activation API, synthesizes input, or relaxes a
post-focus assertion. An unattended run eventually reports
`NATIVE_APPEARANCE_PRECONDITION_REFUSED:owner_focus_required:none`, writes no
PNG, and emits no focus, signal, state, pixel, geometry, or changed-render
verdict. Non-Cocoa probes report `platform_not_cocoa` after proving the exact
selected-diff course. The readiness loop below therefore does not progress
until the owner has focused the window.

The dark side waits for the same thing at the other end of the transition.
Changing the appearance is precisely what takes focus away — a person has to go
to System Settings — and the one-shot measurement requires the window active,
the application's active window, and focus on the exact widget the Light
baseline recorded. Once the local, global, and style-hint schemes all read Dark
and the settle counter has elapsed, every wait entry is reported as
`NATIVE_APPEARANCE_WAITING_FOR_OWNER_RETURN:<episode>` together with independent
`OWNER_WINDOW_ACTIVE`, `OWNER_APPLICATION_ACTIVE_WINDOW`, and
`OWNER_EXACT_FOCUS` facts. The first strict success emits
`NATIVE_APPEARANCE_OWNER_RETURNED`. A state failure can therefore no longer
masquerade as a focus failure, and a repeated wait cannot be silent. The wait is
bounded only by the run budget, which is evaluated at the top of every tick
ahead of the stage that contains it; a run that never gets the owner back
expires and still emits its complete marker record. No automated course can
enter the wait, because every one of them refuses at `platform_not_cocoa`
before the stage exists, and
`application_native_appearance_probe_contract` asserts the new owner-return,
operand, capture, and persistence markers are absent in all three offscreen
courses.

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
  build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
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
  'NATIVE_APPEARANCE_SELECTED_DIFF_READY' \
  'NATIVE_APPEARANCE_SELECTED_DIFF_REASON:none' \
  'NATIVE_APPEARANCE_SELECTED_DIFF_BINDING:111111' \
  'NATIVE_APPEARANCE_SELECTION_WIRE_OK' \
  'NATIVE_APPEARANCE_FIXTURE_DIFF_HELPER_UNUSED_OK' \
  'NATIVE_APPEARANCE_READY:Light' \
  'NATIVE_APPEARANCE_GLOBAL_READY:Light' \
  'NATIVE_APPEARANCE_TRANSITION:Light->Dark' \
  'NATIVE_APPEARANCE_GLOBAL_TRANSITION:Light->Dark' \
  'NATIVE_APPEARANCE_PLATFORM:cocoa' \
  'NATIVE_APPEARANCE_STYLE:QMacStyle' \
  'NATIVE_APPEARANCE_OWNER_RETURNED' \
  'NATIVE_APPEARANCE_OWNER_WINDOW_ACTIVE:true' \
  'NATIVE_APPEARANCE_OWNER_APPLICATION_ACTIVE_WINDOW:true' \
  'NATIVE_APPEARANCE_OWNER_EXACT_FOCUS:true' \
  'NATIVE_APPEARANCE_INITIAL_CAPTURE:captured' \
  'NATIVE_APPEARANCE_CHANGED_CAPTURE:captured' \
  'NATIVE_APPEARANCE_PERSISTENCE:persisted' \
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
targets selected in `docs/issues/60-done/BPR-0048-rfc-0003-focused-tests.md`.
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

`qt_appearance_profile` also runs the core's enumerated contrast pairs against
all four shipped built-in palettes. It measures every variant before failing,
so a shortfall reports the whole picture rather than the first colour it meets;
a failure prints the offending role pair with its measured ratio in hundredths
beside the minimum it missed. Do not repair a failure by lowering a threshold
or removing a pair from the checked set — the check exists to constrain the
palettes, so a threshold tuned until the current colours pass measures nothing.

Two assertions in that suite guard repairs rather than the palettes. The first
requires an intraline emphasis fill to differ from the row fill it marks: fill
luminance is pinned by the contrast band, and at that luminance a saturated red
is very nearly the removed row's own colour, so a repair that satisfies
contrast can silently erase intraline marking while every contrast check still
passes. The second pins the pairs that colour alone cannot separate, which are
inherent to red/green diff fills under dichromacy rather than repairable; it
fails if a palette change creates a new one, or if a recorded one disappears
without the record being updated.

### RFC-BPR-0007 local review ledger

```sh
cmake --build --preset core-only --target \
  test_better_pull_review_review_ledger_core_tests
ctest --test-dir build/core-only --output-on-failure -R '^core_review_ledger$'
```

The receipt policy is exercised under `core-only`, which links neither Qt nor
SQLite, so "the core owns this decision and reaches no storage" is proven by
the build rather than asserted in review.

Two properties in that suite are easy to lose and worth naming. Reading a file
and accepting it are independent facts, so the cases drive all four
combinations rather than a single state machine. And a conversation reopens on
an *edit* as well as on a new comment: the edited observation deliberately
carries the same comment identity as the one already seen, so a cursor that
compared identity alone would pass every other case and fail only this one.

## Troubleshooting and safe verification

### Agent invocation composition

The bounded no-network agent route can be checked without an installed agent
or provider credential:

```sh
cmake --build build/dev --target \
  test_better_pull_review_agent_session_core_tests \
  test_better_pull_review_review_ledger_host_tests \
  test_better_pull_review_agent_launch_tests \
  test_better_pull_review_agent_invocation_host_tests \
  test_better_pull_review_sqlite_review_ledger_tests \
  test_better_pull_review_qt_agent_launch_panel_tests \
  test_better_pull_review_qt_agent_launch_canary_tests \
  test_better_pull_review_agent_invocation_composition_public_path
ctest --test-dir build/dev --output-on-failure -R \
  '^(core_agent_session|application_agent_launch|application_agent_invocation_host|application_review_ledger_host|sqlite_review_ledger_storage|qt_agent_launch_panel|application_agent_invocation_(composition|construction_cut|gesture_cut|cancellation|staleness|cleanup_retry)_contract)$'
```

The positive public contract uses two fresh processes over one data root. The
first selects one of two private notes plus the exact current draft revision on
a real fixture diff, then compares the sanitized child's complete decoded stdin
and proves the hostile unchecked canary is absent. The second launches no child:
it reopens immutable session #1, observes next ID #2 and current classification,
and records zero session admissions. The wrapper requires distinct child-produced
seed and reopen markers, guards both source branches against vacuity, and leaves
the positive CTest result to the wrapper's exit status rather than a pass regex.
SQLite component coverage separately proves post-close physical main/WAL/SHM
equality for reopen/load, bounded retention, restart, exhaustion, canonical
v3-to-v4 migration, retained-WAL byte-preserving refusal, and simultaneous-open
allocation. Its BUILD_TESTING-only preflight seams also replace the accepted
main/WAL/SHM namespace immediately before the original open and substitute or
make nonempty the private clone at cleanup. The former refuses before SQLite
touches the replacement; the latter preserves the substitute/unknown child,
sanitizes retained main/WAL authority, and reports cleanup failure explicitly.
The first process materializes the exact accepted head outside the retained
cache and waits for terminal projection plus cleanup. The two cut
contracts must remain green: they prove that neither omitted host construction
nor a disconnected reviewer gesture can be hidden by direct fixture launch.
The cancellation course stops a live descendant; the staleness course moves
the accepted head while the child is live. Both require a distinct terminal,
empty stale output, exact cleanup, and host quiescence.
These tests do not run installed Codex or Claude and make no Windows launch
success claim.

### GitHub and native credentials

- CLI reads are qualified for tests-disabled macOS on the recorded fixture
  paths, through Use GitHub CLI, the read opt-in and configured `gh` 2.x,
  minor >= 96. Full Git comparison keeps its separate exact `gh`
  2.100.0/2.101.0 qualification. Windows/Linux and ordinary test compositions
  offer no CLI entry. Native HTTPS remains the default. RFC-BPR-0015 is
  Auto-Verified (BPR-0788, 2026-09-23).
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
  fallback for token bytes. Sign out exact-deletes the current access and
  refresh slots and disables the remembered binding. Orderly shutdown retains
  a settled device grant and deletes manual sessions; it never enumerates or
  broadly deletes credentials.
- Prefer the recorded public fixtures and loopback native-transport fixtures
  for operator-safe verification. They do not contact or mutate GitHub: the
  recorded paths substitute transport and vault state, while loopback cases
  exercise bounded read-only REST `GET` and GraphQL `query` admission.
- An abrupt crash during an incomplete grant can leave an unresumable vault
  item. BPR-0541 records settled device bindings, but adds no recovery scan or
  broad credential deletion. Its composed restart evidence uses a runtime-only
  fake vault and reopened SQLite; separate-process evidence covers metadata
  only. It does not establish native-vault continuity across OS processes.

### Signing in with a device code

- Device sign-in is a second way to enrol a profile, not a replacement for the
  token field. Manual token entry stays available on every profile, including
  one the device grant already enrolled. Selecting the device path never
  silently does nothing: it produces one typed terminal — authorized, denied,
  expired, disabled, unavailable, cancelled, or failed.
- The registered identity is a GitHub App owned by the GitHub user
  `MikeMarcin`, with device flow and expiring user tokens enabled, an
  any-account installation policy, and the exact approved permission set. Its
  client identifier is public, is compiled into the binary, and carries no
  secret; there is no secret to protect and none to configure.
- **Installation implication.** A GitHub App's user token reaches a repository
  only where the app is installed, so reviewing an organization's pull
  requests through the device path depends on that organization installing it,
  which is an administrator action outside the reviewer's control.
  BetterPullReview does not require that of anyone — the token path remains
  first-class precisely so administrator installation stays optional, which is
  what keeps "requiring repository administrators to install a GitHub App" an
  explicit product non-goal.
- The reviewer never chooses a scope. The requested permissions are compiled
  in and are the minimum the shipped operation catalog justifies; they are not
  operator-editable, because an editable scope set reintroduces the decision
  this path removes.
- Device sign-in is remembered across orderly quits. Access and refresh tokens
  remain in the platform vault; only exact binding/provenance/lifetime metadata
  is written to the private state database. Startup revalidates the profile and
  authenticated account, then schedules renewal from the remaining lifetime.
  Missing, mismatched, backward or expired material requires sign-in. Transient
  renewal failures retry only within valid lifetime and a bounded failure count;
  terminal failure asks for re-enrollment on the sign-in panel (BPR-0541).
- **Sign out** removes both token slots and disables the remembered binding.
  A manual token replaces and erases the prior device grant, stays session-only,
  and cannot renew from an orphan refresh slot. Metadata is tombstoned before
  replacement and becomes resumable only after the new pair settles. Exact
  access deletion precedes refresh deletion and both terminals count through
  shutdown; cleanup failure is reported, never treated as success.
- For direct regression evidence, run
  `ctest --preset dev --output-on-failure -R '^(application_github_host|application_device_authorization_refresh|sqlite_device_binding_storage|application_device_binding_restart|application_device_authorization_refresh_course|application_device_authorization_orphan_course)$'`.
  The restart course reconstructs the application, defers a renewal, clicks
  Sign out, waits for the next startup load to finish, and checks manual-token
  replacement. The older two-rotation/orphan courses deliberately keep their
  session-only fixture policy and its final erasure expectations.
- Opening a browser can fail, and that is not an authorization failure. The
  code and the address stay on screen and the flow completes on whatever
  device the reviewer finishes it on. An address that does not sit under the
  compiled authority is refused and no browser is opened for it.
- GHES profiles report device sign-in as unavailable and keep the token path.
  The composed capability names the GitHub.com profile it was built for, so a
  begin for a GHES profile — from the mounted panel or from the public-preview
  sign-in — is refused before any transport, vault, or enrollment call
  (BPR-0535; `application_device_authorization_ghes_refusal`). An enterprise
  instance would need its own registered application, which is an
  enterprise-administration action outside RFC-BPR-0016.
- Do not work around an unavailable device path by pasting the client
  identifier, editing an endpoint, or supplying a verification address by
  hand. None of those is an input the application accepts, and the refusals
  that reject them are the point of the design.
- Target-native device-flow courses run on the stations rather than here; see
  [remote stations](remote-stations.md) for the hosts, lanes, launcher, and
  the supported Windows procedure.

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

## Public-preview operation and evidence

The shipped macOS selection enables Public preview beside Connect for the
selected native GitHub.com profile. From fresh start, activation is network-free
and credential-free; entering a public target then uses the normal bounded REST
pager and credential-free smart HTTP. Sign in switches to the existing device
flow. Linux/Windows keep typed preview unavailability; GHES is refused. The mode
provides metadata, diffs, and REST comment groups, with no viewer, GraphQL
threads/resolution, viewed state, triage, private access, or publishing.

`--self-test --public-preview-probe --automated-run-budget-ms 8000` reports the
shipped selection with zero requests and works in tests-disabled Release. Add
`--ghes-host ghe.example.invalid` to check the zero-request GHES refusal. The
platform course selects its own expected mode, verifies its inventory before
running the focused tests, and checks the Release artifact's test-seam absence,
composed hosts, runtime, and dependencies. See the
[RFC-0017 command ledger](../verification/RFC-BPR-0017.md) for exact executed
commands and platform-specific evidence.

The testing-only live-candidate flag forces production anonymous factories for
an explicitly named public target. Its shared nine-request cap includes pages
and replacement transports, and the first wire failure suppresses later sends.
Nine operation kinds do not imply nine production HTTP requests. A dry run
proves wiring only; a completed candidate requires an accepted public snapshot,
head, and observed budget. Do not repeat an attended service read from a stale
packet or treat a fixture or official service description as executed evidence.

## Basic local diagnostics

BPR-0602 uses pinned Quill v13.0.0 through a dedicated logging adapter. The
provider verifies the archive hash and size, stages its headers, and does not
execute upstream tests, examples or install rules. The core-only graph does not
acquire Quill. License and bundled fmt notices are in `third_party/quill/`.

The focused native recovery courses are `logging_session_retention` and
`logging_crash_recovery`. The latter kills a live child after a confirmed flush,
then verifies fresh session ownership and preservation of the terminated session.
It makes no promise about unflushed records. On macOS, run actual disk exhaustion
outside the ordinary test sandbox with:

```sh
python3 tests/logging/verify_logging_disk_full.py build/dev/tests/logging/session_retention
```

That course creates and mounts its own 32 MiB HFS+ image, verifies that the mount
is a different device, observes ENOSPC, checks logger unavailability and restart
after freeing space, then detaches and removes the image. It never fills the host
filesystem. `application_logging_capability-failure` drives a recorded failed
sign-in through the public composition and reads its correlated sanitized error
from the actual file; it contacts no live service.

The Mac app writes separate session segments beneath:

```text
~/Library/Application Support/BetterPullReview/BetterPullReview/logs/
```

Read it directly while diagnosing a run; UI error copying is unnecessary for
these events:

```sh
tail -n 100 "$HOME/Library/Application Support/BetterPullReview/BetterPullReview/logs/"session-*.jsonl
```

BPR-1217 writes one version-three JSON object per line, extending BPR-1215. `event` is the closed
operation/outcome name, with a separate category and `info`, `warning` or `error`
severity. `utc_ms` is milliseconds since the Unix epoch in UTC; `elapsed_us` is
monotonic time since that logger was created. A fresh opaque 128-bit `session`
and increasing `sequence` distinguish launches and profile loggers; join
`session`, `window_label` and `request_label` to correlate operations. Under the
default Anonymized choice, raw `window_session` and `request` are zero. Include
locally preserves those numeric identities after explicit consent. Labels are
stable within the current capture generation; mode changes and Sign out reset
the mapping. The numeric label counter is not reused within a session. Cancellation
is informational. Failed outcomes are errors; other HTTP 4xx/5xx observations
are warnings. `build` contains the application version, a source/toolchain
content digest and configuration. It is diagnostic identity, not a signed
package or authority proof. The digest covers first-party C++ sources, headers
and source CMake files, compiler/platform facts and the version; it excludes
resources, dependencies, build flags and package bytes.

The adapter encodes into a fixed buffer before Quill receives the event. The
encoded record including its newline is at most 16 KiB; summary storage remains
2,048 bytes. Quotes and backslashes are escaped, while control/non-ASCII summary
bytes are refused. This framing guard does not sanitize provider content; the
credential-owning BPR-0971 classifier still decides what may enter the port.
Existing plaintext records remain in place. The maintenance preview recognizes
their v1 prefix and canonical v2/v3 JSONL. The legacy metadata API omits all
prose/identities; BPR-1218's shipped rich preview reclassifies known explanations
and remaps joins, with separately confirmed identifying export.

`github.pull_request` entries identify each load by the typed window/request
labels. Verbose adds a `started` entry for each provider operation, before
`response_received` entries with the
closed `page_operation` name and HTTP status; the final `succeeded` or `failed`
entry gives the last attempted operation and any typed `failure`. Transport
failures may have no HTTP response entry. These fields contain no URL, path,
login, body or token; the separate sanitized capability detail policy remains
unchanged.

The BPR-0559 attended log identified `INTROSPECTION_LIMIT_EXCEEDED` after
successful viewer observation. Capability discovery now uses nine canonical
serial requests per uncached discovery, retaining one logical request identity.
Several capability response records with that identity are expected. The
`github_contract_fixture_security`, `github_capability_probe`, `github_provider`,
`native_https_transport_fixture`, and `application_logging` selectors cover the
repetition-limit repair from document expansion through composed session startup.

During an approved attended sign-in, record the log's starting byte offset and
monitor new sanitized records for completion. The operator should not need to
send a completion message. Observe the app after the terminal event, when device
fields are cleared, then explicitly Sign out. After quitting, verify process
absence and the shutdown record without calling an app UI handle or getter:
those inspection calls can relaunch the app, as the BPR-0605 course demonstrated.

Before selecting an attended app UI, identify the launched PID and its exact
executable path under the approved mount. Select its full `.app` path, never
its display name or a previously cached ordinary-build handle. Immediately
check that the same PID/path is still the sole BetterPullReview instance before
requesting a device code or performing any other action. BPR-0256 v008 showed
that a name-based getter can launch an ordinary build even while the approved
mounted build is already running, separating UI observations from the log being
monitored. A second instance or identity mismatch aborts the course before
sign-in; explicitly Sign out and close any unintended instance during recovery.

Start the ordinary device-code/browser controls promptly after verified UI
selection, leaving only browser authorization to the operator. While device
fields may contain a code, use accessibility observations with emission disabled
and return only non-secret control/status lines. Do not screenshot that panel.
A missing accessibility Value does not establish that a rendered device field
is empty: v009 retained visible code text after terminal sign-in. Hide the
device panel or frame only the account region before taking an account
screenshot. The initial app getter itself emits accessibility state, so bind
before starting device authorization. Confirm the intended account on the same
verified instance before proceeding. An apparently connected UI without matching log
progress calls for process/path correlation, not another sign-in or launch.

The file contains UTC timestamps, application startup/shutdown, GitHub viewer
and capability outcomes, opaque request/window labels, capability HTTP status and a
classified error summary. BPR-0971 reconstructs schema-field, missing-field/type
and repetition-limit explanations, even with an unfamiliar code. Symbols must
occur in the compiled public capability queries; known codes retain their fixed
vocabulary. Unclassified codes, symbols and prose are explicitly omitted.
Filtering happens while the native adapter still owns the active
credential, before any summary reaches Quill. Authentication bodies, headers,
request URLs and source/review bodies are excluded. Local prose can still name
contextual service information. The legacy metadata export omits that prose;
the rich preview retains only reclassified closed explanations. Typed request/window identity privacy now follows the independent capture
choice; older retained records are not rewritten. See the
[diagnostic controls guide](storage-and-diagnostics.md#diagnostic-controls) for
persisted settings, barriers, bounded context and visible failures.

Epoch preparation also records fixed `epoch_stage`, `failure_domain`, `failure`
and optional `pack_failure` names. These distinguish catalog/cache access,
transport acquisition, protocol parsing and revision preparation without
recording Git stderr, paths, repository names or wire bodies. A stage success
means that effect returned successfully; it does not establish an accepted
comparison. Nested catalog recovery currently retains its mapped core reason,
so its earlier adapter detail may already be unavailable. Logging refusal or
backpressure does not change the history result.

The application explicitly owns its terminal diagnostics before Qt startup.
Records have at most 1,968 content bytes plus a bounded loss prefix in each
2 KiB slot; the queue has 64 slots and uses nonblocking admission. Loss counts
saturate and appear on the next admitted record. Ordinary close allows 20 ms
for delivery, then cancels the owned OS writer and joins it. Unavailable output
can lose records, including its loss report; kernel/device stalls have no hard
deadline guarantee. POSIX signal disposition is restored after join, and inherited
descriptor flags are unchanged. Windows uses synchronous-I/O cancellation on its
owned thread; native Windows/Linux verification remains deferred.

Workspace recovery reports a fixed refusal without its path. Startup causes
retain the fixed known composition explanation or an explicit unclassified
exception omission, with first-failure ownership preserved. Qt messages retain
only severity and an omission marker. Typed epoch Quill events replace the old
developer stderr bypass. Git's bounded process detail and agent stderr remain
in their owning result objects, never diagnostic storage/export. The standalone
MCP helper separately caps its content-free startup record at 512 bytes and
owns/cancels/joins its writer. See the
[BPR-0971 completion evidence](../verification/bpr0971-completion.md).

BPR-1216 writes owner-only `session-<session>-<segment>.jsonl` files and
coordinates retention with a persistent private `.retention.lock`. Defaults are
5 MiB per new segment, 50 MiB per profile, 64 files and 30 days since the last
write for inactive history. Startup and backend writes prune inactive files;
active current files are never removed by another session. Completed segments
can be reclaimed under pressure. A bounded directory inventory refuses unknown,
linked, shared-permission or replaced entries. The native transaction uses
nonblocking locks on writes, counting contention as loss; startup makes at most
100 one-millisecond retry waits before reporting busy. Native writes have no
userspace stdio buffer; partial writes report unavailable and remain separate
from the next session. No filesystem latency or crash-durability guarantee is
made. See [storage and diagnostics](storage-and-diagnostics.md) for scope,
legacy compatibility, preview bounds and the retained coordination lock.
Mac native fixtures cover these changes. Windows and Linux native retention
qualification remains pending in RFC-BPR-0027's final milestone.

Summary storage is capped at 2,048 bytes. Each admitted producer has a dropping
64 KiB queue, with at most 16 admitted thread identities and bounded backend
transit buffers. The backend writes each admitted event directly; ordinary shutdown drains after
producer quiescence. This is not crash durability
or a hard filesystem-latency guarantee. Full/unavailable/drop states are exposed
through the typed logging port. Storage & privacy now offers bounded preview,
local export, clear and profile purge through a separate typed maintenance host.
The shipped export rebuilds classified records under an explicit privacy policy,
with fresh joins and optional recent context. It saves the exact preview locally.
Follow the
[storage and diagnostics guide](storage-and-diagnostics.md) for exact labels,
limits, stopped-profile behavior and failure/recovery. RFC-BPR-0027 M1–M4 now
supply structured session retention, controls and export. Native, packaged and
attended acceptance remain open; no complete crash-redaction claim is made.

Focused checks:

```sh
ctest --preset dev --output-on-failure -R '^(native_https_transport_contract|native_https_transport_fixture|github_provider|github_capability_probe|application_logging|quill_logging|quill_logging_backpressure)$'
```

## Whole-review JSON dependency

BPR-0610 uses pinned Glaze v8.3.0 for whole-review request serialization and
typed response parsing inside the GitHub adapter. The central provider checks
archive SHA-256 and size, supports the ordinary offline dependency cache, and
exposes only headers; it does not execute upstream build or install rules.
License and provenance are under `third_party/glaze/`. Core-only builds do not
acquire Glaze. Existing GitHub Qt JSON parsers are unchanged in this slice.

## Bundled code font

[[BPR-1270]]. Qt graphs embed four faces of JetBrains Mono NL (Regular, Bold,
Italic, BoldItalic), the default code font on every platform, from the official
JetBrains Mono v2.304 `JetBrainsMono-2.304.zip` release asset.
`cmake/BprCodeFontProvider.cmake` checks the archive's size and SHA-256 and each
face's SHA-256, and refuses an archive whose `OFL.txt` or `AUTHORS.txt` differs
from the copies retained in `third_party/jetbrains-mono/`. An offline configure
needs the archive in the source cache as `JetBrainsMono-2.304.zip`. Core-only
builds do not acquire it.

The composition registers the faces after `QApplication` exists. A fresh
profile's code font is that family; where registration fails it is the
platform's fixed font. A stored appearance record keeps its family. The focused
checks are `qt_bundled_code_font`, `application_code_font` (the real startup,
`--code-font-probe`) and `package_runtime_contract`.

The focused verification is `github_mutation_contract`, including raw malformed
numeric identities and Unicode responses, plus `live_review_mutation_provider`
for the source-line publication path. The whole-review application workflow
uses its separate review-submission contract and recovery path.

## RFC-BPR-0015 guarded CLI publication fixture

Build the tests-disabled macOS app, then prepare the local-only publication
fixture. The fixture invokes a fake `gh` from an isolated home and never
contacts GitHub. Its launcher does not supply consent or infer a UI result
from process exit.

```sh
cmake --build build/app-release --target app_better_pull_review
python3 tests/end_to_end/rfc0015_cli_publish_fixture.py prepare \
  --directory "$PWD/build/rfc0015-cli-publish-fixture" \
  --body 'Synthetic CLI publication fixture.'
python3 tests/end_to_end/rfc0015_cli_publish_fixture.py launch \
  --directory "$PWD/build/rfc0015-cli-publish-fixture" \
  --app "$PWD/build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview" \
  --cmake-cache "$PWD/build/app-release/CMakeCache.txt" --seconds 300
```

In the app, select the GitHub CLI profile and confirm read opt-in, then open
`octo-org/review-repository#17`. Keep the fixture in its initial `positive`
course until the PR and, for inline comments, the file comparison have loaded.
Select an explicit **right-side source line** before editing: a selected
replacement group may prefer the left side and must refuse a root comment.
Then use `control --course hold` for the intended body. Confirm the separate
write prompt, observe the fixture's held `gh api ... POST` process, then use
`control --course published` with the same body before its 15-second hold
expires. Supply the inline comment's `--path` and one-based `--line` to that
published control. The synthetic
published response supports reconciliation but is not evidence of a live
GitHub write. Focused contract checks are `gh_cli_transport`,
`github_provider`, `github_mutation_contract`,
`live_review_mutation_provider`, and `live_review_submission_provider`.

## Local Mac private alpha

Use the `app-release` preset and `app_better_pull_review` target for ordinary
work. The tests-enabled app refuses normal live publication. The Mac release
post-build step seals the completed application bundle with `codesign`; the
linker's executable-only signature was insufficient for the observed Keychain
sign-in course. `BPR_MACOS_CODESIGN_IDENTITY` defaults to `-` (ad-hoc). For
repeated local use, configure an existing stable signing identity once in the
ignored build cache. `security find-identity -v -p codesigning` lists available
identities without exporting keys. No timestamp service or notarization runs.

```sh
cmake --preset app-release -DBPR_MACOS_CODESIGN_IDENTITY="YOUR EXISTING SIGNING IDENTITY"
cmake --build --preset app-release --target app_better_pull_review
codesign --verify --deep --strict build/app-release/src/app/BetterPullReview.app
```

The [private-alpha route](private-alpha.md) defines the intentionally narrow
delivery scope and the live-Enterprise verification deferral.
