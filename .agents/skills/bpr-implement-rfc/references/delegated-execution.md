# Portable Delegated Execution

Use this reference when a BetterPullReview RFC is broad enough that focused delegation can preserve the primary agent's context, improve independent review, or reduce elapsed time. The portable policy standardizes intent and authority; Claude Code and Codex supply the current execution mechanism.

## Objectives

Delegation is successful only when it improves at least one of these without weakening another:

- implementation quality;
- elapsed throughput;
- expensive primary-agent context and token cost.

Do not optimize for agent count or aggregate parallel activity. A subagent normally adds total model work, so it must replace work the primary agent would otherwise perform, isolate noisy intermediate output, or add a meaningfully independent quality check.

On long multi-commit runs, weigh the third objective heaviest: integrator context spent absorbing raw scan output is the direct precursor of lost run state, so a delegation that merely keeps large reads out of the primary context usually pays for itself.

## Primary-Agent Authority

The primary agent is the BetterPullReview RFC integrator. It alone owns:

- interpretation of the live RFC and acceptance criteria;
- `RFC_BASE_SHA`;
- issue-ID allocation and `docs/issues/NEXT_ID`;
- issue-board folder transitions and front-matter alignment;
- task and RFC ordering;
- staging, commits, pushes, and integration;
- the verification matrix and documentation ledger;
- sufficiency of evidence;
- RFC status and `Auto-Verified` closeout.

Delegated agents may gather evidence or implement one bounded current slice. Their conclusions are inputs to the integrator, not authoritative tracker or closeout decisions.

## Session Identity

Give every delegated session a runtime-visible name beginning with its semantic
role. Prefer the exact role prefix, such as `rfc-scout-public-path`. When a
runtime permits only identifier-style names, normalize the prefix to
underscores and retain a short purpose suffix, such as
`rfc_scout_public_path`. Never use a purpose-only name that hides the role.

At the start of every newly spawned or restarted session, before any tool call
or task work, require the agent's first outward status message to use this
field layout:

```text
Role: <role> | Model: <model> | Effort: <effort>
```

Use the effective runtime-selected model and effort when the runtime exposes
them. Use `runtime-not-exposed` for a value the runtime does not disclose and
`not-supported` when the selected model has no effort setting. If a runtime
cannot set a separate session name, use the native agent definition whose name
is the semantic role and record that limitation in the integrator's final
delegation summary. Apply the same contract to nested `rfc-utility` sessions.

## Default Roles

Use the generated native agents when available. Their shared semantics live in `config/roles.json`.

### `rfc-utility`

Use for exact, low-judgment work that has a direct observable result:

- locating files, symbols, CMake targets, presets, aliases, instruction documents, RFCs, or issue records;
- inventorying changed files, board state, `docs/issues/NEXT_ID`, manifests, schemas, fixtures, or generated adapter drift;
- running an exact command list and reporting exit status and first failure;
- collecting pass/fail evidence without deciding whether it is sufficient;
- reducing logs to failing command, test, diagnostic location, and artifact path.

The utility agent must not decide architecture, ownership, implementation strategy, root cause, verification sufficiency, or closeout. Promote the assignment to `rfc-scout` as soon as semantic interpretation is required.

### `rfc-scout`

Use for read-heavy semantic work that would otherwise pollute the primary context:

- locating ownership and applicable instructions when ownership requires interpretation;
- tracing the selected path through functional core, application host, provider/Git/SQLite adapters, Qt presentation, composition, tests, and packaging;
- auditing CMake presets, feature selection, provider/profile selection, fixture substitution, exports, and public launch paths;
- finding and judging candidate direct verification surfaces;
- diagnosing the first causal failure and likely owning scope;
- identifying product, architecture, security, UX, operator, and agent-documentation impact surfaces.

A scout should replace the corresponding primary-agent exploration. The primary agent performs targeted confirmation rather than repeating the entire scan.

### `rfc-worker`

