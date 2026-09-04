# Runtime Compatibility

Verified against public product documentation on 2026-08-02; remote-operation
sections verified 2026-08-31 (Claude Code CLI v2.1.251 and its docs; Codex CLI
0.151.0 plus the OpenAI Remote connections, App Server, and Hooks
documentation). This file describes current adapters, not permanent skill
semantics.

## Codex

Current local Codex releases support project custom agents in `.codex/agents/*.toml`. A custom agent can set `model`, `model_reasoning_effort`, `sandbox_mode`, and nested session configuration. Applicable skill instructions can request delegation.

Generated Codex agents:

- use `sandbox_mode = "read-only"` for utility, scout, and reviewer roles;
- use `sandbox_mode = "workspace-write"` for bounded workers and the source-immutable test runner because their configure/build/test/package roots must be writable;
- enable nested sessions only for scouts, workers, and reviewers, whose role instructions permit `rfc-utility` children and forbid every other nested role;
- keep `[agents] enabled = false` for `rfc-utility` and `rfc-test-runner` so nesting terminates after one level;
- select models from `config/routing.json`.

Important limitation: live parent permission and sandbox overrides can be reapplied to children. Do not rely on a generated read-only sandbox while running the parent with an override that widens it. Inspect `git status --short` after read-only work and reject unexpected writes.

Merge `config/codex-config.fragment.toml` into `.codex/config.toml` to set a
runtime cap of 10 concurrent child threads. The skill schedules only bounded,
useful lanes; the runtime cap is not a target.

### Remote operation

The station launcher uses Codex Managed Remote as the controller transport and
App Server only for host-local provisioning. On macOS, Linux, and WSL2 it
idempotently runs `codex remote-control start --json` and reaches the managed
daemon through `codex app-server proxy`, whose stdio carries an HTTP WebSocket
upgrade and WebSocket frames rather than JSONL. Native Windows cannot manage
that daemon lifecycle and cannot attach the proxy to Desktop's internal control
socket. Codex Desktop exclusively owns Windows Remote. The launcher opens or
focuses the checkout with `codex app <checkout>`, provisions the task through a
short-lived standalone JSONL App Server, calls no `remoteControl/*` method on
that helper, and closes it before Claude starts. Neither path exposes an App
Server network listener.

Both paths create, resume, name, and verify the persistent station task. A
fresh empty task receives one harmless bootstrap turn and is renamed again
after that turn; this makes it durable and eligible for controller task lists.
Controller enumeration remains a separate driver gate, especially on Windows.
Codex Desktop and the Unix managed daemon may both
outlive the launcher and remain independently usable; the live writer lease and
hooks, not ownership of those processes, bound station mutation. `codex
--remote` remains a diagnostic terminal client, not the registry or dispatch
path.

Codex CLI 0.151.0 is the verified baseline. Newer versions are accepted only
when the applicable daemon or desktop-app startup, App Server transport,
thread methods, and bootstrap turn succeed. Unix additionally probes Remote
Control status/pairing methods. Windows pairing is owned by Desktop's **Control
this PC** flow, and `--pair-codex` refuses there rather than starting a second
Remote owner.

Codex facts that matter for stations:

- Tasks are saved and resumable by immutable thread ID. The launcher resolves
  an exact station name plus checkout, reuses the sole match, creates one when
  absent, materializes it with a first turn, and fails closed on duplicates. A
  narrowly recognized migration may archive an empty launcher-recorded task
  and adopt the sole same-named nonempty replacement; it never renames a task
  with unrelated history. Never automate with `--last`; it resolves by current
  directory and recency, the ambiguity stations remove.
- Cross-host dispatch uses the Codex App's persistent task-graph operations:
  exact `list_threads` resolution, `send_message_to_thread`, `wait_threads`,
  and `read_thread`. `wait_threads` holds a bounded server-side event wait and
  wakes on task progress or completion; only a timeout on a long-running task
  requires another wait. The send acknowledgement is not a result. Same-host
  local task DAGs may exchange `<codex_delegation>` callbacks, but a remote
  station must return its transport-neutral `remote-reply` or `remote-refusal`
  in its own completed turn; reverse-host callbacks are not an accepted route.
  App Server is not a second messaging bus.
  These controller tool names are internal Codex Desktop capabilities, not
  documented App Server RPC method names and not callable by the station's
  Python launcher. The public host-local protocol uses `thread/list`,
  `thread/start`, `thread/resume`, `turn/start`, `thread/read`, and
  `thread/name/set` for provisioning; it does not replace the controller's
  cross-host list/send/wait/read surface.
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
- Managed Remote (ChatGPT mobile/desktop to a connected host) and SSH projects
  are controller transports to an already-provisioned station. They do not
  create write authority, and multiple controllers of one task remain one
  writer. Pairing is human-attended through a signed-in ChatGPT client.
