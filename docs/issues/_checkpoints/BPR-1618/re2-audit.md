# BPR-1618: pinned RE2 implementation audit

Mandate `core-re2-r1`, owner 2026-10-18. Baseline `85d6ed329`.
This records a scoped external-determinism exception, not library purity.

## Sources and configuration

The existing provider verifies archive bytes, SHA-256 and license digests:

| Component | Pin | Archive SHA-256 | License |
| --- | --- | --- | --- |
| RE2 | 2025-11-05 | `87f6029d2f6de8aa023654240a03ada90e876ce9a4676e258dd01ea4c26ffd67` | BSD-3-Clause |
| Abseil | 20250814.2 | `f9148fb00ec98a2396bdf875c99a78e6a70afa662b107862d92b285d857a8320` | Apache-2.0 |

Audited the extracted exact archives and the candidate's verified core-only
build copies. `cmake/BprRe2Provider.cmake` keeps upstream tests, test helpers,
benchmarks, install rules, ICU and PCRE disabled, static libraries and C++23.
No pin, license, archive or vendor source changed. The core-only compilation
database contains 176 production RE2/Abseil units and no test thread-pool unit.

## Threads, synchronization and implementation effects

- RE2's production `re2/` and `util/` sources contain no `std::thread`
  construction, `pthread_create` or `CreateThread`. Its `CMakeLists.txt`
  links Threads for synchronization; that link is not worker creation.
- Abseil's only `std::thread` construction in the audited non-test source/header
  inventory is `synchronization/internal/thread_pool.h`; its CMake target is
  `TESTONLY`, the provider disables test helpers, and RE2 includes no such
  header. `CreateThreadIdentity()` allocates TLS bookkeeping **for the calling
  thread**; it creates no OS thread. `std::thread::hardware_concurrency()` in
  platform CPU detection likewise starts none. Reviewed production source
  creation-call inventory and the compiled dependency closure.
- `re2/dfa.cc` synchronizes per-program DFA caches with `absl::Mutex` and
  `re2/re2.cc`/`prog.h` initialize reusable state with `absl::call_once`.
  Contention executes on the caller: Abseil's `mutex.cc` and `spinlock.cc`
  use `absl::Now()`/`CycleClock::Now()` for contention tuning and diagnostics;
  the platform wait implementations yield or wait/wake existing callers.
  These are real internal clock/OS operations, not a pure-library claim.
- `absl/hash/internal/hash.h::MixingHashState::Seed()` uses ASLR-derived
  address state. `container/internal/raw_hash_set.cc::RandomSeed()` combines
  a TLS/atomic counter with its address, changing table iteration/layout;
  it explicitly avoids abseil-random. Hashtable profiling sampling is disabled
  by the pinned header's configuration. No product identity, result ordering,
  error offset or status uses an Abseil hash value or table traversal order.
  RE2's state caches are lookup structures; ordered instruction transitions
  determine leftmost-first matching independently of buckets.
- Upstream logging machinery can read time and write diagnostics. The unchanged
  wrapper sets UTF-8, `log_errors(false)`, `never_capture(true)`, explicit
  case/literal options and a fixed requested `max_mem`. Expected parse,
  compile, bad-range and DFA-exhaustion diagnostics in `re2/re2.cc` are behind
  `log_errors()`. RE2's invariant failures/ABSL_CHECK/DFATAL diagnostics remain
  possible; they are not claimed side-effect-free or recoverable. The
  unconditional trivial-bytemap diagnostic in `prog.cc` is under `if ((0))`.
  Product code adds no logging or upstream sink/flag configuration.

## Semantic argument and verification boundary

`RE2::Match()` preserves leftmost-first matching when DFA cache allocation
fails: `re2/re2.cc` falls back to OnePass, BitState or NFA with the same
anchor/match kind. No wall-time cutoff, hash seed, worker scheduling or
profiling observation enters the wrapper's options, returned byte spans or
parse-error mapping. Internal contention/cache layout can affect elapsed time,
allocation and when a host cancellation is observed; those were already
implementation effects of the same engine and host-owned cancellation.
Allocation failure can still report the existing resource-limit terminal.
This is not a promise of identical time, memory pressure or cancelled prefixes
under different host schedules, nor a proof against upstream library defects.

BPR-1618 keeps the wrapper's options, error mapping, retained-memory accounting,
scanner/path policy and limits unchanged. The production core-only public
compiler/scanner courses pin Unicode/CRLF/Whole Word/zero-width spans, syntax
refusals, error offsets and resource/cancellation behavior. New regression work
repeats fixed expected matches and diagnostics with fresh compilations, warmed
shared query/path programs, and four test-owned concurrent callers; each caller
also runs the fixed public Unicode/ordering cases. A separate reference course
compares the portable subset to an independent Python oracle. Composed pane/PR
courses verify the factory is supplied through the actual application path.

Only `source_regex_engine.cpp` may include RE2; first-party core cannot include
Abseil directly or link its targets. Existing clock, entropy, thread and effect
prohibitions remain, including within the wrapper. Native Linux/Windows and
full release qualification are outside this relocation and retain their prior
owners; source review covers their wait branches without claiming native runs.
