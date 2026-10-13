---
id: "RFC-BPR-0080"
title: "Evaluate and replace the hand-written SHA-1 and SHA-256"
status: "In Progress"
depends_on: []
area: "BPR"
metadata: {"mandate_revision":"hashing-replacement-r3","rfc_base_sha":"5620a9f15a391dde7750112294f7562f2f64a32f","source_draft":"RFC-DRAFT-0020","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"}}
---

# Evaluate and replace the hand-written SHA-1 and SHA-256

## Adoption

Adopted on 2026-10-13 under mandate `hashing-replacement-r1` from
RFC-DRAFT-0020. Immutable RFC_BASE_SHA: `5620a9f15a391dde7750112294f7562f2f64a32f`. The
[live import](assets/hashing-replacement-v1.LIVE_IMPORT.md) retains the
original draft and all seven criteria in [[BPR-1446]] to [[BPR-1453]].

Owner answers on record at adoption: on 2026-10-13 the owner ruled that core
may link only owner-approved, pinned, pure libraries (glaze, utf8proc, zlib
and fmt). Q1, Q2 and Q3 below were unanswered at adoption; the owner answered
Q1 and Q2 later the same day (below).

## Owner answers and amendment — 2026-10-13

Mandate `hashing-replacement-r2`. The owner answered, as relayed by the
coordinating session on 2026-10-13:

- **Q1, Accept:** add sha1collisiondetection (MIT) to core's approved libraries.
- **Q2, Accept:** add HACL*'s generated SHA-2 C (MIT) to core's approved libraries.
- **Amendment, Accept:** the collision requirement is met by detection proven
  on raw byte streams (SHAttered and SHA-mbles), plus the same library at every
  object-ID construction, verification and publication path, shown by code
  inspection and test. A Git-header collision vector is not required.

Q3 is not asked: the Linux backend has not been measured. The amendment
changes the collision-input evaluation, the sha1dc decision rule and AC03,
each marked below; the [amendment record](assets/hashing-replacement-v1/proposed-owner-amendment-2026-10-13.md)
keeps the text before and after. Nothing else changes: in particular the
throughput conditions still bind both adopt rules (the idle machine was later
waived for macOS; see the owner waiver below), and an Accept approves a library for core without being an adopt
decision. Core's link allow-list and the `AGENTS.md` rule now name both
libraries.

## Owner waiver — 2026-10-13

Mandate `hashing-replacement-r3`. The owner answered on 2026-10-13, as relayed
by the coordinating session: **waive the idle-machine condition, Accept** —
the loaded macOS speed measurements are accepted for the adopt rules. The
waiver covers the macOS arm64 throughput rows [[BPR-1446]] measured on a loaded
machine; it covers load, not missing rows, and it does not change the ratio
thresholds, the Windows and Linux stations' conditions, or AC05's
before-and-after rows. The [waiver record](assets/hashing-replacement-v1/proposed-owner-waiver-2026-10-13.md)
keeps the text before and after.

## Progress, 2026-10-13

The macOS arm64 evaluation ([[BPR-1446]]) is recorded in the
[hash evaluation report](../verification/RFC-BPR-0080.md): sha1dc is eligible
pending Q1, HACL* SHA-2 eligible pending Q2, CommonCrypto passes the host
port's throughput condition with the site decision left to [[BPR-1447]], and
`QCryptographicHash` stays rejected for identities. The Mac was shared with
other builds, so the throughput was measured in thread CPU time rather than on
an idle machine; [[BPR-1446]] stays blocked until an idle re-measurement. The
public collision pairs neither collide nor are detected when hashed as Git
objects; the owner's amendment above settles AC03 without a Git-framed
collision vector.
Windows and Linux have not run. Nothing is adopted.

## Problem and grounded current behavior

On 2026-10-13 the owner asked to "identify a suitable replacement and author a
rfc to evaluate" for the hand-written hashing. The same day the owner ruled that
the core may link only owner-approved, pinned, pure libraries (no I/O, threads,
clocks or globals): glaze, utf8proc, zlib and fmt. A hash is listed among the
systems to ask about before hand-writing one, and the day's audit named
"hand-written hashes that libraries should have supplied"
([root `AGENTS.md`](../../AGENTS.md), [core `AGENTS.md`](../../src/core/AGENTS.md),
commit `fc1d1ae5d`). This RFC proposes a measured evaluation and, where its
rules are met, adoption. It does not claim any result.

