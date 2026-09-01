# Portable Delegated Execution

Use this reference only when bounded delegation will reduce primary-context
load, improve elapsed throughput, isolate writable test output, or add a
meaningfully independent check. Delegation is optional; agent count is not a
quality metric.

## Authority

The primary RFC integrator alone owns:

- interpretation of the live RFC and acceptance criteria;
- `RFC_BASE_SHA`, issue allocation, board transitions, and RFC ordering;
- staging, commits, pushes, and integration;
- verification sufficiency, documentation impact, and RFC status.

A delegated result is an input. It cannot expand acceptance, move cards, commit,
or close an RFC.

## Roles

### `rfc-utility`

Use for exact locating, inventories, prescribed read-only commands, schema or
adapter drift checks, and log condensation. Promote to `rfc-scout` when the
task requires semantic interpretation.

### `rfc-scout`

Use for bounded ownership, execution-path, package-selection,
verification-surface, documentation-impact, or first-cause analysis. A scout
should replace the corresponding broad primary scan; the primary confirms only
material conclusions.

### `rfc-worker`

Use for one current issue-card slice after ownership, non-goals, acceptance
criteria, and focused verification are clear. Give an exclusive file/path
scope. Only one agent writes a shared checkout at a time. The worker does not
commit or mutate tracker/RFC state.

### `rfc-verifier`

Use when an exact selected command set benefits from separate execution or a
writable build root. Isolation is optional unless the user explicitly requires
it or a concrete collision/source-integrity risk makes it useful.

The verifier:

- runs only the supplied candidate, commands, and claims;
- does not edit source, agent documentation, trackers, RFCs, or generated
  repository files;
- returns concise commands, exit statuses, first material diagnostics, relevant
  artifact paths, and uncertainties;
- does not create seals, hash-bound evidence packages, replay records, or
  manifests.

The primary consumes the result and reruns only a check affected by a later
correction. It does not repeat unchanged passing commands.

### `rfc-reviewer` And `rfc-critical-reviewer`

Use an independent reviewer when explicitly requested or when it is likely to
add discriminating evidence for a changed wire/protocol contract,
persistence/migration behavior, authority/security boundary, concurrency
primitive, or destructive migration. Use the critical role when the risk and
expected value justify deeper counterexample search.

Give the reviewer the live acceptance criteria, final bounded diff, commands
already run, and concise result locations. Do not preload the implementation
transcript or ask it to ratify the implementation narrative.

On a Claude driver, prefer the Codex route for an ordinary reviewer of a
Claude-authored implementation, and run a critical review on both provider
routes and reconcile; see Lane Runtime Selection in
[model-routing.md](model-routing.md) and the lane mechanics in
[codex-lanes.md](codex-lanes.md).

A blocking finding must be an evidence-backed failure of an existing acceptance
criterion or a critical regression in the changed surface. Unsupported
suspicions, broader hardening, alternate designs, and requirement expansion are
non-blocking and become follow-up candidates. A correction receives only a
narrow changed-surface recheck; it never starts a whole-candidate review loop.

## Choosing Delegation

| Work shape | Default |
|---|---|
| Small, directly verifiable change | Primary agent |
| Mechanical read-only inventory or command | Optional `rfc-utility` |
| Broad semantic read-only question | Optional `rfc-scout` |
| Clear bounded implementation slice | Optional single `rfc-worker` |
| Exact separate test/build command set | Optional `rfc-verifier` |
| Named high-risk trigger or explicit request | Consider one reviewer |
| Missing safe runtime capability | Execute serially in the primary agent |

Parallel read-only agents must own different questions. Do not request duplicate
same-prompt scans merely to obtain a vote. Stop delegation after it has answered
the bounded question.

## Assignment Contract

Name the role and purpose, then provide only what the role needs:

```text
Role and purpose:
Current card or review target:
Repository root and branch/candidate:
Question or acceptance claims:
Allowed paths or roots:
Explicit non-goals:
Applicable instructions:
Exact commands or evidence:
Requested result:
```

Use a role-prefixed runtime name such as `rfc_scout_backend_selection` or
`rfc_utility_cache_inventory`. Require the startup banner
`Role: <role> | Model: <model> | Effort: <effort>` before analysis so routing
mistakes are visible. Role-to-model assignment and its rationale live in
[model-routing.md](model-routing.md) and `config/routing.json`.

For workers, state that they must not commit, move cards, allocate IDs, or
change RFC status. For verifiers, name the candidate, command/claim set, writable
output root if any, and expected concise result.

## Result Contract

Use the role-specific compact format in `config/roles.json`. Every result
should:

- lead with status or blocking failures;
- cite exact paths, symbols, commands, and result locations;
- distinguish fact, uncertainty, and unsearched surfaces;
- avoid raw logs, copied RFC prose, and broad repository summaries.

Unexpected source writes invalidate a read-only or verifier result. Preserve
unrelated user changes and inspect Git state after delegated work.

## Runtime Fallback

When an adapter, nested agent, read-only sandbox, or per-role model is
unavailable, perform the bounded work serially while preserving the same
acceptance boundary. A fallback may reduce speed or independence; it does not
create new verification requirements. Report the omitted optional lane without
blocking closeout.
