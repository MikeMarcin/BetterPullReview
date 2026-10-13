---
id: "RFC-BPR-0083"
title: "Evaluate replacements for the hand-written Git metadata readers"
status: "In Progress"
depends_on: ["RFC-BPR-0054"]
metadata: {"rfc_base_sha":"9a4f61976bba23727acd77d10c23c041f87208f0","mandate_revision":"git-metadata-readers-r1","milestone_plan":{"id":"git-metadata-readers-v1","path":"docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}}
---

# Evaluate replacements for the hand-written Git metadata readers

## Execution status

Mandate `git-metadata-readers-r1` adopts RFC-DRAFT-0019 unchanged apart from
link bases and the `Acceptance criteria` heading. Immutable RFC_BASE_SHA:
`9a4f61976bba23727acd77d10c23c041f87208f0`. The
[live import](assets/git-metadata-readers-v1.LIVE_IMPORT.md) retains the draft's
bytes and the [milestone plan](plans/git-metadata-readers-v1/milestone-plan.json)
groups the cards:

| Card | Scope |
|---|---|
| [[BPR-1454]] | Immediate fixes 1–7 against RFC-BPR-0054 |
| [[BPR-1455]] | Differential harness against an isolated Git oracle (AC01, AC02 for K) |
| [[BPR-1456]] | Extended canary and containment course on macOS (AC03) |
| [[BPR-1457]] | Fuzz oracles, adversarial timing and the 250,000-entry bench (AC04) |
| [[BPR-1458]] | libgit2 (L) behind an evaluation-only option, on macOS (AC02, AC03, AC05) |
| [[BPR-1459]] | Vendored pieces (V) and gitoxide (G) desk checks and cost records (AC05) |
| [[BPR-1460]] | The decision record (AC06) |
| [[BPR-1461]] | A reftable checkout refused under the owner's Q2 answer (AC07) |
| [[BPR-1462]] | Windows harness, canary and cost measurements |
| [[BPR-1463]] | Linux harness, canary and cost measurements |
| [[BPR-1464]] | Verify and close this RFC |

**Progress (2026-10-13, macOS).** Done: [[BPR-1454]] (fixes 1–7),
[[BPR-1461]] (reftable refused), [[BPR-1455]] and [[BPR-1456]] (harness and
canary course for K), [[BPR-1457]] (fuzz oracles, H6, scale), [[BPR-1458]] (L)
and [[BPR-1459]] (V, G). The [decision record](../verification/RFC-BPR-0083-decision.md)
keeps K for every component: L fails H1, H2, H3 and H6, V fails H6, and G is
rejected, so Q3 is not asked. Remaining: the Windows and Linux stations
([[BPR-1462]], [[BPR-1463]]) and closeout ([[BPR-1464]]). Evidence is in the
[verification ledger](../verification/RFC-BPR-0083.md).

**Owner answers.** Q1 was answered on 2026-10-13: core may link only the
owner-approved pinned pure libraries glaze, utf8proc, zlib and fmt, and any other
third-party code vendored into core needs the owner's explicit approval for that
code ([AGENTS.md](../../AGENTS.md)). A vendored matcher or reftable reader in core
is therefore eligible only pending that approval. **Q2 was answered on
2026-10-13: option (a).** A reftable checkout is refused now under fix 1, with
its own named reason, and whether to read reftable is decided later under C3 by
this evaluation. Q3 (a GPL-2.0 dependency with a linking exception) is open and
is asked only if L passes H1–H3, H5 and H6 for some component.

## Problem and grounded current behavior

On 2026-10-13 the owner asked to "author a rfc to evaluate alternative" to the
hand-written Git metadata readers behind Review Cycles capture. This RFC proposes
that evaluation: a differential harness against real Git, a set of criteria and
decision rules for each component. It adopts nothing by itself. A component is
replaced only where the evidence defined here says so. This is a proposal, not
implementation acceptance.

**What exists.** [RFC-BPR-0054](RFC-BPR-0054-workspace-source-capture-and-revision-material.md)
(Auto-Verified) captures a Git checkout by reading its metadata as data. The
readers arrived with [BPR-1149](../issues/60-done/BPR-1149-capture-scoped-quiescent-revisions-without-source-writes.md)
on 2026-09-29:

| Component | Source | Behavior |
|---|---|---|
| C1 Index | `parse_git_index` in [review_cycle_git.cpp](../../src/core/src/review_cycle_git.cpp) (270 lines, which also hold C2 and C3's parsers) | `DIRC` versions 2–4, including the version-4 prefix varint. Reads stages, skip-worktree and intent-to-add. Flags split (`link`) and sparse (`sdir`) indexes and skips every other extension |
| C2 Repository format | `git_object_format` | A line scan for `[extensions] objectformat` |
| C3 HEAD and references | `parse_git_head`, `parse_git_reference` and `find_packed_reference`, driven by `read_head` and `read_git_metadata` in [review_cycle_capture.cpp](../../src/application/src/review_cycle_capture.cpp) (lines 106–176). A second resolver, `resolve_named`, is in [review_cycle_outgoing.cpp](../../src/application/src/review_cycle_outgoing.cpp) (lines 56–150) | Loose reference, then `packed-refs` |
| C4 Git directory discovery | `gitfile_target`, `commondir_target` and `detect_repository` in [local_workspace_source.cpp](../../src/adapters/workspace/src/local_workspace_source.cpp) (lines 95–118, Windows 495–544, POSIX 1138–1172) | A `.git` directory or `gitdir:` file, then `commondir` |
| C5 Ignore, C6 `filter` attribute | `wildcard_match`, `workspace_ignore_rules` and `workspace_attribute_rules` in [review_cycle_ignore.cpp](../../src/core/src/review_cycle_ignore.cpp) (353 lines) | A wildmatch port for `.gitignore` and `info/exclude`, plus the `filter` attribute alone |

The parsers live in core and take bytes. The application obtains those bytes
through `read_git` on the [workspace port](../../src/application/include/bpr/application/workspace_source_port.hpp).
The adapter follows links to reach the Git directories, because the checkout
chose where they live. Beneath them it opens every component with `O_NOFOLLOW`
(POSIX `read_git`, lines 1384–1425).

**The contract they serve.** Capture is read-only. It runs no hooks, filters,
textconv, lazy fetch or repository scripts (RC54-02), and it traverses beneath
the root without following links ([SNAPSHOTS](assets/bpr-review-cycles-v1/specs/SNAPSHOTS.md)).
The workspace adapter "never runs Git"
([system architecture](../architecture/system-architecture.md), Review
Cycles; [threat model](../architecture/security-threat-model.md)). Core is
"standard/domain C++ only"
([dependency rules](../architecture/system-architecture.md#build-target-and-dependency-direction)),
which [VerifyCoreBoundary.cmake](../../cmake/VerifyCoreBoundary.cmake) and
[VerifyArchitectureBoundaries.cmake](../../cmake/VerifyArchitectureBoundaries.cmake)
enforce.

The ignore matcher matters for confidentiality, because the store holds "untracked
files the checkout's own ignore files do not exclude"
([security overview](../security/corporate-security-overview.md#review-cycles-retained-source-and-disclosure)).
The global excludes and attributes files are deliberately not read, so a path that
only they ignore stays visible as a candidate
([review_cycle_ignore.hpp](../../src/core/include/bpr/core/review_cycle_ignore.hpp)).

**Evidence today.** Core unit tests run the parsers on synthetic indexes
([review_cycle_manifest_tests.cpp](../../tests/core/review_cycle_manifest_tests.cpp)).
The workspace contract tests already drive real Git: index version 4,
skip-worktree, gitlinks, and a canary course that points every hook and
`core.fsmonitor` at a logging script
([review_cycle_capture_tests.cpp](../../tests/workspace_contract/review_cycle_capture_tests.cpp),
lines 141–170 and 401–414).

The `git_output` target ([git_output_fuzz.cpp](../../tests/fuzz/git_output_fuzz.cpp))
fuzzes every reader for crashes and hangs only.
[BPR-0970](../issues/60-done/BPR-0970-cover-custom-parser-and-mapping-boundaries-with-fuzzing.md)
used it to find exponential `*` backtracking: one 6 KiB line took 13 s. The fix
added Git's own abort rules (review_cycle_ignore.cpp, lines 64–71). No test
checks the readers' answers against Git's.

**Observed divergences.** Checked during drafting on 2026-10-13 by reading the
code and running Git 2.50.1 against scratch repositories:

| Gap | Observation | Effect |
|---|---|---|
| Reftable | Git writes `HEAD` as `ref: refs/heads/.invalid`, makes `refs/heads` a regular file and sets `extensions.refStorage = reftable`. The readers ignore that extension. The loose read fails with `ENOTDIR`, which maps to `missing`, and there is no `packed-refs` | Capture succeeds with an empty `HeadCommit`, which looks the same as an unborn branch. Two revisions then compare as `same_head` even after HEAD moved ([review_cycle_views.cpp](../../src/core/src/review_cycle_views.cpp), line 56). Outgoing resolution finds no commit |
| Repository format | Neither `core.repositoryformatversion` nor any extension except `objectFormat` is read | Any on-disk format change is read as if it were absent. Git refuses unknown extensions |
| Loose-reference failure | When a loose read fails with anything but `missing`, `read_head` falls back to `packed-refs` (capture, lines 128–136). A loose symbolic reference yields no commit | `packed-refs` can silently supply a stale commit. `resolve_named` reports the same failure instead |
| POSIX bracket classes | Skipped (review_cycle_ignore.cpp, lines 33–41). Git ignores `a b` under `a[[:space:]]b` and keeps `x y` under `x[![:space:]]y`. The port does the opposite in both cases (from the code) | An ignored file becomes a default candidate, and a negated class hides files Git shows |
| `core.ignoreCase` | On macOS, `git init` sets it to true. Git then ignores `secret.env` under the pattern `Secret.env`, but the matcher compares case-sensitively | Files Git ignores enter capture on the owner's main platform |
| Config syntax | Inline comments are not stripped. Git accepts `objectformat = sha256 ; note` as SHA-256, but the reader sees an invalid format | A valid repository is refused as `unsupported` (fails closed) |
| Index trailer | Located but not verified ([review_cycle_git.hpp](../../src/core/include/bpr/core/review_cycle_git.hpp), lines 64–66). Only the reread after capture detects a change | Corruption that keeps a valid structure produces wrong staged identities |
| Unknown index extensions | Every one is skipped. Git and libgit2 refuse an extension whose signature does not start with `A`–`Z` | A future mandatory extension would be silently misread |
| Split index | Flagged, and coverage is recorded as `partial` (capture, line 844) | Honest, but the tracked set is incomplete |

Two apparent gaps are not real. Git reads repository-format extensions without
following includes: an `include.path` that sets `objectformat = sha256` leaves Git
on SHA-1. Missing `include`/`includeIf` support therefore matters only for other
keys, such as `core.ignoreCase`. And the unread global files are a recorded
decision, not an oversight.

Git 2.51 announced that reftable becomes the default for new repositories in
Git 3.0 ([2.51 notes](https://about.gitlab.com/blog/what-s-new-in-git-2-51-0/)),
so the reftable gap will grow.

## Proposed behavior and boundaries

### Immediate fixes: a separate card

The following fixes restore RFC-BPR-0054's promise of "truthful handling rather
than a lossy fallback". Every outcome of this evaluation needs them, and they are
small. They form one card against RFC-BPR-0054 (provisional DRAFT-0010), scheduled
now and not gated by this RFC. This evaluation's baseline is the fixed readers,
and its harness checks each fix again.

1. Read `core.repositoryformatversion` and every `extensions.*` key. Refuse
   capture and reference resolution as `unsupported`, with a named reason,
   through the path that already refuses an unrecognized object format (capture,
   lines 147–150). Refuse when:
   - the version is above 1;
   - `refStorage` is anything other than `files`;
   - an extension is not on an allowlist of extensions that leave the files
     read unchanged. The allowlist starts as `objectFormat`, `worktreeConfig`,
     `partialClone`, `preciousObjects`, `noop` and `relativeWorktrees`, and the
     card confirms each one against Git's documentation.

   This refuses a reftable checkout instead of returning an empty HEAD; the
   answer to Q2 may change that.
2. For the keys the readers use, parse config with Git's syntax: sections,
   subsections, case rules, quotes, escapes, inline `#` and `;` comments, and
   continuation lines. Includes stay unread.
3. Honor `core.ignoreCase` from the repository config in ignore and attribute
   matching, folding case the way Git's wildmatch does.
4. Implement Git's twelve POSIX bracket classes. As in Git, an unknown class
   makes the whole pattern match nothing.
5. Verify the index trailer with the repository's hash algorithm. Accept an
   all-zero trailer, which Git writes under `index.skipHash`; `feature.manyFiles`
   produced one in the probe.
6. Refuse an index that carries an unrecognized extension whose signature does
   not start with `A`–`Z`. `link` and `sdir` keep their current handling.
7. In capture's `read_head`, report a loose-reference failure other than
   `missing` instead of consulting `packed-refs`, as `resolve_named` does. Follow
   one level of symbolic reference, also as `resolve_named` does.

Each fix is accepted by a workspace contract test that runs real Git and fails on
the current readers.

### Candidates

Each component C1–C6 is evaluated on its own. The Git adapter has its own
discovery code (`source_objects` in [installed_git_process.cpp](../../src/adapters/git/src/installed_git_process.cpp),
lines 7000–7045), which is out of scope. The decision record names it if C4
changes.

- **K: keep the first-party readers, fixed, with a permanent differential
  harness.** This is the baseline. It already meets the toolchain, license,
  SHA-256 and core-boundary constraints.
- **L: libgit2 behind a workspace-adapter port.** libgit2 is GPL-2.0 with a
  linking exception. Its bundled pieces carry zlib, PCRE2, MIT, ISC and other
  notices, plus LGPL WinHTTP definitions on Windows
  ([COPYING](https://github.com/libgit2/libgit2/blob/main/COPYING)). No libgit2
  type crosses the port, and it is built without HTTPS, SSH, NTLM or its CLI.
  The evaluation must settle the following with evidence:
  - **Containment.** References, config and objects have public custom-backend
    interfaces, which could read through `read_git`. But `git_index_open` and the
    ignore and attribute readers open paths themselves. Giving them verified bytes
    would take a private copy or a descriptor path. Any path reads must be shown
    not to follow links or leave the root and the Git directories. Config includes
    must not read files outside them, and the global and system search paths must
    be empty, so that global files stay unread as decided.
  - **Coverage.** libgit2 refuses `link` and `sdir` as unsupported mandatory
    extensions ([index.c](https://github.com/libgit2/libgit2/blob/main/src/libgit2/index.c)).
    Today's partial capture of a split index and full capture of a sparse index
    would both become refusals. SHA-256 needs an opt-in experimental build in the
    1.9 series. A reftable backend exists on `main`
    ([reference](https://libgit2.org/docs/reference/main/sys/refdb_backend/git_refdb_backend_reftable.html)),
    but no release note up to v1.9.7 announces it
    ([releases](https://github.com/libgit2/libgit2/releases)).
  - **No execution.** libgit2 runs no hooks or external filter drivers. Confirm
    this with the canary course.
  - **Cost.** Process-global initialization and options, a provider module like
    [BprSQLiteProvider.cmake](../../cmake/BprSQLiteProvider.cmake), and the
    packaging work described under the evaluation method.
- **V: vendored pure-computation pieces in core.** Examples are a reference
  wildmatch for C5 or a reftable reader for C3. The license of libgit2's
  `wildmatch.c` needs a determination: the file's header claims GPL-2.0 with the
  linking exception, while COPYING calls wildmatch public domain. That
  determination counts as cost. V cannot proceed until Q1 is answered. Git's own
  sources are GPL-2.0-only, and they stay out of scope while the first-party
  license is unset ([DISTRIBUTION.txt](../../packaging/DISTRIBUTION.txt)).
- **G: gitoxide, desk check only, expected to be rejected.** It is MIT or
  Apache-2.0 and thorough about Git semantics, but it is Rust-only. Using it would
  mean a second toolchain on three platforms, an FFI layer BPR maintains, and a
  large crate graph in which each crate needs an SPDX entry, notices and
  corresponding source. Its fuzzing and sanitizers would also run outside the
  CMake and ctest presets. Its tests may serve as a semantic reference, but
  nothing is linked.
- **Not evaluated: running installed Git during capture.** That contradicts the
  workspace adapter's never-run-Git contract, and repository config can name
  executables. Only an owner amendment of that contract would reopen it.

### Evaluation method

**Oracle.** Tests may run Git, but the product may not. A workspace contract
harness creates each corpus repository with the installed Git, asks Git and each
candidate the same questions, and compares the answers for each path or
reference:

| Component | Git oracle |
|---|---|
| C1 | `git ls-files --stage -z`, with `-t`, `--sparse` and `--debug` for skip-worktree, sparse directories and entry flags such as intent-to-add |
| C2 | `git rev-parse --show-object-format --show-ref-format` and `git config --type=bool core.ignorecase` |
| C3 | `git symbolic-ref -q HEAD` and `git rev-parse --verify -q` for HEAD and named references |
| C4 | `git rev-parse --path-format=absolute --git-dir --git-common-dir` |
| C5 | `git check-ignore --no-index -v --stdin -z` |
| C6 | `git check-attr filter --stdin -z` |

The oracle runs with `GIT_CONFIG_NOSYSTEM=1`, an empty global config,
`HOME` and `XDG_CONFIG_HOME` set to empty directories, and `GIT_OPTIONAL_LOCKS=0`,
which keeps global files out just as capture does. When the installed Git lacks a
case's feature (reftable needs 2.45), the case reports *skipped* with the version
found; it never reports a pass. A negative control, a scratch build with one fix
reverted, must turn the harness red.

**Corpus.**

- Object formats and index variants: SHA-1 and SHA-256; index versions 2, 3
  and 4; `feature.manyFiles` and `index.skipHash`; a split index; a cone-mode
  sparse checkout with a sparse index; 250,000 entries
  (`maximum_cycle_capture_entries`).
- Index entries: skip-worktree, intent-to-add and unmerged stages.
- Repository layouts: linked worktrees, including `relativeWorktrees`; reftable.
- HEAD states: packed-only, loose, symbolic and detached references, and an
  unborn branch.
- Config: `include` and `includeIf`, inline comments, quoting, escapes,
  subsections, and `core.ignoreCase` both true and false.
- Ignore files: POSIX classes, escapes, trailing spaces, negation, `**`,
  directory-only rules, CRLF line endings and non-UTF-8 paths.
- Attributes: `filter` set, unset and unspecified, and macros.
- Adversarial patterns, including the BPR-0970 input and new fuzz finds.

**Safety.** Extend the existing canary course with canaries outside the allowed
scope that detect any open, such as a FIFO named by `include.path` or a symlinked
`.gitignore` that points outside the root. After every case, check that the
checkout is byte-identical, unmodified and free of lock files. Run this on macOS,
Linux and Windows.

**Fuzzing.** Every parser that stays or is adopted keeps a target in the existing
harness. Add an oracle wherever one is cheap: serialize-then-parse round trips
for the index, and for C5, differential fuzzing against a reference matcher
compiled into the fuzz target only.

**Performance.** Extend the existing
[scale bench](../../tests/workspace_contract/review_cycle_scale_bench.cpp)
to a 250,000-entry checkout. Record the median of five runs of index parsing and
an ignore walk, plus peak memory. Following that bench's convention, compare the
numbers in the decision record rather than asserting them in ctest.

**Cost.** For each candidate, record:

- an SPDX expression, notices, provenance and a
  [dependencies.json](../../packaging/dependencies.json) entry drafted to its
  conventions, run through `verify_release_sources.py` as a trial, not a release;
- the stripped binary-size and build-time deltas on each platform, measured
  rather than estimated;
- toolchain requirements;
- the upstream release and security-advisory cadence;
- the size of the integration code.

### Decision rules

A candidate is eligible for a component only if it passes every hard gate for
that component:

- **H1:** No silent divergence across the corpus. Every difference is a fix, an
  explicit refusal with a reason, or a documented deliberate choice.
- **H2:** The canary and containment checks pass on all three platforms.
- **H3:** Where the component depends on the object format, SHA-1 and SHA-256
  both work in the shipped configuration, not an experimental one.
- **H4:** No copyleft obligation reaches first-party code. Packaging, notices and
  corresponding source are complete, and any new license has the owner's approval
  (Q3).
- **H5:** It builds with the existing toolchains on all three platforms. The
  `core-only` preset and the architecture checks pass, with no library in core
  unless Q1 allows one.
- **H6:** No adversarial case takes more than 2× the fixed first-party time or
  more than 1 s.

Then, for each component:

1. Keep K if it passes H1, H2 and H6 once the fix card lands. K is the default
   and wins ties.
2. Adopt a candidate only in one of two cases. Either K fails a hard gate that
   would take more new code to meet than integrating the candidate would (port,
   provider module and packaging). Or the candidate is the cheaper way to provide
   a capability in scope, such as reading reftable under Q2. A library's full
   cost counts once against every component that would use it, so adopting
   libgit2 for a single component pays all of it.
3. A replacement must not make the scale bench slower or raise its peak memory
   by more than 10%.
4. After the decision, each component has exactly one product implementation,
   and the harness stays as a regression gate whatever the choice.

Record each component's verdict, the gate or rule that decided it and links to
the evidence in a committed decision record. If anything moves, update the
[system architecture](../architecture/system-architecture.md) and the
workspace adapter's description.

### Open questions

- **Q1, pending with the owner: may core contain vendored pure-computation
  code?** The answer decides whether V can exist and how H5 applies to a vendored
  matcher or reftable reader. Core holds generated Unicode tables today but no
  third-party code ([dependencies.json](../../packaging/dependencies.json)).
  The RE2 source-matcher reservation
  ([BPR-1431](../issues/_reservations/BPR-1431--BPR-1431--e2ce4383ccc3.json))
  meets the same boundary. Until Q1 is answered, vendored code may only be
  measured as a test-only oracle or behind an adapter port.
- **Q2: how should a reftable checkout be handled?** *Answered by the owner on
  2026-10-13: (a).*
  - (a) Refuse it now under fix 1 and decide whether to read it under C3.
    Recommended: it needs no manifest change, and reading reftable becomes
    necessary as Git 3.0 approaches.
  - (b) Refuse it indefinitely.
  - (c) Capture it with an explicit "head unavailable" marker, which changes the
    manifest and its codec.

  The answer affects fix 1 and C3.
- **Q3: would the owner accept a dependency under GPL-2.0 with a linking
  exception in the shipped binary?** Ask only if L passes H1–H3, H5 and H6 for
  some component. The evaluation proceeds without the answer, but adopting L waits
  for it.

## Acceptance criteria

- **AC01:** The differential harness runs in the workspace contract suite over
  the corpus with an isolated oracle. It reports agreement for each component
  and case, reports a missing Git feature as skipped with the version found, and
  turns red under the negative control.
- **AC02:** Every divergence of each evaluated implementation is classified under
  H1, and no component's product implementation keeps a silent divergence.
- **AC03:** The extended canary course records containment, no-execution and
  no-write results for K and for every L component evaluated, on macOS, Linux and
  Windows.
- **AC04:** Every parser that stays or is adopted has a fuzz target in the
  existing harness with the oracle defined for it. The BPR-0970 input and the
  adversarial corpus meet H6, and performance is reported against rule 3.
- **AC05:** Each evaluated candidate has a cost record that lists its license and
  SPDX expression, the trial packaging result, the binary-size and build-time
  deltas for each platform, its SHA-256 and reftable status, and the integration
  size. G's desk check records the reasons it was rejected.
- **AC06:** The committed decision record gives each component's verdict, the
  gate or rule applied, links to the evidence, and every question answer it relies
  on. An adopted library sits behind the workspace-adapter port with no type
  crossing inward. The `core-only` build, the architecture-boundary checks and the
  documentation gate pass.
- **AC07:** After the decision, a reftable checkout produces Git's HEAD commit, or
  the refusal or marker chosen under Q2. It never produces an empty `HeadCommit`
  that looks like an unborn branch.
