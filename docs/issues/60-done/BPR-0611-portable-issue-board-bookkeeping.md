---
schema: "implement-rfc.card.v1"
id: "BPR-0611"
title: "Portable issue-board validation and bookkeeping"
status: "Done"
rfcs: []
depends_on: []
---

# Portable issue-board validation and bookkeeping

Implement the owner's requested implement-rfc workflow improvement. This is
workflow maintenance, not a product RFC or a change to RFC acceptance.

## Acceptance criteria

- One strict parser and schema-backed card validator serve the queue, mover,
  portable audit, and repository documentation gate. Board validation checks
  identity, RFC references, dependency graphs, and configured lane rules.
- The portable CLI reports RFC-linked work and deterministic readiness without
  inferring scope from prose or changing the board.
- A move preserves unrelated bytes, synchronizes lane metadata, repairs live
  links, protects captured records, and refuses invalid or competing writes.
  Dry runs are read-only; ordinary write failures roll back applied changes.
- The skill routes bookkeeping through the commands; generated adapters agree.

## Verification

- `python3 .agents/skills/implement-rfc/scripts/validate_portable_agents.py`:
  168 tests passed at the integration checkpoint (two pre-existing optional
  remote-envelope JSON Schema checks skipped because `jsonschema` is absent).
  Generated adapters passed their drift check. The new card-schema evaluator
  requires no optional package and its checks ran.
- `python3 -m unittest discover -s .agents/skills/implement-rfc/tests -p
  'test_issue_board.py'`: final 28 tests passed after adding post-publication
  failure and external-edit rollback cases. These include the public CLI on a
  temporary board with alternate project identifiers and lane names, strict
  schema errors, queue ambiguity, metadata/link preservation, lock contention,
  dry-run non-mutation, and ordinary failure recovery.
- `python3 tests/scripts/verify_documentation_tests.py`: 62 passed against the
  shared parser and board validator.
- `python3 scripts/verify_documentation.py --root .`: `DOCUMENTATION_OK`.
- Live `issue_board.py validate --json` passed without migrating existing cards;
  `queue --rfc RFC-BPR-0008` truthfully reported two concurrent in-progress cards.
  `audit_board.py --repo-root .` passed with only process/follow-up warnings.
- Final `render_runtime_adapters.py` and `validate_portable_agents.py --skip-tests`
  regenerated and validated all 17 generated files. The skill-creator
  `quick_validate.py` check and `git diff --check` passed.

The tool itself moves this card to Done. Temporary fixture directories and
transaction files are cleaned by their consumers; no worktree was created.
Unrelated concurrent product/dependency changes were excluded from this slice.
No product build matrix or cross-project checkout comparison was run. Actual
agent token savings are unmeasured. Moves are not crash-atomic across files;
captured evidence and unsupported Markdown extensions require explicit review.
