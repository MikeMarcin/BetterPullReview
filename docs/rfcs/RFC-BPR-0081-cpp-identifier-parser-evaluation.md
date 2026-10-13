---
id: "RFC-BPR-0081"
title: "Evaluate a mature parser for C++ identifier refinement"
status: "In Progress"
depends_on: ["RFC-BPR-0072"]
metadata: {"rfc_base_sha":"530d92eaff1578413c0e669f476e740387441152","mandate_revision":"cpp-identifier-parser-evaluation-r1","milestone_plan":{"id":"cpp-identifier-parser-evaluation-v1","path":"docs/rfcs/plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json"}}
---

# Evaluate a mature parser for C++ identifier refinement

## Execution status

Mandate `cpp-identifier-parser-evaluation-r1` adopts the original proposal
unchanged. Immutable RFC_BASE_SHA: `530d92eaff1578413c0e669f476e740387441152`. The
[live import](assets/cpp-identifier-parser-evaluation-v1.LIVE_IMPORT.md) and the
[milestone plan](plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json)
retain its scope. Cards: [BPR-1440](../issues/60-done/BPR-1440-commit-the-labelled-cpp-identifier-corpus.md)
(labelled corpus and gap suite),
[BPR-1441](../issues/60-done/BPR-1441-stop-the-cpp-refinement-partway-and-fuzz-the-classifier.md)
(cancellation test and syntax fuzz target),
[BPR-1442](../issues/60-done/BPR-1442-measure-tree-sitter-cpp-on-macos.md)
(harness, pins and macOS measurements),
[BPR-1443](../issues/20-ready/BPR-1443-measure-the-parser-evaluation-on-windows.md)
(Windows x64),
[BPR-1444](../issues/20-ready/BPR-1444-measure-the-parser-evaluation-on-linux.md)
(Linux x64 and the Linux fuzz campaign) and
[BPR-1445](../issues/10-backlog/BPR-1445-close-rfc-0081.md) (closeout).
**Outcome on macOS (2026-10-13): extend.** Candidate T fails the Contract,
Bounded-work and Memory gates at both pins, so it is rejected. The Contract
failure comes from the grammar: tree-sitter-cpp accepts no attribute on an
enumerator. The gap suite confirms H1-H8 on the shipped heuristic.
[BPR-1474](../issues/20-ready/BPR-1474-fix-the-confirmed-cpp-identifier-gap-classes.md)
fixes those classes.

The [report](../verification/RFC-BPR-0081.md) records every gate, its
measurements and the conditions for running the evaluation again. Q1 (may an
untagged tree-sitter-cpp commit be pinned?) is still unanswered, and moot,
because `master` fails the same gates. The Windows and Linux measurements
(AC03) are still owed to BPR-1443 and BPR-1444; they cannot change the outcome.

## Problem and grounded current behavior

On 2026-10-13 the owner asked for an RFC to investigate one item from the
day's library audit. The item calls the C++ identifier refinement "a heuristic
C++ scope parser on top of KSyntaxHighlighting" and adds "tree-sitter-cpp is
the mature option if this grows". The same day the owner limited the core to
pinned, pure, owner-approved libraries (glaze, utf8proc, zlib, fmt) and asked to
be consulted before a parser or similar system is hand-written there
([root `AGENTS.md`](../../AGENTS.md),
[core `AGENTS.md`](../../src/core/AGENTS.md)). The refinement is in an
adapter, so that rule does not bind it, but the audit asks the same question of
it. This RFC proposes a measured evaluation that ends in a recorded decision:
adopt, extend or keep. It changes no shipped behavior and claims no result. It
follows the shape of
RFC-DRAFT-0018, the diff-engine evaluation draft.

