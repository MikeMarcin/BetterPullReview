---
schema: "implement-rfc.card.v1"
id: "BPR-2365"
title: "A Codex agent's model list is asked for only once the Agent tab is shown and past setup, so its Model menu stays empty until the first question; an empty or failed list is kept until a restart, and the run is not logged"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2365--BPR-2365--d94affcf7529.json","kind":"defect","found_by":"owner, 2026-11-23, build 92 on macOS: a custom Codex (JSONL) agent run through a sandbox launcher showed no models in its Model menu while the Agent tab was preparing a workspace. The models appeared only after a question was asked. Replayed by hand from a terminal, the launcher's `app-server` answered `initialize` and `model/list` with clean JSON and exited 0.6 s after stdin closed, 4.9 s in all","related_cards":["BPR-1753","BPR-2256"],"related_rfcs":[]}
acceptance: []
---

# BPR-2365: A Codex agent's model list is asked for only once the Agent tab is shown and past setup, so its Model menu stays empty until the first question

A Codex agent's models come from its own harness: owner look v006 runs
`<Program> <Arguments> app-server` and sends `initialize`, `initialized` and
`model/list` (`agent_setup_host::load_catalog`,
`src/application/src/agent_setup_host.cpp`, around line 1042;
`build_agent_model_catalog_launch`, `src/application/src/agent_launch.cpp`,
around line 5104). Until that run's answer is in, a Codex agent has no
models at all: unlike Claude Code, there is no built-in list to fall back
on (`src/app/src/github_application/window_agent_setup.cpp`, around line
2262).

That run is started too late, its failures are kept for the rest of the
launch, and none of it is logged.

## Observed

The owner, 2026-11-23, build 92 on macOS. A custom agent speaking Codex
(JSONL) whose Program is a sandbox launcher, with that launcher's own two
fixed arguments.

1. Before any question in the pull request's Agent tab, the run settings
   pill's **Model** menu for that agent listed no models. Earlier in the
   same launch, the tab had waited minutes on its workspace (see the
   sibling card on the sparse cache). The menu's exact contents at that
   point were not recorded; by the code, it holds only "Default (the
   agent's own setting)" and **Other…** until the run starts.
2. The owner asked a question. A few seconds later the Model menu listed the
   harness's models.

The session log has no line for the catalog run, before or after.

Replayed by hand from a terminal, with a Dock app's `PATH`
(`/usr/bin:/bin:/usr/sbin:/sbin`), the same command and the same three
requests:

| | Result |
|---|---|
| stdout | four JSON lines: the `initialize` reply (`id` 0), `configWarning`, `remoteControl/status/changed`, the `model/list` reply (`id` 1) with `data` and `nextCursor`. No CR, no terminal codes |
| stdin closed after the list | the process exited 0.6 s later, exit 0 |
| total | 4.9 s, inside `agent_status_timeout_milliseconds` (15 s) |

So the harness answers and BetterPullReview reads it. The menu was empty
because the run had not been started.

## Cause

1. **Asked for late.** The only callers are the Agent tab's
   `CatalogDispatch` (`src/adapters/qt/src/agent_tab_view.cpp`):
   - its refresh, only when `isVisible()` and the state is neither
     `setting_up` nor `not_set_up` (around line 1295);
   - the run settings source's `Catalog` (around line 954).

   A tab waiting on its workspace or its review ([[BPR-2256]]) never asks.
   Neither do Settings or a tab that is hidden.
2. **No sign it is coming.** The picker says "Asking <agent> for its
   models…" only while `Listing`, which is set only once the run is
   started (`src/adapters/qt/src/agent_run_settings_picker.cpp`, around
   line 242). Before that, the menu is indistinguishable from an agent that
   has no models.
3. **A failure is kept until a restart.** The result is stored in
   `Projection.Catalogs[entry]` whatever it is. An empty one, from a
   timeout, a nonzero exit, a malformed frame or an error reply, is kept,
   and `load_catalog` returns early for any entry already in `Catalogs`
   (around line 1047). Only `reset()` clears it (around line 1116).
4. **Not logged.** `load_catalog` writes no diagnostic. A failed list can't
   be told from one never asked for.

## Change

### 1. Ask as soon as the agent is known

- Start the catalog run for each Codex entry when the setup host has it
  (setup loaded, an entry added, or its Program or arguments changed),
  off the lane as today. Don't wait for the Agent tab to be visible or
  ready.
- Changing an entry's Program or Arguments forgets its catalog and asks
  again.
- Keep the one-at-a-time rule per entry (`Cataloguing`).

### 2. Say what the menu is waiting for

- Before the list is in, the Model menu says "Asking <agent> for its
  models…", whether or not the run has started.
- If the run failed, the menu says the agent did not list its models and
  offers to ask again, beside **Other…**.

### 3. Don't keep a failure

- Store a catalog only when the run succeeded and returned at least one
  model. A failed or empty run is retried the next time the menu opens, at
  most once a minute per entry.

### 4. Log it

- One `agent.catalog` line per run: entry protocol, outcome, failure class,
  exit code, duration, model count. No model names or output (the
  BPR-1828 content policy).

### 5. Tests

- Setup host: a Codex entry's catalog run starts when the entry is loaded,
  with no Agent tab shown.
- A run that times out or exits nonzero leaves no catalog, logs its
  failure, and runs again when asked.
- Changing an entry's Program forgets its catalog.
- Qt: before a catalog is in, a Codex agent's Model menu shows the asking
  line; after a failure, it shows the failure and an ask-again action.

## Acceptance

- AC1. A Codex agent's models are in its Model menu without asking a
  question first, once its harness has answered.
- AC2. Until then the menu says it is asking; after a failure it says so.
- AC3. A failed or empty list is asked for again without a restart.
- AC4. Every catalog run is logged with its outcome and duration.
- AC5. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Owner step

On the Mac, with the launcher's Codex agent: open a pull request whose
workspace is still preparing and open the Model menu. The models are there,
or "Asking…" is, within a few seconds, before any question.

## Out of scope

- The Claude Code effort list (`build_agent_help_launch`) is fetched the
  same way and gains the same timing. It has a built-in fallback, so it is
  not a visible defect.
- Why the workspace was preparing: the sibling card on a sparse cache taken
  as complete.
