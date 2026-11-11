---
schema: "implement-rfc.card.v1"
id: "BPR-2007"
title: "Give an agent session a scratch folder it may write and run in"
status: "Parked"
rfcs: ["RFC-BPR-0104"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0019-agent-scratch-folder.md"}
depends_on: ["BPR-2005"]
parked_on: "human-review"
---

# Give an agent session a scratch folder it may write and run in

## Scope and acceptance

Implements
[RFC-BPR-0104](../../rfcs/RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md).

Create the per-session scratch folder outside the repository and the
workspace, bound and remove it with the session, pass each agent's runtime
exactly the confined write and execute allowance when the per-agent setting is
on, tell the agent the path, show the grant line and what each runtime
enforces, and list written files and run commands in the turn's folded
activity line.

Covers AC01 to AC03, AC05, AC06, AC08 and AC09. The owner settled the
default on 2026-11-10: the scratch folder starts on, its starting state is a
per-agent setting, and a change made in a pull request is remembered for that
pull request and agent. Verify each allowance against the installed agents'
own documentation and behaviour rather than from memory.

This is part of `agent-scratch` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Implementation record (2026-11-11, `882ebed54`)

One candidate with [[BPR-2008]], under this card's claim (BPR-2008 could not be
readied while this card was not Done). Frontend-design was loaded before the Qt
work. Facts, criteria and runs: [ledger](../../verification/RFC-BPR-0104.md).

- AC01, AC02: pass, live with installed Claude Code 2.1.319 and Codex
  0.167.0 on BPR-Test #40 (read only), and at the launch and host layers.
- AC03: partly met. The folder is outside the repository and workspace,
  owner-only, removed with a turn that has no native conversation; the
  256 MiB bound is checked when a turn starts (a full folder gets no write
  allowance, and the agent is told why). A write crossing the bound during a
  turn is not refused; a native conversation's folder is kept as its
  retained workspace is (RFC-BPR-0096 removes neither automatically). Asked
  of the owner as an Accept/Reject below.
- AC05, AC06, AC08: pass (unit, Qt and storage rows).
- AC09: pass for restart, another pull request, another agent, the starting
  state and the next turn (live); New session reads the same stored choice
  and has no separate course.
- Established, and recorded in the RFC: Claude's `:*` rules never matched a
  file in the folder and no rule matches a quoted path, so the rules are
  globs and the folder's path has no space (macOS uses the cache folder);
  Codex's permissions profile replaces the configured sandbox mode.
- Settings live on `agent_entry` (`ScratchStarts`, `BuildsAndTests`), so
  RFC-BPR-0107's export carries them; the per-pull-request choice and
  answer are local state. Export is not implemented here.

## Owner look (filed, not waited on)

Session `e97c6a3d72ac570115fc961555b09d6d`, state
`/Users/jaeger/.attended-review-state/agent-scratch-v001-2026-11-11/state`
(URL in its `serve.log`), cursor 12, host PID 72034 (Tailscale child 72042).
Frozen root `/Users/jaeger/agent-scratch-v001.weg1sgre` from `c19749f58892`
(app SHA-256 `cb283436e169…`, launcher check passed). Candidates, ready and
protected: `BPR-2007-grant-menu`, `BPR-2007-agent-sheet`,
`BPR-2008-build-question`, `RFC-BPR-0104-AC03-bound`.

## Owner hand-back, session `e97c6a3d…` (2026-11-11)

Read from `session.json` (handed back at sequence 116); host 72034/72042
stopped, root `agent-scratch-v001.weg1sgre` removed with
`freeze_attended_build.py remove`.

- `BPR-2007-agent-sheet`: Accept (`6515a88d`).
- `BPR-2008-build-question`: Accept (`df4d82a8`).
- `BPR-2007-grant-menu`: **Reject** (`5a9f122e`): explainer text into
  tooltips, no "runs the program setup found", "Use scratch folder", /model
  and /effort only where true, "Allow build and run tests" as a toggle.
- `RFC-BPR-0104-AC03-bound`: **Reject** (`8443d4bd`): the limit a setting,
  scratch folders cleaned up, disk use seen in Insights and cleaned up from
  Settings.

## Post-review round (2026-11-11)

Frontend-design was loaded before the Qt work. The independent critical
review of the launch allowances (AC07) returned pass-with-findings; every
finding is fixed or disclosed, test first where a test could run
([ledger](../../verification/RFC-BPR-0104.md#owner-look-e97c6a3d-and-the-critical-review-2026-11-11)).
A re-review of the fixes is owed.

- Picker: Use scratch folder and Allow build and run tests, both toggles for
  this pull request, explained only in tooltips; each checked only while the
  launch carries it; /model and /effort as the Model and Effort menus'
  tooltips only where true.
- Limit a setting (256 MiB to begin with), a turn that reaches it stopped;
  unused folders removed after the kept days (30); Settings shows the
  folders' size and has Clear scratch folders. Insights and workspace cleanup
  are [[BPR-2101]] (Backlog).
- Codex grant only where it cannot widen the reviewer's configuration;
  Claude's rules tightened (no direct-run or chmod rule, `Bash(*/..*)`
  denied); glob characters refused in a grant path; the build question
  re-checks the pull request.
- Footer: an agent not yet checked is drawn quiet, not red (owner note "Why
  is it red?" on BPR-2051's footer).

The card stays parked on `human-review` for the new look (below) and the
re-review.

Second post-review round (2026-11-11): the re-review of `cf8a9c12d`
returned pass-with-findings; N1 (Codex composition: the reviewer's own
checkout, the home Codex reads, attached flags, `/etc/codex/config.toml`, a
BOM, inline tables), N2 (an end-to-end course from the configuration to the
launch), N3 and N4 (scratch state per pull request and agent, kept across a
restart), N5 (the parent-step denial names the runners) and N6 (a full folder
gives Claude no run rule) are fixed; dispositions in the
[ledger](../../verification/RFC-BPR-0104.md#re-review-of-the-fixes-2026-11-11).
Another independent re-review is owed.

## Owner look (filed 2026-11-11, not waited on)

Session `246ef52c2815c6c16b3aeb5dacc5a765`, state
`/Users/jaeger/.attended-review-state/agent-grant-v002-2026-11-11/state`
(URL in `../serve.log` beside it), cursor 9, host PID 89403 (Tailscale child
89420). Frozen root `/Users/jaeger/agent-grant-v002.y_rhztpu` from
`b2edc1fa3b56` (launcher check reached `ATTENDED_PULL_REQUEST_OPENED`; the
looks `agent_footer --look` and `better_pull_review_qt_settings_window_tests
--agents-roster-look` are in its build). Candidates, one question each, ready
and protected: `BPR-2007-picker-toggles-v2`, `RFC-BPR-0104-scratch-settings`,
`BPR-2051-footer-setup-quiet`. Native renders (Cocoa off screen, light and
dark) are their media. Insights disk use has no candidate: it is BPR-2101,
not built.
