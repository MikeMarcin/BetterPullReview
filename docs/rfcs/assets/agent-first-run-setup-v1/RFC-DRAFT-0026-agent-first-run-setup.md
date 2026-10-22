---
id: "RFC-DRAFT-0026"
title: "Set up an agent the first time the Agent tab opens"
status: "Proposed"
metadata: {"origin": {"date": "2026-10-22", "channel": "owner chat, relayed by the coordinator"}, "card": "BPR-PENDING-AN", "revision": "r1 2026-10-22", "authoring": "author-rfc; frontend-design:frontend-design loaded for the design frames", "design_frames": "docs/design/BPR-PENDING-AN-agent-setup-v1/index.html", "observed_at": "b69b82c9b", "owner_questions": "Q1-Q6 open; asked in a fresh attended-review session (see the card)"}
---

# Set up an agent the first time the Agent tab opens

## Problem and grounded current behavior

The owner, 2026-10-22, verbatim:

> I think we need some ux for setting up your agent the first time you go to
> the agent tab. Maybe a little wizard I'm not sure.

Observed on `b69b82c9b`, a tests-on release build rendered offscreen on the
`conventional-comments-v1` fixture with a fresh profile, light and dark
([frames](../../../design/BPR-PENDING-AN-agent-setup-v1/index.html#today)):

- **The tab assumes an agent.** The Agent tab ([[BPR-1594]],
  `src/adapters/qt/src/agent_tab_view.cpp`) shows its `not_set_up` empty
  state ("Set up an agent", which opens Settings › Agents & context) only when
  the launch panel offers no profile (`refresh`, line 436). On github.com and
  on an explicit GHES host the invocation host always offers profiles
  (`agent_invocation_host.cpp`, `publish_offerings`; `built_in_agent_policy`),
  so a first-time user never sees it. The tab opens on a conversation.
- **Four protocol names, Codex first.** The menu lists the built-in registry
  (`core::built_in_agent_profiles`): "Codex App Server", "Codex JSONL",
  "Approved Claude stream JSON", "External terminal harness". The first is
  selected whatever is installed; in the default 375-pixel drawer it reads
  "Codex Ap…".
- **Nothing is looked for until Ask.** The AI agent boundary says so on
  purpose: "Merely showing the panel performs no executable discovery. A
  missing executable becomes `failed_to_start` only after an explicit start
  gesture" ([ai-agent-boundary.md](../../../architecture/ai-agent-boundary.md#provider-profiles)).
  The one search is Claude's ([[BPR-1301]]: `PATH`, then
  `core::usual_claude_folders`), and only Settings shows its result
  ("Using /opt/homebrew/bin/claude (found on PATH).").
- **The first Ask fails, in jargon.** It ended "Codex App Server stopped
  before answering. Finished: failed_to_start. Your question is kept."
- **Codex cannot start from a shipped build.** The Codex profiles name the
  bare `codex`. Only Claude's launch is resolved to an absolute path
  (`application::wrapped_claude_process`). On macOS and Linux the bundled
  supervisor refuses a program that is not absolute
  (`unix_agent_supervisor.cpp`, `argv[1][0] != '/'`), and on Windows
  `CreateProcessW` is given the bare name as its application name, which is
  never searched for. Tests replace the executable with an absolute fixture
  path (`CodexAgentExecutable`), so no test has caught it. The external
  terminal profile (`bpr-agent-terminal`) has the same defect.
- **The owner's own first run failed before any agent ran** (2026-10-22,
  relayed by the coordinator with a photograph). Wanting to use their Claude
  Code wrapper, the owner chose **External terminal harness**, scope
  "Selected lines", head `fba81b4`. The tab said "External terminal harness
  stopped before answering. Finished: workspace_unavailable. Your question is
  kept.", and **Show the log** showed Git's own words: "error: Could not read
  984772c… fatal: Failed to traverse parents of commit fba81b4…".
  `workspace_unavailable` means the disposable review workspace could not be
  made from the cache (`agent_invocation_host.cpp`, `workspace_outcome`,
  [[BPR-0348]]), so no agent was started; the likely cause, a cache missing a
  parent commit, is being fixed separately. Three things show here: the
  failure named its harness although the harness never ran; the reason was a
  stable code and a raw Git log; and the only actions were Ask again and the
  log.