- Multiple Codex sessions in different checkouts on one machine are fine;
  give each station its own build, temp, and evidence roots so caches and
  ports do not collide.
- `.codex/hooks.json` re-asserts the guard on station prompts and before common
  mutating tools, and clears turn state on stop. Project-hook trust is
  human-attended through `/hooks`. OpenAI documents that specialized tool paths
  may bypass generic hooks, so the hook remains defense in depth; Git state,
  board state, restricted credentials, and driver-side reply validation stay
  authoritative.
- Both Claude and Codex endpoints can remain connected, but concurrent
  cross-provider turns in one station are unsupported. Dispatch through one
  driver at a time.

## Claude Code

Current Claude Code supports custom agents in `.claude/agents/*.md`, per-agent models and effort, tool allowlists, permission modes, maximum turns, foreground or background execution, and worktree isolation.

Generated Claude agents:

- use `permissionMode: plan` plus a read-focused tool allowlist for utility, scout, and reviewer roles;
- use `permissionMode: acceptEdits` for bounded workers and the test runner; the test runner has no Edit/Write tools and its instructions prohibit repository-source edits, including through Bash;
- list `Agent(rfc-utility)` for scouts, workers, and reviewers; the parenthesized type restriction is honored when one of those definitions runs as the main thread, but current Claude Code ignores the type list inside a delegated subagent, so the utility-only nesting boundary is enforced by role instructions there, exactly as on Codex;
- omit the `Agent` tool from `rfc-utility` and `rfc-test-runner` so nesting terminates after one level;
- rely on Claude Code's support for nested subagents: a delegated scout, worker, or reviewer may launch its own `rfc-utility` children within the runtime depth limit, and the primary agent launches utility lanes directly only when depth or capability is unavailable;
- leave foreground/background selection to the primary agent so independent utility lanes can run concurrently within the configured cap;
- select stable Anthropic model IDs from `config/routing.json`.

Parent permission modes can take precedence over a subagent's `permissionMode`. Before relying on read-only scouting or review, keep the parent in a permission mode that does not widen the child, and inspect `git status --short` after the run. The read-focused agents retain Bash for Git inspection and read-only verification commands, but `permissionMode: plan` denies mutating commands: configure, build, or test runs that write artifacts stay with the primary agent, bounded worker, or test runner. Unexpected repository-source writes from a test runner invalidate the result.

The checked-in route requires Claude Code 2.1.219 or later, the floor established for Claude Opus 5. Unverified as of 2026-09-04: whether Claude Fable 5.1, selected by the `deep` and `critical` routes, raises that floor. Configuration validation is not a live model-availability test. Worktree-isolated subagents received important main-checkout safety fixes by 2.1.210, but the default portable policy still does not enable static worktree isolation: a custom worktree subagent may branch from the repository's default branch rather than the primary session's current `HEAD`. Use worktree writers only after explicitly proving the desired base and integration behavior.

Claude Code keeps its default concurrency limit of 10. The portable skill uses
only bounded lanes that materially help the current task.

Claude Code officially discovers project skills from `.claude/skills/`; Codex uses the cross-client `.agents/skills/` location. The renderer therefore creates a small `.claude/skills/implement-rfc/SKILL.md` entrypoint that loads the canonical `.agents/skills/implement-rfc/SKILL.md`. Do not maintain two full skill copies.

### Long multi-commit sessions

Claude Code has no agent-callable goal tool; its goal command is user-invoked, so the skill's Start step cannot anchor the objective there. Use the persistent session task list instead: mirror the requested RFC order and closeout phases as a courtesy view while the issue board and closeout cards stay authoritative.