**Code.** [`sha1.cpp`](../../src/core/src/sha1.cpp) (97 lines) is plain
SHA-1 and feeds every byte through `push()` (`update`, lines 9–15).
[`sha256.cpp`](../../src/core/src/sha256.cpp) (191 lines) has hashed
whole blocks since
[BPR-1358](../issues/60-done/BPR-1358-the-core-sha-256-hashes-one-byte-per-call.md).
On an i9-10900KF it went from 219 to 289 MB/s. BPR-1358's coordinator declined
hardware instructions through intrinsics, because after
[BPR-1349](../issues/60-done/BPR-1349-a-feedback-count-refresh-rebuilds-the-whole-snapshot-on-the-window-thread.md)
a refresh no longer hashed. That BPR-1349 work found SHA-256 to be most of a
window-thread feedback refresh (about 110 MB/s, 8 MB of bodies at 10k
conversations). A first observation still hashes every body, in
[`feedback_convention.cpp`](../../src/core/src/feedback_convention.cpp)
`digest_of`. Fifty-four product files across core, application, adapters and
app include the two headers. Adapters and the app also call `core::sha256`, so
one SHA-256 serves the whole product. Nine product files use
`QCryptographicHash`.

**Roles.** Every SHA-256 output below is persisted or seen outside the process,
so a replacement must keep every digest byte-identical (BPR-1358's boundary).

| Role | Algorithm | Exactness | Representative sources |
|---|---|---|---|
| Git object identity: blob, tree and commit, built and checked for Review Cycle publication ([RFC-BPR-0059](RFC-BPR-0059-clean-pr-publication-and-cycle-continuity.md)) | SHA-1 or SHA-256, following the repository's object format | Git-exact | [`git_objects.hpp`](../../src/core/include/bpr/core/git_objects.hpp), `review_cycle_candidate.cpp`, `review_cycle_promotion.cpp`, `review_cycle_publisher.cpp`, `github_publication_session.cpp` |
| Integrity of blobs fetched over the network | SHA-1 only | Git-exact; security-relevant | `blob_matches` in [`epoch_application_host.cpp`](../../src/application/src/epoch_application_host.cpp) (about 5358–5376); `QCryptographicHash` in [`gh_cli_git_smart_transport.cpp`](../../src/adapters/github/src/gh_cli_git_smart_transport.cpp) (about 343 and 477) |
| Pack transfer checksum | SHA-1 or SHA-256 | standard; no collision detection needed | `pack_sink` in [`installed_git_cache.cpp`](../../src/adapters/git/src/installed_git_cache.cpp) (about 2262) |
| Digests other software must reproduce | SHA-256 | standard | GitHub file anchor `pull_request_file_digest` ([`pull_request_location.cpp`](../../src/core/src/pull_request_location.cpp), about 141); Review Cycle bundle object names; update and resource checks through `QCryptographicHash` |
| Content fingerprints and reuse keys | SHA-256; one SHA-1 | internal, persisted | feedback, private-thread and writing digests, preflight; syntax cache key ([`review_cycle_desktop.cpp`](../../src/app/src/review_cycle_desktop.cpp), about 635: a Git SHA-1 identity of the whole document, called from the view's callback) |
| Persistence identities and idempotency keys | SHA-256 | internal, persisted | `epoch_model.cpp`, `epoch_transition.cpp`, `diff_model.cpp`, `review_cycle.cpp`, `github_window_read_host.cpp`, mutation contracts |
| Stored-record integrity | SHA-256 | internal, persisted | SQLite overview, recovery and maintenance stores |

**The SHA-1 gap.** Git uses
[sha1collisiondetection](https://github.com/cr-marcstevens/sha1collisiondetection)
(sha1dc) by default, and this Mac's Git 2.50.1 reports `SHA-1: SHA1_DC`.
[GitHub rejects](https://github.blog/2017-03-20-sha-1-collision-detection-on-github-com/)
content that looks like part of a collision attack. Git builds sha1dc with
`-DSHA1DC_INIT_SAFE_HASH_DEFAULT=0` ([Makefile](https://github.com/git/git/blob/master/Makefile)),
so it outputs plain SHA-1. When sha1dc detects an attack, Git dies with "SHA-1
appears to be part of a collision attack" ([`sha1dc_git.c`](https://github.com/git/git/blob/master/sha1dc_git.c)).
It does not substitute a different digest. Since Git 2.47/2.48, pack checksums
use a faster, non-detecting SHA-1
([Git 2.48 highlights](https://github.blog/open-source/git/highlights-from-git-2-48/)).
BPR checks fetched bytes with plain SHA-1, in the host and again in the
transport. That conflicts with [`sha1.hpp`](../../src/core/include/bpr/core/sha1.hpp)'s
claim that SHA-1 "is never used as a security digest".

**Tests.** `core_sha256` ([`sha256_tests.cpp`](../../tests/core/sha256_tests.cpp))
covers the FIPS 180-4 vectors, 14 boundary lengths, 1 MiB, every split point
of 200 bytes, and uneven chunks. `core_git_objects` ([`git_objects_tests.cpp`](../../tests/core/git_objects_tests.cpp))
has three SHA-1 vectors, a chunking check, and blob, tree and commit
identities matched to installed Git in both formats. No row exercises
collision inputs.

**Boundaries.** The [architecture](../architecture/system-architecture.md#build-target-and-dependency-direction)
still describes the core as "standard/domain C++ only".
[`VerifyCoreBoundary.cmake`](../../cmake/VerifyCoreBoundary.cmake)
forbids OS, clock, thread and I/O APIs, and
[the core target](../../src/core/CMakeLists.txt) links nothing. That
text predates the owner's approved-library list and needs reconciling.
RFC-DRAFT-0018 (the diff-engine evaluation draft)
repeats it. The application target depends inward on core only and may not
use Qt; adapters may use Qt and OS APIs. Release
requires pinned sources in [`packaging/dependencies.json`](../../packaging/dependencies.json),
`third_party/<name>/PROVENANCE.md`, notices, SPDX and the corresponding-source
asset ([release packages](../development/release-packages.md#exact-dependency-and-license-evidence)).
SQLite's [provider](../../cmake/BprSQLiteProvider.cmake) is the pattern
for a verified source archive built as a first-party static target. On Linux,
OpenSSL is already a system dependency through Qt's TLS plugin. The pinned
Qt 6.11.2 has `QT_FEATURE_openssl_hash -1`, so `QCryptographicHash` uses Qt's
bundled SHA-1 and its RFC 6234 SHA-256 ([source](https://github.com/qt/qtbase/blob/dev/src/corelib/tools/qcryptographichash.cpp)).

**Grounding measurement.** These are not acceptance evidence. Apple M4 Pro
(FEAT_SHA1 and FEAT_SHA256 present), macOS 26, Homebrew clang 23.1.1 `-O3`,
a scratch harness over the core sources, best of 5 runs (7 for the bodies row):

| Input | Core SHA-1 | Core SHA-256 | CommonCrypto SHA-1 / SHA-256 | `QCryptographicHash` SHA-1 / SHA-256 |
|---|---|---|---|---|
| 64 MiB, one update | 283 MB/s | 433 MB/s | 3,277 / 3,311 MB/s | 1,178 / 247 MB/s |
| 20,015 bodies of 400 B (BPR-1349's shape) | — | 21.6 ms | — / 4.2 ms | — / 36.6 ms |

`git hash-object` hashed a 256 MiB random blob in 0.38 s of user time, about
700 MB/s through sha1dc. The same blob took 0.64 s in a SHA-256 repository.
The core's blob identities equalled Git's in both object formats.

## Proposed behavior and boundaries

### Recommendation

| Role | Recommended | Why |
|---|---|---|
| SHA-1 Git identity and verification of fetched bytes, in every layer | **sha1dc**, pinned, in its own static C target linked by core, configured as Git configures it | Git's and GitHub's own SHA-1. Pure computation (state in a caller context, constant tables). Expected to be faster than today's byte-at-a-time code. Needs Q1. |
| SHA-256 in core: identities, keys, record digests | **HACL\*** generated `Hacl_Hash_SHA2` C, pinned, in its own static target linked by core | Formally verified portable C, MIT, and what CPython's built-in hashlib uses ([gh-99108](https://github.com/python/cpython/issues/99108)). Same speed class as today. Removes the last first-party hash. Needs Q2. |
| SHA-256 over large content the host already holds | **Platform crypto behind a narrow host port**, adopted per site only if the evidence rules below pass: CommonCrypto (macOS), CNG `BCrypt*` (Windows), system libcrypto EVP (Linux) | Maintained and hardware-accelerated (7.6× the core on the M4 Pro), with no vendored code. OS APIs keep it out of core. |
| Pack checksums | Keep `QCryptographicHash` | Not an identity; Git also uses a non-detecting SHA-1 here. |

Hashing cannot move wholesale behind a host port. Core decisions compute
identities synchronously. `build_git_trees` hashes each tree it builds, the
publication checks recompute commit identities, `epoch_occurrence_id` and
cycle round digests derive identities, and the feedback reuse rule digests
bodies. A port would have to be threaded through those pure functions as a
capability, or the decisions would move into the host. The architecture's
prohibited shortcuts rule out the latter: semantic policy duplicated in host
and core. So whatever hashes for core must be pure code that core links.

Hardware acceleration cannot meet that purity bar. Every maintained
accelerated library examined detects CPU features through the OS or keeps
them in process state:
- Botan queries `getauxval`, `sysctlbyname` and `IsProcessorFeaturePresent`
  and probes instructions.
- OpenSSL keeps a capability global with an environment override.

Acceleration therefore goes to the host, and only where the host already has
the bytes and core needs only the digest. The first candidate is BPR-1349's
feedback bodies: the host would supply each body's digest with the
observation, computed off the window thread. Other candidates are Review
Cycle capture and export of large files, and update archives. Digests are
identical, so persisted values stay valid.

### Candidates considered

| Candidate | License | Acceleration | Fit | Verdict |
|---|---|---|---|---|
| sha1collisiondetection | MIT | none; upstream says it "takes less than twice the amount of time as regular SHA-1" | pure C; Git-exact | SHA-1 |
| HACL\* `Hacl_Hash_SHA2` | MIT (generated C) | none | pure; verified; the streaming API allocates, so the wrapper uses block-level calls or proves the contract | core SHA-256 |
| CommonCrypto / CNG / libcrypto | system | yes where the CPU has it | OS APIs: an adapter only; no collision detection | bulk SHA-256, evidence-gated |
| `QCryptographicHash` | Qt (shipped) | none in the pinned build | Qt is barred from core and application; SHA-256 measured slower than core | keep only the existing non-identity uses |
| Botan 3 | BSD-2-Clause | yes | OS-based CPU detection; adds nothing over platform crypto in the host | reject |
| OpenSSL or BoringSSL vendored subset | Apache-2.0 / ISC-style | yes | perlasm build, process-global state, heavy release burden | reject; Linux uses the system libcrypto through the port |
| Crypto++ | BSL-1.0 | yes | large; runtime CPU probing | reject |
| libsodium | ISC | no | SHA-256 only; library-wide initialization | reject; HACL\* fits better |
| Intel ISA-L crypto | BSD-3-Clause | x86 multi-buffer | job-oriented API, needs nasm | reject |
| Single-file or first-party intrinsics | various / ours | yes | still hand-written code; dispatch needs a global or an OS query; declined in BPR-1358 | reject |
| Linux AF_ALG | kernel | not guaranteed | a socket round trip per digest | reject |
| Keep and optimize first-party | ours | no | no collision detection | the control arm only |

### Owner questions

Each question names the candidate, its license, its layer and its fit, as
[`AGENTS.md`](../../AGENTS.md) requires. They are asked as Accept or
Reject. The SHA-1 question is pending in the coordinating session. A
recommendation is not an answer, and each question's dependent scope stays
unready until it is answered.

- **Q1 (answered Accept, 2026-10-13): add sha1collisiondetection (MIT) to core's approved
  libraries?** It would be its own static C target linked only by
  `better_pull_review_core`, configured as Git builds it. It fits: pure, no
  I/O, threads, clocks or mutable globals, and it is Git's own SHA-1. It does
  not fit: it is C, it is not accelerated, and core-only builds would acquire
  it. *Recommended: Accept.* If rejected, external bytes are verified with
  sha1dc in an adapter behind a host port, and core keeps first-party SHA-1
  for local candidate identities, which Git and GitHub re-check on push. The
  report must state that remaining gap.
- **Q2 (answered Accept, 2026-10-13): add HACL\*'s generated SHA-2 C (MIT) to core's approved
  libraries?** It would be its own static target linked by core. It fits:
  pure, verified, widely vendored. It does not fit: no speed gain, and the
  streaming layer allocates. *Recommended: Accept.* If rejected, core keeps
  its first-party SHA-256.
- **Q3 (unanswered; only if the Linux port backend passes its rule): may the
  Linux build link the system libcrypto directly?** That would make libcrypto
  a load-time requirement and add a package dependency record. Today Qt's TLS
  plugin loads it. *Recommended: Accept if the Linux measurement passes.* If
  rejected, Linux keeps the core path.

### Evaluation

Candidates first run in an opt-in, non-shipping harness target. Pinned,
hash-verified sources are acquired the way the build acquires CPM sources.
Nothing enters `third_party/`, `dependencies.json` or a package before
adoption. The current core implementations are the baseline in every run.
The deliverable is a committed report with raw results and one decision per
component: adopt, reject, or eligible pending an owner answer.

- **Known answers.** NIST CAVP byte-oriented SHA-1 and SHA-256 short and long
  message vectors, the existing `core_sha256` and `core_git_objects` rows,
  boundary lengths, split points and uneven chunking.
- **Differential.** Seeded inputs up to 256 MiB against the baseline,
  CommonCrypto or libcrypto, and `git hash-object` in both object formats
  (`git init --object-format=sha256`). Trees and commits go against
  `mktree`/`write-tree`/`commit-tree`. Every reachable object of this
  repository's history must reproduce its object ID. The about 160
  digest-using rows that BPR-1358 named must pass unchanged.
- **Collision inputs.** The SHAttered pair (their colliding 320-byte
  prefixes, or the PDFs if the fixture policy admits them), the published
  SHA-mbles pair and sha1dc's own test vectors. *Amended 2026-10-13:* each
  must be detected when hashed as a raw byte stream, and core identity
  construction, host blob verification, transport verification and Review
  Cycle publication must each be shown, by code inspection and test, to compute
  SHA-1 with that same detecting library. A Git-header collision vector is not
  required. A plain-SHA-1 control must show the colliding digests equal. The
  repository sweep must detect nothing.
- **Throughput.** Workloads:
  - W1: 64 MiB in one update.
  - W2: 20,015 bodies of 400 B.
  - W3: 1 MiB, 16 MiB (the blob materialization ceiling) and 256 MiB in
    64 KiB updates.

  Conditions follow BPR-1349 and BPR-1358 and the
  [RFC-BPR-0013 convention](RFC-BPR-0013-performance-resilience-resource-limits.md#performance-budgets):
  a Release `-O3` tests-on tree, an idle machine, `-j1`, and medians of 7.
  (*Waived 2026-10-13 for the macOS arm64 rows: the owner accepted BPR-1446's
  measurements on a loaded machine.*)
  Each run records the CPU, its SHA-extension flags (`hw.optional.arm.*`,
  `/proc/cpuinfo` `sha_ni`), the OS and the compiler. macOS arm64 is
  required. The Linux and Windows stations run when available, and a missing
  station is recorded. The Linux station's Comet Lake CPU is expected to lack
  SHA-NI, so it measures fallback paths. The universal build's x86_64 slice
  is checked for correctness only.
- **Window thread.** `application_feedback_counter_costs` first observation
  at 1k and 10k; `application_large_review_responsiveness` and
  `application_first_tab_comparison`; Review Cycle capture and export of a
  16 MiB file; a syntax request for a 16 MiB document. Structural counters
  record the bytes hashed on the window thread, as BPR-1349's
  `bodies_hashed` does.

### Decision rules

- **Correctness gate, every component.** Zero mismatches across the known
  answers, the differential runs, the repository sweep and the digest-using
  rows. One mismatch rejects the component.
- **sha1dc.** Adopt when Q1 is Accepted, the gate passes, every collision
  input is detected on raw byte streams and every listed point is shown by
  code inspection and test to use the same detecting library (*amended
  2026-10-13*), the sweep detects nothing, and W1
  through W3 are at least 1.0× the current core SHA-1 on every measured
  target.
- **HACL\*.** Adopt when Q2 is Accepted, the gate passes, W1 through W3 are at
  least 0.9× the first-party SHA-256 on every measured target, and the
  `noexcept` stream contract holds without per-update allocation.
- **Host port, per site and per platform backend.** Adopt when the gate
  passes and inputs of 1 MiB or more hash at least 3× faster than core on a
  SHA-extension target and at least 0.9× core on a target without them. The
  site must also either cut its window-thread hashing time by at least 10 ms
  at 10k, or move that hashing off the window thread. The Linux backend also
  needs Q3. Otherwise record the rejection and keep core.
- **`QCryptographicHash`.** Identity and bulk roles stay rejected unless the
  measurements contradict the grounding table.

### Adoption

These changes follow only from an adopt decision.
- Every API that yields a SHA-1 Git identity reports detection as a refusal,
  never as a digest: the stream and one-shot functions, `git_object_hasher`,
  `git_blob_identity`, `build_git_trees` and commit identity. The exact
  result types are left to implementation.
- `blob_matches` and the transport's own check both use that function.
- A detected input ends at the existing failure statuses (for example
  `diff_blob_status::failure`, or a refused publication candidate) with a
  reason code that names collision detection.
- The replaced first-party file is deleted, not kept as a fallback; the
  known-answer rows stay.
- Packaging follows the SQLite provider pattern: a pinned archive and
  extracted-source digests, `PROVENANCE.md` and the license, a
  `dependencies.json` entry, the notices, SPDX and the corresponding source.
  Offline configure works, and core-only acquires from the shared cache.
- `architecture_boundaries` admits exactly the approved targets.
- The architecture and core `AGENTS.md` text, the release-readiness inventory
  and `sha1.hpp`'s security wording are updated.
- No persisted data migrates, and no algorithm or comparison revision changes.

Out of scope: changing any digest algorithm, converting between object
formats, cryptographic signing or update trust
([RFC-BPR-0014](RFC-BPR-0014-cross-platform-packaging-security-release.md)),
HMAC, key derivation and randomness.

## Acceptance criteria

- **AC01:** A committed report records raw results, hardware, SHA-extension
  flags and builds, and applies the decision rules. It gives one adopt,
  reject or pending decision per component, and records the owner's answers
  verbatim with dates.
- **AC02:** Each adopted implementation shows zero mismatches across the
  known answers, the differential runs, the full repository object sweep and
  the existing digest-using rows. Data persisted before adoption still loads
  and matches.
- **AC03:** *Amended 2026-10-13.* With sha1dc adopted, the SHAttered and
  SHA-mbles inputs are detected and refused when hashed as raw byte streams
  while the plain-SHA-1 control collides. Code inspection and tests show that
  core identity construction, host blob verification, transport verification
  and Review Cycle publication all compute SHA-1 with that same detecting
  library and treat a detection as a refusal. Ordinary objects, and pack
  checksums of valid packs, are still accepted.
- **AC04:** Measurements on each available target support every adopt and
  reject decision. Each rejection names the failing measurement or the owner
  answer.
- **AC05:** The window-thread rows pass, with before-and-after results and
  counters recorded. No adopted change makes a recorded row worse by more than
  its run-to-run spread.
- **AC06:** `core-only` and `dev` build. The architecture checks admit only
  the approved pure targets in core. Any platform crypto sits in an adapter
  behind the host port.
- **AC07:** For each adopted library, the release inventory, provenance,
  notices, SPDX and corresponding source are complete, and the package audit
  passes on macOS. Unavailable stations are recorded explicitly.
