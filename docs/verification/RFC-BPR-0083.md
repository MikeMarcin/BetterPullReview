# RFC-BPR-0083 verification

Mandate `git-metadata-readers-r1`; immutable RFC_BASE_SHA
`9a4f61976bba23727acd77d10c23c041f87208f0`. The
[live import](../rfcs/assets/git-metadata-readers-v1.LIVE_IMPORT.md) retains the
draft. The owner answered Q2 with option (a) on 2026-10-13; Q3 is open. All
results below are macOS (arm64, Apple clang, Git 2.50.1 (Apple Git-155));
nothing here is a Windows or Linux result.

## BPR-1454 and BPR-1461: reader fixes and the reftable refusal

[[BPR-1454]] fixes 1–7; [[BPR-1461]] holds AC07's refusal. The product still
never runs Git: every new read goes through the workspace port's `read_git`.

| Fix | Row in `review_cycle_git_readers` (`--row`) | Fails at base `9a4f61976` | Passes |
|---|---|---|---|
| 1 format version, unknown and version-0 extensions | `format` | yes: version 2 captured | yes |
| 1 reftable (Q2 a), capture and material check | `reftable` | yes: captured with empty HEAD | yes |
| 1 reftable, outgoing resolution (migrated checkout) | `outgoing-reftable` | yes: planned without refusal | yes |
| 2 config syntax (inline comment on `objectformat`) | `config` | yes: refused as unsupported | yes |
| 3 `core.ignoreCase`, ignore and `filter`, `config.worktree` | `ignore-case` | yes: `secret.env` captured | yes |
| 4 POSIX classes | `posix-classes` | yes: `a b` captured | yes |
| 5 index trailer, `index.skipHash` accepted | `index-checksum` | yes: corrupt index captured | yes |
| 6 required index extension | `index-extension` | yes: captured | yes |
| 7 one symbolic level | `head-symbolic` | yes: empty `HeadCommit` | yes |
| 7 no stale `packed-refs` fallback | `head-stale` | yes: stale commit captured | yes |

**Negative control.** With the test unchanged and the base readers restored
(`git checkout 9a4f61976 --` the core and application reader sources), each row
run alone failed at the assertion named in the table; with the fixed readers
restored, all rows pass. Fixes 5 and 6 and both halves of fix 7 are separate rows
so each shows its own failure.

**Unit and boundary checks.** `core_review_cycle_manifest` covers Git's config
syntax, booleans and integers, the repository-format rules (allowlist, version 0
and 1, case-sensitive values, reftable, `compatObjectFormat` declined, includes
unread, `config.worktree`), the index trailer and extension rules, Git's
loose-reference rule, the twelve POSIX classes, abort rules for bad brackets,
WM_CASEFOLD with Git's quirks (a single bracket member and an escaped byte keep
their case), Git's trailing-space trim and attribute states. The matcher keeps
its star-group recursion on an explicit stack, so a pattern with many star
groups cannot exhaust a worker thread's stack.

Commands (dev preset, `build/dev`): `ctest -R
"review_cycle|workspace_source|architecture|cycle_cli|git_readers"` passed 60 of
60, including `architecture_boundaries`, `architecture_boundary_contract`,
`review_cycle_capture`, `review_cycle_outgoing`, `review_cycle_workbench` and the
Qt and public-path review-cycle rows. A full `ninja` of the dev tree built. The
`core-only` preset built, and its rows passed except `documentation_structure`,
`documentation_issue_graph_contract` and `issue_id_allocation_contract`, which
need the `.agents` skill scripts a worktree lacks (the documentation gate passes
with them on `PYTHONPATH`), and `coding_format_check`, which flagged a then
unformatted file outside this change. clang-format 22 and changed-line
clang-tidy are clean on the touched sources. `test_network_isolation_manifest`
accepts the new row; it still reports seven `application_source_pr_*` rows that
another change registered on main without entries.

**Independent review.** An rfc-reviewer compared the candidate with Git 2.50.1
using randomized rule files (about 30 runs of 250 ignore rules and 40 attribute
runs) and found no blocking defect. Four findings were fixed before acceptance,
each with a test: an anchored pattern's literal part is now compared on its own
as Git's `match_pathname` does, so `config/local**/*.pem` ignores
`config/local/a/key.pem` (this predates the card and let capture keep files Git
ignores); a NUL ends only its own `.gitattributes` line; a quoted macro's name
ends at its first blank; and macro expansion runs on an explicit stack with
keyed lookups. The other findings are passed to [[BPR-1455]] for classification:
a rule file over 1 MiB or past 100,000 rules is dropped (paths it ignores are
captured; Git reads up to 100 MB), a config without
`core.repositoryformatversion` but with a version 1 extension is refused where
Git reads it as SHA-1, `\v` or `\f` at a value's start and a NUL in a value
are refused, a loose reference that is a directory is reported rather than
falling back to `packed-refs`, a second symbolic level resolves to no commit,
and Git under a UTF-8 locale on macOS folds some Latin-1 bytes that these
readers, folding ASCII only, do not.

