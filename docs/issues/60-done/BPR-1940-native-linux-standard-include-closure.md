---
schema: "implement-rfc.card.v1"
id: "BPR-1940"
title: "Close native Linux compiler and workspace cancellation failures"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"portability","found_by":"v0.4.2 build39 native Ubuntu GCC Release compilation","related_cards":["BPR-1939"],"milestone_plan":{"id":"bpr-1940-linux-include-closure","path":"docs/rfcs/plans/bpr-1940-linux-include-closure/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4efa875f7908a36e6642bc859fb8d1c3bccc0a3e","evidence":{"kind":"observation","detail":"Direct cstdint include, equivalent guarded optional construction and exact-owned workspace pack copying/read-only quoted cache alternate; source/semantic self-review, unchanged strict inventory/guards/budgets and actual Git special-path + cancellation snapshot positive controls."}},{"criterion":"AC2","outcome":"pass","revision":"4efa875f7908a36e6642bc859fb8d1c3bccc0a3e","evidence":{"kind":"observation","detail":"Full native ordinary Release/Debug/tests-off PASS159.62/1362.82/25.28s at db864/6f37 identical src/tests; 67/67/59 feature/flag keys unchanged; first2 and final6 PASS with exact documented runtime path deltas; all initial negatives/raw receipts retained; combined native UI qualification explicitly pending coordinator."}},{"criterion":"AC3","outcome":"pass","revision":"4efa875f7908a36e6642bc859fb8d1c3bccc0a3e","evidence":{"kind":"observation","detail":"Frozen Mac/Windows39 inputs untouched; all native development graphs empty cohort/producer; all owned descendants reaped, native source clean, exact owned runtime root removed; no release/package/publication credit."}},{"criterion":"AC4","outcome":"pass","revision":"4efa875f7908a36e6642bc859fb8d1c3bccc0a3e","evidence":{"kind":"observation","detail":"Authoritative main pushed4efa875f7908a36e6642bc859fb8d1c3bccc0a3e including accepted latestb3e0966; combined actual4/4contract/Tour controls and build PASS; format22/tidy22/architecture/docs/board PASS with source bindings retained. Full release qualification separately pending."}}]
---

# BPR-1940: Native Linux compile closure

## Acceptance

- AC1. Give `defect_identity.hpp` direct `<cstdint>` ownership for its public
  fixed-width integer declarations, preserve its existing `<cstddef>`, and
  correct any further missing standard includes exposed by the full native
  build only at their direct owners. No behavior, flags, assertions or unrelated
  style changes. Under the explicitly approved compiler-closure extension,
  default-initialize `privateRun` and copy `ExplicitPrivateRun` only inside
  `if (!aWritingOnly)`; preserve fallback, request move/construction, guards
  and the single dispatch, with original-warning and semantic controls. Under
  the separately approved cancellation correction, run workspace pack copying
  in its exactly owned object directory with one correctly quoted read-only
  source-cache alternate; preserve inventory, privacy, authority, cleanup,
  containment and all other behavior. Never delete unknown cache entries.
- AC2. Finish complete native Linux Release and Debug development builds with
  keep-going, retaining every initial failure and correction iteration. Bind
  private source revisions/patches, configured features and original compiler,
  optimization, hardening and warning flags; do not stop at a single-TU proof.
  Complete the real wrapper cancellation/retry and six existing semantic
  controls, recording any explicitly approved fixture-storage path deltas.
- AC3. Keep frozen Mac and Windows build39 inputs/output bytes untouched. Any
  explicitly agreed tests-off feature build uses an ordinary unsealed
  development identity; never forge build39, bypass a release proof or claim
  release acceptance for development output.
- AC4. Focused source/compile checks, formatting and documentation pass, then
  integrate the complete bounded corrections into authoritative main.
  Retain full release qualification as a separate coordinator obligation.

## Diagnosis and authorized development boundary

`defect_identity.hpp` declares `std::uint64_t` and `std::uint8_t` without
`<cstdint>`; it already owns `std::size_t` via `<cstddef>`. Native GCC exposes
this missing declaration, then emits cascading aggregate-initializer errors in
the implementation. The declaration header was introduced by the profile
runtime split (`240c2d4d6a`). The initial correction is one direct include; the milestone
also owns only additional missing-standard-include closure found by the full
native compiler.