**What the refinement does.** [RFC-BPR-0072](RFC-BPR-0072-cpp-identifier-context.md)
(Auto-Verified through
[BPR-1411](../issues/60-done/BPR-1411-recognize-cpp-namespace-and-enum-context.md)
and [BPR-1412](../issues/60-done/BPR-1412-close-rfc-0072.md)) added
[`cpp_identifier_context.cpp`](../../src/adapters/syntax/src/cpp_identifier_context.cpp)
in commit `c58bdebda` on 2026-10-12. The file is 539 lines and has not changed
since. It runs only for the C++ definition. It runs after KSyntaxHighlighting
6.28.1 has produced complete-blob spans, and only when that pass is available
([`packaged_syntax_classifier.cpp`](../../src/adapters/syntax/src/packaged_syntax_classifier.cpp),
`classify_blob`, about 1024–1070). It makes five passes over the whole blob:

1. `prepare_roles` (about 113–155) copies the grammar's role onto every byte and
   protects whole directive lines, including continued lines.
2. `tokenize` (157–183) cuts ASCII identifiers, `::`, `->` and single
   punctuation characters from code-role bytes only. Comments, strings and
   directives therefore supply no token.
3. `match_delimiters` (185–210) pairs `()`, `[]` and `{}`. It also guesses
   template angles: a `<` after an identifier or `>`, when not followed by `=`,
   with the guess cleared at `;`, `{` or `}`.
4. `collect_declarations` (360–415) builds a scope tree. It records namespace
   chains, aliases and `using namespace`. It records enum, class, struct and
   union headers, scanning at most 256 tokens to the next `{` or `;`. It splits
   enumerators at top-level commas, skipping nested parentheses, brackets and
   angles. Local and parameter names that look like declarators
   (`looks_like_declarator`, 319–330) are recorded as shadows.
5. `classify_uses` and `classify_chain` (432–484) resolve qualified chains
   against that tree. Unknown, shadowed, aliased, duplicated and object- or
   template-qualified names stay neutral, and a qualified ALL_CAPS name that is
   not invoked loses its macro role.

`paint` (240–247) overrides only the normal, macro, type and function roles.
`publish` (486–521) cuts the result back into per-line spans, enforces the span
limit and publishes nothing after cancellation or overflow. The C++ definition
version carries identifier revision 6 in its upper half (`record_of`, about
162–173), so spans cached under older rules are keyed away.

**What text it sees.** Only whole texts. The classifier refuses a blob over
8 MiB (`maximum_classified_blob_bytes`) or one that is not valid UTF-8. A hunk
is a projection of the whole-blob result (`classify_hunk`). Four callers supply
text:

| Caller | Text classified |
|---|---|
| PR comparison panes, through the epoch host's blob source ([`github_application.cpp`](../../src/app/src/github_application.cpp), about 8466) | each side's complete blob |
| Review Cycle suggestion composer ([`suggestion_composer.cpp`](../../src/adapters/qt/src/suggestion_composer.cpp) `text_changed`, about 169; [`review_cycle_desktop.cpp`](../../src/app/src/review_cycle_desktop.cpp) `submit_syntax`, about 641) | the whole edited file after each edit; the latest text wins, and at most two are classified at once |
| Review Cycle comparison ([`review_cycle_view.cpp`](../../src/adapters/qt/src/review_cycle_view.cpp), about 1338, [BPR-1281](../issues/60-done/BPR-1281-syntax-colour-in-the-cycle-comparison-views.md)) | each side rebuilt from the full alignment rows |
| Share-sheet note code ([`review_cycle_share_sheet.cpp`](../../src/adapters/qt/src/review_cycle_share_sheet.cpp) `note_code_view`, about 665–750, [BPR-1161](../issues/60-done/BPR-1161-expose-share-routes-and-exact-delivery-observations-in-the-gui.md)) | an excerpt: only the rows shown, with an empty line for each omitted row |

The excerpt is the only fragment the refinement receives. It can begin inside an
enum, a namespace, a comment or a raw string, and it can lose a closing brace.
The other inputs where error recovery matters are malformed or macro-heavy
whole files.