**Recorded divergences kept deliberately.** Includes are not followed (Git reads
repository-format keys without them too, but an include can set
`core.ignoreCase`); a negative `core.repositoryformatversion`, which Git reads as
version 0, is refused; `extensions.compatObjectFormat`, which Git accepts, is
refused as an extension these readers do not read. The differential harness
([[BPR-1455]]) classifies these with every other divergence.

## BPR-1457: fuzz oracles, adversarial timing and scale

[[BPR-1457]] measures K, the fixed first-party readers, on macOS. All timings
come from the dev preset (Debug, `-O0 -g0`, Apple clang) on an M-series Mac that
was building other trees at the same time, so they bound K's cost rather than
benchmark it; a candidate is compared with K only in the same build and run.

**Fuzz oracles.** `fuzz_git_output` (selectors 0, 1 and 3) now asserts more
than the absence of a crash. Selector 1: every index it parses, written back by
the test-only `tests/support/git_metadata_writers.hpp` with a zero trailer and
with the repository hash, parses to the same version, entries and flags.
Selector 0: every config it parses, written back canonically, parses to the
same entries. Selector 3: a fully escaped path matches exactly itself, `**`
matches every path and `*` exactly the paths without `/`. The same round trips
run on fixed inputs in `core_review_cycle_manifest`. New seeds:
`index-v4-sha1`, `index-v3-split-sha1`, `ignore-classes-folded`,
`attributes-macros` and `names-config-syntax`. The differential oracle against
a reference matcher (V) is [[BPR-1459]]'s, compiled into a test-only target.

**H6 (one reader call each, `bench_review_cycle_scale --adversarial`).**

| Case | ms |
|---|---|
| BPR-0970 shape: 6 KB of `*a` then `b` against a 4,095-byte name | 0.52 |
| the same, folded | 0.46 |
| 2,000 `**/` then `x` against a 2,047-segment path | 0.16 |
| 1,200 `**/a*` then `b` against the same path | 70 |
| 2,000 `*/` then `*b` against the same path | 0.11 |
| 400 `[[:alpha:]a-z]*` then `!`, folded | 0.17 |
| a 1 MiB `.gitignore` (60,000 star rules): load | 135 |
| that file against one path | 1.3 (1,963 before the byte prefilter) |
| 100,000 rules whose bytes all occur in the path: load | 174 |
| those rules against one path (no rule skipped) | 450 |
| a 50,001-link attribute macro chain: load and evaluate | 594 |
| a config of 65,535 entries | 230 |

Every case is under H6's 1 s bound for K. The one that was not, one path against
a 1 MiB rule file at 1.96 s, led to a sound prefilter in `review_cycle_ignore.cpp`:
a rule is skipped when the subject lacks a byte its literals require (literals
before the first bracket, folded under `core.ignoreCase`, escaped bytes as
written). The worst case the prefilter cannot skip is the 450 ms row.

**Scale (`--git --git-index-entries 250000`).** A 250,000-entry SHA-1 index
(21.2 MB) that `/usr/bin/git ls-files` reads: index parse median of five 787 ms
with the trailer checked and 174 ms under `index.skipHash` (a first, more
loaded run gave 1,619 ms); an ignore walk of all 250,000 paths against a
20-rule representative `.gitignore`, each directory once and then each file,
median of five 358 ms (55,699 ignored); peak resident memory 135 MB. These are
recorded for rule 3, not asserted; the ctest row `review_cycle_git_reader_scale`
runs the same code at 2,000 entries with the adversarial cases.

**Fuzz runs.** `fuzz_git_output_corpus` replays the 31 seeds clean, and a
campaign (`-seed=1457 -max_total_time=600 -max_len=65536 -timeout=10`, Apple
clang with LLVM 23's libFuzzer runtime, ASan and UBSan, `BPR_BUILD_QT=OFF
BPR_BUILD_TOUR_CLI=ON`) ran 5,284,057 inputs at about 8,800 a second with no
crash, sanitizer report, oracle failure or timeout; its slowest unit took under
a second and its peak RSS was 721 MB. The campaign wrote only to a scratch copy
of the corpus.