The coordinator preserves frozen source
`b5dc26e2cb2ca28435806e4f3e3aa72ccc4a2439`, original failure receipts and
accepted Mac/Windows artifacts. The failed Linux checkout may be explicitly
handed over as a private development branch after archival and idle proof.
Until that handback there is no native access. Reused Release/Debug build
caches keep their normal flags and features and run with keep-going. A numbered
release cohort is not reused for changed source; ordinary local configuration
has an empty cohort/producer and build number zero. No package, signer,
publication or cohort allocation is part of this card.


## Initial correction and verification binding

The first source commit `fe05c06dc` adds `<cstdint>` to the declaration header;
its existing `<cstddef>` is retained. The matching implementation compiles on
macOS and focused clang-tidy22/format22 pass. The application source-content
contract correctly rejected the changed header before its binding was updated.
An independent recomputation first reproduces the original ordered 142-file
manifest digest, then proves that only this header changed. Its new digest is
resealed without changing any inventory member, production mutation-branch
pin, assertion or grant. The original composed source contract then passes
(1.93 s). The actual drift failure and before/after manifests are retained in
external `candidate39/implementer1940/`.

Native development is pending full compile closure. The original build39
source and three cache metadata sets were archived by the coordinator before
handing over the rejected Linux checkout. The initial SSH-context launcher
correctly refused the missing D-host storage proof before any source switch or
compiler ran; the maintained guard is not weakened. The exact same driver is
to run through the established Limited interactive context. It creates a
private branch, records the source commit and configured feature/flag choices,
and serially finishes Release/Debug and the supported tests-off graph with
keep-going. Source changes remain unlanded on authoritative main until these
full development builds complete or a concrete out-of-scope error is reported.


## Explicit scope amendment — GCC optional construction

The full keep-going Release graph exposed `window_agent_chat.cpp:742` under
GCC15 `-O3 -Werror=maybe-uninitialized`. Both ternary branches construct a valid
optional, and engaged copies/assignments construct the vector payload; no
source-level uninitialized state was found in that path. A compiler false
positive remains a hypothesis until original-flags negative/positive and
semantic controls support it. The raw diagnostic points to destruction at823
and is retained in `release-observation1.stdout.log`.

After reviewing the owning code path and minimal correction plan, the
coordinator explicitly approved exactly default initialization followed by a
guarded copy for non-writing requests. Fallback, request construction/move,
all guards and the sole dispatch stay unchanged. This supersedes the original
include-only scope for this one additional diagnostic, without authorizing
warning suppression or other cleanup. The live plan retains every original
obligation/exit and adds this explicit check; its amendment receipt is external.
No C++ edit is made until the current three native graphs finish.

The semantic cases are: writing-only always has no private intent and never
asks the fallback; a non-writing explicit intent is copied exactly and takes
precedence; a non-writing empty explicit intent may receive the same fallback
for the same binding/conversation flag; an unavailable/empty fallback stays
empty. Named existing controls will include
`application_agent_invocation_composition_contract` (exact explicit private
sources), `application_agent_chat_composition_contract`,
`application_agent_chat_conversation_contract`,
`application_private_review_native`, `application_review_tour_writing`
(writing-only) and `application_review_round_trip_composition_contract`.
Source inspection and the two-file content-binding review retain the unchanged
fallback and dispatch mechanics; no test replaces those checks with a copy of
the rewritten conditional.


## Complete initial native sweep and approved correction

Private native source `26844c207575eb55789564775a0bbc53fd4da19b` finished all
three ordinary graphs before the optional code was edited: Release exit1
(1067.09 s), Debug exit0 (3014.04 s), tests-off exit1 (589.15 s). Each optimized
failure is exactly `window_agent_chat.cpp`'s recorded optional/vector warning;
there are no additional missing-standard-include diagnostics. All owned
compiler descendants were reaped. The raw terminal logs/receipts and original
flags are retained in external `release-terminal-r1`, `debug-terminal-r1`,
`tests-off-terminal-r1` and `native-r1-final-receipt.json`.

