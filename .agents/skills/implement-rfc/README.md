# Portable `implement-rfc`

This directory is the canonical RFC implementation skill for this repository
and the sources used to generate its native Codex and Claude Code agent
definitions. The same directory, minus two files, is carried verbatim between
projects.

## Policy

Ordinary RFC work is acceptance-focused:

- the live RFC, issue board, owning instructions, and selected product path
  define the work;
- focused tests plus one direct public/selected-path lane are the default;
- verification tables are catalogs of available commands, not mandatory
  matrices;
- unchanged passing evidence is reused;
- delegation, isolation, and independent review are optional, risk-triggered
  tools; the `Closeout review runtime:` line is recorded, not audited;
- full-project validation, replay, hermetic evidence, and broad soaks require
  an explicit user request;
- temporary worktrees and build roots are removed when their result is
  consumed.

Retired and available only through Git history: the parallel-slices writer
mode and its lane-plan validator, the blocking delegation audit, and the
prescribed verification-matrix, documentation-ledger, and phase-checklist
formats.

## What is project-specific

Exactly two files:

- `config/project.json` — machine-read facts: project tokens, tracker paths,
  lane names and their status words, the next-ID source, the closeout-card
  detection rule, the structure-gate commands, the attended-review skill (or
  `null`), the station registry document, workstream branches, and per-role
  instruction addenda. `schemas/project.schema.json` documents the contract;
  `scripts/project_config.py` enforces it.
- `references/project.md` — the project's own rules in prose: what to read
  first, the side-effect boundary, what counts as a direct surface, the
  closeout-card contract, attended checkpoints, platform lanes.

Everything else is the portable set. `generated/portable-manifest.json`
records its files and digests, and
`scripts/validate_portable_agents.py --compare <other-checkout-skill-dir>`
fails when two projects have drifted.

## Maintained Sources

- `SKILL.md`: authoritative workflow and finish condition.
- `config/roles.json`: portable role semantics (six roles).
- `config/routing.json`: capability-class routing for Codex and Claude.
- `references/pack-lifecycle.md`: pack import, lane authority, milestone
  integration, renumbering, handoff, and cleanup.
- `references/delegated-execution.md`: optional role/delegation guidance.
- `references/model-routing.md`: capability classes, role-to-model
  assignment rationale, and per-lane runtime selection.
- `references/codex-lanes.md`: launching delegated lanes on the Codex
  runtime from a Claude driver.
- `references/remote-routing.md`: remote-station identity and writer authority.
- `references/runtime-compatibility.md`: adapter and runtime-specific facts.
- `scripts/project_config.py`: loader and validator for `config/project.json`.
- `scripts/render_runtime_adapters.py`: deterministic native adapter renderer
  and portable-manifest writer.
- `scripts/validate_portable_agents.py`: skill tests, adapter drift check,
  cross-project comparison.
- `scripts/audit_board.py`: issue-board/RFC conformance audit; transcripts
  informational.
- `scripts/codex_lane.py`: routed Codex lane launcher over the openai-codex
  plugin's companion runtime.
- `scripts/station_lease.py`, `scripts/remote_station_guard.py`,
  `scripts/launch_remote_station.py`, `scripts/codex_station.py`: remote
  station identity, single-writer protection, and launch.

The primary agent remains the only tracker, commit, integration, and
RFC-status authority. Delegated agents return bounded inputs to that decision.

## Render And Validate

From the repository root:

```sh
python3 .agents/skills/implement-rfc/scripts/render_runtime_adapters.py
python3 .agents/skills/implement-rfc/scripts/validate_portable_agents.py
```

Merge `config/codex-config.fragment.toml` into `.codex/config.toml` when
custom agents are not already enabled. Claude Code discovers the generated
entrypoint at `.claude/skills/implement-rfc/SKILL.md`; it points back to the
single canonical skill rather than duplicating it.

Generated files:

- `.codex/agents/rfc-{utility,scout,worker,verifier,reviewer,critical-reviewer}.toml`
- `.claude/agents/rfc-{utility,scout,worker,verifier,reviewer,critical-reviewer}.md`
- `.claude/skills/implement-rfc/SKILL.md`
- `generated/runtime-adapters.json`, `generated/portable-manifest.json`

Keep generated adapters in the same commit as their sources.

## Routing Changes

Change role assignments or provider routes only in `config/routing.json`,
update `verified_as_of`, then render and validate. For candidate evaluation,
use `--repo-root` and `--routing-override` with a temporary directory.

## Porting To Another Project

1. Copy this directory to `.agents/skills/implement-rfc/` in the new
   repository.
2. Rewrite `config/project.json` and `references/project.md` for that project.
3. Render, validate, and commit the generated files.
4. From either repository, run `validate_portable_agents.py --compare` against
   the other to confirm the portable set is identical.

## Bedrock

Keep generated Claude agent files unchanged. If Claude Code's
`modelOverrides` setting is supported and approved in the target environment,
map the stable model IDs to inference-profile ARNs outside the portable skill
using `config/claude-bedrock-model-overrides.example.json`. Confirm each
agent's startup banner before relying on the mapping.
