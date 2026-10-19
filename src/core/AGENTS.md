# Functional Core Instructions

The core owns deterministic decisions, compact interaction state, request
identity/generation, semantic requests and deltas, and reason-coded
diagnostics.

- Use standard/domain C++, plus only the owner-approved pure libraries
  (glaze, utf8proc, zlib, fmt, sha1collisiondetection, HACL* SHA-2,
  rapidfuzz-cpp, and dtl only for per-line intraline comparison and the
  Review Cycle suggestion hunks' line edit bounded at 512 changes; owner
  2026-10-13), plus the owner-approved deterministic platform SHA-256
  implementation (CommonCrypto/CNG with HACL fallback; RFC-BPR-0080 r4,
  2026-10-18). Native APIs and handles stay in its exact private sources.
  Pinned RE2/Abseil are additionally approved for source matching (BPR-1618,
  owner 2026-10-18): RE2 stays private to `source_regex_engine.cpp`; internal
  locks, hash state and contention clocks may affect timing/cache layout but
  must not affect match spans, ordering or diagnostics. No worker creation.
  This grants no general core thread, clock, entropy or logging capability.
- Before hand-writing a major general-purpose system here (parser, codec,
  compressor, hash, regex or diff engine, Unicode tables, protocol), ask the
  owner whether a suitable library should be allowed instead (root `AGENTS.md`).
- Do not include Qt, filesystem, network, database, process, thread, future,
  timer, provider or credential APIs. Apart from the scoped SHA-256 backend,
  OS capabilities remain outside core.
- Duration/time-point arithmetic and caller-owned explicitly seeded PRNG state
  are permitted. Do not read clocks or implicit entropy (`random_device`,
  global C RNGs). Keep the algorithm/mapping specified when random values
  must match across platforms; standard distributions do not promise that.
- Keep nondeterministic facts in explicit bounded observations.
- Reject stale completions by typed identity in constant time.
- Bind repeated diff semantic queries to one validated immutable result; keep
  unindexed public overloads fail closed.
- Bound optional move discovery with explicit deterministic work and segment
  ceilings. Exhaustion must preserve the monotonic primary diff and discard
  partial relation metadata.
- Test every semantic rule without `QApplication`.
- Run the `core-only` preset and both architecture-boundary tests.
