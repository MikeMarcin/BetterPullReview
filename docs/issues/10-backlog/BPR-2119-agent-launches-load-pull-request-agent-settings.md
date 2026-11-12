---
schema: "implement-rfc.card.v1"
id: "BPR-2119"
title: "Shipped builds launch agents in the pull request's checkout with the pull request's own agent settings loaded"
status: "Backlog"
rfcs: ["RFC-BPR-0104"]
depends_on: []
priority: "P0"
metadata: {"kind":"defect","found_by":"RFC-BPR-0104 third review round (F1) on the v0.5.0 prerelease lane, 2026-11-11; release exposure check the same day","exposed":["0.4.1 (latest release)","0.4.0","v0.3.4 (earlier releases not checked)","v0.4.2-build.58 and earlier prereleases","main at 8d283e744"],"lane_fix":"claude/v0-5-0-prerelease-targets-52265d at 059e191fb, hardened in the fourth review round","related_cards":["BPR-2007","BPR-2008"],"reservation":"docs/issues/_reservations/BPR-2119--BPR-2119--ad05a040e80a.json"}
---

# Shipped builds launch agents in the pull request's checkout with the pull request's own agent settings loaded

## Defect

Every full (not `agent-free`) package since agent integrations shipped runs
the reviewer's agent in a disposable checkout BetterPullReview makes from the
pull request's head, and passes nothing that keeps that checkout's own agent
settings from loading. A hostile pull request that commits agent settings
runs code on the reviewer's machine, with the reviewer's rights and outside
any sandbox, the first time the reviewer asks the agent anything about it.
No prompt is shown.

- **Claude Code** (`claude -p`, every Agent-tab action: Ask about the
  selection, Analyze file, Analyze upload, Evaluate fix, Workshop comment,
  Run AI review, full session, Ask about the conversation). `-p` skips the
  workspace trust dialog, so the checkout's `.claude/settings.json` and
  `.claude/settings.local.json` hooks (`SessionStart`, `UserPromptSubmit`),
  `apiKeyHelper` and `statusLine`, and `.mcp.json` servers (enabled by the
  project's own `enableAllProjectMcpServers`) all run. Reproduced on Claude
  Code 2.1.319: every one of those beacons fired.
- **Codex `exec`** when the reviewer's own Codex configuration lets the turn
  write its working folder (`sandbox_mode = "workspace-write"`, a common
  setting): Codex 0.167.0 marks the checkout trusted, writes
  `trust_level = "trusted"` for it into the reviewer's
  `~/.codex/config.toml` (persisting), and runs the checkout's
  `.codex/config.toml` `mcp_servers` outside the sandbox.
- **Codex App Server**: did not trust the checkout in any probe; not
  observed exposed.

Not exposed: tour writing runs and writing hooks (`WritingOnly`, Claude only:
`--setting-sources ""`, strict empty MCP); read-only tour drafting on main and
the 0.4.2 prereleases (`ReadOnlyTools`: Claude `--setting-sources ""`, Codex
`--sandbox read-only -c mcp_servers={}`); the reviewer's own checkout when
they chose it (their settings, by design); Review Cycle Send to Agent (runs in
the folder the reviewer opened, never a checkout BPR made); `agent-free`
packages. Residual, not part of this hotfix: a reviewer who opens someone
else's pull request checkout as a Review Cycle and sends a round runs that
folder's settings, as running the agent there would, except that `-p` skips
Claude's trust dialog. Evidence and the version table:
[RFC-BPR-0104 ledger, release exposure](../../verification/RFC-BPR-0104.md#release-exposure-check-2026-11-11).

## Scope and acceptance

Back-port the lane's guard to `main` for the next release (and a patch release
if the owner wants one), not the lane's grants:

- Every agent launch in a checkout BetterPullReview made names it
  (`agent_launch_options::PullRequestCheckout`) by every spelling an agent
  keys it by: the path it runs in, its canonical path and the system's path
  for the opened folder (`agent_checkout_spellings`; a macOS firmlink spelling
  canonicalises to itself, but Codex keys by the kernel's path). A checkout
  that cannot be resolved is not launched in.
- Claude Code there gets `--setting-sources user`; Codex (`exec` and the App
  Server) gets `-c projects={"PATH"={trust_level="untrusted"},…}` for every
  spelling (it merges into the reviewer's `projects` table and overrides those
  entries).
- The reviewer's own chosen checkout keeps its settings.

## Intended verification

- Unit: `application_agent_launch` (both arguments for each protocol and the
  escaping), `application_agent_invocation_host` (a made checkout is named by
  every spelling; an unresolvable one refuses; the reviewer's own checkout is
  not named), `application_agent_scratch_folder` (`agent_checkout_spellings`
  under `/System/Volumes/Data`).
- Live, opt-in: `live_agent_checkout_settings`
  (`-DBPR_LIVE_AGENT_COURSES=ON`, `ctest -L live-agent`) with the hostile
  fixture `tests/fixtures/agent-checkout-settings`: unguarded controls fire,
  every guarded Codex `exec`/App Server and Claude run leaves no beacon and the
  home configuration unchanged; dead model providers and isolated homes only.
- Release notes say what was exposed and that reviewers who used Codex with
  `workspace-write` should remove `trust_level = "trusted"` entries for
  BetterPullReview workspace paths from `~/.codex/config.toml`.