Only after that terminal boundary, the approved default construction and
`if (!aWritingOnly)` copy were applied on the isolated branch. Source review
retains all four cases in `optional-structure-review.md`, including read-only
Tour suppression and unchanged exception/unwind behavior. No fallback, request
move, admission/privacy guard or dispatch was edited. The false-positive
classification remains a hypothesis pending unchanged-flags native positives
and the six existing semantic courses.

The two reviewed app-file hashes are bound by the unchanged ordered 142-file
manifest, independently computed as
`87e6df9b12f9c5859091bfaeee6d2cb0e0b89b6c1bf6de65e11d30723fa12a36`
and confirmed by the maintained CMake digest printer. The full-main and
production mutation-branch pins remain unchanged. The digest printer verifies
nothing by itself and receives no runtime acceptance credit. Both changed TUs
compile/check with Clang locally; clang-format22 and warnings-as-errors
tidy22 pass. The retry will finish the same three normal graphs and run the
six named existing courses once on native Release; no duplicate semantic
matrix is added.

## Corrected native builds and retained runtime failures

Corrected private native source `34d97535d3dfdb6c98eac05dbea022b5568fa6a9`
has exactly the same `src` and `tests` Git trees as isolated source
`c586ebdd3b01a342d6b9f96aeeacfe3b0dd6dd92`. All three full keep-going
development builds pass: Release 67.15 s, Debug 781.52 s, tests-off 35.99 s.
The protected 67/67/59 cache feature/flag keys have no changes, and each graph
retains an empty cohort/producer. The two original optimized failures remain
in the first-sweep receipts; no warning was suppressed.

The six approved native Release controls were run once. Agent chat composition
and conversation pass (21.06/22.51 s). Invocation passes both explicit-private
seed and reopen phases, then its setup-wrapper phase reaches the original
90 s outer limit. Tour writing also reaches its original 90 s limit.
Round-trip fails at its original 40 s internal open-stage budget; private
review fails in seed stage 2 while entering Review, with three workbench
requests outstanding. These are actual failures, not acceptance. The private
course has not reached a private source selection or agent request at that
stage. The exact CTest environment uses the same offscreen backend, Windows
Qt style, sanitized boundary and network sandbox for the passing and failing
courses. Authoritative-main landing remains held pending diagnosis.

One coordinator-approved repeat of the unchanged private-review course adds
only an external, identity-bound read-only process sampler. It reaches the
original Python 110 s limit. Its application and workers are observed blocked
on page I/O and ext4 journal commits; two worker IDs have journal-wait samples
spanning 32.06/30.06 s. These spans do not claim continuous waits. Build and
fixture temporary storage both use the existing D-backed ext4 loop image.
This is concrete evidence of I/O blocking, with no additional source defect
established. A coordinator-approved single diagnostic comparison changes only
the bounded, owned fixture temporary directory to native root ext4, with an
explicit 256 MiB forecast, unchanged C32 GiB reserve and original D guard.
It does not change source, predicates, time limits or durability.

External `native-final-r2`, `private-diagnostic-r3`,
`preserved-cache-review-r2.json` and the runtime checkpoint retain exact
commands, source identities, JUnit and raw outputs. Every command in both
completed iterations has no owned descendants remaining; source and configured
cache bytes remain unchanged after the diagnostic. Neither developer build
nor diagnostic is frozen-source release qualification.

The first actual native-root comparison passes the unchanged private-review
course in 4.63 s, including all three phases. Its only environment delta is
`TMPDIR`/`TMP`/`TEMP` to the newly owned native ext4 root. The original D guard
continues to run; C retains its 32 GiB reserve and the forecast is 256 MiB.
The fixture root peaks at 2,285,568 allocated bytes and is removed only after
the sampler stops and all owned processes are reaped. Source/cache bytes are
unchanged. The earlier R4 operator attempt passed bytes to a GiB argument;
the capacity guard correctly refused before launching any test and removed
its owned root. The corrected R5 receipt records exactly 268,435,456 forecast
bytes. Both attempts remain in external evidence.

This comparison strongly identifies the D-loop fixture I/O as the cause of
the private-review failure; it does not establish that all other failures
have the same cause. Invocation and round-trip also have fixed build-tree
fixture roots that a temporary-directory change alone does not move. Further
controls or operational routing require a bounded explicit decision; no
source or timeout change is inferred from this diagnostic pass.

