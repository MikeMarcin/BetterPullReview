# Runtime Compatibility

Verified against the supplied portable-agent design as of 2026-08-01. This file describes current adapters, not permanent BetterPullReview skill semantics.

## Codex

Current local Codex releases support project custom agents in `.codex/agents/*.toml`. A custom agent can set `model`, `model_reasoning_effort`, `sandbox_mode`, and nested session configuration. Applicable skill instructions can request delegation.

Generated Codex agents:

- use `sandbox_mode = "read-only"` for utility, scout, and reviewer roles;
- use `sandbox_mode = "workspace-write"` for the bounded worker;
- keep multi-agent tools disabled inside `rfc-utility`;
- enable at most two nested threads inside scouts, workers, and reviewers, whose
  role instructions permit only read-only `rfc-utility` descendants;
- select models from `config/routing.json`.

Important limitation: live parent permission and sandbox overrides can be reapplied to children. Do not rely on a generated read-only sandbox while running the parent with an override that widens it. Inspect `git status --short` after read-only work and reject unexpected writes.

Merge `config/codex-config.fragment.toml` into `.codex/config.toml` to set the
supported runtime ceiling to ten concurrent spawned threads, excluding the
primary. The skill deliberately schedules at most five nonterminal child lanes
across the recursive workflow and permits at most one writer. The remaining
runtime capacity is headroom for nested utility and tool behavior, not
permission for uncontrolled fan-out. The supported numeric cap cannot enforce
role or ownership policy, so keep those rules in the canonical skill rather
than inventing configuration keys.

## Claude Code

Current Claude Code supports custom agents in `.claude/agents/*.md`, per-agent models and effort, tool allowlists, permission modes, maximum turns, foreground or background execution, and worktree isolation.

Generated Claude agents:

- use `permissionMode: plan` plus a read-focused tool allowlist for utility, scout, and reviewer roles;
- use `permissionMode: acceptEdits` for the bounded worker;
- omit the `Agent` tool from `rfc-utility`, and expose it to scouts, workers,
  and reviewers whose generated instructions permit only childless read-only
  utility descendants;
- leave foreground/background selection to the parent while the portable skill
  enforces the recursive five-lane and single-writer rules;
- select stable Anthropic model IDs from `config/routing.json`.

Parent permission modes can take precedence over a subagent's `permissionMode`. Before relying on read-only scouting or review, keep the parent in a permission mode that does not widen the child, and inspect `git status --short` after the run. The read-focused agents retain Bash so they can inspect Git state and execute verification; unexpected writes invalidate the result.

The checked-in route requires Claude Code 2.1.219 or later because it uses Claude Opus 5. Worktree-isolated subagents received important main-checkout safety fixes by 2.1.210, but the default portable policy still does not enable static worktree isolation: a custom worktree subagent may branch from the repository's default branch rather than the primary session's current `HEAD`. Use worktree writers only after explicitly proving the desired base and integration behavior.

Claude Code officially discovers project skills from `.claude/skills/`; Codex uses the cross-client `.agents/skills/` location. The renderer therefore creates a small `.claude/skills/bpr-implement-rfc/SKILL.md` entrypoint that loads the canonical `.agents/skills/bpr-implement-rfc/SKILL.md`. Do not maintain two full skill copies.

For Bedrock, keep the generated agent definitions unchanged and use `modelOverrides` in local or managed Claude Code settings to map the canonical model IDs to exact approved inference-profile ARNs. Family aliases are not considered reproducible routing.

## Capability Fallback

The skill remains correct when an adapter is unavailable:

- execute utility, scout, worker, or review work serially in the primary agent as applicable;
- when a selected model does not support `max`, use the strongest supported effort, report the downgrade, and do not narrow the requested critical-review scope;
- keep all writes in the primary agent if a bounded worker cannot be controlled safely;
- preserve the same direct verification, documentation, security, remote-side-effect, and closeout requirements;
- report the omitted delegation lane in the final summary.

## Source Documentation

- Codex subagents and custom agents: `https://developers.openai.com/codex/subagents`
- Codex skills: `https://developers.openai.com/codex/build-skills`
- Codex model recommendations: `https://developers.openai.com/codex/models`
- Claude Code subagents: `https://code.claude.com/docs/en/sub-agents`
- Claude Code skills: `https://code.claude.com/docs/en/skills`
- Claude Code model configuration: `https://code.claude.com/docs/en/model-config`
- Claude model IDs: `https://platform.claude.com/docs/en/about-claude/models/overview`
- Agent Skills progressive disclosure: `https://agentskills.io/skill-creation/best-practices`