Use only after the current issue-card slice, ownership, architecture, non-goals, and focused verification are already clear. The worker may edit the shared workspace, but the primary agent must refrain from editing while it runs. Only one writing agent may be active.

The delegation must name an explicit exclusive file list or path scope. If
that ownership is missing, overlaps unexplained changes, or proves too narrow
for the required fix, the worker stops and reports instead of broadening its
writes.

Do not delegate implementation when the slice involves unresolved architecture, broad tracker changes, credentials or live remote mutation, destructive migration, intertwined public contracts, or changes whose path ownership remains uncertain. Keep those with the primary agent.

The worker does not commit or mutate tracker state. The primary agent reviews the complete diff, runs or reproduces focused verification, moves the issue card, and commits.

### `rfc-reviewer`

Use for a fresh independent closeout review of substantial non-critical RFCs. Give the reviewer:

- the live RFC and acceptance criteria;
- `RFC_BASE_SHA..HEAD` or the equivalent final diff;
- the verification matrix;
- the documentation ledger;
- the commands already run and concise evidence locations.

Do not preload the implementation transcript, worker rationale, or the primary agent's defense of the design. The reviewer should try to falsify claims rather than ratify the implementation narrative.

### `rfc-critical-reviewer`

Use instead of `rfc-reviewer` when the RFC is capability-critical under `references/model-routing.md`. It receives the same fresh evidence package but is expected to pursue counterexamples, adversarial sequences, authority leaks, stale-result races, recovery failures, unsafe mutation targets, compatibility failures, and proof gaps more exhaustively.

Do not run the ordinary and critical reviewer together by default. Use a second review only when a material disagreement remains after reproduction or the user explicitly asks for redundant review.

For either reviewer, the primary agent reproduces every blocking finding. Unsupported assertions do not block closeout, but unresolved material findings do.

## When To Delegate

Use this default decision table.

| Work shape | Default |
|---|---|
| Small, single-surface, directly verifiable change | Primary agent only |
| Exact locating, inventory, command execution, schema/drift check, or log condensation | One `rfc-utility` |
| Post-resume board-state and queue inventory snapshot for reorientation | One `rfc-utility`; the primary agent still decides the next task |
| Broad ownership, public execution-path, build-selection, verification, documentation-impact, or root-cause analysis | One `rfc-scout` |
| Two genuinely independent semantic read-only questions | Up to two direct scout/reviewer lanes; nested utilities may support them within the five-lane cap |
| Clear current issue-card slice | Optional single `rfc-worker`; primary agent does not edit concurrently; worker should offload prescribed tests or inventories to utility lanes when useful |
| Substantial non-critical public behavior, provider, diff, UI, persistence, package, integration, migration, or application RFC | One `rfc-reviewer` before status closeout |
| Capability-critical RFC under `references/model-routing.md` | One `rfc-critical-reviewer` instead of the ordinary reviewer |
| Missing native agent capability | Execute the same role serially in the primary agent |

A second read-only agent must own a different question. Do not ask two agents to scan the same files with the same prompt merely to obtain a vote.

## Concurrency And Mutation Rules

- Preserve the skill's one-RFC-at-a-time and one-issue-slice-at-a-time discipline.
- Before every spawn, restart, or follow-up task, recursively count every
  nonterminal descendant, including initializing or status-unknown agents, and
  classify its role. Across the complete descendant tree, schedule at most five
  concurrent child lanes, excluding the primary agent. Do not reuse a lane
  until the prior agent has left Working/nonterminal status.
- Permit at most one writing agent across the complete tree. The primary does
  not edit while a writing child runs. Read-only work may coexist only when it
  cannot overlap or redirect the settled writer assignment.
- Permit scouts, workers, ordinary reviewers, and critical reviewers to spawn
  only read-only `rfc-utility` descendants for exact locating, inventories,
  prescribed commands/tests, schema or generated drift checks, and log
  condensation. Each of those roles has a runtime-local nested cap of two,
  while the recursive workflow cap of five remains authoritative.
