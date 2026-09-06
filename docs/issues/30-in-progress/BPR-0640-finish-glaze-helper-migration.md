---
schema: "implement-rfc.card.v1"
id: "BPR-0640"
title: "Finish Glaze test helpers and migration integration"
status: "In Progress"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0639"]
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
