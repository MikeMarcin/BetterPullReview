---
id: "RFC-DRAFT-0026"
title: "Set up an agent the first time the Agent tab opens"
status: "Proposed"
metadata: {"origin": {"date": "2026-10-22", "channel": "owner chat, relayed by the coordinator"}, "card": "BPR-1747", "revision": "r2 2026-10-22", "revisions": ["r1: agent, check and ready steps", "r2: the owner's first-run failure, the Repository step and worktrees, the review brief, and owner decisions D1 (an editable review skill) and D2 (clone folders)"], "authoring": "author-rfc; frontend-design:frontend-design loaded for the design frames", "design_frames": "docs/design/BPR-1747-agent-setup-v1/index.html", "observed_at": "b69b82c9b", "owner_decisions": "D1 and D2 answered in chat 2026-10-22; Q1-Q9 open in a fresh attended-review session (see the card)"}
---

# Set up an agent the first time the Agent tab opens

## Problem and grounded current behavior

The owner, 2026-10-22, verbatim:

> I think we need some ux for setting up your agent the first time you go to
> the agent tab. Maybe a little wizard I'm not sure.

Observed on `b69b82c9b`, a tests-on release build rendered offscreen on the
`conventional-comments-v1` fixture with a fresh profile, light and dark
([frames](../../../design/BPR-1747-agent-setup-v1/index.html#today)):

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
  [[BPR-0348]]), so no agent was started; the cause, a commit missing from
  the cache, is being fixed separately. The failure named a harness that
  never ran, the reason was a stable code and a raw Git log, and the only
  actions were Ask again and the log.
- **"External terminal harness" means something else.** It is the fallback
  protocol for a corporate harness with no machine-readable interface: a
  versioned plain-text envelope on standard input, plain text back
  ([ai-agent-boundary.md](../../../architecture/ai-agent-boundary.md#protocol-and-process-lifecycle)).
  Its program, `bpr-agent-terminal`, ships nowhere. A Claude wrapper is not
  chosen there: it is Settings › Agents & context › Claude › Process wrapper,
  used by the Claude profile. The menu explains none of this.
- **What the agent works in today.** By default, a disposable workspace: a
  new repository the Git adapter initializes, fills with `pack-objects
  --revs` of the head from the application's own bare cache, and checks out
  detached (`installed_git_process.cpp`; [workspaces](../../../architecture/ai-agent-boundary.md#workspaces)).
  It is not shallow by design: the cache is a full, filterless fetch of the
  base and head ([app-owned bare repository](../../../architecture/git-and-epoch-model.md#app-owned-bare-repository)),
  so the head's ancestry, blame and callers are there when the cache is
  intact. What it lacks is everything else a real clone has: other
  branches, tags and remotes; LFS content (filters are off) and submodules;
  and it depends on the cache's integrity, which is what failed the owner.
  Neither it nor a clone carries untracked build output or installed
  dependencies.
- **"Use my working copy" cannot be used.** The existing-checkout mode
  needs `github_application_config::ExistingCheckout`, which no shipped path
  sets (`profile_desktop.cpp` resets it), so the button always ends
  `no_checkout_available`. Even when set, it is the reviewer's checkout
  itself, with their uncommitted work, not a copy at the head. Nothing links
  a GitHub repository to a local clone.
- **What the agent is told.** The version-two context supplement carries
  identities: repository, PR URL and ID, base and head OIDs, the epoch pair,
  file identities and read state, defects, the selection, and chosen notes.
  It does not carry the description, the review threads or the diff; "the
  agent retrieves PR data through its own GitHub tools"
  ([context supplement](../../../architecture/ai-agent-boundary.md#context-supplement)).
  No review method is sent.
- **The MCP context helper is not a setup step.** It is the application's
  own read-only executable, started without arguments and pinned to a frozen
  snapshot (`mcp_context_helper_supervisor.hpp`). No shipped launch gives an
  agent an MCP configuration naming it. A user has nothing to register.
- **`bpr cycle` and `bpr tour` are the other direction.** A harness runs
  `bpr` with the `bpr-cycle` or `pr-tour` skill (`packaging/agent-skills/`).
  `bpr` ships only as the `TourCLI` install component built from source; no
  release package carries it
  ([release-packaging-maintainers.md](../../../development/release-packaging-maintainers.md#agent-free-variants-further-detail)).

So the working first-run path today is: install Claude Code, sign in to it
in a terminal, open a pull request, find "Approved Claude stream JSON" in the
menu, and Ask, in a workspace that works only while the cache is whole.
Codex does not work. The design page's
[table](../../../design/BPR-1747-agent-setup-v1/index.html) marks
which steps the app could detect, do or only explain.

The owner's own review skill, relayed by the coordinator, shows what an
agentic review needs: the pull request's context (description, base, diff,
threads and the exact head), the code locally (fetch the pull request and
make a dedicated worktree of an existing clone, or clone first), following
the change through whole functions, callers, tests and configuration,
verifying each finding by a failure path, a focused test or a comparison with
base, and saying where evidence is missing.

## Proposed behavior and boundaries

### When setup shows

The Agent tab shows **Set up an agent** in place of the conversation when no
agent has been chosen for this application profile, or when the pull
request's repository has no linked clone and no recorded choice of the
private copy. It shows again when the chosen agent or the linked clone can no
longer be found ([Partly set up](#partly-set-up)). The empty `not_set_up`
state with its Settings button is replaced by this. Its shape is Q1; the
frames show A, the recommendation.

The setup has four steps, a real sequence: **Agent**, **Repository**,
**Check it works**, **Ready**. A finished step folds to one line with its
choice and **Change**. For a later pull request in another repository, only
the Repository step is new. One line says what an agent here is: it runs on
this computer with its own sign-in, in its own worktree of the reviewer's
repository at the head, and publishes nothing unless the reviewer makes it a
comment.

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

### Repository: the reviewer's clone, remembered

Owner decision D2 (below) settles where clones are looked for.

**Clone folders** is a list in Settings › Repositories, empty at first. It
holds absolute folders the reviewer chose, such as `~/Documents/GitHub`, and
is saved for the application profile. When the Repository step shows, on
**Look again**, and after the list changes, the host looks in each listed
folder for a clone of the pull request's repository:

- **Where it looks.** The folder itself and its directories down to two
  levels below it (`~/Documents/GitHub/engine`, `~/src/org/engine`), at most
  2,000 directories per folder. It follows no symbolic link, junction or
  alias, and skips hidden directories other than `.git`.
- **What it reads.** A candidate is a directory whose `.git` is a
  directory (a linked worktree's or submodule's `.git` file is not followed;
  the main clone is found instead). The host reads that `.git/config`, at
  most 64 KiB, and nothing else; `include` directives are not followed. It
  runs no Git, no shell and no program, and writes nothing.
- **How it matches.** The core reads each `[remote "NAME"]` section's
  `url` values and normalizes them: `https://HOST/OWNER/NAME(.git)`,
  `ssh://[USER@]HOST[:PORT]/OWNER/NAME(.git)` and `USER@HOST:OWNER/NAME(.git)`.
  A clone matches when one remote's host equals the pull request's Git host
  (github.com, or the configured GHES host) and its owner and name equal the
  repository's, compared case-insensitively. User information in a URL (a
  token in `https://TOKEN@github.com/…`) is dropped before anything else
  sees the URL; no URL is logged, stored or shown with it.
- **What a row shows.** The clone's folder, the matching remote by name and
  host path (`origin  github.com/bpr-fixtures/engine`), which clone folder it
  is in, and its current branch read from `.git/HEAD`.

The step then:

- **One match:** it is chosen, and **Use this clone** confirms it.
- **Several matches** (two clones, or a clone with the repository as
  `upstream` and another as `origin`): all are listed, the previously linked
  one first, then by folder order in the list, then by path; none is chosen
  until the reviewer picks one.
- **No folder listed:** "Where do your clones live?" with **Add a folder…**
  and one-click suggestions for usual folders that exist (`~/Documents/GitHub`,
  `~/Developer`, `~/src`, `~/code`, `~/Projects`; existence is checked, their
  contents are not read until one is added).
- **None found:** "No clone of OWNER/NAME in your clone folders", where it
  looked, **Choose its folder…** (a folder anywhere; it must match the same
  way), **Clone into FOLDER** (the first clone folder; Q8 decides where the
  clone's objects come from), **Use a private copy**, and **Edit clone
  folders…**.

**The link is remembered** for the application profile, keyed by the Git
host and the repository's GitHub identity (its node ID, so a renamed
repository keeps its link), and holds the clone's absolute path and the
matching remote's name. Settings › Repositories › Linked clones lists each
link with **Change…** and **Forget**. Before each use the host checks the
link again by the same metadata read: a folder that moved, stopped being a
clone or no longer has a matching remote reopens the Repository step with
that reason. Choosing **Use a private copy** is remembered for the
repository the same way and is changeable there.

**The private copy** is today's disposable workspace, kept as the fallback
(Q7). Its row states its limits in one line: "the head and its history,
without your branches, tags, LFS files or submodules".

### The agent's worktree

With a linked clone, each agent session gets its own worktree of that clone
at the session's head (Q7):

- **Getting the head.** The host makes the head's objects present in the
  clone (Q8: from the application's own copy, or by the clone's own remote),
  then runs `git worktree add --detach WORKTREE HEAD_OID` in the clone. No
  branch is created and no ref is moved; the reviewer's checkout, index,
  branches, stashes and configuration are untouched. What the clone does
  gain is stated in the Repository step: the head's objects, and one
  `.git/worktrees/ENTRY` administrative entry while the worktree exists.
- **Running Git in a clone the reviewer owns.** Every command BetterPullReview
  runs there turns repository-configured programs off: hooks (an empty
  application-owned hooks path), `core.fsmonitor`, and filters, so LFS files
  are pointers, as in the private copy. Submodules are not checked out. These
  run through the Git adapter's serialized process lane with its bounds and
  quarantine, never a shell.
- **Where it lives.** Under the application's data root,
  `agent-worktrees/OWNER-NAME-PR-HEAD7-ID`, owner-private, with the same
  ownership marker and crash reclamation as the disposable workspace. Never
  inside the clone's own working tree.
- **How long.** A worktree belongs to one agent session and its follow-up
  turns. It is removed (`git worktree remove --force` on that exact path,
  after the marker check) when its session is superseded, when the review is
  closed, and when more than three exist for one repository, oldest first.
  Leftovers from a crash are reclaimed at the next start. The host never runs
  `git worktree prune`, which would touch the reviewer's own worktrees.
  Settings › Repositories shows the count and size.
- **A new head or a force push.** The session stays bound to the head it
  read, as today ([[BPR-0532]]'s realignment is unchanged): its worktree
  stays at that head until the session ends. **Start a new session** fetches
  the new head and makes a new worktree. A force push that removes the old
  head from GitHub does not remove it from the worktree or the clone.
- **Writes inside it are the agent's.** Running tests and builds writes into
  the worktree, and all of it goes when the worktree is removed.
- **Read-only, stated honestly.** BetterPullReview pushes, publishes and
  comments nothing for the agent, passes it no credential, and the brief says
  the agent may not push, comment or change the reviewer's checkout. But a
  worktree of a real clone has that clone's remotes, and the agent's own
  tools may reach the reviewer's own Git credentials. Whether it can push is
  the harness's permission policy (Claude's `-p` refuses tools that need
  approval unless the reviewer allowed them), not something BetterPullReview
  can enforce. The private copy has no remote.

### The review brief

Every review request carries a brief, as context supplement version three
(an amendment of RFC-BPR-0011's version two, which it extends):

- the pull request: host, repository, number, URL, title and description
  (at most 16 KiB, cut at a line with the cut stated);
- the base branch name and base OID, and the exact head OID, which names what
  was reviewed;
- the changed files with their status, and the diff named rather than
  copied: `git diff BASE...HEAD` in the worktree;
- the review threads: unresolved first, each with its file and line anchor,
  authors' logins and bodies, at most 64 KiB in all, with omissions counted;
- the worktree's path, whether it is a worktree of the reviewer's clone or
  the private copy, and that copy's limits;
- the version-two fields as now, and the review skill (D1, Q9).

The Ready step shows the brief as **What the agent gets**, which the reviewer
can open at any time from the context row. Everything in it is what the
signed-in reviewer can already read on GitHub; no credential, token or
environment value has a field.

### The review skill

Owner decision D1 (below) settles that the review method ships as a skill
the reviewer can edit.

- **The shipped default** is an Agent Skills package, `bpr-pr-review`
  (`SKILL.md` and its references), kept in `packaging/agent-skills/` beside
  `pr-tour` and `bpr-cycle` and built into the application as a resource. Its
  front matter names `metadata.bpr-version`, an integer raised whenever the
  default changes. Its text is the owner's workflow above: read the brief;
  follow the change through the worktree; verify a finding before reporting
  it; compare with base to say whether the pull request introduced it; say
  what could not be verified. It grants nothing: the agent's own tools and
  permissions still govern.
- **Editing.** Settings › Agents & context › Review skill shows the skill's
  `SKILL.md` in a plain-text editor with **Save**, **Open in editor** (the
  profile's copy, in the system's editor for its type, through the host) and
  **Restore default**. The first edit copies the default into the application
  profile's data folder, `agent-skills/bpr-pr-review/`, and records the
  `bpr-version` and SHA-256 of the default it came from. A saved skill must
  keep its `name` and `description` and stay under 64 KiB in all; a refused
  save says why and keeps the text. **Restore default** removes the edited
  copy after saving it to Downloads, unless the reviewer unticks that, as the
  existing resets do. This follows the writing hooks' pattern (Settings ›
  AI assistance, `bpr tour hooks show|edit|reset`).
- **Surviving updates.** An update replaces only the built-in default. The
  edited copy is profile data, untouched by updates and kept through
  uninstall like other profile data.
- **Offering a newer default.** When the built-in `bpr-version` is above the
  one an edited copy came from, the Review skill section says "Version N is
  available" with **Compare** (the two side by side), **Use version N** (the
  edited copy goes to Downloads first) and **Keep mine** (remembered for that
  version). An unedited default simply follows the update. Nothing overwrites
  an edited copy silently.
- **Reaching the agent** is Q9. With A, the recommendation, the skill's text
  goes in the brief of every review request, the same for Claude Code, Codex
  and the plain-text harness, and nothing is written to any harness's
  configuration. The reviewer's own skills in their harness still load as the
  harness loads them.

### Check it works

One explicit **Check** proves the two things a first question needs, each a
row with its own result:

1. **The agent answers.** A bounded run of the chosen agent through the
   host's supervised launcher, with that profile's environment allowlist and
   no review context (Q3; the recommendation is the agent's own status
   command: `claude auth status`, and Codex's equivalent, with a `--version`
   read).
2. **The agent's worktree can be made.** The host makes the worktree for
   the current head as an Ask would and keeps it for the first session. With
   the private copy chosen, it prepares and removes that copy instead.

The two run in that order, are Stop-able, and each is bounded (15 seconds
for the agent; the Git adapter's existing bounds for fetch and checkout). A
row's failure does not hide the other row's result.

- **Both pass:** the step is done and the setup goes to Ready.
- **Not signed in:** "Claude Code isn't signed in", the exact command to run
  in a terminal (`claude auth login`, `codex login`) with **Copy**, and
  **Check again**. BetterPullReview never asks for, reads or stores a
  credential; the command is text the reviewer runs.
- **The worktree or copy cannot be made:** what failed in the reviewer's
  terms and the recovery the host actually offers. A missing head commit:
  **Get the pull request again**. A linked clone that moved or no longer
  matches: **Choose its folder…**. The private copy failing (the owner's
  case, "This Mac's saved copy of the repository is missing commit 984772c,
  which comes before the head fba81b4. No agent was started."): **Get the
  pull request again** or **Link your clone instead**. Then **Check again**.
- **The agent fails to start or answers something unreadable:** what
  happened in words, with **Check again**.

Every failure, here and in a later Ask, leads with words the core chooses
from the typed outcome and its bounded evidence: which part failed (the
agent, the worktree or the copy), whether the agent ran, and the next
action. A stable code such as `failed_to_start` or `workspace_unavailable`,
and raw Git or agent output, are behind **Show details**, bounded, never the
only words. A failure is named after what failed: a worktree that could not
be made does not say "External terminal harness stopped".

### Ready, and how it says success

The steps fold into one green line above the conversation, "Claude Code is
ready", with "Signed in", the version when the check read one, and where it
works ("a worktree of ~/Documents/GitHub/engine at 7f3a9c1"). Below it,
**What the agent gets** shows the brief. The line can be dismissed and goes
by itself after the first question. The context row appears with the chosen
agent, and the question field takes the keyboard.

The agent choice is saved for the application profile, beside the Claude
process wrapper; the repository link is saved per repository as above. Every
later review opens on them. Neither holds a credential. Review Cycles **Send
to Agent** offers the same saved agent first.

### Partly set up

- **The saved agent is no longer found** (moved, uninstalled, wrapper
  refused): the stopped callout says so with the path ("Claude Code isn't at
  /opt/homebrew/bin/claude any more") and offers **Set up again**, which opens
  the setup with detection run again. The question is kept.
- **The linked clone is no longer a match:** the Repository step reopens
  with its reason and the clone folders searched again.
- **A check that has not passed is not remembered as passed:** closing the
  tab mid-setup and coming back resumes at the first unfinished step.
- **A later Ask that fails for a setup reason returns to setup.** When an
  Ask ends `failed_to_start`, `not_signed_in` or `workspace_unavailable`, the
  stopped callout uses the words of the failing check and offers its
  recovery and **Check again** (that check, then the question if it passes),
  rather than only Ask again. The question is kept. Other outcomes (the agent
  stopped mid-answer, the head moved) keep their present callouts.

### Running setup again

**Set up agent…** and **Change repository folder…** are in the Agent tab's
History menu. Settings › Agents & context has **Set up agent…**, and Settings
› Repositories has the clone folders and linked clones. Each opens the
setup, or that step, with detection run again and the current choices
selected.

### Boundaries

- The core decides; the host performs; Qt presents. The core owns the setup
  state and transitions, row order and wording, policy filtering, remote URL
  parsing and matching, the brief's bounds, the skill's version comparison,
  and the decoding of a check's bounded output. The application host owns the
  metadata reads, the Git commands for fetch, worktree and cleanup, the
  check's launch, cancellation, the skill files and the saved choices. The
  agent and Git adapters own locating, inspecting and running. The widget
  raises intents and performs no file, process, network or settings effect.
- The agent's authority does not grow: no new tool, environment name or
  credential field. The brief adds only what the reviewer can already read
  on GitHub. The MCP helper is unchanged and nothing in setup configures it.
- Clone discovery reads only folders the reviewer listed or chose, and only
  `.git/config` and `.git/HEAD` in them.
- Agent-free builds have no setup, no clone folders and no review skill, as
  they have no Agent tab.
- Out of scope: the app installing or signing in to an agent; registering the
  MCP helper with a harness; the existing-checkout mode, which stays as it is;
  and, subject to Q6, setting up `bpr cycle`, `bpr tour` and their skills for
  use from an agent.

### Owner decisions

- **D1. The review workflow ships as a skill the reviewer can edit.** The
  owner, 2026-10-22, verbatim, relayed by the coordinator: "I think we ship
  it as a skill and make it editable by the user." Settled in
  [The review skill](#the-review-skill); how it reaches the agent is Q9.
- **D2. A setting lists the folders that hold the reviewer's clones.** Same
  message, verbatim: "We probably also give the user a setting to point to
  their folders where it can find existing repo clones." Settled in
  [Repository](#repository-the-reviewers-clone-remembered).

### Decisions for the owner

Asked one per question in a fresh attended-review session; affected scope is
unready until answered. D1 and D2 are not asked again.

- **Q1. Shape.** A: set up inside the Agent tab (recommended: it is where the
  reviewer already is, and success lands on the conversation). B: an empty
  state with next steps that detects nothing and sends the reviewer to
  Settings. C: a wizard sheet over the window with Back and Continue.
- **Q2. Detection when the tab first opens.** Accept or Reject. Accepting
  amends the AI agent boundary's "Merely showing the panel performs no
  executable discovery" to "Showing setup searches file metadata for the
  permitted agents and in the listed clone folders; nothing is started until
  an explicit gesture." Rejecting keeps detection behind a **Look** button in
  each step.
- **Q3. The agent check.** A: the agent's status command (recommended: it
  answers the sign-in question without a model request). B: one short
  request with no code ("Reply with OK"), which proves the whole path and
  spends a small request. C: no agent check; the first real answer is the
  check. The worktree row is checked in every case.
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
- **Q7. Worktrees of the reviewer's clone by default.** Accept or Reject.
  Accepting makes a worktree of the linked clone the default place the agent
  works, with the private copy as the fallback whose limits are stated, and
  accepts what [The agent's worktree](#the-agents-worktree) says it does to
  the clone and that "no push" there is the harness's policy, not an
  enforcement. Rejecting keeps the private copy as the default, with a linked
  clone as an option.
- **Q8. Where the head's objects, and a new clone's, come from.** A: the
  application's own copy (recommended: local, no network and no credential;
  the head is already there, verified, whenever the review shows it). B: the
  clone's own remote, through the reviewer's Git and credentials, as
  `git fetch` would run in their terminal; this also works when the
  application's copy is damaged, as in the owner's failure. A clone made by
  **Clone into FOLDER** gets `origin` set to the repository's GitHub URL
  either way.
- **Q9. How the review skill reaches the agent.** A: in the request's brief,
  for every harness, writing nothing to any harness's configuration
  (recommended). B: written into each agent worktree as a project skill
  (`.claude/skills/bpr-pr-review/`, `.agents/skills/bpr-pr-review/`), where
  the harness loads it by its own conventions. C: installed once into the
  harness's user skill folder (`~/.claude/skills/`, Codex's equivalent) by an
  explicit button, and kept in step with each edit.

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
- **AC04:** For fixture clone folders holding no clone, one clone, two
  clones, a clone matched by `upstream` only, a URL carrying user
  information, an SSH URL, a linked worktree, a symbolic link to a clone and
  a folder three levels down, the Repository step lists exactly the matches
  defined in Repository, in its order; the read set is only `.git/config`
  and `.git/HEAD` files, no process starts, nothing is written, and no user
  information appears in any store, log or label.
- **AC05:** A chosen clone is remembered per repository across windows and
  restarts, survives a repository rename, and a moved or no longer matching
  clone reopens the Repository step with its reason; Settings lists, changes
  and forgets links and edits clone folders.
- **AC06:** On a fixture clone, a session's worktree is created detached at
  the exact head under the application's data root, with hooks, fsmonitor
  and filters off; the clone's branches, refs other than the worktree entry,
  index, stashes, configuration and checkout are byte-identical before and
  after; the worktree is removed when its session is superseded, when the
  review closes and past three per repository; a crash leftover is reclaimed;
  `git worktree prune` never runs.
- **AC07:** A new head leaves the old session's worktree at its head until
  the session ends, and **Start a new session** makes a new worktree at the
  new head.
- **AC08:** Every review request's supplement is version three with the
  brief's fields and bounds; a description or threads over their bound are
  cut and counted; no credential, token or environment value is present.
- **AC09:** The review skill: the default is the built-in `bpr-pr-review`;
  an edit creates the profile copy with its base version and digest; Save
  refuses a skill without `name` or `description` or over 64 KiB and keeps
  the text; Restore default saves a copy first; an update with a higher
  `bpr-version` offers Compare, Use version N and Keep mine and never changes
  an edited copy by itself; the skill reaches the agent as Q9 decides.
- **AC10:** One Check runs the agent check once through the supervised
  launcher with the profile's allowlist and no review context, then makes the
  worktree (or the private copy); both are Stop-able and bounded, and the
  passed, not-signed-in, worktree-failed, copy-failed and failed-to-start
  results each show their defined words and actions (fixture agents, a fixture
  clone, and a fixture cache missing the head's parent commit).
- **AC11:** After a passed check the tab shows the Ready line and the brief,
  and the question field has focus; a new window and a new review open on the
  saved agent and link without setup; Send to Agent offers the agent first.
- **AC12:** An Ask that ends `workspace_unavailable`, `failed_to_start` or
  `not_signed_in` shows its check's words, not the harness's, with that
  check's recovery and Check again; no stable code or Git output is visible
  until Show details. Removing the saved agent's executable leads the next
  Ask to **Set up again**; History and Settings open the setup with the
  current choices selected.
- **AC13:** No credential, token or environment value is read, stored,
  logged or shown by setup; the saved choices hold only identities, paths
  and remote names (store inspection and log scan).
- **AC14:** The owner, attended, on a frozen build: "The first time I open
  the Agent tab it tells me what to do, and I end up asking a question that
  the agent answers from my repository."
