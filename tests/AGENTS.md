# Test Instructions

Tests are direct evidence, not naming evidence.

- Core tests instantiate no `QApplication`.
- Host tests use fake/in-memory ports and the manual executor.
- Qt tests cover translation and presentation mechanics without duplicating
  every core rule.
- End-to-end tests use the real composition root.
- Runtime-surface tests inspect the launched process on macOS, Linux, and
  Windows; keep platform queries process-scoped and directly unit-tested.
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
  and exact cleanup. A followed redirect is evidence that CLI mode is
  unavailable, never a passing trust result.
- Native HTTPS fixtures use only loopback CONNECT proxies, a disposable CA
  and leaf, runtime-random framed standard-input credentials, fixed output,
  exact authority/SNI/Host/request observations, and bounded teardown. The
  test-only proxy/CA seam must be absent with `BPR_BUILD_TESTS=OFF`; its
  candidate-fallback evidence is not system-proxy reachability, public
  provider composition, or permission to contact GitHub. Use uninstrumented
  runs for cancellation/timeout behavior; a dependency trace may isolate one
  trusted-success case because tracing perturbs the fixture's timing.
- A test must fail or visibly disagree when its claimed behavior is false.
