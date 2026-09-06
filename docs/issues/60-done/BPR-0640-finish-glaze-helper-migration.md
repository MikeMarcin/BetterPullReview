---
schema: "implement-rfc.card.v1"
id: "BPR-0640"
title: "Finish Glaze test helpers and migration integration"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0639"]
acceptance: [{"criterion":"inventory-and-fixture-fidelity","outcome":"pass","revision":"e80f7a15c1bfcb98c123805d65b44efff5009feb","evidence":{"kind":"observation","detail":"All five remaining C++ Qt JSON helpers use Glaze; fresh src/tests inventory finds only intentional Python boundary guard. Explicit-null/signed/fraction/uint64 literal fixture guard, frozen canonical bodies and raw malformed cases retained; five rebuilt registered isolated helper checks pass."}},{"criterion":"designated-identity-policy","outcome":"pass","revision":"e80f7a15c1bfcb98c123805d65b44efff5009feb","evidence":{"kind":"observation","detail":"Runner compiled only. Local registered parser/canary passes .35s after fixing independent review duplicate-policy counterexamples; exact uint64, mandatory nested fields, last-member semantics, full-input and Unicode checks pass without live reader."}},{"criterion":"migration-integration","outcome":"pass","revision":"e80f7a15c1bfcb98c123805d65b44efff5009feb","evidence":{"kind":"observation","detail":"Final selected integration initially12/13 passed97.90s; stale whole-source fixture hashes traced to e05d0055 and refreshed. Affected round-trip rerun passes50.97s; unchanged passing auth/read/provider/two restart-recovery/GitHub/GHES evidence retained."}},{"criterion":"post-migration-observations","outcome":"pass","revision":"e80f7a15c1bfcb98c123805d65b44efff5009feb","evidence":{"kind":"observation","detail":"Large-review replay11.69s, composed515-file1000-thread benchmark22.90s and loaded-close14.01s pass on M4 Pro Mac Debug. Card records phase timings, exact source, batched input and native footprint4.657/93.938/54.813MiB baseline/preclose/destroyed, within existing160/96MiB budgets."}},{"criterion":"hygiene-and-boundaries","outcome":"pass","revision":"e80f7a15c1bfcb98c123805d65b44efff5009feb","evidence":{"kind":"observation","detail":"Actual architecture gate9.34s and focused positive/hostile target tests2/2 pass. Documentation gate/62 tests, touched C++ formatting and diff checks pass; task logs and consumed fixture outputs removed. No live/production session, credential, Actions, push or paid CI."}}]
---

# Finish Glaze test helpers and migration integration

## Bounded acceptance and ownership

Own the remaining repository-owned C++ test/helper Qt JSON inventory, associated
test target Glaze links and final migration bookkeeping. Replace helpers with
Glaze and retain independently authored raw malformed-input fixtures and literal
canonical-byte expectations. Audit src/tests for all Qt JSON entry points; only
intentional documentation and boundary guards may remain. Run one focused
integration pass covering migrated auth/read/pagination/mutation/recovery and
public composition; refresh affected parser-sensitive evidence before resuming
performance work. Record platform limitations without reopening unrelated RFC
acceptance. No Python/CMake JSON tooling or third-party migration.

Migration anchor: BPR-0612. Migration base `4e6a6013f3589711f614fba7b9c46b8967313796`.
Preserve core/host boundaries and pinned dependency provenance. All execution
is local with synthetic fixtures; no Actions, push, paid CI or production
account/process operations. Full migration acceptance remains on BPR-0612.

## Literal expectations and four fixture readers

Selection `2098957e`; claim `8ab699a1a77a3812d48a06b1021bb4b06fa58192`.
Source `7de8c974f109754889976035c05a241da61f2feb` freezes canonical expectations
and removes all Qt JSON from the read-contract, anonymous-read, provider and
thread-map test files.

`canonical_json_expectations.hpp` contains fifteen literal request bodies:
five read shapes with maximum PR number and all printable cursor/thread bytes,
nine capability queries, and one line comment containing every control byte,
Unicode, quotes and literal backslash-u text. These were captured once from the
existing independent Qt 6.10.3 writer oracles while all three registered isolated
contract tests passed (3/3, 5.05s). The five read bodies agreed between both host
contracts; repeated capability bodies also agreed. Runtime expectations no
longer use either Qt or Glaze serialization and now bind the complete query
body as well as its JSON framing. All temporary oracle instrumentation was
removed before the source commit.

The read-contract loader uses private typed manifest records and checked Glaze
query extraction/rewriting. Manifest method/catalog checks and source response
bytes remain unchanged. Anonymous provenance/capture records retain exact body
and capture-file digest comparisons, live/derived counts and synthetic-case
provenance checks. Provider capability-part rewriting owns raw Glaze values,
selects exact aliases and leaves invalid/error envelopes for the production
parser to reject. Thread-map fixture records retain the constructed-provenance
assertion, parent IDs, optional lines and named-case topology. Each affected test
target links pinned Glaze explicitly; production code is unchanged.

Local Mac evidence:

- All six affected test targets build.
- Registered isolated selection passes 6/6 in 31.69s:
  `qt_github_thread_map_scale_100` 0.93s, `github_read_contract` 0.62s,
  `github_anonymous_read_contract` 0.46s, `github_mutation_contract` 0.33s,
  `github_provider` 20.82s and `github_capability_probe` 8.51s.
- After migrating the final read-contract fixture loader/query helpers, its
  rebuilt `github_read_contract` passes in 0.57s (0.58s total). The other five
  checks' relevant inputs are unchanged and their passing evidence is retained.
- Documentation gate, touched C++ format check and `git diff --check` pass.
- Fresh src/tests inventory leaves five C++ Qt JSON files: mutation, capability,
  device authorization and paged-read contract tests, plus the designated-live
  mutation runner. The Python architecture boundary guard is intentional.