**Threading, cancellation and budgets.** The host posts each classification to
the profile's appearance worker, which is a single thread
([`worker_executor.cpp`](../../src/app/src/worker_executor.cpp)). It keeps
one `std::stop_source` per request
([`syntax_classification_application_host.cpp`](../../src/application/src/syntax_classification_application_host.cpp),
`submit_classification`, about 333–370), and an exception becomes
`unavailable` (`perform_classification`). The grammar pass checks for a stop
every 512 lines (`CancellationLineInterval`). The refinement checks every 4,096
bytes or tokens in its long loops, and on every step elsewhere. The defaults
allow 262,144 spans per blob, 8 outstanding requests and 64 cached span sets
([`appearance_runtime_ports.hpp`](../../src/application/include/bpr/application/appearance_runtime_ports.hpp),
about 24–55). Temporary state grows linearly with the blob. It holds one role
byte per source byte, a 20-byte record per token, and a scope record with its
own hash map for every `{`. Nobody has measured the constant factor.

**Evidence that exists.** `syntax_classification_contract` runs the 98-line
[fixture](../../tests/fixtures/syntax/cpp_identifier_context.cpp) through
the public classifier
([`syntax_classification_contract_tests.cpp`](../../tests/syntax_contract/syntax_classification_contract_tests.cpp),
`cpp_context_contract`, about 1009–1094). It covers RFC-BPR-0072's
declarations, uses, negative controls, protected text, hunk projection, cache
retirement, span exhaustion and cancellation. `application_cpp_identifier_appearance`
renders both panes in Light and Dark from the `cpp-identifiers-v1` pack.
`qt_syntax_pane_composition` covers pane composition and the 8 MiB refusal.

The evidence has gaps:

- The cancellation check requests the stop before classifying, so the grammar
  pass returns at line 0. No test stops the refinement itself.
- `tests/fuzz` has no syntax target.
- The [BPR-1411 checkpoint](../issues/_checkpoints/cpp-identifier-context-v1/README.md)
  records command durations only, so the refinement's time and memory have
  never been measured. `application_large_review_benchmark` times
  `syntax_first_file` and `syntax_navigation` on a 20,000-line C++ comparison
  ([`large_review_benchmark.cpp`](../../tests/end_to_end/large_review_benchmark.cpp),
  about 1063–1115), but it does not time the refinement separately.
- No open card reports a C++ misclassification since BPR-1411.

So the heuristic is unmeasured, not known to be failing.

**Weak points found by reading.** The cases below come from reading the code.
None is tested. They are hypotheses for the evaluation's gap suite, not
reported defects.

| ID | Input | Expected from reading the code |
|---|---|---|
| H1 | `namespace A::inline B {` (C++20) | `inline` is read as part of the namespace chain, the `{` check then fails, and nothing is recognized |
| H2 | `namespace [[deprecated]] N {` | no name directly follows the keyword, so the namespace is not recognized |
| H3 | `enum { A = X < 3, B = Y > 2, C }` | `<` and `>` are paired as a template, which hides `B`, so `B` loses its enumerator role |
| H4 | `enum E { COLOUR_LIST(ENTRY) }` (an X-macro) | the macro invocation is painted as an enumerator and loses its macro role |
| H5 | `#if X` / `enum class E : int {` / `#else` / `enum E {` / `#endif` | both branches open a scope, so the outer enum body never closes; every later name after a top-level comma becomes an "enumerator" |
| H6 | `int Reason{0};` then `Reason::ID` | a brace-initialized name is not recorded as a shadow, though `auto Reason = …` is, so the conservative rule applies inconsistently |
| H7 | `enum E { café }` | a non-ASCII byte ends the token, so only `caf` is painted |
| H8 | 8 MiB of `{`, or 8 MiB of one-letter names | the largest temporary footprint the 8 MiB limit admits |