- A nested utility must receive the exact command or mechanical question,
  cannot spawn descendants, and cannot decide architecture, root cause,
  implementation, verification sufficiency, or closeout. Its parent remains
  responsible for interpreting and reporting the evidence.
- Do not assume worktree isolation is safe or based on the current parent `HEAD`. Use it only after the active runtime proves the exact base and integration behavior needed for the current task.
- Preserve unrelated user changes. A writing worker must stop and report when its required edit overlaps unexplained existing modifications.
- Never put credentials, tokens, cookies, secret-bearing logs, or unnecessary source bodies into a delegation message. A delegated agent may inspect the repository under the same local trust boundary, but its result should cite paths and symbols rather than reproduce private source.
- Never delegate a live GitHub or GHES mutation unless the user has already identified the safe repository and pull request and the current issue explicitly requires that validation.

## Delegation Message

Every delegation message should be self-contained and short. Include only what the role needs:

```text
Session name:
Role:
Runtime model / effort:
RFC / issue:
Objective:
Relevant acceptance criteria:
Allowed paths or scopes:
Explicit non-goals:
Applicable instruction files:
Required verification or evidence:
Remote-side-effect boundary:
Output requested:
```

For a reviewer, identify the immutable base and final head. For a worker,
enumerate the exclusive allowed files or path scopes and explicitly state that
it must stop if those scopes are insufficient or overlap unexplained changes.
Also state that it must not commit, move issue cards, change
`docs/issues/NEXT_ID`, or change RFC status. Tell scouts, workers, and reviewers
whether nested utility work is expected, and remind them that utilities are
read-only, mechanical, childless, and counted in the recursive five-lane cap.
The session name must retain the role prefix, and the model/effort entry must
agree with the startup identity line or identify a runtime override.

## Result Contract

Require the compact role-specific result format defined in `config/roles.json`. Across roles:

- return status first;
- cite exact files, symbols, configuration keys, commands, and result locations;
- distinguish fact, uncertainty, and not-searched surfaces;
- do not paste raw logs, copied RFC prose, credentials, provider bodies, private source bodies, or broad repository summaries;
- store large non-secret outputs in files and return their paths.

## Quality And Cost Guardrails

- Delegate replacement work, not duplicate work.
- Use `rfc-utility` for directly observable mechanical work; promote to `rfc-scout` when semantic interpretation begins.
- Use `rfc-scout` for any semantic scan expected to span more than a few files; on long multi-commit runs context isolation is worth the extra model call by default, and keeping such a scan inline is the choice that needs justification.
- Keep deterministic searching, filtering, schema validation, and command execution in scripts when the script already exists; the utility role is for orchestrating or condensing such work, not replacing deterministic tools.
- Keep the primary integrator and implementation worker at the deep route by default. Do not opt either down merely because an issue card is bounded.
- Do not silently downgrade ordinary or critical review to save tokens.
- Stop after one useful semantic delegation round unless new evidence exposes a distinct unresolved question. Use utility lanes as needed for prescribed mechanical evidence without duplicating commands.
- Prefer one appropriately strong independent reviewer over several same-prompt reviewers.
- Measure success by correctly verified closeout and avoided rework, not by lowest raw token count.

## Runtime Portability

The skill refers to roles and guarantees, not vendor feature names. Generated adapters translate those roles into current Claude Code and Codex custom-agent formats.

If a runtime cannot provide a requested capability:

- no delegated agents: perform the role serially in the primary context;
- no reliable read-only enforcement: use a read-only instruction set, inspect `git status --short` afterward, and treat unexpected writes as a failed result;
- no per-agent model override: use the current session model while preserving the role instructions;
- no requested `max` effort: use the strongest available supported effort, report the downgrade, and preserve the same critical-review scope and acceptance gates;
- no safe writer isolation: keep one shared-workspace writer and stop primary-agent editing while it runs;
- no independent context: perform a separate skeptical review pass after clearing irrelevant implementation details where the runtime permits it.

A fallback may reduce speed or cost efficiency. It must not weaken acceptance, verification, documentation, security, remote-side-effect safety, or closeout gates.