BPR-0640 remains In Progress. Complete those five helper migrations, preserve
the new literal expectations and existing raw hostile/numeric cases, then run
the final focused migration integration and refresh affected parser-sensitive
evidence before closing BPR-0612 or resuming performance work. The designated
runner may be compiled and checked with local fixtures; no live invocation is
authorized by this migration.

All runs were local synthetic fixtures behind registered isolation wrappers.
No production session, credential, live GitHub, Actions, push or paid CI operation
ran. Consumed canonical capture/build logs, temporary source backups and migration
script, and the thread-map output image were removed. Reusable build trees remain.

## Final helper migration and integration

Source `e80f7a15c1bfcb98c123805d65b44efff5009feb` removes Qt JSON from all five remaining C++ helper
files. The shared Glaze fixture helper preserves explicit nulls, signed numbers,
fractions and full-width unsigned integers; a literal byte assertion guards
those distinctions. Malformed syntax, hostile Unicode and precision-boundary
tokens still bypass fixture serialization, and the fifteen canonical request
expectations remain fixed literals.

Mutation fixtures retain exact source/author/review identity and recovery cases.
Device fixtures retain recorded versus documented provenance, original digest
checks, observed-field inventories and synthetic transient substitutions.
Capability fixtures retain full schema counts, digest checks, reordered arrays,
semantic fault injection and all nine parts. Pager fixtures retain resource,
authority, continuation, rate, atomic-failure and exact-identity cases.

The designated runner's identity boundary uses owned Glaze raw objects with
required nested fields and exact typed uint64 values. Selecting final object
members before scalar decoding preserves the prior last-member policy even
when an earlier duplicate had an incompatible scalar or incomplete object. Its parser is separately
callable from a local fixture without constructing the live reader. Tests cover
every output field, UINT64_MAX, an ID above 2^53, fraction/overflow/type/missing
refusal, last duplicate member (including overwritten null/incomplete values), Unicode, malformed skipped values and trailing
input. The runner was compiled but never launched; the registered isolated
runner-contract test invokes only the local parser and existing fixed canary
fixtures. Its exact architecture link inventory now includes pinned Glaze
privately, with the positive and hostile composition checks retained.

Final source inventory: `rg -l 'QJson|fromJson\(|toJson\(' src tests` finds
only the intentional Python architecture guard. No repository-owned C++ Qt JSON
usage remains. The core/public ports and pinned dependency provenance are
unchanged.

### Local verification

All affected targets and final integration targets build. Focused registered
isolated checks pass: mutation 0.56s, device authorization 0.40s, capability
5.86s, pager 2.18s, and the final local runner parser/canary 0.35s (0.37s total).

The integration selection initially passed 12/13 in 97.90s. Its sole failure was
the round-trip fixture's stale whole-application source fingerprint, before
runtime. Comparing against its last binding at `5590d3ff` identified only the
already reviewed BPR-0639 Glaze application/capture migration `e05d0055`.
The two whole-source/manifest bindings were refreshed; the production mutation
branch binding is unchanged. The affected registered
`application_review_round_trip_composition_contract` then passes in 50.97s.

Unchanged passing integration evidence from that selection:

- Read contract 0.17s; anonymous read 0.09s; provider 10.25s.
- Line-comment restart recovery 0.51s; whole-review restart recovery 0.79s.
- Registered device course 5.68s; refresh course 7.93s.
- GitHub.com composed path 12.03s; GHES 3.18 composed path 11.76s.
- Large-review replay across the stored heads 11.69s.
- Composed large-review benchmark 22.90s; loaded-close resource course 14.01s.

A bounded independent source review identified the overwritten-invalid-member
case in the first typed runner parser. The raw-object selection above fixes it;
the local regression includes both concrete counterexamples. No other findings
were reported. The fix is verified on the affected parser/canary lane only.

The actual architecture gate passes (9.34s); its focused positive/hostile
designated target tests pass 2/2. Documentation gate and its 62 tests, touched
C++ format check and `git diff --check` pass.

### Refreshed parser-sensitive observations

Same M4 Pro Mac, 48 GiB, 14 logical processors, macOS 26.6.2 arm64, Qt 6.10.3,
Debug/offscreen Fusion at 1.0 scale and 1600x1000. The unchanged large-review-v1
workload has 515 files, 1,000 threads and 1,000 comments, with 29 fixture account
requests. This is a local synthetic measurement, not a live rate-limit claim.

The composed benchmark measures paged review/Git load at 10,760.4ms, large-file
selection at 1,984.4ms, and 32 rapid selections at 8,307.2ms. It verifies exact
large/final source, 31 selections with outstanding diff work, and 1,000 wheel
events without scroll index adoption, geometry rebuilding or pane measurement.
Maximum phase UI timer interval is 69.42ms; maximum batched measurement input
interval is 6.22ms. These remain observed Debug timings, not portable guarantees.

Loaded-close retains both 20,001-block documents until close, then drains jobs
and cache and releases raw diff/index/window/documents by destruction while
preserving epoch storage. Physical footprint is 4.657 MiB before application,
48.141 MiB after review load, 93.938 MiB before close, 93.235 MiB after drain and
54.813 MiB after destruction. Existing 160/96 MiB growth/cleanup budgets pass.
Allocator/native retention is distinct from object release.

All runs use local synthetic fixtures and registered isolation wrappers. No
production account/session, credential, live GitHub, Actions, push or paid CI
operation ran. Temporary build/test logs and consumed benchmark JSON/images
were removed; reusable build trees remain. BPR-0612 can now close from these
slices. Broader RFC, attended and Windows/Linux acceptance remains separate.
