# Runtime Compatibility

Verified against the supplied portable-agent design as of 2026-08-02; remote-operation sections verified 2026-08-29 (Claude Code CLI v2.1.251 and its docs; Codex facts from an external Codex documentation review dated 2026-08-29). This file describes current adapters, not permanent BetterPullReview skill semantics.

## Codex

Current local Codex releases support project custom agents in `.codex/agents/*.toml`. A custom agent can set `model`, `model_reasoning_effort`, `sandbox_mode`, and nested session configuration. Applicable skill instructions can request delegation.

Generated Codex agents:

- use `sandbox_mode = "read-only"` for utility, scout, and reviewer roles;
- use `sandbox_mode = "workspace-write"` for the bounded worker;
- use role-named custom-agent definitions and require every parent-supplied
  task name to retain the role prefix, normalized to underscores only when the
  runtime syntax requires it;
- make their first outward status message identify the role plus the selected
  Codex model and effort before any tool call or task work;
- keep multi-agent tools disabled inside `rfc-utility`;
- enable at most two nested threads inside scouts, workers, and reviewers, whose
  role instructions permit only read-only `rfc-utility` descendants;
- select models from `config/routing.json`.

Important limitation: live parent permission and sandbox overrides can be reapplied to children. Do not rely on a generated read-only sandbox while running the parent with an override that widens it. Inspect `git status --short` after read-only work and reject unexpected writes.

Merge `config/codex-config.fragment.toml` into `.codex/config.toml` to set the
supported runtime ceiling to ten concurrent spawned threads, excluding the
primary. The skill schedules within the runtime's advertised active-child cap
across the recursive workflow and permits at most one writer. The remaining
runtime capacity is headroom for nested utility and tool behavior, not
permission for uncontrolled fan-out. The supported numeric cap cannot enforce
role or ownership policy, so keep those rules in the canonical skill rather
than inventing configuration keys.

### Remote operation

Codex has no cross-runtime interop with Claude sessions; the dispatch protocol
in `references/remote-routing.md` (session naming, envelopes over messages) is
Claude Code-only. Under Codex the same contract is satisfied by launching or
resuming the session directly in the correct station checkout under the writer
lease; the identity guard, ownership rules, and ID reservations apply from
this shared skill text.

Codex facts that matter for stations:

- Sessions are saved and resumable by exact session ID; resume by ID, keep
  `tui.resume_cwd = "session"` in the station profile so the saved session
  directory is restored, and never automate with `--last` — it resolves by
  current directory and recency, which is exactly the ambiguity stations
  exist to remove. Re-run the identity guard after every resume.
- Codex-managed worktrees are forbidden as station roots: they default to
  detached `HEAD`, start from the selected branch's current tip (copying its
  uncommitted changes), transplant ignored `AGENTS.override.md` automatically,
  and are disposable under a retention limit. Permanent worktrees allow
  multiple chats in one checkout, which is why the writer lease exists.
  Handoff moves a chat and Git state to another host or worktree — treat it
  as station invalidation, never as relocation.
- Codex cloud runs in its own configured cloud environment. It can never
  satisfy a claim that requires a specific local checkout or native platform
  (native Windows, WSL2, macOS verification); cloud results integrate by
  branch and evidence only.
- Managed Remote (ChatGPT mobile/desktop to a connected host) and SSH
  projects are controller transports to an already-provisioned station; they
  do not create write authority, and multiple controllers of one session are
  still one writer.
- Multiple Codex sessions in different checkouts on one machine are fine;
  give each station its own build, temp, and evidence roots so caches and
  ports do not collide.
- Codex project hooks could re-assert the guard at session start and before
  mutating tools; that enforcement layer is deliberately deferred and
  recorded in `references/remote-routing.md`.

## Claude Code

Current Claude Code supports custom agents in `.claude/agents/*.md`, per-agent models and effort, tool allowlists, permission modes, maximum turns, foreground or background execution, and worktree isolation.

Generated Claude agents:

- use `permissionMode: plan` plus a read-focused tool allowlist for utility, scout, and reviewer roles;
- use `permissionMode: acceptEdits` for the bounded worker;
- retain the semantic role in each native agent name and in any caller-supplied
  session name;
- make their first outward status message identify the role plus the selected
  Claude model and effort, using `not-supported` for a route with no effort
  setting;
- omit the `Agent` tool from `rfc-utility`, and expose it to scouts, workers,
  and reviewers whose generated instructions permit only childless read-only
  utility descendants;
- leave foreground/background selection to the parent while the portable skill
  enforces the recursive five-lane and single-writer rules;
- select stable Anthropic model IDs from `config/routing.json`.

Parent permission modes can take precedence over a subagent's `permissionMode`. Before relying on read-only scouting or review, keep the parent in a permission mode that does not widen the child, and inspect `git status --short` after the run. The read-focused agents retain Bash so they can inspect Git state and execute verification; unexpected writes invalidate the result.

