# Functional Core Instructions

The core owns deterministic decisions, compact interaction state, request
identity/generation, semantic requests and deltas, and reason-coded
diagnostics.

- Use standard/domain C++, plus only the owner-approved pure libraries
  (glaze, utf8proc, zlib, fmt; owner 2026-10-13).
- Before hand-writing a major general-purpose system here (parser, codec,
  compressor, hash, regex or diff engine, Unicode tables, protocol), ask the
  owner whether a suitable library should be allowed instead (root `AGENTS.md`).
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