The reviewed external six-course CTest projection retains all original
properties, sandbox commands, working directories and budgets, with exactly
two physical `BPR_ROOT` argument changes and inherited temporary-directory
changes. Original graph, source trees, cache and static input/binary hashes
are verified before/after. It finishes with five passes: chat composition
1.73 s, conversation 1.62 s, round-trip 2.62 s, Tour writing 9.21 s and private
review 3.96 s. Invocation still passes both explicit-private phases, then
fails its existing `wrapper-setup-ready` predicate after 49.83 s. That failure
is retained in external `six-semantics-r6`; five passes are not six-course
acceptance. No original-registration or release qualification is claimed.

The remaining predicate follows an intentional workspace failure, private-copy
retry, Stop, and a final Check. Its existing output does not distinguish
agent, workspace and UI readiness. The coordinator authorizes bounded
test-only failure facts and one setup-wrapper diagnostic phase, with the same
predicates and budgets, before any product correction. The temporary source
and binary are explicitly diagnostic; original source bytes are restored
deliberately after the probe, and its binary cannot receive uninstrumented
acceptance credit.

## Explicit scope amendment — cancelled pack ownership

The enum-only R9 trace identifies the final retry's first failure as source
cache `validated_inventory` returning `invalid_repository`, before a new
workspace Git command starts. The R10 read-only nofollow metadata sampler
finds a newly created `objects/pack/tmp_pack_*` in that source cache after the
cancelled copy. Existing pack/index identities and config/HEAD/manifest hashes
remain intact. The strict inventory correctly refuses that unknown pack entry.
The original failures and exact metadata remain external; no pattern-based
deletion or inventory relaxation is authorized.

The coordinator explicitly approves changing only the copy branch's
`pack-objects` primary repository to the exactly owned workspace, with cache
objects supplied as one command-scoped read-only alternate through the
existing environment facility. The same tips, exclusions, output basename,
hash verification, timeout, stop token and lifetime guard remain. Git's
temporary pack is then owned by the workspace's existing cleanup. No lasting
cache alternate is written; an existing linked-clone alternate stays as it was.
Single-path Git quoting must retain separators, quotes, controls, backslashes
and UTF-8 as one entry, without broadening inherited environment authority.

The live plan retains the original obligations and adds this bounded owning
correction and focused coverage. Its checked amendment receipt is external
`plan-amendment-r3.json`. Required verification includes the existing real
wrapper Stop/retry negative control, final six controls, meaningful adapter
coverage for source-cache preservation and special paths, affected normal
native graphs, format/tidy/source/documentation checks and authoritative main.
This is source engineering acceptance; full frozen-source native release
qualification and supported final runtime routing remain the coordinator's
separate obligations.

The earlier bare-`argv[0]` Git wrapper hypothesis is not this fix: R8 proves
the synthetic bare invocation refuses, but actual Git 2.53 re-entry uses the
absolute wrapper path and passes. No unrelated wrapper-path patch is adopted.
All R7/R9 temporary source diagnostics were restored; their binaries remain
explicitly diagnostic and receive no uninstrumented acceptance credit.


## Cancellation correction and complete native positive

Source `6f37c4e36107ba9a2cc5a1df551d4a083320b1b2` implements the approved
copy-branch ownership correction. Its adapter-local single-path quoting passes
Git itself using a source-cache path with separators, quotes, backslash,
controls and UTF-8 on POSIX. The test selects its legal separator/space form
on Windows; that corrected-source native platform proof remains with the
coordinator. Existing
borrowed/plain copy and cleanup checks now compare source-cache inventory.
The existing wrapper course snapshots its known owned cache before Stop and
requires exact file metadata and SHA256 equality after the final retry is
ready. The snapshot refuses links and is bounded to 256 entries, 1 MiB per file
and 8 MiB total; it emits no file bodies. Assertions, budgets, operation flow,
strict cache inventory and cleanup ownership remain intact.

Private native source `db8641f9046c3cea7fe3a25974d1b3f84cbfff4e` has exactly
the same `src`/`tests` Git trees as that source:
`35b021b67f71a4bc49d9aeb28f569f55eb7f9682` /
`b00330917d5cadd2e02788f04af30d6b14c0208a`.
All three complete keep-going graphs pass with original compiler, optimization,
hardening and warning flags: Release 159.62 s, Debug 1362.82 s, tests-off 25.28 s.
The protected 67/67/59 cache feature/flag keys are unchanged; all graphs use an
ordinary empty cohort/producer. No diagnostic marker remains in the main or
invocation binary.

