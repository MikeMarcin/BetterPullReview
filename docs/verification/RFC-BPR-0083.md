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
