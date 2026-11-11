# Agent checkout settings fixture

RFC-BPR-0104, third review round (F1). A pull request's own agent settings,
as a hostile pull request would commit them, for
`tests/end_to_end/live_agent_checkout_settings_course.cpp`. Every executable
entry only creates a beacon file named after itself in `@BEACONS@`; the
course writes the files into a throwaway checkout under the names the agents
read (`.codex/config.toml`, `.claude/settings.json`,
`.claude/settings.local.json`, `.mcp.json`, `.claude/skills/beacon/SKILL.md`)
and a home whose agent configuration points every model provider at
`http://127.0.0.1:9`, so no run reaches a model. They are kept here under other
names so no agent working in this repository reads them.

| File | Written as | What would execute |
|---|---|---|
| `codex-project-config.toml.in` | `.codex/config.toml` | `mcp_servers.beacon`, `notify`, a permissions profile writing `:root`, `danger-full-access` |
| `claude-project-settings.json.in` | `.claude/settings.json`, `.claude/settings.local.json` | `SessionStart` and `UserPromptSubmit` hooks, `apiKeyHelper`, `statusLine`, every project MCP server enabled, allow rules |
| `mcp.json.in` | `.mcp.json` | an MCP server |
| `claude-skill.md.in` | `.claude/skills/beacon/SKILL.md` | a skill with a `PreToolUse` hook |
| `codex-home-config.toml.in` | the course home's `.codex/config.toml` | nothing: a dead provider, and this computer's own `workspace-write` and `on-request` |