Claude Code compacts long conversations, and compaction can silently drop the in-memory task queue mid-run. The skill's reorientation procedure exists for exactly this failure: re-derive the next card from the live board and the closeout card, and never conclude from memory that no work remains. For pack-scale runs, a larger context window can delay compaction but does not remove reorientation. Delegate bounded read-heavy work when doing so materially protects the primary context; delegation is not required merely because a run is long.

Re-invoking the skill with the same arguments is the supported resume path after an interruption or a fresh session: reorientation recovers `RFC_BASE_SHA`, the ledgers, the phase checklist, and the next task from the closeout card, the board, and `git log`.

### Remote Control

`claude remote-control` (verified v2.1.251) is a per-directory server: launched
inside a checkout with a stable `--name`, it hosts a session pinned to that
directory, reachable from other machines' sessions and from claude.ai/code and
the mobile app. Multiple servers on one machine in different directories are
supported — that is how several stations share a host.

- Launch stations with the interactive form, `claude --remote-control
  <station> --permission-mode <mode>`: the running process is the served
  session, so the name exists and binds in peer lists the moment it starts,
  and the supervisor already runs Claude in the foreground for its whole
  life. The operator therefore sees Claude open in the launch terminal; that
  session is the station and stays running, driven by messages, the web or
  mobile app, or the keyboard. The launcher infers the workstream from the
  station file or the checkout's branch, so the documented command takes no
  argument. The `claude remote-control` *server* is the wrong shape for a
  station whatever its `--spawn` mode: `session`, the default `same-dir`,
  and `worktree` all create sessions only when a controller connects, so a
  station launched that way has no session and no peer-list entry until
  someone opens its environment (measured 2026-09-01: four server-form
  probes, `--create-session-in-dir` and `--capacity 1` included, created
  none in 40 seconds; the interactive form was listed under its name within
  seconds). `--spawn worktree` is additionally wrong for every workstream
  branch because it branches from the repository's default branch, and
  `same-dir` lets several sessions mix context in one checkout. The spawn
  mode is persisted per project in `~/.claude.json`
  (`remoteControlSpawnMode`); the launcher no longer depends on it.
- Resuming a station: a recorded server-created session comes back through
  `claude remote-control --session-id <id>`; a session created by the
  interactive form comes back through `claude --continue --remote-control
  <station>` in the station directory, which reconnects through that
  conversation's reconnection record. The launcher chooses by what the
  station file and the directory's conversation records hold.
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

For Bedrock, keep the generated agent definitions unchanged and use `modelOverrides` in local or managed Claude Code settings to map the canonical model IDs to exact approved inference-profile ARNs. Family aliases are not considered reproducible routing. Unverified as of 2026-08-02: the `modelOverrides` setting could not be confirmed in official Claude Code documentation. Before relying on role-to-model routing through Bedrock, confirm on the Bedrock-configured machine that each generated agent's startup banner reports the expected model; if the setting is ignored, escalate to organization-managed model policy instead.

## Capability Fallback

The skill remains correct when an adapter is unavailable:

- execute utility, scout, test-runner, or review work serially in the primary agent;
- when a selected model does not support `max`, use the strongest supported effort, report the downgrade, and do not narrow the requested critical-review scope;
- keep all writes in the primary agent if a bounded worker or test runner cannot be controlled safely, and record the serial fallback;
- preserve the same direct verification and closeout requirements;
- report the omitted delegation lane in the final summary.

## Source Documentation

- Codex subagents and custom agents: `https://developers.openai.com/codex/subagents`
- Codex skills: `https://developers.openai.com/codex/build-skills`
- Codex model recommendations: `https://developers.openai.com/codex/models`
- Codex Remote connections: `https://learn.chatgpt.com/docs/remote-connections`
- Codex App Server: `https://learn.chatgpt.com/docs/app-server`
- Codex hooks: `https://learn.chatgpt.com/docs/hooks`
- Claude Code subagents: `https://code.claude.com/docs/en/sub-agents`
- Claude Code skills: `https://code.claude.com/docs/en/skills`
- Claude Code model configuration: `https://code.claude.com/docs/en/model-config`
- Claude model IDs: `https://platform.claude.com/docs/en/about-claude/models/overview`
- Agent Skills progressive disclosure: `https://agentskills.io/skill-creation/best-practices`
