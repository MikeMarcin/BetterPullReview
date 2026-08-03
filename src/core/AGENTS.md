# Functional Core Instructions

The core owns deterministic decisions, compact interaction state, request
identity/generation, semantic requests and deltas, and reason-coded
diagnostics.

- Use standard/domain C++ only.
- Do not include Qt, filesystem, network, database, process, clock, random,
  thread, future, timer, provider, or credential APIs.
- Keep nondeterministic facts in explicit bounded observations.
- Reject stale completions by typed identity in constant time.
- Bind repeated diff semantic queries to one validated immutable result; keep
  unindexed public overloads fail closed.
- Bound optional move discovery with explicit deterministic work and segment
  ceilings. Exhaustion must preserve the monotonic primary diff and discard
  partial relation metadata.
- Test every semantic rule without `QApplication`.
- Run the `core-only` preset and both architecture-boundary tests.
