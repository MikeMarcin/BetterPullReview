# Codex Delegated Lanes

Read this before launching any delegated lane on the Codex runtime from a
Claude Code primary session. A Codex lane satisfies the same delegation
contract as a native Claude lane — role identity, mutation limits, single
writer per checkout, and result handling — and
[delegated-execution.md](delegated-execution.md) remains authoritative; this
file only maps that contract onto the mixed-runtime mechanism. A Codex
*driver* is unaffected: it keeps its generated `.codex/agents` definitions
and the standard fallback rules, and it cannot launch Claude lanes — that
gap is known and deliberately left open.

## When A Codex Lane Is The Better Choice

- **Cross-family independent review.** A reviewer from a different model
  family is more genuinely independent than any fresh-context same-family
  agent. When independent review is selected and the implementation was
  written by Claude, prefer the Codex route for an ordinary `rfc-reviewer`;
  for a critical review, run `rfc-critical-reviewer` on both families and
  reconcile (see Lane Runtime Selection in
  [model-routing.md](model-routing.md)).
- **Mechanical bulk.** Offloading bulk `rfc-utility` and `rfc-verifier`
  token volume to Codex preserves the primary integrator's budget, the run's
  scarcest resource, when the allowance economics favor it.
- **Second opinion when stuck.** That is `codex:codex-rescue`'s job; it
  needs no lane machinery. Use it for ad-hoc rescue only, never for
  structured RFC lanes: its contract (default write, no status polling, no
  role naming) conflicts with the lane rules below.

## Launching

Use the launcher, never a hand-rolled companion or Codex CLI invocation:

```sh
python3 .agents/skills/implement-rfc/scripts/codex_lane.py launch \
  --role rfc-reviewer --lane-name rfc_reviewer_snp_0042_closeout \
  --task-file /path/to/assignment.md [--background]
```

The launcher resolves the installed openai-codex plugin's companion runtime
(override with `IMPLEMENT_RFC_CODEX_COMPANION`), maps the role to its checked-in
Codex model and effort from `config/routing.json`, prepends the rendered
role prompt — including the startup identity line — to the assignment, and
appends a launch record to the checkout's Codex lane log
(`<git-common-dir>/<project>-codex-lanes.jsonl`, named from `config/project.json`).

It also prepends a one-line banner naming the lane and role, because the
companion labels a job by shortening the prompt's first line — without the
banner every lane in the operator's task list reads identically.

Lane rules:

1. The assignment follows the assignment contract in
   [delegated-execution.md](delegated-execution.md). The launcher supplies
   the role instructions and identity line; do not restate them.
2. `--lane-name` must keep the role prefix (`rfc_reviewer_*`,
   `rfc_utility_*`); the launcher refuses other names.
3. Read-only roles run in the companion's read-only sandbox; the launcher
   refuses `--write` for them. Inspect `git status --short` after the lane
   anyway — an unexpected source write invalidates the result.
4. The proven uses are read-only: reviewers, utility, scout, and
   verifier commands whose outputs live outside the source tree. The
   launcher's `--write` path exists with its worktree guard (a write lane
   must run in its own worktree under the lane commit authority in
   [pack-lifecycle.md](pack-lifecycle.md)), but it is not an established
   workflow; reach for a native worker first.
5. Prefer `--background` for long lanes; a foreground launch occupies the
   primary's Bash call until the lane finishes.

## Accounting And Continuity

- A Codex lane occupies a delegation lane exactly as a native agent does.
- Companion job state is per working directory and invisible to native
  agent listings: query Codex lanes explicitly with
  `codex_lane.py status -- --cwd <lane cwd> --json`. A compaction summary is
  not live status for Codex lanes either — after reorientation, query again.
- Collect results with `codex_lane.py result -- <job-id> --cwd <lane cwd>`;
  cancel abandoned lanes with `codex_lane.py cancel` rather than leaving
  them running.
- The companion runtime caps reasoning effort at `xhigh`. When a route names
  `max` (the mechanical and deep-max routes), the launcher downgrades to
  `xhigh` and reports it; record the downgrade in the closeout card per the
  standard fallback rule. Review scope and acceptance gates never narrow
  because of the downgrade. The native `.codex/agents` adapters are
  unaffected.

## Result Handling

The skill's contract governs Codex lane output, not the plugin's interactive
conventions:

- A Codex reviewer's findings are inputs to the integrator: reproduce every
  blocking finding directly, then continue the closeout flow. Do not stop to
  ask the user which findings to fix; that plugin rule applies to ad-hoc
  interactive reviews, not to a delegated lane inside this skill.
- A Codex lane never commits, moves cards, allocates IDs, or changes RFC
  status; the role instructions already forbid it, and the primary remains
  the sole integrator.
- Treat lane output as evidence, not authority, exactly as for native
  lanes: cite paths and symbols, keep secrets out.

## Recording The Review Runtime

Whenever an independent review runs, record one line per review on the
closeout card:

```text
Closeout review runtime: codex (gpt-5.6-sol, xhigh)
Closeout review runtime: claude (claude-fable-5-1, max)
Closeout review runtime: claude (claude-opus-5, xhigh) — waived: <reason cross-family was skipped>
```

A dual-family critical review records both lines. The line exists because
the cross-family preference has silently drifted before: in the PlaceOfMind
trial, two consecutive closeouts slipped back to same-family review with no
decision behind it, and nothing on the board made the deviation visible.
Recording is the whole obligation — no audit enforces it, and review itself
stays optional and risk-triggered per `SKILL.md`.

## Session Hygiene

- Keep the codex plugin's stop-time review gate disabled during RFC runs
  (`/codex:setup` toggles it); it duplicates the skill's own review lanes at
  material cost.
- Do not route structured lanes through `codex:codex-rescue`, and do not let
  a lane spawn lanes on the other runtime; nesting stays within the runtime
  that owns the lane, under the same one-level `rfc-utility` limit.
- When no Codex runtime is installed or the companion invocation fails, fall
  back per the standard rule: run the same role on the native runtime or
  serially in the primary, and record the fallback and its trigger.