The first native controls pass: real wrapper Stop/retry with the new exact
cache snapshot 3.27 s, and the special-source-path borrowed/plain inventory
control 0.29 s. The final six existing semantic controls all pass:

| Existing course | Seconds |
|---|---:|
| `application_agent_chat_composition_contract` | 1.60 |
| `application_agent_chat_conversation_contract` | 1.97 |
| `application_agent_invocation_composition_contract` | 7.14 |
| `application_review_round_trip_composition_contract` | 2.02 |
| `application_review_tour_writing` | 8.99 |
| `application_private_review_native` | 3.82 |

These controls use the explicitly reviewed external CTest projection: all
original properties, environment modifications, working directories,
sandbox/binary commands and timeouts are preserved, except exactly two
physical `BPR_ROOT` arguments and inherited `TMPDIR`/`TMP`/`TEMP` route fixtures
to the owned native-root ext4 directory. Runtime usage stays under 256 MiB,
with the original D storage guard, C32 GiB reserve and root 8 GiB reserve.
Original configured graph and all static source/binary inputs are verified
unchanged before/after. This is engineering evidence with path deltas,
not original-registration or frozen-release qualification credit.

R11's full Release build passed, then its external driver stopped before any
semantic test with a string/Path helper error. Its complete raw receipt remains
in `candidate39/implementer1940/native-final-r11`. After exact source/cache/input
seals and terminal idle proof, R12 corrects only that operator helper and reuses
the completed Release proof. Its raw JUnit, logs, complete source/ownership and
receipt are in `native-final-r12`; `verification-r12-results.json` independently
checks them. Both iterations have no owned descendants remaining. The native
source is clean and the exact owned runtime root is removed. Earlier compile,
runtime, operator and diagnostic negatives remain retained as above.

Local focused real-Git special-path control, warnings-as-errors clang-tidy22
for all three changed TUs, clang-format22, architecture boundaries and the
prior source/optional controls pass. Review finds no source-level uninitialized
optional payload; the equivalent explicit construction passes the original
GCC flags and the real writing/private/fallback compositions without a warning
suppression. These observations support the optimizer false-positive diagnosis
without changing dispatch or admission policy.


## Authoritative integration and acceptance boundary

Accepted main `b3e0966e94f7fad64f0174a6dd84252f86683d52` (BPR-1941, owner
accepted) is reconciled in `4efa875f7908a36e6642bc859fb8d1c3bccc0a3e` and
intentionally pushed to authoritative main. The only merge overlap is the app
manifest pin: the unchanged ordered 142-file inventory independently reproduces
main's `ab83c7ed…`, then binds the combined source as
`51f86c4a1713325a3a779983f1fa4857b073f597b7bd9dbb0d4654f1a0e4de91`.
The maintained printer agrees; full-app and production-mutation pins remain
unchanged. Compared with accepted main, exactly the two reviewed BPR-1940 app
files differ. BPR-1941's source and reseal comments are retained.

The combined local app and three Tour control targets build successfully.
Original registered `core_tour_agent_session`,
`application_tour_agent_conversation`, `qt_tour_side_chat` and
`application_review_round_trip_composition_contract` all pass (4/4, 4.80 s).
Clang-format22 on all 24 changed C++ files, documentation and board validation
pass. External `composition-reseal-r13.json`, `combined-build-r13.log`,
`combined-controls-r13.xml`, `combined-format-r13.log` and
`combined-docs-r13.log` retain the actual observations. Combined `src`/`tests`
trees are `6793870ef44b30314482920e24e536114f9757c4` /
`09d71640ba35fc901b567acd60523e78e59c360f`.

The earlier native three-graph and six-course evidence belongs to the exact
pre-merge fix source; it does not cover BPR-1941's new UI/native composition.
Full frozen-source native qualification, supported full-suite Ubuntu runtime
storage, and all package/signing/trust/publication obligations remain with the
release coordinator. The coordinator authorizes a separate bounded tooling
issue for the missing configured test-runtime-root route; it is not folded
into this completed source correction. No release output was packaged,
signed, allocated or published by this card.
