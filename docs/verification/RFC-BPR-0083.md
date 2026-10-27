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

## BPR-1455 and BPR-1456: the differential harness and canary course

`test_git_metadata_harness` (ctest `git_metadata_harness`) builds 44 corpus
repositories with the installed Git and asks an isolated Git oracle
(`GIT_CONFIG_NOSYSTEM=1`, an empty `GIT_CONFIG_GLOBAL`, empty `HOME` and
`XDG_CONFIG_HOME`, `GIT_OPTIONAL_LOCKS=0`) and each candidate the C1–C6
questions; C5 is also checked through a real capture of each repository
(`C5-capture`). Candidates implement
`tests/workspace_contract/git_metadata_candidate.hpp`, whose answers are
standard types only; K is `git_metadata_first_party.cpp`, composed as capture
composes the readers. `--report FILE` writes a TSV, `--case NAME` runs one case
and `--large` adds the 250,000-entry case.

**K on macOS (Git 2.50.1), after the follow-ups below:**

| Component | Agree | Diverge (classified) | Refused (classified) | Skipped |
|---|---|---|---|---|
| C1 index | 38 | 0 | 5 | 1 |
| C2 format | 37 | 2 | 4 | 1 |
| C3 references | 38 | 1 | 4 | 1 |
| C4 discovery | 42 | 1 | 0 | 1 |
| C5 ignore | 34 | 3 | 6 | 1 |
| C5 through capture | 33 | 2 | 7 | 1 |
| C6 `filter` | 38 | 1 | 4 | 1 |

Unclassified divergences: 0. The skipped case, `case-distinct-files`, needs a
case-sensitive file system. `--large --case large-250000` (500 directories of
500 committed files) agrees for C1 on all 250,000 entries and for C2–C6 (C5 and
C6 on 2,000 sampled paths); it skips the capture cross-check by design. Every checkout was byte-identical afterwards (43)
and nothing was written.

**H1 registry for K.**

