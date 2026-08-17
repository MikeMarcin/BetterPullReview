---
id: BPR-0170
title: Persist enrollment across restarts
status: Done
rfc: RFC-BPR-0010
depends_on: [BPR-0162]
---

# Persist enrollment across restarts

BPR-0162 built the retention *policy* — what is kept, what departs, what the
queue shows — and no durable store. The register lives for as long as the
process does.

That is a real gap, not a technicality. Observer enrollment is supposed to
persist "until closure or manual removal", and a queue rebuilt from scratch on
every launch would re-derive membership from whatever the paths look like
*now*, which is precisely the recomputation BPR-0162 exists to avoid. The
policy would be correct and the behaviour would be wrong.

## What is needed

A store beside the review ledger, following the pattern RFC-BPR-0007
established: its own SQLite database under `state/v1`, a `PRAGMA user_version`,
forward-only migration, and a typed failure enumeration. The state directory
comes from `detail::open_state_directory`, which BPR-0132 made the one
bootstrap.

## Required direct evidence

- An item enrolled by a path rule, with the path then removed from the pull
  request, is still enrolled after a separate process opens the store — a
  restart claim needs a second process, not a reopened handle.
- A departed item keeps its departure reason across the restart, so the
  reviewer is told what happened rather than seeing it silently absent.
- The store holds no source and no comment body.
- Deleting the enrollment store loses only enrollment, exactly as deleting the
  review ledger loses only receipts.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What was done

`detail::queue_enrollment_database`: its own SQLite file beside the review
ledger, `PRAGMA user_version`, forward-only refusal, `STRICT` tables, and a
typed failure. The application id is `0x42505251` — "BPRQ", distinct from the
ledger's "BPRD", so pointing one store at the other's file fails at the
identity check rather than at the first query for a table that is not there.

### The schema version is 1, and honestly so

The review ledger starts at 2, and its comment is right that a migration
mechanism whose only version is the initial one has never migrated anything.
Inventing a second version here to satisfy that would be theatre: a migration
that does nothing exercises nothing.

What *is* exercised is the refusal, and that took two attempts.

### A vacuous test, caught by its own break

`test_a_newer_database_fails_closed` originally asserted only that the open
failed. Removing the forward-only guard **did not fail it**: without the guard
the open still fails, because the first `CREATE TABLE` hits a table that
already exists. The test would have passed against a build with no
forward-only policy whatsoever.

It now asserts the *reason* — `schema_mismatch` rather than whatever the
collision produces — and the break lands. This is the second time this exact
trap has appeared in this repository's storage tests; the shape is "the wrong
code fails too, for a different reason", and only asserting the reason
distinguishes them.

### Evidence

| Test | Asserts |
|---|---|
| enrollment survives a separate process | A forked child writes; this process reads back the rule that brought the item in and the departure reason of one that left |
| a newer database fails closed | Refused with `schema_mismatch`, and the file is unchanged afterwards |
| the schema holds no repository content | Every column of every table checked against eight prohibited names, read from the schema the build creates |
| deleting the queue leaves the ledger intact | Reading progress survives clearing the queue |
| storing twice replaces rather than duplicates | One row, carrying the later state |

Four breaks, each landing on its own assertion:

| Break | First failure |
|---|---|
| forward-only guard removed | `because this build will not read a schema it does not know` |
| the departure reason is not stored | `with the reason it left` |
| storing twice duplicates the row | `carrying the later state` |
| the schema gains a `comment_body` column | `no column names anything a pull request contains` |

### On the restart claim

A forked child does the writing and the parent — a different process, with its
own SQLite library state and its own page cache — does the reading. Reopening a
handle in one process would prove the rows are in memory somewhere, which was
never in doubt.
