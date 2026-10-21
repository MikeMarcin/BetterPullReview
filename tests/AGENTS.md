# Test Instructions

Tests are direct evidence, not naming evidence.

- Core tests instantiate no `QApplication`.
- Host tests use fake/in-memory ports and the manual executor.
- Host lifecycle tests include reentrant projection feedback, bounded FIFO
  generations, stale/duplicate terminals, exact credential deletion during
  profile switches, projection exceptions, and shutdown while every
  provider/vault phase is active.
- Capability-cache tests may exercise the standalone cache API, but production
  host tests must prove a caller-populated cache cannot bypass a live probe.
- Pager/provider fixtures must prove full REST/GraphQL exhaustion, cursor
  progress, aggregate limits, exact authority, resource-scoped rate metadata,
  structured error classification, cancellation, and atomic failure.
- Qt tests cover translation and presentation mechanics without duplicating
  every core rule.
- GitHub Qt tests prove the safe projection boundary, exact stable-identity
  insert/remove/minimal-move/data-change results, persistent indexes, literal
  remote text, command-owned action state, identity invalidation, focus,
  accessibility, minimum layout, and 1.0x/1.25x/2.0x rasters. Headless Linux
  uses Qt's basic Windows style to avoid the offscreen Fusion cached-painter
  fault; native-style UX remains a separate target-native gate.
- End-to-end tests use the real composition root.
- A scratch root in the system temporary directory is made by the test at a
  random name, only if nothing is at that name, owner-private, and removed
  only by the test that made it: `mkdtemp`, `QTemporaryDir`, the SQLite
  contracts' `tests/sqlite_contract/scratch_root.hpp`, or
  `bpr::tests::make_scratch_directory` in `tests/support/scratch_directory.hpp`,
  whose Windows branch both share (BPR-1138).
  Never remove a fixed or predictable temporary path before creating it
  (BPR-1126). Never name a root from the clock, a counter or the process id
  and then use whatever is already at that name. A file that a child process
  writes goes inside such a root, not directly in the temporary directory
  (BPR-1128). A fixture that the application launches cannot be given a
  path, so it writes beside its own executable, which the course copies into
  such a root (BPR-1139). On Windows a root must also carry the access its
  row needs: a row inside the AppContainer needs the container's SID.
- A course's application data root is the `data` child of a temporary
  directory the course has narrowed to owner-private, made by that language's
  helper (`docs/development/build-and-test-reference.md`, "A course's data root sits one
  owned level down"). Every row fails on `AGENT_WORKSPACE_RECOVERY_REFUSED`
  unless it registers `EXPECTS_WORKSPACE_RECOVERY_REFUSAL`.
- A pull request named in the Home bar opens on Overview, which shows no
  comparison and no drawer. A course that reads either states its modality
  and needs through `enter_course_modality`; which modality shows what is
  written once, in `docs/verification/fixture-catalog.md` ("The modality a
  course states").
- Selected diff UI claims require a production-workbench interaction trace and
  a full-window rendered public path through the installed Git/cache actor;
  isolated painter output is insufficient.
- Scale evidence must record factual platform, architecture, Qt, scale, input,
  fixture-size, bounded-work, and elapsed-time context without converting one
  machine's timing into a universal frame-rate guarantee. Run interactive Qt
  fixtures headlessly. Keep timing, native desktop, global-resource and
  unclassified courses serial; only the explicit Windows correctness allowlist
  may overlap under its audited resource locks (BPR-1670).
- Runtime-surface tests inspect the launched process on macOS, Linux, and
  Windows; keep platform queries process-scoped and directly unit-tested.
- Do not name a test identifier `near`, `far`, `small`, `min` or `max`: the
  Windows SDK defines them as macros, and MSVC then fails to compile the file
  (BPR-1156 hit `near` twice). Pixel helpers use `colour_near`.
- Do not call C library functions MSVC deprecates (C4996 is an error under
  `/WX`): `std::getenv`, `std::fopen`, `strcpy`, `sprintf` and similar. Use
  `qEnvironmentVariable`, `QFile`, or `_wfopen_s` behind `_WIN32`
  (BPR-1177 and BPR-1180 broke the Windows build this way).
- Bind range-for elements by reference (`const auto& [a, b]`). GCC's
  `-Werror=range-loop-construct` rejects a by-value copy that Clang accepts,
  so the Mac build does not catch it (BPR-1078 broke the Linux build this way).
- Do not build a struct that has defaulted `std::string` members as a
  designated-initializer temporary that is then copied or moved
  (`x.Field = T{ .A = …, .B = … };`). GCC 15 at `-O3 -Werror` reports
  `maybe-uninitialized` on the defaulted strings, which the Debug and Clang
  builds never show. Default-construct a named local and assign its members
  one by one, then move it (BPR-1297's optimized build hit this in a
  BPR-1085 test).
- Do not hold an iterator, pointer or reference into a widget's model or card
  list across `processEvents`, a menu popup or a refresh; look the item up
  again afterwards. MSVC's checked iterators abort on the stale one, while
  GCC and Clang read it silently (BPR-1255's Windows run hit this).
- Fixtures are sanitized and deterministic; never include credentials or
  private repository content.
- Native credential fixtures use an adapter-owned integration service and
  exact synthetic authority, runtime-only stdin canaries, a resource-locked
  setup/lifecycle/cleanup sequence, fixed secret-free output, and exact
  post-test absence. Never touch a production service, perform broad vault
  cleanup, or claim cross-rebuild cleanup without stable package identity.
- GitHub CLI process fixtures launch only a canonical same-test executable
  from an isolated non-repository directory and compare the complete child
  argument/environment/input contract. Trust probes use loopback-only
  endpoints, an owner-private digest-stable executable copy and temporary
  config, a runtime-random synthetic token, fixed structural observations,
  and exact cleanup. A followed redirect is a recorded configured-CLI
  residual risk under the RFC-BPR-0015 opt-in profile, never a passing
  containment result. Keep the exact observed behavior and platform scope.
- Native HTTPS fixtures use only loopback CONNECT proxies, a disposable CA
  and leaf, runtime-random framed standard-input credentials, fixed output,
  exact authority/SNI/Host/request observations, and bounded teardown. The
  test-only proxy/CA seam must be absent with `BUILD_TESTING=OFF`; its
  candidate-fallback evidence is not system-proxy reachability, public
  provider composition, or permission to contact GitHub. Use uninstrumented
  runs for cancellation/timeout behavior; a dependency trace may isolate one
  trusted-success case because tracing perturbs the fixture's timing.
- Fuzz targets in `tests/fuzz` call the production entry point the
  inventory in `docs/verification/BPR-0970-parser-fuzzing.md` names, through
  the same seam a caller uses; a reader that is only reachable through a
  process or a socket moves to a header the adapter itself calls, never to a
  test-only backdoor. A target asserts the contract's properties, not only
  the absence of a crash, and a fixed finding goes back into the corpus as a
  regression seed.
- A test must fail or visibly disagree when its claimed behavior is false.
- Registering a test `WINDOWS_UNCONTAINED` requires its entry in
  `WINDOWS_UNCONTAINED_TESTS` (`scripts/verify_test_network_isolation.py`) in
  the same commit ([[BPR-1343]]). Off Windows, write
  `"unmeasured (registered on <platform>)"`. `test_network_isolation_manifest`
  accepts that marker off Windows and fails without an entry on every platform;
  Windows keeps failing until the station measures the row contained and
  records its refusal.