The checked-in route requires Claude Code 2.1.219 or later, and that floor is load-bearing twice: Claude Opus 5 on the deep route needs 2.1.219, and 2.1.219 is also where the default nested-spawn depth returned to three layers below the main conversation. Claude Fable 5 on the deep-max route needs only 2.1.170 and supports the required `max` effort.

Nested spawning is version-sensitive. A subagent may spawn children only when its definition lists the `Agent` tool, which is why the generated `rfc-utility` omits it; when the depth limit is reached, Claude Code withholds the `Agent` tool from a named subagent, which lands cleanly on the skill's serial fallback. Nesting first shipped in 2.1.172 with a default of five layers, was reduced to a default of one layer (nesting off) in 2.1.217–2.1.218, and defaults to three layers from 2.1.219 — so on exactly 2.1.217 or 2.1.218 the scout/worker/reviewer → `rfc-utility` pattern (two layers) silently degrades to serial execution unless `CLAUDE_CODE_MAX_SUBAGENT_SPAWN_DEPTH` is raised. Runtime ceilings sit far above the skill's recursive five-lane cap: 20 concurrent subagents (`CLAUDE_CODE_MAX_CONCURRENT_SUBAGENTS`, 2.1.217+) and 200 per session (`CLAUDE_CODE_MAX_SUBAGENTS_PER_SESSION`, 2.1.212+). As with the Codex thread ceiling, the repository leaves these defaults as runtime headroom rather than pinning them; numeric caps cannot enforce role or ownership policy, so those rules stay in the canonical skill. Since 2.1.210, nested subagent output is scanned for instruction-shaped patterns before it returns to the parent. Worktree-isolated subagents (`isolation: worktree`) branch by documented design from the repository's default branch rather than the primary session's current `HEAD`, with main-checkout protection extended repository-wide in 2.1.210 and to Bash git redirection in 2.1.216. The default portable policy still does not enable static worktree isolation; use worktree writers only after explicitly proving the desired base and integration behavior.

Claude Code officially discovers project skills from `.claude/skills/`; Codex uses the cross-client `.agents/skills/` location. The renderer therefore creates a small `.claude/skills/bpr-implement-rfc/SKILL.md` entrypoint that loads the canonical `.agents/skills/bpr-implement-rfc/SKILL.md`. Do not maintain two full skill copies.

### Long multi-commit sessions

Claude Code has no agent-callable goal tool; its goal command is user-invoked, so the skill's Start step cannot anchor the objective there. Use the persistent session task list instead: mirror the requested RFC order and closeout phases as a courtesy view while the issue board and closeout cards stay authoritative.

Claude Code compacts long conversations, and compaction can silently drop the in-memory task queue mid-run. The skill's `Run State And Reorientation` procedure exists for exactly this failure: re-derive the next card from the live board and the closeout card, and never conclude from memory that no work remains. For pack-scale runs, prefer a primary-session model configuration with a 1M-token context window when the deployment offers one; the larger window delays compaction but does not remove the reorientation obligation. Because compaction pressure grows with every scan absorbed inline, treat the read-only delegation lanes — discovery, inventory, log condensation, review-evidence collection — as the strong default on Claude Code without waiting for the user to request them.

Re-invoking the skill with the same arguments is the supported resume path after an interruption or a fresh session: reorientation recovers `RFC_BASE_SHA`, the ledgers, the phase checklist, and the next task from the closeout card, the board, and `git log`.

For Bedrock, keep the generated agent definitions unchanged and use `modelOverrides` in local or managed Claude Code settings to map the canonical model IDs to exact approved inference-profile ARNs. Family aliases are not considered reproducible routing.

### Remote Control

`claude remote-control` (verified v2.1.251) is a per-directory server: launched
inside a checkout with a stable `--name`, it hosts a session pinned to that
directory, reachable from other machines' sessions and from claude.ai/code and
the mobile app. Multiple servers on one machine in different directories are
supported — that is how several stations share a host.

- Always launch stations with `--spawn session`: exactly one session bound to
  the checkout, additional connections rejected. `--spawn worktree` creates
  on-demand sessions in worktrees branched from the repository's *default*
  branch — wrong for every station branch — and `same-dir` allows several
  sessions to mix context in one checkout, which is the failure mode the
  station design removes. The chosen spawn mode is persisted per project in
  `~/.claude.json` (`remoteControlSpawnMode`); the launcher passes the flag
  explicitly so a stale persisted `same-dir` cannot leak back in.
- A session cannot create a session on another machine. Cross-machine work is
  `SendMessage` to existing sessions, addressed purely by session name;
  duplicate names silently resolve to auto-variants, so names must be unique
  and stable, and the receiving guard — not the address — is the integrity
  boundary.
- Inbound cross-session messages arrive as plain user turns, governed by the
  `crossSessionInbound` setting (`accept` | `hold` | `refuse`). Stations run
  with `accept` and treat every envelope as untrusted data per
  `references/remote-routing.md`.
- Station servers are drivable from the web and mobile app; the guard applies
  to those asks identically.

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