**The accuracy reference is the accepted contract, not C++.** RFC-BPR-0072 says
that "Ambiguous scope, shadowing or aliases must not propagate a guessed
identity". Its fixture expects `auto hidden = Reason::ID` to stay neutral after
a local `auto Reason`. A compiler disagrees: lookup of a name followed by `::`
"considers only namespaces, types, and templates whose specializations are
types" ([basic.lookup.qual.general](https://eel.is/c++draft/basic.lookup.qual.general)),
so it resolves that use to the enum. A more semantic parser could therefore fail
the contract by being more correct. Changing an accepted expectation is an owner
decision, not an accuracy gain.

**Build and supply.** The [syntax adapter](../../src/adapters/syntax/CMakeLists.txt)
is a static library that links `KF6::SyntaxHighlighting` and Qt Core privately.
It exists only when `BPR_BUILD_QT` is on, so `core-only` acquires neither
([architecture](../architecture/system-architecture.md#rfc-bpr-0006-appearance-allocation)).

RE2 is the most recent library added to an adapter
([BPR-1431](../issues/60-done/BPR-1431-search-with-re2.md)). Its provider,
[`BprRe2Provider.cmake`](../../cmake/BprRe2Provider.cmake), sets the
pattern: a pinned, tagged archive checked by SHA-256, size and licence digest,
re-extracted into the build tree, with offline mode supported and source
substitution forbidden. Its notices and provenance are in [`third_party/`](../../third_party/). It
has an entry in [`packaging/dependencies.json`](../../packaging/dependencies.json)
and in `NOTICE_FILES` in
[`package_runtime.py`](../../packaging/package_runtime.py). Statically
compiled inputs also go into the SPDX file and the separate corresponding-source
asset ([release packages](../development/release-packages.md#exact-dependency-and-license-evidence)).
BPR-1431 was accepted through five dependency checks:
`package_runtime_contract`, `release_security_audit_contract`,
`binary_dependency_audit_contract`, `binary_dependency_surface` and
`dependency_modernization_contract`.

## Proposed behavior and boundaries

### Outcome and scope

The evaluation builds a harness that does not ship. It sits behind a new
`BPR_BUILD_SYNTAX_EVALUATION` option, which is off by default and valid only
with `BPR_BUILD_QT`. The harness compiles each candidate refinement beside the
baseline. Both receive the same KSyntaxHighlighting spans, and both use the
baseline's painting precedence, `publish`, limits and status rules.

No shipped target, package or release script may acquire, link or bundle a
candidate. Pinned sources are acquired the way `BprRe2Provider.cmake` acquires
RE2. Nothing enters `third_party/` or `packaging/dependencies.json` unless the
candidate is adopted.

The deliverable is a committed report with raw results and the labelled corpus.
It records one outcome (adopt, extend or keep) and the condition for running
the evaluation again. Production replacement, the identifier revision bump and
admitting the dependency belong to a follow-up card that cites the report. Two
test additions stay whatever the outcome, because they also test the shipped
classifier: a syntax fuzz target, and a test that stops the refinement partway
through.

### Candidate T: tree-sitter-cpp as a structural front end

KSyntaxHighlighting stays the authority for every role it styles. T replaces
only the refinement's structural guesswork, and resolution within the blob stays
first-party:

| Replaced by tree-sitter | Kept first-party |
|---|---|
| tokenization, and pairing of delimiters and template angles (`tokenize`, `match_delimiters`) | grammar protection: a parser node supplies no fact for bytes the grammar styles as a comment, string or directive |
| recognition of namespace, alias, `using namespace`, enum and type headers, and of enumerator boundaries inside initializers (`namespace_definition`, `namespace_alias_definition`, `enum_specifier`, `enumerator`) | the scope and symbol table, and the conservative handling of shadows, aliases, conflicts, and object and template paths |
| detecting declarators that shadow a name | painting precedence, the macro convention, `publish`, and the span and cancellation rules |

Resolution cannot move into the parser. In tree-sitter-cpp, a plain qualifier
before `::` is always a `namespace_identifier`, whether it names a namespace,
an enum or a class (`qualified_identifier.scope` in
[node-types.json](https://github.com/tree-sitter/tree-sitter-cpp/blob/v0.23.4/src/node-types.json)).
The grammar's own
[highlights.scm](https://github.com/tree-sitter/tree-sitter-cpp/blob/v0.23.4/queries/highlights.scm)
colours a `namespace_identifier` as a type when it begins with a capital.
tree-sitter knows syntax, not names.

T lives only in the adapter:

- A provider in the RE2 style builds a private static target with SYSTEM
  includes. It compiles the runtime's single-file source `lib/src/lib.c` as C11
  with WebAssembly support off (`TREE_SITTER_FEATURE_WASM` undefined), plus the
  grammar's `parser.c` and `scanner.c`.
- Only the syntax adapter links that target, and only in the Qt build.
- No tree-sitter type leaves `cpp_identifier_context.cpp`, and
  `refine_cpp_identifiers` keeps its signature.
- The core links nothing new. tree-sitter is not an approved core library, and
  its allocator hooks are global to the process.
- Each classification uses one parser on the calling worker thread. A progress
  callback returns true once the `std::stop_token` is set, the parser is reset
  after any cancelled parse, and the adapter never calls `ts_set_allocator`.

Facts about the candidate, checked on 2026-10-13:

| Fact | Source |
|---|---|
| The runtime is MIT-licensed C11 and described as "Dependency-free". The latest releases are v0.27.1 (2026-10-08) and v0.27.0 (2026-08-30). It accepts grammars built for ABI 13 to 15 | [tree-sitter](https://tree-sitter.github.io/tree-sitter/), [releases](https://github.com/tree-sitter/tree-sitter/releases), [api.h v0.27.1](https://github.com/tree-sitter/tree-sitter/blob/v0.27.1/lib/include/tree_sitter/api.h) |
| `ts_parser_set_timeout_micros` and `ts_parser_set_cancellation_flag` were deprecated in 0.25 ("this will be removed in 0.26") and are absent from 0.27.1. Cancellation now goes through `ts_parser_parse_with_options` and a `progress_callback`. When the callback returns true the parse returns `NULL`, and the next parse resumes the old one unless `ts_parser_reset` is called | [api.h v0.25.10](https://github.com/tree-sitter/tree-sitter/blob/v0.25.10/lib/include/tree_sitter/api.h), [api.h v0.27.1](https://github.com/tree-sitter/tree-sitter/blob/v0.27.1/lib/include/tree_sitter/api.h) |
| The parser calls the progress callback every 100 parse operations (`OP_COUNT_PER_PARSER_CALLBACK_CHECK`) | [lib/src/parser.c](https://github.com/tree-sitter/tree-sitter/blob/v0.27.1/lib/src/parser.c) |
| The default allocator calls `abort()` when an allocation fails, and replacing it affects the whole process | [lib/src/alloc.c](https://github.com/tree-sitter/tree-sitter/blob/v0.27.1/lib/src/alloc.c) |
| A `TSTree` is not thread-safe. `ts_tree_copy` makes a cheap reference-counted copy | [advanced parsing](https://tree-sitter.github.io/tree-sitter/using-parsers/3-advanced-parsing.html) |
| tree-sitter-cpp is MIT-licensed. Its last tag, v0.23.4 (2024-11-11), targets ABI 14 with 8,637 states, and its `parser.c` is 17,299,078 bytes. `master` was regenerated for ABI 15 on 2026-02-25: 11,734 states and 25,857,209 bytes. `master` has commits through 2026-09-17, including support for reflection (P2996) | [releases](https://github.com/tree-sitter/tree-sitter-cpp/releases), [PyPI](https://pypi.org/project/tree-sitter-cpp/#files), [parser.c v0.23.4](https://github.com/tree-sitter/tree-sitter-cpp/blob/v0.23.4/src/parser.c), [commits](https://github.com/tree-sitter/tree-sitter-cpp/commits/master) |
| The generated `parser.c` turns off optimization for itself (`#pragma optimize("", off)`, `clang optimize off`, GCC `O0`). The PyPI wheels, which contain the compiled grammar, are 288–335 kB compressed. Estimated from the header's defines, assuming 16-bit entries, the large-state parse table alone is about 2.5 MB at v0.23.4 and about 4.4 MB on `master` | [parser.c v0.23.4](https://github.com/tree-sitter/tree-sitter-cpp/blob/v0.23.4/src/parser.c), [PyPI](https://pypi.org/project/tree-sitter-cpp/#files) |
| The parser marks errors as `ERROR` and `MISSING` nodes and continues. It does not expand macros. Open issue #293 (2024-11-11, no maintainer reply) shows two macros turning a class into a `function_definition` with an `ERROR` node | [tree-sitter](https://tree-sitter.github.io/tree-sitter/), [#293](https://github.com/tree-sitter/tree-sitter-cpp/issues/293) |
| The grammar runs `tree-sitter/fuzz-action` only when `scanner.c` changes (Linux only, 60 seconds by default). There is no OSS-Fuzz project directory named `tree-sitter`. The ten 2026 CVEs in an openSUSE tree-sitter update are all in wasmtime, which a native build without WebAssembly does not compile | [fuzz.yml](https://github.com/tree-sitter/tree-sitter-cpp/blob/master/.github/workflows/fuzz.yml), [fuzz-action](https://github.com/tree-sitter/fuzz-action), [LWN](https://lwn.net/Articles/1073533/) |

### Alternatives

- **Extend the heuristic (E).** Make targeted fixes for the gap classes the
  evaluation confirms. This is the "extend" outcome, bounded under "Decision
  rules".
- **KSyntaxHighlighting alone.** A definition is a per-line state machine with
  a context stack. It can mark the name after `namespace`, but it cannot keep a
  symbol table for the whole blob, so it cannot recognize qualified uses. The
  packaged `isocpp.xml` (6.28.1, `DispatchNamespace`) recognizes namespaces
  only through a regex for `std::`, `boost::`, `details::`, `aux::` and
  `internals::`. The catalog loads only packaged definitions, so this route
  means forking upstream data. Considered and not proposed.
- **libclang.** It can resolve names only when given the translation unit's
  include paths and compile flags. RFC-BPR-0072's adapter "performs no
  repository reads or compiler queries", and that RFC places "compiler/AST
  analysis" out of scope. libclang is also licensed Apache-2.0 WITH
  LLVM-exception, and its wheel is about 25 MB per platform
  ([wheels](https://pypi.org/project/libclang/#files)). Not proposed as a
  candidate. The evaluation uses clang offline, only to produce labels.
- **Replacing KSyntaxHighlighting for C++ with tree-sitter highlighting.** This
  would change every C++ role and the acceptance of
  [RFC-BPR-0006](RFC-BPR-0006-syntax-highlighting-themes-appearance.md).
  It needs its own RFC and is out of scope here.
- **Keep (K).** Changing nothing is a valid outcome.

### Corpus

1. **Contract and fixtures.** `tests/fixtures/syntax/*.cpp`, the
   `cpp-identifiers-v1` and `header-source-v1` packs, the C++ files in the
   `uiforetw-74` and `directxmath-336` recordings, and the 20,000-line
   comparison in `large-review-v1`.
2. **First-party.** BetterPullReview's own `src/` at a pinned commit: 993 C++
   files, 18.3 MB in total. The largest, `github_application.cpp`, is 1.42 MB
   and 31,402 lines.
3. **Third-party, not committed.** The C++ in archives the build already pins:
   Abseil 20250814.2, RE2 2025-11-05, Glaze 8.3.0, Quill 13.0.0 and
   KSyntaxHighlighting 6.28.1. Each is identified by its archive digest.
4. **Gap suite.** Small committed fixtures for H1–H8. It also covers namespace
   macros, `extern "C"` guards split by `#ifdef`, merge conflict markers and
   C++26 reflection syntax.
5. **Fragments.** Share-sheet excerpts built from (1)–(3) by `note_code_view`'s
   rule, and hunks that begin inside an enum, a namespace, a comment or a raw
   string.
6. **Malformed and pathological input.** Files cut off at random offsets,
   unbalanced braces, each kind of bracket nested 100,000 deep, a single 1 MiB
   line, 8 MiB of `{`, 8 MiB of short names, and random valid UTF-8.
7. **Composer replay.** Recorded edit sequences on (1) and (2), sent as
   successive whole texts.

### Accuracy

The contract suite is authoritative. Labels are fixed before any candidate
runs, for the categories RFC-DRAFT-0011 defines:

- namespace declarations: simple, nested, multiline, alias, `using namespace`
- enumerator declarations: scoped, unscoped, with nested initializers, with
  attributes
- qualified uses of enum members with evidence in the blob
- namespace qualifiers
- conservative negatives
- ALL_CAPS macros that must keep their role
- protected text

For the first-party corpus (2), clang's JSON AST supplies the labels. Clang runs
offline from the `dev` preset's compile commands and is never shipped. A label
counts only when its declaration is in the same blob. Where the accepted
conservative rule differs from clang, the contract's outcome is the label.

For (3) to (5), a stratified sample of 50 identifiers per category per source is
labelled by hand. For every run, the report gives false positives and false
negatives per category. It also marks each hypothesis H1–H8 as confirmed or
refuted.

### Measurements

Measurement follows the
[RFC-BPR-0013 convention](RFC-BPR-0013-performance-resilience-resource-limits.md#performance-budgets)
and the practice of
[BPR-1366](../issues/60-done/BPR-1366-a-prepared-diff-computation-stops-within-milliseconds-when-the-reviewer-acts.md):
optimized builds, interleaved rounds on an idle station, medians and p95, and
recorded hardware. Runs use the macOS reference station and the Windows x64 and
Linux x64 stations. Results are grouped into five size classes: S (under
64 KiB), M (64 KiB–1 MiB), L (1–8 MiB), pathological and fragment. Record:

- grammar-pass time, refinement time and their total, per file
- time until the spans arrive in the composed application:
  `syntax_first_file` and `syntax_navigation` in
  `application_large_review_benchmark`, with the candidate compiled in. The
  product classifies only whole blobs, so this is also the latency for the
  visible region. Parsing only the visible region would break RFC-BPR-0072's
  agreement between whole-blob and hunk spans
- composer time per edit
- peak heap used by the refinement, from a counting allocator in the harness,
  and the process's peak resident memory
- time from a stop, requested at a uniformly random point, to the return
- shipped executable size per architecture slice (both macOS universal slices,
  Windows and Linux), adapter archive size, clean `dev` build wall time, and the
  compile time of the parser translation unit
- a run of the whole corpus under AddressSanitizer and
  UndefinedBehaviorSanitizer
- a new libFuzzer target over the public C++ classifier, under
  `BPR_BUILD_FUZZERS`, run for 4 CPU-hours per candidate on macOS and Linux
- whether spans are identical across three runs and three platforms
- the licence of each shipped file, the notices, and the effect on
  corresponding source

T is measured at the v0.23.4 grammar tag with runtime v0.27.1. `master` is
measured too, at a recorded commit; see Q1.

### Decision rules

These thresholds are fixed before measurement. Changing one afterwards is an
owner decision, and the report records it.

T is rejected if any gate fails:

| Gate | Requirement |
|---|---|
| Contract | `syntax_classification_contract`, `qt_syntax_pane_composition`, `application_cpp_identifier_appearance`, `application_appearance_host` and the syntax checks in the large-review benchmark all pass. A changed expectation counts as a failure until the owner accepts it |
| No new errors | No category gains a false positive compared with the baseline. Macro outcomes outside the structural exceptions are unchanged |
| Bounded work | On L and pathological inputs, on every platform, the time from stop to return is at most 10 ms at p95 and at most 50 ms in the worst case. Cancellation and overflow never publish partial spans. Nothing crashes, no input takes more than 10 s, and the sanitizers and fuzzer report nothing. Deep nesting completes or is refused cleanly |
| Memory | Peak refinement heap is at most twice the baseline's in each class, and at most 512 MiB for any input the classifier accepts. The allocator's abort path is then reachable only in a process already close to running out of memory |
| Determinism | Spans are identical across runs and platforms |
| Placement | T lives only in the adapter. `core-only` and both architecture-boundary checks pass, and no tree-sitter symbol appears outside the syntax adapter |
| Supply | The runtime and grammar carry MIT notices. The archive is pinned and tagged (see Q1), with SHA-256, size and licence digest. The report records how T would pass the five dependency checks BPR-1431 passed |

A T that passes every gate is adopted only if both conditions hold:

- **Gain.** T removes at least 25% of the baseline's false negatives within
  RFC-BPR-0072's scope across the labelled corpus, and at least 20 occurrences.
  Alternatively, T passes at least half of the gap hypotheses confirmed on the
  baseline.
- **Cost.** The median total classification time in each class is at most 1.25
  times the baseline's. The medians for `syntax_first_file` and
  `syntax_navigation` are at most 1.10 times the baseline's. T adds at most
  8 MiB per architecture slice, and the clean `dev` build takes at most 10%
  longer.

If T is not adopted, one of the other two outcomes applies:

- **Extend.** If the gap suite confirms baseline failures within
  RFC-BPR-0072's scope, a follow-up card fixes those classes in the heuristic.
  It may not take `cpp_identifier_context.cpp` past 800 lines or add a pass over
  the whole blob. A fix that would need either reopens the parser decision,
  using this report's numbers. Macro expansion, cross-file uses and unqualified
  uses are recorded as out of scope, not fixed.
- **Keep.** Otherwise the report records when to run the evaluation again: when
  the file reaches 800 lines, or when a new identifier category needs
  structural recognition.

The owner does not need to look at results and judge them, because roles are
scored against fixed labels. If a contract expectation would change, that one
change goes to the owner as an Accept or Reject question in its own short
attended-review session.

### Open questions

- **Q1. May the adapter pin an untagged tree-sitter-cpp commit?** The last tag,
  v0.23.4 from 2024-11-11, targets ABI 14. `master` has ABI 15, was regenerated
  on 2026-02-25, supports newer C++ (reflection, annotations and expansion
  statements) and is about 50% larger. Every current pin is a tagged release or
  LTS. The evaluation measures both. If only `master` passes, the outcome is
  "eligible pending this answer".

## Acceptance criteria

- **AC01:** With the evaluation option on, the harness runs the baseline and
  each T pin over the whole corpus through the unchanged public classifier
  path. The committed report shows whether each gate passed, with its commands,
  counts and raw results. With the option off, no target acquires or links
  tree-sitter, as the configure log and a link-dependency check show.
- **AC02:** The labels are committed before any candidate results; third-party
  sources are named by digest. For every run, the report gives false positives
  and false negatives per category, and marks each of H1–H8 as confirmed or
  refuted on the baseline and on T.
- **AC03:** The macOS, Windows and Linux measurements cover every size class and
  every item under "Measurements", and they measure the baseline in the same
  runs.
- **AC04:** A test stops the refinement partway through and observes
  `cancelled` with no spans, and the syntax fuzz target runs in the fuzz build.
  Both remain after the evaluation. The report gives sanitizer, fuzz,
  deep-nesting and 8 MiB results for the baseline and for T.
- **AC05:** The recorded outcome (adopt, extend, keep, or eligible pending Q1)
  follows mechanically from "Decision rules" and states when to run the
  evaluation again. Any deviation is recorded as an owner decision.
- **AC06:** The report records the licence of each shipped file (runtime,
  generated parser and scanner) and the notice, SPDX and corresponding-source
  changes adoption would need. It also records the archive digests, the size
  added per slice and the build cost.
- **AC07:** On the shipping configuration, the evaluation work leaves spans
  byte-identical over the corpus compared with the revision before it, and
  `core-only` and both architecture-boundary checks pass.
