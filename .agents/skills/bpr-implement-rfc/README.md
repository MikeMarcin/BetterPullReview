# Portable BetterPullReview `implement-rfc` Agents

This directory contains the canonical BetterPullReview RFC implementation skill plus generated native agent definitions for Codex and Claude Code.

## Design

- `.agents/skills/bpr-implement-rfc/SKILL.md` is authoritative.
- `agents/openai.yaml` supplies the Codex skill interface metadata.
- `config/roles.json` defines portable role semantics.
- `config/routing.json` maps semantic capability classes to current Codex and Claude models.
- `scripts/render_runtime_adapters.py` generates native `.codex/agents`, `.claude/agents`, and the small Claude skill entrypoint.
- The primary agent remains the only RFC integrator and the only authority for `RFC_BASE_SHA`, issue IDs, `docs/issues/NEXT_ID`, board transitions, staging, commits, verification sufficiency, and RFC status.
- The workflow schedules at most five recursively active child lanes while the
  repository Codex ceiling remains ten for runtime headroom. It permits one
  total writer with explicit exclusive file ownership. Scouts, workers, and
  reviewers may spawn only childless read-only utility agents for exact
  inventories, prescribed tests, drift checks, and log condensation. Utility
  work remains isolated from semantic judgment; implementation uses the deep
  route, and capability-critical review has a separate max-effort role.
- A missing runtime feature falls back to serial primary-agent execution without weakening BetterPullReview's verification, documentation, security, or remote-side-effect gates.

## Install Or Refresh

From the BetterPullReview repository root:

```sh
python .agents/skills/bpr-implement-rfc/scripts/render_runtime_adapters.py
python .agents/skills/bpr-implement-rfc/scripts/validate_portable_agents.py
```

Merge the following fragment into `.codex/config.toml` when the table is not already configured:

```toml
[agents]
enabled = true
max_concurrent_threads_per_session = 10
```

The canonical fragment is at `config/codex-config.fragment.toml`.

## Claude Through Amazon Bedrock

Keep the generated Claude agent files unchanged. Merge the `modelOverrides` object from `config/claude-bedrock-model-overrides.example.json` into the applicable Claude Code settings source and replace the example values with approved inference-profile ARNs.

Do not set `CLAUDE_CODE_SUBAGENT_MODEL` when role-specific routing is expected.

## Change A Model Assignment

1. Edit `config/routing.json` only.
2. Update `verified_as_of`.
3. Render and validate:

   ```sh
   python .agents/skills/bpr-implement-rfc/scripts/render_runtime_adapters.py
   python .agents/skills/bpr-implement-rfc/scripts/validate_portable_agents.py
   ```

4. Commit the routing source and generated outputs together.

For candidate evaluation without touching checked-in generated files, use `--repo-root` and `--routing-override` with a temporary directory. An override can change a role assignment, a provider model, or an effort value.

## Files Generated

- `.codex/agents/rfc-utility.toml`
- `.codex/agents/rfc-scout.toml`
- `.codex/agents/rfc-worker.toml`
- `.codex/agents/rfc-reviewer.toml`
- `.codex/agents/rfc-critical-reviewer.toml`
- `.claude/agents/rfc-utility.md`
- `.claude/agents/rfc-scout.md`
- `.claude/agents/rfc-worker.md`
- `.claude/agents/rfc-reviewer.md`
- `.claude/agents/rfc-critical-reviewer.md`
- `.claude/skills/bpr-implement-rfc/SKILL.md`
- `.agents/skills/bpr-implement-rfc/generated/runtime-adapters.json`