| Case | Components | Class | Reason |
|---|---|---|---|
| `reftable` | C1, C2, C3, C5, C6, capture | refusal | owner's Q2 answer (a), fix 1 |
| `format-compat-object-format` | same | deliberate | Git accepts it; K refuses it by name as an extension it does not read |
| `format-negative-version` | same | deliberate | Git reads it as version 0; K refuses it |
| `format-missing-version` | same | deliberate | without the version key Git does not check extensions; K treats it as version 0 and refuses a version 1 extension |
| `split-index` | C1, capture | refusal | the shared index is never opened: a split part with stripped-name entries is refused, otherwise it is read and marked partial (which happens depends on Git's stat timing) |
| `config-include`, `config-include-if` | C2, C5, capture | deliberate | includes are not followed |
| `reference-directory` | C3 | refusal | a loose reference that is a directory is reported, with no `packed-refs` fallback |
| `config-bad-ignorecase` | C4 | deliberate | discovery reads no config; Git dies before printing the directories |
| `ignorecase-latin1` | C5 | deliberate | K folds ASCII; Git under a UTF-8 locale on macOS folds Latin-1 (under `LC_ALL=C` it does not) |
| `attributes-deleted-from-worktree` | C6 | deliberate | capture reads the worktree's attributes; Git falls back to the index's copy |
| `ignore-over-1mib`, `ignore-over-100000-rules` | C5, capture | refusal | capture refuses by name (below) |

**Reader changes the harness led to.** (1) A `.gitignore` or `.gitattributes`
past capture's bound (1 MiB or 100,000 rules) used to be dropped, so capture kept
the files it ignores; capture now refuses with `rule_file_bound` naming the file,
and `info/exclude` and `info/attributes` likewise (test:
`capture_refuses_rule_bounds`). (2) A HEAD two symbolic levels deep resolved to no
commit, which looks like an unborn branch; both resolvers now follow symbolic
references as deep as Git (`SYMREF_MAXDEPTH`, five reads), and
`head-symbolic-too-deep` agrees with Git on both sides of the limit. This goes
past fix 7's "one level" to remove a silent divergence.

**Negative control.** With fix 4 reverted in `bracket()` (a class never
matches), the harness exits 1 with 17 unclassified divergences, for example
`C5 ignorecase-false "a b" oracle=ignored answer=kept` and
`C5-capture "7Q" oracle=ignored answer="not excluded, captured"`; the source was
restored byte for byte.

**Canary course (BPR-1456), K on macOS.** For every component C1–C6:
containment, no execution and no writes pass. The `include.path` FIFO outside
the root was never opened; the rules behind symlinked `.gitignore` and
`.gitattributes` files pointing outside the root were not followed; the hooks,
`core.fsmonitor`, filter and textconv logger never ran; the checkout and `.git`
stayed byte-identical with no lock files. A positive control under real Git
fires the FIFO and fsmonitor canaries, so they can detect an open or a run. The
FIFO canary polls every 200 µs, so a non-blocking open that closes at once could
escape it; a blocking reader is always caught.

## BPR-1458: candidate L, libgit2

**Acquisition.** `BPR_EVALUATE_LIBGIT2` (default off, `tests/CMakeLists.txt`)
adds `tests/evaluation/`, whose `cmake/BprLibgit2Provider.cmake` fetches libgit2
1.9.7 through `bpr_fetch_verified_archive` (from `BprRe2Provider.cmake`):
`https://codeload.github.com/libgit2/libgit2/tar.gz/refs/tags/v1.9.7`, SHA-256
`1a4fbe7589e814777ae76b64734ad80f4ecad22cd33a22682a2aaea4ae5375e7`, 7,769,162
bytes, COPYING SHA-256
`e0938121c0554985fe17196ac38fc590d149268e9c46e0c0e7a2eae1354fae71`, re-extracted
under `build/_bpr/libgit2`. One `cmake -P` download learned the pin (v1.9.8 to
v1.9.15 answered 404). libgit2 builds static with no HTTPS, SSH, NTLM, GSSAPI,
CLI, tests, examples or fuzzers, bundled PCRE2 for regex and builtin SHA-256,
and installs nothing. Integrating it needed `CMAKE_POLICY_VERSION_MINIMUM 3.10`,
`CMAKE_POLICY_DEFAULT_CMP0126 NEW` (otherwise libgit2's cache writes silently
re-enabled SecureTransport HTTPS and system regex), removing its cached C90
standard, and the system zlib: libgit2's bundled zlib and core's pinned zlib
both define `_z_errmsg`. With the option off, configure mentions no libgit2,
`build.ninja` has no libgit2 reference, nothing is staged under `_bpr`, and the
shipped `app_bpr` links no git2 library (its link command hashes the same with
the option on and off).

**Harness, L beside K (`git_metadata_harness` with the option on, macOS).**

| Component | Agree | Diverge | Refused | Skipped |
|---|---|---|---|---|
| C1 | 35 | 2 | 7 | 1 |
| C2 | 37 | 2 | 5 | 1 |
| C3 | 36 | 3 | 5 | 1 |
| C4 | 37 | 2 | 5 | 1 |
| C5 | 33 | 5 | 5 | 2 |
| C6 | 31 | 5 | 7 | 2 |

Classified refusals: SHA-256 repositories in the shipped build (`unknown
object format 'sha256'`), reftable, `compatObjectFormat`, and split and sparse
indexes (C1, C6: `unsupported mandatory extension`). **Silent divergences**
(registry class `silent`, so L is not eligible for the component under H1):
a version 0 repository naming `extensions.refStorage` read as files (all
components; Git refuses it); a config Git refuses (`core.ignorecase = maybe`)
read anyway (all components); symbolic references followed past Git's five
reads (C3); `config/local**/*.pem` keeping a path Git ignores, the literal-prefix
defect BPR-1454 fixed in K (C5); a directory-only attribute pattern applied to a
directory path Git leaves unspecified, and C-quoted attribute patterns not read
(C6). L's adversarial ignore case is not run (C5, C6): libgit2's
`src/util/wildmatch.c:198-199` returns `WM_NOMATCH` where Git aborts, and
`tool_libgit2_probe` measured `*a` repeated 14 times at 4.8 s, with 18 or more
unfinished after 10 s (K: 0.5 ms for 3,000 repetitions).

**Containment, execution and writes (H2, macOS).** The canary course: for every
component L opened the `include.path` FIFO outside the root; C2 followed the
outside include and C5 and C6 followed the symlinked rule files out of the root.
A dylib shim logging every path call (`libgit2_course.py`) shows why: the
symlinked rule files are opened with `O_CLOEXEC` only (libgit2's source has no
`O_NOFOLLOW`; `attr_file.c:155-161`, `futils.c:98-100`), includes are joined
unrooted and read by path (`config_file.c:523-529`, `:147`), the index is read by
path (`index.c:696`), and `core.excludesFile` and `core.attributesFile` targets
outside the root are opened. Global files stayed unread with all four search
levels set to empty and the home directory cleared. L ran no process (hooks,
fsmonitor, filter and textconv canaries never fired; the only process-creating
code, `transports/ssh_exec.c:258`, is not built) and wrote nothing.

**SHA-256 and reftable (H3).** The shipped configuration refuses SHA-256; the
experimental build (`BPR_EVALUATE_LIBGIT2_SHA256=ON`) reads it and otherwise
answers the same, and libgit2's changelog moves SHA-256 out of experimental only
in 2.0, with API and ABI changes. 1.9.7 contains no reftable code.

**Cost record (macOS arm64).**

- **License (SPDX):** `GPL-2.0-only WITH GCC-exception-2.0 AND LGPL-2.1-or-later
  AND BSD-3-Clause WITH PCRE2-exception AND MIT AND BSD-3-Clause AND
  BSD-2-Clause AND CC0-1.0 AND LicenseRef-libgit2-wildmatch`, from COPYING and
  the compiled parts' headers. The linking exception's text is the GCC exception
  2.0's. xdiff is LGPL-2.1-or-later (`deps/xdiff/xdiff.h`), which COPYING does
  not mention, and 33 `xdl_` symbols are linked. Both points need legal review.
- **Trial packaging:** `tests/evaluation/libgit2/dependencies.libgit2-draft.json`
  through `release.verify` with a patched component list:
  `RELEASE_SOURCE_ASSET_OK` ("legal_approval: pending separately"); a wrong
  SHA-256 gives `corresponding-source-digest-mismatch`. `packaging/dependencies.json`
  is unchanged.
- **Build time of libgit2 alone** (one clean run each on a busy machine):
  Release 28.5 s at `-j1`, 3.8 s at `-j10`; Debug `-O0 -g0` 21.3 s and 2.7 s.
- **Stripped binary size** (the same program with K, and with K and L):
  Release +1,249,264 bytes (108,184 to 1,357,448); with `-Wl,-dead_strip`, which
  the product does not use, +686,256; Debug +2,008,352. L also links
  `/usr/lib/libz.1.dylib`.
- **Toolchain:** a C90 compiler and CMake with the three workarounds above;
  shipping it needs the system zlib or a symbol-prefixed copy (core's pinned
  zlib has no deflate).
- **Releases and advisories** (from the archive's changelog and SECURITY.md,
  which carry no dates): eight 1.9 releases, three of them security releases
  (1.9.2, 1.9.5 with five CVEs, 1.9.7); fixes promised for the latest two series;
  1.9 is the last 1.x series and 2.0 will change API and ABI.
- **Integration size:** 102 lines of provider, 38 of CMake and 298 of candidate;
  evaluation-only tools 160 (probe), 61 (size), 201 (V differential) and 614
  (course).
- **Windows and Linux:** not measured, by the owner's 2026-10-26 amendment.

A build tree made with the option on inside the checkout fails
`architecture_boundaries`: its recursive scan of `CMakeLists.txt` files reaches
the staged libgit2 sources under `build/`. The option-off tree is unaffected;
an evaluation build outside the checkout, or anchoring that scan as the script
already does elsewhere, avoids it.

## BPR-1459: candidates V and G

**V, a vendored matcher.** libgit2's `wildmatch.c` header claims GPL-2.0 with the
linking exception, while COPYING (lines 1074–1098) gives Rich Salz's BSD-style
terms with a binary acknowledgement ("This product includes software developed
by Rich Salz"); neither calls it public domain, as the RFC's drafting assumed.
Both sets of terms bind: libgit2's and Git's modifications under GPL-2.0 with
the exception, the original under Rich Salz's terms. `tool_wildmatch_differential`
(evaluation-only, linked against the verified archive, never core or the
product) compared it with `core::wildcard_match` on 7,000,000 generated pairs in
the C locale (seeds 83, 1, 2, 3): full agreement with and without folding. In an
`en_US.UTF-8` process (`setlocale(LC_ALL, "")`, as a Qt application does) it
disagrees on 486 exact and 1,096 folded pairs per million, one class: the C
library's character classes treat bytes such as 0xC9 and 0xE9 as letters and fold
them. It also carries the exponential star behaviour above. Vendoring it into core
would need the owner's approval for that code (Q1), the abort fix, ASCII-only
classes and the dual notices; it adds no capability K lacks. A reftable reader has
no libgit2 source to vendor in 1.9.7. **V is not eligible as is (H6), and at best
eligible pending the owner's approval and those changes.**

**G, gitoxide (desk check; nothing downloaded or linked; from the evaluator's
knowledge, not re-checked against upstream).** MIT OR Apache-2.0; a second
toolchain (rustup, cargo and a recent stable Rust) on macOS, Windows and Linux,
plus CMake–cargo glue; no C API, so BPR would write and maintain the FFI, with no
type crossing the port; dozens of `gix-*` crates and their dependencies, each
needing an SPDX entry, notices and corresponding source per release; fuzzing and
sanitizers through cargo-fuzz and nightly Rust, outside the CMake and ctest
presets; no released reftable support or default-build SHA-256 known. **Rejected:**
its toolchain, FFI and packaging cost exceeds K's fix cost for every component,
and it offers no capability in scope that K lacks. Its tests may still serve as a
semantic reference.

## Owner amendment, 2026-10-26: station halves

The owner accepted the
[proposed amendment](../rfcs/assets/git-metadata-readers-v1/proposed-owner-amendment-2026-10-26.md).
L's Windows and Linux measurements are dropped. For K on Linux, the v0.3.4
Ubuntu 26.04 Release and Debug suites at `62bfc09f6` (Git 2.53) pass
`git_metadata_harness`, `review_cycle_git_readers`,
`review_cycle_git_reader_scale` and `core_review_cycle_manifest`. The
harness row passes only with no unclassified divergence, no write and a
passing canary course. CTest truncated the passing output at 1 KiB, so the
per-component `GMR summary` and `GMR canary` lines are not retained. For K
on Windows, the same rows pass in the v0.3.4 Windows 11 suites (Git 2.55),
but the canary course reports "skipped" there. Windows containment for K
stays on [[BPR-1462]].
