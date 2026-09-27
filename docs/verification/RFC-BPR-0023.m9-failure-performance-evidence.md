# RFC-BPR-0023 M9 failure, security and performance evidence

Scope: BPR-0588, 2026-09-27, macOS arm64 local fixture runs. This record
reuses passing M3–M8 evidence where the tested code has not changed and adds
one integrated nine-test pass after M8 plus direct M9 measurements. The tests
used synthetic repositories and local process/SQLite fixtures only.

## Focused integrated pass

The `dev` preset built the private core, SQLite store/host, GitHub publication,
Qt panel, real-child canary, shipped app and agent canary targets. The following
CTest selection passed **9/9 in 169.08 seconds** on the integrated candidate:

```sh
ctest --preset dev -R '^(core_private_review|sqlite_private_review_storage|application_private_review_host|qt_private_review_panel|application_private_review_(native|file|retry)|github_review_comment_permalink|qt_agent_launch_canary)$' --output-on-failure
```

| Evidence lane | Direct observation |
|---|---|
| Core and SQLite | Typed actors, exact scopes/subjects, immutable revisions, 1 MiB assistant body via 512 KiB continuations, 64-row keyset pages, quotas, CAS/high-water, v1–v6 migration, purge/loss and killed transactions. |
| Host and real child | Selected content alone reaches deferred child input; edited, deleted and cross-review sources refuse. Parent credential-like environment values do not enter the allowlisted child environment. |
| Qt and public app | Semantic private/local labels, bounded changed-row model updates, real composition-root seed/compose/publish across restarts, native and file representation, and lost-result retry. |
| Publication adapter | Separate Publish path, prepare/cancel/refusal zero-POST cases, real competing SQLite writer after remote fixture success, failed refresh, quota reserve, reopen and read-only reconciliation without second POST. |

The SQLite course used actual POSIX `SIGKILL` before and after the transaction
commit points for private content, draft linkage and publication lineage, plus
a kill during v6-to-v7 migration. On reopen it observed either the complete
pre-state or complete post-state. The publication course separately exercised
remote fixture success followed by a local SQLite lock and repeated repair.
These tests do not establish an unchanged live GitHub state; that read remains
on parked BPR-0584.

## Measured bounded work

Commands were run on the built test executables with macOS `/usr/bin/time -l`.
Numbers are observations for these deterministic fixtures, not cross-platform
or worst-case performance guarantees.

| Measurement | Result |
|---|---:|
| Assistant body payload retained in one thread | 67,108,864 bytes (64 MiB) |
| SQLite database before body-fill fixture | 4,096 bytes |
| SQLite database after body-fill fixture | 67,473,408 bytes |
| Measured database growth | 67,469,312 bytes |
| SQLite storage course wall time / max resident set | 12.13 s / 29,147,136 bytes |
| SQLite storage course peak footprint (`time -l`) | 25,067,904 bytes |
| Keyset thread/revision page bound | 64 rows; query-plan assertion found indexed seek without temp sort or retained-row scan |
| Body continuation bound | 524,288 bytes; exact 1 MiB body advances in two chunks |
| Qt active turn rows / changed rows | 64 / 1 |
| Qt model signals across insertion and ten edits | 64 row inserts, 10 data changes, 0 resets |
| Qt selected row under one changed-row update | Persistent index and selection retained |
| Qt 200% course wall time / max resident set | 0.15 s / 61,177,856 bytes |
| Qt 200% course peak footprint (`time -l`) | 30,835,432 bytes |

The 64 MiB body write leaves the next one-byte write refused as a whole, with
the durable identity frontier unchanged. Other store tests check the count and
byte quotas for every retained kind and the signed high-water limit. The host
checks selected-source and total invocation-input bounds; no body page or
composition source is silently truncated.

## Secret and authority boundary

The real-child canary compared the complete observed environment, argv and
stdin with the allowlist while placing credential-like values only in its
parent. The SQLite course scanned all files in its owned root, including WAL,
for ignored credential envelope canaries. The host course checked that an
unselected private body was absent from actual invocation input. A separate
format scan over 25 changed M9 files found zero GitHub classic/fine-grained,
AWS access-key, OpenAI project-key or private-key-header patterns. This scan is
supplementary; it cannot establish that arbitrary secret strings are absent.

## RFC criterion matrix

| AC | Local direct evidence | Remaining external obligation |
|---|---|---|
| 01 | Core, SQLite restart, public three-process reopen | — |
| 02 | Host selected side-chat invocation, real child, public reopen | — |
| 03 | Core exhaustive actor, SQLite migration, Qt history | — |
| 04 | Core successor/digest, SQLite byte retention, Qt history | — |
| 05 | Protocol and SQLite provenance, real-child facts, Qt conflict text | — |
| 06 | Core/store multiple run IDs, pruning/restart and retained findings | — |
| 07 | Core semantic identities, host stale admission, Qt refusal | — |
| 08 | Core/host modes, Qt mode intents, public composition root | — |
| 09 | Public compose phase reports zero mutation calls before Publish | — |
| 10 | Core partitions, Qt two-target choice/Split, retained source digest | — |
| 11 | SQLite atomic draft link, public restart and accepted body | — |
| 12 | Synthetic public path and existing mutation coordinator/adapter | Qualifying designated or signed publication trace on BPR-0584 |
| 13 | Local/server receipt fixture and independent fixture JSON read | Independent read of real qualifying server body and actor on BPR-0584 |
| 14 | SQLite v1–v6 exact migration and canonical schema inventory | — |
| 15 | Store/host quota and page tests; measured database, memory and Qt work | — |
| 16 | Host exact opt-in/revalidation and real-child unselected-content canary | — |
| 17 | Local kill/purge/loss and synthetic shared-authority comparison | Independent unchanged GitHub read on BPR-0584 |
| 18 | Public and Qt rendered scale/theme/keyboard evidence in [BPR-0589 record](RFC-BPR-0023.m9-rendered-evidence.md) | Final independent review at M10 |

The precise live blocker is **BPR-0584**: no designated safe GitHub/GHES
repository, authenticated reviewer and permitted comment target have been
provided, and no signed recorded fixture with an independently readable server
response has been supplied. An independent public body/actor reader and an
unchanged-state read after local deletion/crash are therefore unavailable. The
local fixtures satisfy the repository's safe-mutation policy for this card but
do not close AC-12, AC-13 or AC-17. The RFC and M9 external exit remain pending.
