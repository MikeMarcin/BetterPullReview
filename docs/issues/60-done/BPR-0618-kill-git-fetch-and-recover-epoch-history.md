---
schema: "implement-rfc.card.v1"
id: "BPR-0618"
title: "Kill Git fetch and recover durable epoch history"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0616"]
---

# Kill Git fetch and recover durable epoch history

Completed on the Mac in `21255d9c`, following the installed-Git preparation
repair in `081fc9d1`. The original failing harness is `cf4730da`.

## Acceptance and result

The real epoch-maintenance fixture uses the production application host,
installed Git cache and SQLite catalog with local fake smart-HTTP pack bytes.
The parent keeps each child alive at its checkpoint, verifies forced death,
reaps it and restarts through ordinary host acceptance.

| Required boundary or invariant | Direct evidence |
|---|---|
| Kill during a partial pack fetch | The receiving file contains exactly 115 of the fixture's 230 pack bytes at death. Fresh host recovery passes. |
| Kill after completed spool acquisition | Recovery exact-cleans the owned, unrecorded completed spool and ordinary acquisition reaches ready. |
| Kill after installed-Git revision preparation | The install fact is already journaled; recovery passes through normal abort/retry. |
| Kill after the first visible epoch | Recovery preserves its complete original epoch identity. |
| Exact comparison and no duplicated history | Every course retains the expected commit pair, one visible epoch and matching Git ref pair, with no unfinished operation. Another process restart retains the same result. |
| Independent persisted-state integrity | Git fsck/ref inspection, SQLite integrity and foreign keys pass; no unfinished receiving/completed spool remains. |

## Implementation and ownership

The host now persists receiving-file ownership before payload acquisition and
installation facts before staging refs/spool deletion. Both witnesses run on
the effect worker. Missing, repeated, foreign or conflicting facts cannot
advance the selected path. Cancellation or storage refusal stops acquisition.
The core and Qt presentation remain unchanged.

Catalog schema v3 adds `append_receiving`: operation, opaque exact file identity
and payload limit. The v2 migration validates the legacy schema, adds the table
and updates version metadata transactionally, preserving existing authority,
epoch and operation identities. The existing v1-to-v2 migration remains a
separate atomic step. Legacy operations gain no invented ownership. Normal
terminal compaction and cache cleanup remove receiving facts.

The Git adapter can exact-clean a receiving or completed name with that durable
ownership and bound. It cannot import an unrecorded completed pack or delete
an unrelated file by filename. Tests substitute files after cache reopen,
verify refusal preserves both original and replacement, then restore the exact
identity and prove idempotent cleanup. Sink destruction occurs after the
operation lock releases, including refused initialization.

Changed owners: application cache/catalog ports and epoch host, concrete Git
and SQLite adapters, their direct fixtures, the shared bounded process-kill
helper and `docs/architecture/git-and-epoch-model.md`.

## Verification

All results below use `ctest --test-dir build/dev --output-on-failure -R` with
exactly the named selectors. The affected fixture targets built successfully.

| Selectors | Result |
|---|---|
| `epoch_kill_recovery` | All four courses pass, 107.67 s, including independent inspection and another restart each. |
| `sqlite_epoch_catalog_storage_contract`, `sqlite_epoch_catalog_behavior_contract`, `sqlite_anonymous_epoch_authority` | 3/3, 41.11 s; exact v1/v2 migration, rollback before commit, populated-catalog identity preservation, receiving record/replay/reopen, conflict/limit refusal and compaction. |
| `application_epoch_host`, `git_cache_contract`, `epoch_maintenance_integration` | 3/3, 231.26 s; host refusal/cancellation, concrete file substitution, exact cleanup and real prune/delete. |
| `application_epoch_catalog_contract`, `architecture_boundaries`, `architecture_boundary_contract` | 3/3, 69.00 s. |

Documentation validation, all 62 documentation tests, scoped clang-format,
code-simplifier inspection and `git diff --check` pass. The initial regression
failed fetch/spool/prepared recovery; each now has direct passing evidence.
The epoch child allowance is 30 seconds after a successful prepared restart
measured 15.67 seconds; the shared review-publication allowance stays 15 seconds.
This is a fixture deadline, not a product performance budget.

## Limits and continuation

These four named checkpoints do not cover death before receiving ownership
commits, interruption inside `index-pack` or a SQLite transaction, AI-session
kill, or power loss. An unjournaled empty file or legacy spool remains protected.
BPR-0619 continues with live SQLite transaction interruption; the other RFC-0013
criteria remain open in BPR-0617. No RFC closeout is claimed here.

All new roots were contained temporary fixtures and were removed after their
consumers ended; children were reaped. The existing `build/dev` remains. No
separate build root, production app launch, credential access, sign-out,
GitHub Actions, push, remote mutation or paid CI occurred. Other platforms,
the full matrix and tidy were not run.