- **"External terminal harness" means something else.** It is the fallback
  protocol for a corporate harness with no machine-readable interface: a
  versioned plain-text envelope on standard input, plain text back
  ([ai-agent-boundary.md](../../../architecture/ai-agent-boundary.md#protocol-and-process-lifecycle)).
  Its program, `bpr-agent-terminal`, ships nowhere, and like Codex it is a bare
  name that cannot start. A Claude wrapper is not chosen there: it is Settings
  › Agents & context › Claude › Process wrapper, used by the Claude profile.
  The menu explains none of this, so the owner's reading ("my wrapper is a
  terminal harness") was the natural one.
- **Sign-in is the agent's, and unchecked.** BetterPullReview has "no agent
  sign-in prompt or check" (owner, 2026-10-06, "Accept (no switch)"). A Claude
  request that answers `authentication_failed` ends as `not_signed_in`, which
  only tour writing words ("Run claude auth login in a terminal").
- **The MCP context helper is not a setup step.** It is the application's
  own read-only executable, started without arguments and pinned to a frozen
  snapshot (`mcp_context_helper_supervisor.hpp`). The composition pins a
  snapshot per invocation (`github_application.cpp`, `launch_agent_invocation`)
  but no shipped launch gives an agent an MCP configuration naming it. A user
  has nothing to register.
- **`bpr cycle` and `bpr tour` are the other direction.** A harness runs
  `bpr` with the `bpr-cycle` or `pr-tour` skill (`packaging/agent-skills/`).
  `bpr` ships only as the `TourCLI` install component built from source; no
  release package carries it
  ([release-packaging-maintainers.md](../../../development/release-packaging-maintainers.md#agent-free-variants-further-detail)).
  The skills are distributed through MASK and AgentSkills.

So the working first-run path today is: install Claude Code, sign in to it
in a terminal, open a pull request, find "Approved Claude stream JSON" in the
menu, and Ask. Codex does not work. The design page's
[table](../../../design/BPR-PENDING-AN-agent-setup-v1/index.html) marks
which steps the app could detect, do or only explain.

## Proposed behavior and boundaries

### When setup shows

The Agent tab shows **Set up an agent** in place of the conversation when no
agent has been chosen for this application profile, and again when the
chosen agent can no longer be found (see [Partly set up](#partly-set-up)).
The empty `not_set_up` state with its Settings button is replaced by this.
Its shape is Q1; the frames show A, the recommendation.

The setup has three steps, a real sequence: **Agent**, **Check it works**,
**Ready**. One line says what an agent here is: it runs on this computer with
its own sign-in, reads a private copy of the code, and publishes nothing
unless the reviewer makes it a comment.

### Agent: detection, then a choice

When setup shows, and on **Look again**, the host looks for each agent the
current Git host's policy permits (Q2). Looking reads file metadata only, as
the Claude search does today: no process starts, no shell runs, nothing is
written, and the application's own `PATH` is used. Claude is found as it is
now (a saved process wrapper wins, then `PATH`, then the usual folders).
Codex is found the same way (Q5).

Each found agent is one row: its plain name (Q4), the absolute path that
would run, and how it was found ("Found on PATH.", "Found in a usual install
folder.", "Your process wrapper."). A row the host policy refuses stays
visible, disabled, saying why ("Not allowed for github.example.com."), and a
required agent is the only selectable row.

- **One found:** it is chosen. The step is done.
- **Several:** Claude Code first, then Codex; none is chosen until the
  reviewer picks one.
- **None:** "No agent found on this Mac" with where it looked; **Install
  Claude Code** and **Install Codex** open their vendors' install pages in the
  browser through the host; **Look again**; **Use a program I choose…** opens
  the Settings row for a program path.

### Check it works

One explicit **Check** proves the two things a first question needs, each a
row with its own result:

1. **The agent answers.** A bounded run of the chosen agent through the
   host's supervised launcher, with that profile's environment allowlist and
   no review context (Q3; the recommendation is the agent's own status
   command: `claude auth status`, and Codex's equivalent, with a `--version`
   read).
2. **The private copy of this pull request can be made.** The host prepares
   the disposable review workspace for the current head exactly as an Ask
   would (the same cache transfer, checks and owner-private destination),
   then removes it by the existing marker-checked cleanup. With **Use my
   working copy** chosen, the row checks that choice instead. No agent sees
   it.

The two run in that order, are Stop-able, and each times out (15 seconds for
the agent, the workspace's existing bound for the copy). A row's failure does
not hide the other row's result.

- **Both pass:** the step is done and the setup goes to Ready.
- **Not signed in:** "Claude Code isn't signed in", the exact command to run
  in a terminal (`claude auth login`, `codex login`) with **Copy**, and
  **Check again**. BetterPullReview never asks for, reads or stores a
  credential; the command is text the reviewer runs.
- **The copy cannot be made:** "The private copy of this pull request
  couldn't be made", one sentence of cause in the reviewer's terms (for the
  owner's case: "This Mac's saved copy of the repository is missing commit
  984772c, which comes before the head fba81b4. No agent was started."), and
  the recovery actions the host actually offers: **Get the pull request
  again** (fetch the head into the cache once more, through whatever repair
  the separate fix provides) and **Use my working copy instead** (the
  existing checkout choice, disclosed as today). Then **Check again**.
- **The agent fails to start or answers something unreadable:** what
  happened in words, with **Check again**.

Every failure, here and in a later Ask, leads with words the core chooses
from the typed outcome and its bounded evidence: which part failed (the
agent, or the copy), whether the agent ran, and the next action. A stable
code such as `failed_to_start` or `workspace_unavailable`, and raw Git or
agent output, are behind **Show details**, bounded, never the only words. A
failure is named after what failed: a copy that could not be made does not
say "External terminal harness stopped".

### Ready, and how it says success

The steps fold into one green line above the conversation, "Claude Code is
ready", with "Signed in", the version when the check read one, and the path.
It can be dismissed and goes by itself after the first question. The context
row appears with the chosen agent, and the question field takes the keyboard.

The choice is saved for the application profile, beside the Claude process
wrapper, and every later review opens on it. The saved value is a profile
identity and, for a program the reviewer chose, its path; never a credential.
Review Cycles **Send to Agent** offers the same saved agent first.

### Partly set up

- The saved agent is no longer found (moved, uninstalled, wrapper refused):
  the stopped callout says so with the path ("Claude Code isn't at
  /opt/homebrew/bin/claude any more") and offers **Set up again**, which opens
  the setup with detection run again. The question is kept.
- A check that has not passed is not remembered as passed: closing the tab
  mid-setup and coming back resumes at the first unfinished step.
- **A later Ask that fails for a setup reason returns to setup.** When an
  Ask ends `failed_to_start`, `not_signed_in` or `workspace_unavailable`, the
  stopped callout uses the words of the failing check and offers **Check
  again** (that check, then the question if it passes) and the same recovery
  actions, rather than only Ask again. The question is kept. Other outcomes
  (the agent stopped mid-answer, the head moved) keep their present callouts.

### Running setup again

**Set up agent…** is in the Agent tab's History menu and on Settings ›
Agents & context. It opens the setup with detection run again and the
current choice selected.

### Boundaries

- The core decides; the host performs; Qt presents. The core owns the setup
  state, its transitions, the order and wording of rows, policy filtering and
  the decoding of a check's bounded output. The application host owns the
  metadata search, the check's launch, cancellation and the saved choice. The
  agent adapter owns locating and inspecting files. The widget raises
  intents and performs no file, process, network or settings effect.
- The agent stays read-only and unchanged: no new authority, no tool, no
  environment name, and no credential field. The MCP helper is unchanged and
  nothing in setup configures it.
- Agent-free builds have no setup, as they have no Agent tab.
- Out of scope: the app installing or signing in to an agent; registering the
  MCP helper with a harness; and, subject to Q6, setting up `bpr cycle`,
  `bpr tour` and their skills for use from an agent.

### Decisions for the owner

Asked one per question in a fresh attended-review session; affected scope is
unready until answered.

- **Q1. Shape.** A: set up inside the Agent tab (recommended: it is where the
  reviewer already is, and success lands on the conversation). B: an empty
  state with next steps that detects nothing and sends the reviewer to
  Settings. C: a wizard sheet over the window with Back and Continue.
- **Q2. Detection when the tab first opens.** Accept or Reject. Accepting
  amends the AI agent boundary's "Merely showing the panel performs no
  executable discovery" to "Showing setup searches file metadata for the
  permitted agents; nothing is started until an explicit gesture." Rejecting
  keeps detection behind a **Find agents** button.
- **Q3. The check.** A: the agent's status command (recommended: it answers
  the sign-in question without a model request). B: one short request with no
  code ("Reply with OK"), which proves the whole path and spends a small
  request. C: no check; the first real answer is the check.
- **Q4. Plain names.** Accept or Reject. The menu says "Claude Code" and
  "Codex". The Codex protocol (App Server or JSONL) moves to Settings ›
  Agents & context, with App Server as Codex's default. The external terminal
  harness leaves the Agent tab's menu and appears in Settings as "A harness
  that reads plain text", with one line saying it is for a tool that has no
  Claude or Codex interface, and that a Claude wrapper belongs under Claude.
  The owner's first run chose it for a Claude wrapper.
- **Q5. Find Codex as Claude is found.** Accept or Reject. `PATH`, then
  usual install folders (`HOME/.local/bin`, `/opt/homebrew/bin`,
  `/usr/local/bin`, `HOME/.npm-global/bin`; on Windows only `codex.exe`, never
  a `.cmd`), and a Codex program path in Settings under the same rules as
  Claude's process wrapper. The plain-text harness is never searched for; it
  runs only a program path the reviewer chose. Without this, Codex and the
  plain-text harness never start from a shipped build.
- **Q6. Scope.** Accept or Reject. Leave "use BetterPullReview from your
  agent" (`bpr cycle`, `bpr tour` and their skills) to its own card, since
  `bpr` is in no release package.

## Acceptance

- **AC01:** With no agent chosen for the profile, opening the Agent tab on a
  github.com fixture shows the setup, not a conversation, in light and dark,
  and no process has started (launcher probe count unchanged).
- **AC02:** Detection reports each permitted agent with its absolute path
  and how it was found, for fixture `PATH` and home folders holding none, one
  and both of `claude` and `codex`, and a saved wrapper; an explicit GHES host
  shows Codex disabled with its reason.
- **AC03:** With Codex found, a shipped-configuration Ask (no fixture
  executable override) starts the found absolute `codex`; with none found it
  ends with words naming where it looked, not a bare `failed_to_start`.
- **AC04:** One Check runs the agent check once through the supervised
  launcher with the profile's allowlist and no review context, then prepares
  and removes one disposable workspace for the current head; both are
  Stop-able and bounded, and the passed, not-signed-in, copy-failed and
  failed-to-start results each show their defined words and actions (fixture
  agents, and a fixture cache missing the head's parent commit).
- **AC05:** After a passed check the tab shows the Ready line and the
  question field has focus; a new window and a new review open on the saved
  agent without setup; Send to Agent offers it first.
- **AC06:** Removing the saved agent's executable leads the next Ask to the
  partly set up callout and **Set up again**; History and Settings both open
  the setup with the current choice selected.
- **AC07:** No credential, token or environment value is read, stored,
  logged or shown by setup; the saved choice holds only a profile identity
  and an optional path (store inspection and log scan).
- **AC08:** An Ask that ends `workspace_unavailable` on that fixture cache
  shows the copy's words, not the harness's, with Check again and Use my
  working copy instead; no stable code or Git output is visible until Show
  details; `failed_to_start` and `not_signed_in` likewise lead with their
  check's words.
- **AC09:** The owner, attended, on a frozen build: "The first time I open
  the Agent tab it tells me what to do, and I end up asking a question."
