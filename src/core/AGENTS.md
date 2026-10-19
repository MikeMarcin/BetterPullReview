# Functional Core Instructions

The core owns deterministic decisions, compact interaction state, request
identity/generation, semantic requests and deltas, and reason-coded
diagnostics.

- Apply the [external-determinism contract](../../docs/architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md#21-external-determinism)
  to first-party and library-backed computation. Keep semantic state and inputs
  explicit; pass caller-owned or referenced state rather than retaining hidden
  process-global decision state. Internal timing/layout mechanisms alone do not
  disqualify a dependency.
- Keep Qt and effectful/provider APIs outside core. Supply nondeterministic facts
  as bounded observations; emit typed requests for effects. The host owns
  scheduling, thread creation, cancellation and lifecycle.
- Keep failure/resource, cancellation and cross-platform semantics within each
  operation's stated contract. Lexical paths, chrono arithmetic and explicitly
  seeded state are computation; filesystem access and implicit application time,
  entropy or environment reads remain effects.
- Use only owner-approved dependencies under the [implementation inventory](../../docs/architecture/system-architecture.md#core-implementation-inventory).
  Preserve pins, private wrappers and exact use/link ownership, including dtl's
  bounded uses, SHA-1 collision detection, xdiff's separate dynamic library and
  RE2's no-worker policy. Eligibility under external determinism does not grant
  adoption approval or arbitrary OS capabilities.
- Before hand-writing a major general-purpose system here (parser, codec,
  compressor, hash, regex or diff engine, Unicode tables, protocol), ask the
  owner whether a suitable library should be allowed instead (root `AGENTS.md`).
- Reject stale completions by typed identity in constant time.
- Bind repeated diff semantic queries to one validated immutable result; keep
  unindexed public overloads fail closed.
- Bound optional move discovery with explicit deterministic work and segment
  ceilings. Exhaustion must preserve the monotonic primary diff and discard
  partial relation metadata.
- Test every semantic rule without `QApplication`.
- Run the `core-only` preset and both architecture-boundary tests.
