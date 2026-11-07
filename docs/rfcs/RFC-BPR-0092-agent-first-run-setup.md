---
id: "RFC-BPR-0092"
title: "Set up an agent the first time the Agent tab opens"
status: "Auto-Verified"
area: "BPR"
depends_on: ["RFC-BPR-0003", "RFC-BPR-0011", "RFC-BPR-0012", "RFC-BPR-0060", "RFC-BPR-0085"]
metadata: {"origin": {"date": "2026-10-22", "channel": "owner chat, relayed by the coordinator"}, "card": "BPR-1747", "closeout": "BPR-1756 (2026-10-24)", "revision": "r3 2026-10-22, owner answers Q1-Q9 recorded", "revisions": ["r1: agent, check and ready steps", "r2: the owner's first-run failure, the Repository step and worktrees, the review brief, and owner decisions D1 (an editable review skill) and D2 (clone folders)", "r3: adopted with the owner's answers to Q1-Q9 and BPR-1738's sparse workspace"], "draft": "docs/rfcs/assets/agent-first-run-setup-v1/RFC-DRAFT-0026-agent-first-run-setup.md", "import": "docs/rfcs/assets/agent-first-run-setup-v1.LIVE_IMPORT.md", "authoring": "author-rfc; frontend-design:frontend-design loaded for the design frames", "design_frames": "docs/design/BPR-1747-agent-setup-v1/index.html", "observed_at": "b69b82c9b", "owner_questions": "Q1-Q9 answered 2026-10-22: Q1 and Q3 in attended session 349862f0cb991e14b6cb57cf80653c5f (closed), Q2 and Q4-Q9 in chat because the page blocked them"}
---

# Set up an agent the first time the Agent tab opens

Auto-Verified, revision r3, closed by [[BPR-1756]] on 2026-10-24.
Adopted from
[RFC-DRAFT-0026](assets/agent-first-run-setup-v1/RFC-DRAFT-0026-agent-first-run-setup.md)
([import record](assets/agent-first-run-setup-v1.LIVE_IMPORT.md)) after the
owner answered its nine questions and made decisions D1 and D2. Card
[[BPR-1747]] holds the phase 1 finding; the milestone cards are listed
under [Milestones](#milestones). Every criterion passes at `08acf5b10` and
the owner accepted the look (AC14) on 2026-10-24; the closeout card
[[BPR-1756]] records them, and set this RFC's status once its
dependency RFC-BPR-0085 became `Auto-Verified` ([[BPR-1595]]).

## Problem and grounded current behavior

The owner, 2026-10-22, verbatim:

> I think we need some ux for setting up your agent the first time you go to
> the agent tab. Maybe a little wizard I'm not sure.

Observed on `b69b82c9b`, a tests-on release build rendered offscreen on the
`conventional-comments-v1` fixture with a fresh profile, light and dark
([frames](../design/BPR-1747-agent-setup-v1/index.html#today)):

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
  gesture" ([ai-agent-boundary.md](../architecture/ai-agent-boundary.md#provider-profiles)).
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
  [[BPR-0348]]), so no agent was started. The failure named a harness that
  never ran, the reason was a stable code and a raw Git log, and the only
  actions were Ask again and the log. [[BPR-1738]] has since fixed the
  cause, below, and gave the banner a closed reason with Git's text behind
  **Show the log**.
- **"External terminal harness" means something else.** It is the fallback
  protocol for a corporate harness with no machine-readable interface: a
  versioned plain-text envelope on standard input, plain text back
  ([ai-agent-boundary.md](../architecture/ai-agent-boundary.md#protocol-and-process-lifecycle)).
  Its program, `bpr-agent-terminal`, ships nowhere. A Claude wrapper is not
  chosen there: it is Settings › Agents & context › Claude › Process wrapper,
  used by the Claude profile. The menu explains none of this.
- **What the agent works in today.** By default, a disposable workspace: a
  new repository the Git adapter initializes, fills from the application's
  own bare cache, and checks out detached (`installed_git_process.cpp`;
  [workspaces](../architecture/ai-agent-boundary.md#workspaces)). What it
  holds depends on how the review was read:
  - A **complete** review's cache is a full, filterless fetch of the base and
    head ([app-owned bare repository](../architecture/git-and-epoch-model.md#app-owned-bare-repository)),
    and the workspace is `pack-objects --revs` of the head, so the head's
    ancestry, blame and callers are there.
  - A **sparse** review, read over the `gh` transport, holds each epoch's
    commit and trees and no parent or blob, which is why the owner's
    workspace walked into a missing parent. Since [[BPR-1738]] its
    workspace is the head's snapshot alone: a `--depth=1` clone of the head
    through the review's lease, with the head as its shallow boundary. It has
    every file at the head and no history, so no blame, no `git log` and no
    comparison with base beyond the two trees. ([[BPR-1873]] removed that
    snapshot: see the amendment under
    [Repository](#repository-the-reviewers-clone-remembered).)

  Either way it has no branches, tags or remotes, no LFS content (filters
  are off) and no submodules. Neither it nor a clone carries untracked build
  output or installed dependencies.
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
  ([context supplement](../architecture/ai-agent-boundary.md#context-supplement)).
  No review method is sent.
- **The MCP context helper is not a setup step.** It is the application's
  own read-only executable, started without arguments and pinned to a frozen
  snapshot (`mcp_context_helper_supervisor.hpp`). No shipped launch gives an
  agent an MCP configuration naming it. A user has nothing to register.
- **`bpr cycle` and `bpr tour` are the other direction.** A harness runs
  `bpr` with the `bpr-cycle` or `pr-tour` skill (`packaging/agent-skills/`).
  `bpr` ships only as the `TourCLI` install component built from source; no
  release package carries it
  ([release-packaging-maintainers.md](../development/release-packaging-maintainers.md#agent-free-variants-further-detail)).

So the working first-run path today is: install Claude Code, sign in to it
in a terminal, open a pull request, find "Approved Claude stream JSON" in the
menu, and Ask, in a workspace that works only while the cache is whole.
Codex does not work. The design page's
[table](../design/BPR-1747-agent-setup-v1/index.html) marks
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
state with its Settings button is replaced by this. The setup lives in the
Agent tab itself (Q1 A).

The setup has four steps, a real sequence: **Agent**, **Repository**,
**Check it works**, **Ready**. A finished step folds to one line with its
choice and **Change**. For a later pull request in another repository, only
the Repository step is new. One line says what an agent here is: it runs on
this computer with its own sign-in, in its own worktree of the reviewer's
repository at the head, and publishes nothing unless the reviewer makes it a
comment.

### Agent: detection, then a choice

When setup shows, and on **Look again**, the host looks for each agent the
current Git host's policy permits (Q2 accepted; this amends the AI agent
boundary's "Merely showing the panel performs no executable discovery").
Looking reads file metadata only, as the Claude search does today: no
process starts, no shell runs, nothing is written, and the application's own
`PATH` is used. Claude is found as it is now (a saved process wrapper wins,
then `PATH`, then the usual folders). Codex is found the same way (Q5
accepted): a saved Codex program path wins, then `PATH`, then
`HOME/.local/bin`, `/opt/homebrew/bin`, `/usr/local/bin` and
`HOME/.npm-global/bin`; on Windows only `codex.exe`, never a `.cmd`. The
Codex program path is a Settings row under the same rules as Claude's process
wrapper. The plain-text harness is never searched for: it runs only a
program path the reviewer chose. A launch of either resolves the same way at
every launch, so a shipped build starts the found absolute program instead of
handing the bare name to a launcher that refuses it.

Each found agent is one row: its plain name (Q4 accepted), the absolute path that
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

The Agent tab's menu says "Claude Code" and "Codex" (Q4 accepted). Codex's
protocol, App Server by default or JSONL, is a Settings › Agents & context
choice. The external terminal harness leaves the menu and appears in Settings
as "A harness that reads plain text", with one line saying it is for a tool
that has no Claude or Codex interface and that a Claude wrapper belongs under
Claude › Process wrapper. It is offered in the Agent step only once its
program path is set.

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
  way), **Clone into FOLDER** (the first clone folder; see
  [Getting the head](#the-agents-worktree) for where its objects come from),
  **Use a private copy**, and **Edit clone folders…**.

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
(Q7 accepted). Its row states its limits in one line, by how the review was
read: "the head and its history, without your branches, tags, LFS files or
submodules" for a complete review. A sparse review has no private copy of
its own: see the amendment below.

> **Amendment [[BPR-1873]] (owner decision, 2026-11-05).** The head snapshot
> download of [[BPR-1738]] is removed, never run automatically and not
> offered behind a prompt. It cloned every file of the head for every agent
> question, under the acquisition runner's 32 MiB per-file limit, so a real
> head of a large repository failed in seconds and was reported as "could not
> be read". A sparse review's agent now works in a clone:
>
> - **A linked clone** is borrowed as before ([[BPR-1751]]). When it lacks the
>   head, only what it lacks is fetched into the agent's private workspace,
>   never into the clone ([The agent's worktree](#the-agents-worktree)).
> - **No linked clone:** the agent asks, once per repository, to **Link a
>   clone…** or to **Let BetterPullReview keep one**, or to ask about the pull
>   request's files only, which it then says plainly. The Repository step is
>   that question: it already shows, before any question, for a repository
>   with no remembered choice, and Settings › Repositories › Linked clones
>   changes or forgets the choice. For a sparse review its no-clone choice is
>   a card, **This pull request only**, with its cost in one line: "No
>   callers, tests or history" (see the managed clone below for
>   the choice as a whole; owner looks on [[BPR-1879]] v002 to v007 asked for
>   shorter copy and a clearer default).
> - **This pull request only** ([[BPR-1873]] slice 2). The workspace holds
>   exactly the pull request's files at the head: the head's side of
>   base...head, deletions and submodules excepted. The host uses bytes the
>   review already holds and reads the rest through the review's own lease
>   (batches of 16, then single reads), checking every file against its blob;
>   at most 1,000 files and 96 MiB. The Git adapter packs the head's commit
>   and trees from the sparse cache, stores the blobs and sparse-checks-out
>   exactly those paths, so the rest of the head is absent, `git status` is
>   clean, and the workspace is shallow at the head. The brief names it
>   `private_snapshot`; the review skill (`bpr-version` 4) tells the agent to
>   say it saw only the pull request's files; and the turn's footer says "It
>   sees only this pull request's files" while it runs and "It saw only this
>   pull request's files" when it answers.
> - **A BetterPullReview-managed clone** ([[BPR-1879]]) is a
>   full clone (every object), not
>   a blobless one: a blobless clone would need lazy blob fetches, and those
>   must never run inside the agent's sandbox or with its environment; a full
>   clone's one-time cost is stated from GitHub's repository `size` before it
>   starts. It lives under the profile's application data next to the
>   repository cache, is shown in Storage and is removed by profile purge.
>   Whether it also replaces that repository's sparse review cache is left to
>   a later card.
>
>   As built (owner looks v005 to v007): with no clone linked, a review read
>   sparsely through `gh` (every real review once `gh` is connected) asks
>   one question, cards in order of strength with one default button for
>   the card chosen. **Link your clone** is first, chosen at first and marked
>   "Recommended". The step searches the clone folders itself once it shows
>   (no separate search link): clones found are offered on the card, the
>   first chosen, and the button is **Link this clone** (with **Choose
>   another folder…** and **Edit clone folders…** small on the card); with
>   none it says "None in FOLDERS" and the button is **Choose its folder…**
>   (with **Edit clone folders…**); with no folder listed yet the card holds
>   the folder line and the button is **Add folder**. **Keep a clone for me**
>   is second: its figure is SIZE, its line "Full history, kept in LOCATION"
>   (shortened in the middle, the whole path as its tooltip); its button
>   **Keep a clone**. **This pull request only** is last, "No callers, tests
>   or history", its button **Use this pull request only**; it has that one
>   name everywhere (the step's summary, the check's fallback **This pull
>   request only, for now**). A review that holds every file (in practice a
>   fixture) needs no clone: its Repository step is done without one and
>   never asks; opened, it offers Link your clone alone, as an improvement,
>   and its summary is "Files BetterPullReview has". No wording names a
>   private copy. A fixture course (tests-on builds) keeps a clone as a
>   rehearsal, bytes arriving until Cancel and nothing written, and the
>   attended look `--attended-repository-look` shows a fixture review's
>   setup as a sparse review's, so a frozen look shows the choice and
>   its progress. SIZE is GitHub's `size` from the review's repository
>   read, in decimal units. The review's own lease
>   runs `gh repo clone URL STORE --no-upstream -- --no-checkout --quiet`
>   with the same pinned private configuration, environment, hooks off, no
>   fsmonitor, `core.sharedRepository=0600`, identity checks before and
>   after, and the same write confinement to a private temporary root as
>   every sparse acquisition, but on its own runner with no byte or time
>   limit (owner decision 2026-11-05: "I don't really care about a limit, a
>   cancel and a progress would be better"): no per-file ceiling, no store
>   threshold and no deadline; every other runner keeps its limits, and
>   `architecture_boundaries` pins that runner's factory and makes it the only
>   one whose deadline is lifted. The store is
>   checked to hold a commit at `HEAD`, made owner-private, and moved whole
>   to `agent-clones-v1/HOST-REPOSITORY_ID` in the profile. While it runs its
>   card becomes the progress and is the whole step (no header, other cards
>   or links; owner look v004): "Keeping a clone", the
>   bytes received as its figure ("42 MB of 126 MB", sampled twice a second),
>   a thin bar and **Cancel**; past SIZE, or with no SIZE, the bar is busy and
>   only the bytes received show. Objects and percent are not
>   shown: they exist only in Git's standard error, which the adapter never
>   exposes. Cancel is honoured at every step, from qualification to the
>   final move; Cancel, a
>   failure or another repository on screen leaves nothing in the profile. A
>   failure is a few words on its card ("No connection to GitHub.") and the
>   button reads **Retry**. When it is in place it
>   is linked as the repository's clone, so the setup moves on and later
>   questions borrow it exactly as a linked clone: a head it holds is lent
>   with no fetch, and a head it lacks is fetched as the delta above, into
>   the workspace. [[BPR-1884]] amends this for the kept clone: a head it
>   lacks is fetched into the kept clone itself first (below). A kept clone already in
>   place is linked without a download. Settings › Repositories › Linked
>   clones names it "kept by BetterPullReview". Insights › Storage measures
>   `agent-clones-v1` as "Agent clones"; profile purge removes
>   it (each kept clone may hold only its `.git` folder, so a checked-out
>   file blocks the purge); the resets keep it.
>
> **Amendment [[BPR-1884]] (2026-11-06): keeping the kept clone current.**
> Without maintenance a kept clone ages, each question's delta grows, and on
> an active repository it reaches the delta's 32 MiB per-file bound and the
> question fails as `too_large`. The policy is **fetch on miss, into the kept
> clone, before the question**:
>
> - When a sparse review's question finds that its linked clone is the one
>   BetterPullReview keeps (the link is `Managed`, so its folder is directly
>   inside `agent-clones-v1`) and that clone lacks the head, the host first
>   brings the kept clone up to the pull request, then lends the head from
>   it. A head the clone holds is lent with nothing fetched, as before.
> - Only what the pull request needs is fetched: its head OID and its base
>   OID, each only when the clone lacks it. The default branch is never
>   fetched (owner, 2026-11-06): a head fetch already brings in every
>   ancestor the clone lacks.
> - The fetch is the delta's own form (`gh repo clone --bare
>   --revision=OID --reference=KEPT` into a private temporary root, the same
>   pinned private configuration, environment, hooks off, no fsmonitor,
>   identity checks before and after, and `gh` writing only inside that
>   root), but on the kept clone's runner, with no per-file bound: it is the
>   clone's maintenance, not a per-question download. Each received pack is
>   self-contained; it is copied into the kept clone owner-private, under a
>   temporary name and renamed with its index last, so Git sees only whole
>   packs. Credential-free local Git then checks the head's and base's
>   trees and names them `refs/bpr/pull/N/head` and `refs/bpr/pull/N/base`,
>   so a later delta or maintenance fetch offers them as haves and Git's
>   own maintenance keeps them.
> - Why this policy keeps questions fast: the fetch receives the same objects
>   the delta would (what the clone lacks of the head), plus the base only
>   when the clone lacks it, so it never blocks a question meaningfully
>   longer than the delta did, and an up-to-date clone costs nothing. A
>   background or idle schedule was rejected: it spends fetches on
>   repositories with no question, and a question arriving first still
>   needed a delta.
> - It runs in the host, before the workspace exists, on the question's own
>   cancellable stop, through the review's `gh` lease: never inside the
>   agent's sandbox or with the agent's environment, and never in a widget.
> - A reviewer-linked clone is never fetched into or written: the host asks
>   only for a `Managed` link, and the transport refuses any folder that is
>   not a kept clone's working folder directly inside `agent-clones-v1`.
> - A fetch that fails or is cancelled while the question goes on leaves the
>   kept clone as valid as it was, the question proceeds through the delta
>   above, and the outcome is a `clone_refresh` diagnostics line
>   (`refresh_failed` or `cancelled`).
> - Not built: a "last updated" time in Storage, and running `git
>   maintenance` or `gc --auto` on the kept clone (which can repack a large
>   repository for minutes inside a question). Each refresh adds at most two
>   packs, which accumulate until that is added. Both are left to a later
>   card.
>
> The sparse row's former text, "the head's files only, without history,
> branches, tags, LFS files or submodules", described the snapshot and is
> retired with it.

### The agent's worktree

With a linked clone, each agent session gets its own worktree of that clone
at the session's head (Q7 accepted):

> **Implementation note ([[BPR-1751]], 2026-10-22, accepted by the
> owner in [[BPR-1755]]'s look v001, Q2).** Built as the existing private
> disposable workspace with the clone's history borrowed through one
> `objects/info/alternates` line, not as `git worktree add`: a worktree's
> `.git` file cannot hold the workspace's owner-private marker and journaled
> cleanup, and it would write into the clone and carry its remotes. Nothing
> is written to the clone; only what it lacks is copied from the
> application's cache (nothing when it holds the head); a sparse review's
> workspace is shallow only when the clone lacks a parent of the head; the
> workspace has no remote. The fetch, `worktree remove`, limit of three and
> sparse **Copy** command below have nothing to govern under this form. On
> Reject, a later card builds the form below.

> **Amendment [[BPR-1873]] (2026-11-05): a clone that lacks the head.** It
> supersedes [[BPR-1738]]'s snapshot fetch and [[BPR-1772]]'s manual step
> ("Fetch the pull request's branch in your clone, then ask again"), and
> step 3 of **Getting the head** below with its `git -C CLONE fetch REMOTE
> pull/N/head` **Copy** command. BetterPullReview fetches what the clone
> lacks itself, without touching the clone:
>
> - The review's own `gh` lease runs `gh repo clone --bare
>   --revision=HEAD_OID --reference=CLONE` into a private temporary store,
>   under the same pinned private configuration, environment
>   (`GIT_CONFIG_NOSYSTEM`, `GIT_CONFIG_GLOBAL=/dev/null`,
>   `GIT_ALLOW_PROTOCOL=https`), hooks off, no fsmonitor, identity checks and
>   bounds as every sparse acquisition, through `gh`'s own credential helper.
>   `--reference` writes one alternates line naming the clone's objects, so
>   Git's negotiation offers the clone's refs as haves (the default
>   `core.alternateRefsCommand`) and the server sends only the objects the
>   clone lacks. On macOS the `gh` launch may write only inside the temporary
>   store, so the clone cannot be written.
> - The store's head commit and tree are checked, then
>   `rev-list --objects HEAD_OID --not --alternate-refs` names exactly the
>   objects reachable from the head and from no ref of the clone, and
>   `pack-objects` packs those, self-contained. The received pack is not
>   handed on as it is: completing its thin deltas copied in bases the clone
>   already holds.
> - The workspace borrows the clone through its alternates line, indexes that
>   pack, checks the head's commit and tree (`cat-file --batch-check`), and
>   is never shallow. It keeps no remote and no refs beyond its detached
>   `HEAD`.
> - A delta past a bound (the 32 MiB per-file limit, the 96 MiB store, or the
>   4 MiB object list) is `too_large`, worded as the pull request's new
>   objects being larger than the agent's private copy may fetch, never "could
>   not be read". A clone that `RLIMIT_FSIZE` stopped is recognized by a file
>   at the limit, whether `gh` exits non-zero or is ended.
>
> Measured on `MikeMarcin/BPR-Test` (PR #150, 2026-11-05) with a clone that
> lacked one new commit: GitHub accepted the head OID as a want (protocol
> v2, the head being the tip of `refs/pull/150/head`), the client sent the
> clone's ref tips as haves (16 `have` lines), the received pack was 3,467 bytes
> and five objects, none of them in the clone, and the clone's files were
> byte-identical before and after. On a second commit that edited a file the
> clone held, the received pack carried one base the clone already had; the
> `--not --alternate-refs` pack carried only the four new objects. Because
> GitHub advertises every pull request's head as `refs/pull/N/head`, no
> separate fallback to fetching that ref is needed: a head that is not
> advertised any more is refused like any other unavailable head.

- **Getting the head** (Q8 A: from the application's own copy, never
  through the reviewer's remote or credentials).
  1. If the clone already holds the head commit, nothing is fetched.
  2. Otherwise the host runs `git fetch --no-tags --no-write-fetch-head`
     in the clone from the application's own bare cache by its local path,
     for the head OID only. It is local, uses no network and no credential,
     and Git's negotiation sends only what the clone lacks. A complete
     review's cache holds the head's whole closure, so the clone stays
     connected.
  3. A sparse review's cache holds no parents or blobs, so a fetch from it
     cannot be complete and is not attempted: an incomplete closure would
     leave the reviewer's clone with a commit whose parents it lacks. The
     step says so ("This review was read without history, so BetterPullReview
     can't add its head to your clone"), with the command to run in a
     terminal, `git -C CLONE fetch REMOTE pull/N/head` with **Copy**, then
     **Check again**, and **Use the private copy for now**.

  Then it runs `git worktree add --detach WORKTREE HEAD_OID` in the clone. No
  branch is created and no ref is moved; the reviewer's checkout, index,
  branches, stashes and configuration are untouched. What the clone does
  gain is stated in the Repository step: the head's objects, and one
  `.git/worktrees/ENTRY` administrative entry while the worktree exists.
  **Clone into FOLDER** clones from the application's own copy for a
  complete review and then sets `origin` to the repository's GitHub URL; for
  a sparse review it offers the `git clone` command to copy instead.
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
- the version-two fields as now, and the review skill's text (D1, Q9 A).

> **Implementation note ([[BPR-1752]], 2026-10-22).** The brief's
> lines follow every version-two field and are written only when a brief is
> present, as the feedback guidance's are: a comment-workshop press and a
> writing request carry none, and their bytes differ from version two only in
> the `schema` line. The workspace is named by kind (`linked_clone_history`,
> `private_copy`, `private_snapshot`), set once the workspace is made; its
> path is the agent's working directory rather than a field. The private copy
> now also holds the base the cache has, so the named diff works when the
> base is not an ancestor of the head; a snapshot names no diff.

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
- **Reaching the agent** (Q9 A). The skill's text goes in the brief of every
  review request, the same for Claude Code, Codex and the plain-text harness,
  and nothing is written to any harness's configuration. The reviewer's own skills in their harness still load as the
  harness loads them.

> **Implementation note ([[BPR-1752]], 2026-10-22, accepted by the
> owner in [[BPR-1755]]'s look v001, Q2).** The edited copy and its record live in
> the profile's `agent-setup.sqlite3` (settings, which resets and purge
> already cover), not as files under `agent-skills/bpr-pr-review/`: the
> profile maintenance inventory admits only paths it knows, and a new folder
> there is purge work of its own. **Open in editor** is therefore not offered
> yet; the Settings editor ([[BPR-1753]]) is how the skill is edited.
> The package ships `SKILL.md` alone, with no references.

### Check it works

> **Follow-up [[BPR-1769]] (2026-10-26).** The reported corporate Bedrock
> wrapper answers the existing auth-status command; its pretty JSON is valid.
> Setup must retain the effective saved program and fixed arguments when it
> auto-selects a migrated preset, and save that exact checked entry. The
> canonical migrated list governs detection and launch; Reset also clears
> the old wrapper/model migration fields. Custom checks are availability,
> never an invented sign-in result. Failed rows disclose separate bounded
> literal output; progress lasts until both checks settle, Stop reaches the
> workspace, and the private-copy fallback retries directly. Metadata rescan
> actions belong to Agent and Repository, not a pinned Check-step footer.
> Historical acceptance above is unchanged; the new card records repair
> evidence and the unverified corporate workspace failure.


One explicit **Check** proves the two things a first question needs, each a
row with its own result:

1. **The agent answers.** A bounded run of the chosen agent through the
   host's supervised launcher, with that profile's environment allowlist and
   no review context: the agent's own status command (Q3 A), `claude auth
   status` and Codex's equivalent, and a `--version` read. It sends no
   question, no code and no model request.
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
  terms and the recovery the host actually offers, then **Check again**. A
  head the clone lacks on a sparse review: the fetch command to copy, and
  **Use the private copy for now** (superseded by [[BPR-1873]]: what the
  clone lacks is fetched into the workspace, and a failed fetch says why in
  the same words as an Ask). A linked clone that moved or no longer
  matches: **Choose its folder…**. The private copy failing, with
  [[BPR-1738]]'s closed reason in words (the owner's case read "This Mac's
  saved copy of the repository is missing commit 984772c, which comes before
  the head fba81b4. No agent was started."): **Get the pull request again**
  or **Link your clone instead**.
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

Owner look v006 replaced **Set up agent…** and **Change repository
folder…** with **Reset agents…**, in the Agent tab's History menu and in
Settings › Agents & context: it asks first, then clears every agent setting,
the clone folders and links and an edited review skill, and the setup starts
again at Agent. Settings › Repositories keeps the clone folders and linked
clones.

### Boundaries

- The core decides; the host performs; Qt presents. The core owns the setup
  state and transitions, row order and wording, policy filtering, remote URL
  parsing and matching, the brief's bounds, the skill's version comparison,
  and the decoding of a check's bounded output. The application host owns the
  metadata reads, the Git commands for fetch, worktree and cleanup, the
  check's launch, cancellation, the skill files and the saved choices. The
  agent and Git adapters own locating, inspecting and running. The widget
  raises intents and performs no file, process, network or settings effect.

  > **Implementation note ([[BPR-1754]], 2026-10-22).** As built, the
  > core owns the setup state, rows, policy filtering, matching, the brief's
  > bounds and the skill's front matter and versions; the words for each
  > state and failure are Qt's (the repository's rule that wording lives in
  > Qt), and a status check's bounded output is decoded in the application
  > host with the project's bounded JSON parser, beside the launch that
  > produced it. No Git command runs in the reviewer's clone
  > ([[BPR-1751]]). Which step the setup shows open, and whether it is
  > in place of the conversation, is the view's presentation state, decided
  > from the two hosts' projections.
- The agent's authority does not grow: no new tool, environment name or
  credential field. The brief adds only what the reviewer can already read
  on GitHub. The MCP helper is unchanged and nothing in setup configures it.
- Clone discovery reads only folders the reviewer listed or chose, and only
  `.git/config` and `.git/HEAD` in them.
- Agent-free builds have no setup, no clone folders and no review skill, as
  they have no Agent tab.
- Out of scope: the app installing or signing in to an agent; registering the
  MCP helper with a harness; the existing-checkout mode, which stays as it is;
  and setting up `bpr cycle`, `bpr tour` and their skills for use from an
  agent, which is its own card (Q6 accepted).

### Owner decisions (2026-10-22)

All eleven are the owner's, dated 2026-10-22. Q1 and Q3 were answered in
attended session `349862f0cb991e14b6cb57cf80653c5f` (sequences 29 and 31). The
page's Accept/Reject and Next controls failed on the other questions, so the
owner gave Q2 and Q4 to Q9 in chat, verbatim: "I want to accept all your
recommendations but it's not letting me hit accept or next question on most
of the questions." The coordinator relayed the answers below; the session is
closed and its protections released.

- **D1. The review workflow ships as a skill the reviewer can edit.** The
  owner, verbatim: "I think we ship it as a skill and make it editable by the
  user." Settled in [The review skill](#the-review-skill).
- **D2. A setting lists the folders that hold the reviewer's clones.** Same
  message, verbatim: "We probably also give the user a setting to point to
  their folders where it can find existing repo clones." Settled in
  [Repository](#repository-the-reviewers-clone-remembered).
- **Q1 A.** Setup lives inside the Agent tab (session).
- **Q2 accepted.** Setup looks for the permitted agents by file metadata and
  in the listed clone folders by reading `.git/config` and `.git/HEAD` when it
  shows; nothing starts until an explicit gesture. This amends the AI agent
  boundary (chat).
- **Q3 A.** The agent check is the agent's own status command (session).
- **Q4 accepted.** Plain names in the menu; Codex's protocol and the
  plain-text harness move to Settings (chat).
- **Q5 accepted.** Codex is found as Claude is, with a Codex program path; the
  plain-text harness runs only a chosen program path (chat).
- **Q6 accepted.** "Use BetterPullReview from your agent" is its own card
  (chat).
- **Q7 accepted.** A worktree of the linked clone is the default place the
  agent works, the private copy is the stated fallback, and "no push" in a
  worktree is the harness's policy, not an enforcement (chat).
- **Q8 A.** The head's objects come from the application's own copy, never
  the reviewer's remote or credentials (chat).
- **Q9 A.** The review skill's text goes in each review request's brief
  (chat).

### Owner decisions (2026-10-23): the conversation

Look v002 (session `077adf53047441a3224b3687ceddf38e`) was rejected, among
other things because the question "Does this work?" got no visible answer,
the question stayed in the field, the "Ask the agent" button was unclear, and
there was no model or effort picker. The Agent tab is now a chat thread: each
turn is the question as sent, the agent's answer as it streams (its Markdown
rendered, links and images inert), and how the turn ended (who answered, in
how long, the model the agent reported, the cost when the harness reported
one, and the findings it brought). The question field clears once a question
is sent. The owner then answered three forks in chat, relayed by the
coordinator, verbatim:

- **(a) "Suggested prompts described that way sound great."** There is no
  separate one-click action. With an empty thread, clickable suggestions sit
  above the question field under "Ask about this pull request, or try:":
  **Review this pull request**, **Review** *the open file's path* when a file
  is open, **What could this change break?**, and **Explain the selected
  lines (a–b)** only when lines are selected. A suggestion is sent as written,
  as an ordinary chat turn, so the thread shows what was asked. Once the
  thread has turns, the suggestions are a **Suggestions** menu in the field.
  Every review prompt answers with a summary in words, even with no finding:
  the built-in review skill (version 2) says so.
- **(b) "Continuous session per PR with an option to clear session."** One
  conversation per pull request. The application carries it, rather than any
  harness's own resumable session (a Claude session is keyed by its working
  directory, and the agent's workspace is new for each turn), so it continues
  in whichever agent answers: each request's brief carries the earlier turns,
  oldest first (`brief.conversation.N.asked` and `.answered`, each cut to
  8 KiB, the newest 50 within 48 KiB, the older counted). The thread is kept
  in the profile's `agent-setup.sqlite3` for each pull request (host, owner,
  name and number; at most 50 turns of 64 KiB per text, and 500 pull
  requests, the least recently written going first), so it comes back when
  the pull request is opened again, in this run or a later one. **Clear**
  (and History › **Clear session**) empties the thread and starts the next
  question afresh; it asks first only when turns would leave the thread.
- **(c) "A chip in the message box like VS Code."** The model and effort are
  a chip inside the question field ("opus · high effort", or "Default
  model"), with no control in the context row. Its tooltip names the program
  that runs and the model the last answer reported; its menu offers the
  agent's own models and effort levels, saved for the profile. `/model NAME`
  and `/effort LEVEL` (or `default`) in the field do the same and are not
  sent.
- **Findings become cards, notes and drafts only on a gesture ("Yep build it
  like that").** Asked how agent responses become notes, the owner approved
  this design, which keeps RFC-BPR-0012's rule that nothing is automatic and
  the agent's text stays a local proposal until the reviewer acts. A turn's
  structured findings, validated and anchored by the core as RFC-BPR-0012
  has them, are cards in the thread under that turn's answer, each with its
  file, lines, claim, evidence and suggested comment and three actions:
  **Add as draft comment** (a private line draft from the suggested comment,
  through the existing promotion of private text into the line's own draft),
  **Save as note** (a private note on those lines, never published) and
  **Dismiss**. A plain answer offers **Save as note** for the whole reply, or
  for the part selected in it. Once acted on, a card says Drafted, Saved as a
  private note, or Dismissed with **Undo**; a refusal says why and leaves the
  card open. No harness lets its model add `bprFindings` to the result frame
  the harness writes, so the review skill (version 2) asks the agent to end a
  review with a block fenced `bpr-findings` (path, one-based start and end
  lines, severity, category, confidence, claim, evidence, suggested
  comment); the host reads it from the answer, states the provider and
  analyzed revisions from the session, anchors a finding on a file the turn
  froze to its first line there, and leaves any other finding unanchored for
  the core to deliver as such. The reply is shown without the block. A draft
  or note is made only at lines of the file on screen; for another file the
  card says to open it and try again.
- **Amendment: agents are a list the reviewer manages.** The owner,
  verbatim: "For the agent picker. I should be able to define more than one
  custom agent. For example at work we have a wrapper process that you can
  pass nothing to in order to get claude models and --codex to in order to get
  codex models. Being able to use both is great and I would prefer to not
  have to jump through hoops to switch between them. I also won't want to see
  the default claude/codex apps there as we're not allowed to use them. So
  basically i'm saying that the agent picker that we have for the wizard is
  pretty good as a shortcut but for the actual settings it show be more like
  "add agent" where you can pick a popular one like claude/codex if they
  aren't already added or a custom one." An agent is an entry
  (`core::agent_entry`): a name the reviewer chooses, a protocol (Claude
  stream JSON, Codex App Server or JSONL, or plain text), a program (a full
  path; empty only for a popular agent, whose own program is looked for at
  each launch), fixed arguments (at most 16, each without a control
  character), and a default model and effort the protocol takes. At most 16
  entries; each launches as a fixed profile slot of its protocol, allowed
  where that protocol's built-in profile is, and the picker offers only the
  entries. Settings › Agents & context is the list, with **Add agent…**
  (Claude Code and Codex when found and not added, or **Custom agent…**),
  **Edit…**, **Remove** and reordering. Setup keeps its detect-and-pick
  shortcut, which adds the picked agent as an entry, and gains **Custom
  agent…**; a custom agent's program is what setup checks, with its fixed
  arguments before the status command. The chip in the question field is
  agent · model · effort and lists every entry, one press to switch. A launch
  of an entry runs its program with its arguments before the protocol's own;
  nothing from a pull request, a question or a setting is put into them and
  no shell runs them; the program is inspected again at each launch under
  the existing wrapper rules. The Claude process wrapper and Codex program
  settings move into the list once, the chosen agent first.
- **Each agent has its own session ("Yeah separate session for each agent
  makes the most sense to me. Obviously anything we save on the PR as notes
  or drafts are outside of that session context.").** A pull request has one
  continuous session per agent entry. Switching agents in the chip shows that
  agent's own thread and session, and switching back resumes it; a pull
  request opens on the agent it was last asked with; Clear session clears
  only the current agent's. Notes and drafts made from a card or a reply are
  the pull request's private review state, not a session's: clearing or
  switching never removes them, and they stay in the notes and drafts views.
- **No run time limit by default ("Don't have a timeout limit, cancel is
  sufficient.", then "I guess we can keep a timeout as an option, but i
  wouldnt want it on by default for my work.").** The second replaces the
  first. The 120-second run limit is gone: a run goes on until the agent
  finishes or the reviewer presses Stop, which is always in reach (the
  question field's Send becomes Stop) and stops the process group through
  the existing supervision; closing the window or quitting still stops it,
  and the run's time stays shown. Settings › Agents & context has **Stop a
  run after N min**, one limit for every agent (simpler than per agent),
  off by default and for every profile that had none; a run it ends stops as
  Stop does and says so. "No limit" is the longest wait the launchers' `int`
  of milliseconds can name, a little under 25 days. The setup's own sign-in
  check keeps its 15-second bound: it is a probe, not a run.
- **The owner's three recommendations ("Go with your recommendation for the
  3 decisions").**
  1. The Claude › Process wrapper and Model settings are removed; their
     values moved into the Claude Code agent. Tour writing asks the agent
     chosen in the Agent tab when it speaks Claude Code's stream JSON,
     otherwise the first in the list that does; Review Cycles Send to Agent
     offers the list's agents, the pull request's current one first.
  2. A custom agent's setup check confirms only that its program can be
     launched; an entry may name its own check, fixed arguments run under
     the same no-shell rules, which must exit 0. Claude Code and Codex keep
     their sign-in status check.
  3. When a finding's file is not the one on screen, Add as draft comment
     and Save as note open that file in Review, go to the line, then make the
     draft or note; when the file or the line is not there at the current
     head, the card says so and stays open.
- **Look v003's cleanup (Reject, "Great progress. It actually functions
  now.").** Setup's Repository step adds one folder (a line with Browse…,
  then Add); the folder list is Settings' alone. The question field is one
  rounded field with one row under the words, after the Claude Code and
  Codex composers: Context and Suggestions as icons and what the agent may
  read, then the agent pill and Send (Stop while it works), the pill an icon
  alone when narrow. The header is the session's title with History and New
  session as icons. Each reply is a card with Copy (the whole reply as
  Markdown), Save as note and its time, its code coloured by the shared
  Markdown code host as comments' code is. A saved note's foot is its
  actions alone, none cut short.
- **Look v004's notes (Reject, "much closer but not quite there").** Inline
  code in an answer is coloured too, through the same host, as the language
  most of the pull request's files are in (`core::markdown_inline_code_selector`;
  a `.h` counts as C++ when C++ is there; prose and data leave it plain);
  comment bodies are unchanged. Save as note becomes **Go to note**, which
  opens the note in Notes, on a reply and on a finding card. The model menu
  and the pill name models with their versions: Claude Code's list is Opus
  5.5, Sonnet 5.5, Haiku 5.5 and Fable 5.1 (`claude-opus-5-5` and so on, as
  `--model` takes them), a bare alias is "Sonnet (newest)", and the agent's
  own default is named by the model it last reported; Codex has no list of
  its own, so its models are what it reports or what is typed. The suggested
  prompts stay above the field once the thread has turns, and a Suggestions
  toggle in the field's row shows or hides them, kept for the profile. The
  selected lines' prompt names its file ("Explain src/foo.cpp line 34"), and
  a file and line a question or an answer names is a link that opens it in
  Review at that line, when the path is one of the pull request's files.
  Setup's "Or add" row is gone: the folder line holds the usual folder that
  exists, so Add works at once. An answer's last line is no longer cut off:
  its height is measured at the width its card gives it. The header names the
  pull request.
- **Look v005 (Reject, "Very close, i think the setup wizard is done. Just a
  little more to go.").** The owner accepted the setup wizard: its four
  steps as v005 showed them. The suggested prompts are now the thread's last
  item, after the last turn, scrolling with it, never over it; the toggle
  stays. With a multi-line selection the prompt reads "Explain the selected
  block (*path* lines a–b)", with one line "Explain *path* line N", and the
  prompt sent is a link to them. A link to a file and line now scrolls the
  code to it: the window revealed nothing before, only selected the line,
  so a link into the file already open left the code where it was; it now
  reveals the line through the comparison's conversation anchor, waiting for
  a long file's geometry as a thread's pick does ([[BPR-1698]]). Command-click
  and a plain click both do it.
- **Look v006 (Reject).** Models and effort levels are the harness's own: a
  Codex-protocol agent is asked, through its own program and fixed
  arguments (the owner's `--codex` wrapper included), `app-server` then
  `model/list`, and its models are listed by the names Codex shows
  ("GPT-6.1-Sol"), each with the reasoning efforts it takes (`max` and
  `ultra` among them); Claude Code's levels are read from its `--help`. They
  are asked when the Agent tab shows the agent or its menu opens, once per
  window; until they answer the menu says so and offers the known levels,
  and Other… stays. An effort is now any one lowercase word, the harness
  saying which. The agent menu ends with Agent settings…. The selected-lines
  suggestion follows the code's selection at once, during a drag too: it is
  read from the panes' own selection once the event that changed it has
  finished, not from the selected line as a press reports it. Reset agents…
  replaces the ways back into the setup (see Running setup again); it leaves
  notes, drafts and agent conversations.
- **Look v007 (Accept, decision `52f55363-457e-4397-ad30-d3962a4b80a6`): "Hide
  suggestions until the the agent is done thinking. Get rid of teh icon on
  agent when it's full width like the other tabs do. Only use it for the
  narrow. Otherwise we're done."** The two notes are the accept's
  conditions, built without another look: the suggested prompts are hidden
  while the agent works and return when its turn has finished, and at full
  width the Agent tab is its name alone, as the other tabs are, its glyph
  kept for the narrow row. Reset agents also forgets which agent each pull
  request was last asked with, so a new agent whose id an old one had does
  not open on that agent's thread; every turn stays.

### Amendment [[BPR-1900]] (2026-11-06): the tour session

The owner, 2026-11-06: "Feels like something like `rg` might be useful for a
tour writer to gather context", "to me the tour itself is much more of a
second agent session, separate and mostly background from my main agent chat
session", and "And then it can fill its tour stops and problem and solution
from all that context." Tour authoring is its own agent session per review,
separate from the Agent tab's conversation, with the agent, model and effort
the reviewer picked ([[BPR-1878]]'s bar). It runs through its own agent host,
launcher, lane and projection, so tour runs are never chat turns and chat runs
never tour runs. **Draft the tour** drafts the whole tour in the background
from the review's private workspace ([[BPR-1873]]) with read-only tools and
returns a proposal the author decides part by part; the core anchors every
proposed stop to real head lines of the pull request's files. The
[AI agent boundary](../architecture/ai-agent-boundary.md#the-tour-sessions-read-only-drafting-bpr-1900)
and [review tours](../architecture/review-tours.md) describe the whole of it.
The three questions the card left open are settled as recommended below; each
waits on the owner's look on the card's attended review.

- **T1. Fresh runs, not `--resume` (superseded by [[BPR-1914]]: one
  conversation per tour, replayed; see below).** Each tour request is a
  fresh run given the tour so far (the Problem, the Solution and each stop's
  title and lines) and the pull request, with session persistence off. The
  tour draft is the session's memory: it is what the author sees and edits,
  so a run never reasons from an earlier answer the author has since
  changed or discarded, and nothing about the tour is kept in an agent's own
  store. It is also the only shape Codex's `exec` can share, and it keeps
  BPR-1375's "no session saved". The cost is that a follow-up repeats some
  reading; drafting runs are minutes apart and bounded, so that is cheap.
  `--resume` would make the agent's hidden conversation part of the tour's
  state, and a resumed session could carry tool results from a head the
  review has since moved past.
- **T2. The transcript opens in a window of its own (amended by
  [[BPR-1913]]; first a panel on the Tour tab).**
  **Transcript** at the top of the tab opens the session's runs, newest first:
  what was asked, what it searched, read and ran, and what it answered or how
  it ended. It never opens by itself. The owner, 2026-11-06: "I don't like the
  way the transcript is in the tour window, when it failed I couldn't copy it
  to the clipboard, except for one response at a time. And the button at the
  top looks like a tab, except they're not tabs." The panel in the Tour tab's
  editing column is replaced by a separate, non-modal **Tour session
  transcript** window: the runs on the left, the chosen run as one selectable
  text on the right (a selection spans its entries), and **Copy run** and
  **Copy all runs**, which put plain text that reads as Markdown on the
  clipboard: the request, how the run ended, each step's kind and its command
  or path with any read-only-list note, and the answer or, for a failed run,
  its reason with the failure's own log. A window was chosen over a sheet or
  a drawer surface: a sheet would block the tour while it is open, and a
  drawer surface would take room from the editing column again; the window
  sits beside the main window while the author edits. **Draft the tour** is
  the row's one bordered action, at its own width; **Transcript (n)** is a bare
  action with a page glyph at the row's far end, never a toggle, so the two
  no longer read as a pair of tabs. [[BPR-1892]]'s notice keeps its words
  and its button becomes **Show the run**, which opens the transcript at that
  run. The Agent tab with a session switcher was not chosen: it would send the
  author away from the tour to see a tour run, which is what BPR-1892 removed,
  and it would mix the two sessions in one place the owner asked to keep apart.
- **T3. The read-only allowlist per agent (recommended).** Claude Code:
  `--tools Read,Grep,Glob,Bash`, `--allowedTools` Read, Grep, Glob and the
  shell programs `rg`, `grep`, `ls`, `cat`, `head`, `tail`, `wc`, `nl`, `find`
  and `git log`, `show`, `blame`, `diff`, `grep`, `ls-files`;
  `--disallowedTools Edit,Write,MultiEdit,NotebookEdit,WebFetch,WebSearch,Task`;
  an empty strict MCP configuration, no setting sources, no slash commands, no
  saved session, hooks and auto-memory off. The app also reads Claude's own
  frames (`core::decide_read_only_tool`): another tool in the init frame, any
  MCP server, or a tool outside the four ends the run; a shell command the app
  does not admit is held until its result. [[BPR-1912]] supersedes the first
  rule here, that a held command whose result says it ran ends the run as
  `tool_refused`: Claude's permission check is the gate, it approves read-only
  commands of its own beyond `--allowedTools` (2.1.314 through a wrapper ran
  `awk`), and when the result arrives the command has already run. Such a
  command is now named in the transcript as run outside the read-only list,
  counted on the proposal ("Claude ran 1 command outside the read-only list.
  See the transcript.") and in the `agent.run` diagnostic as `outside_list=N`,
  and the run goes on; a write or delete Claude ran is reported the same way
  (owner decision at pickup, as the card recommended). A command Claude denied
  reads "not run, outside the read-only list".
  Checked against installed Claude Code 2.1.310 on 2026-11-06: its init lists
  exactly the four tools, no MCP server and no plugin, and a whole-tour draft
  of the fixture pull request ran to a 12-stop proposal ([[BPR-1900]]). Codex:
  `codex exec --json --sandbox read-only` with approvals off, no MCP server and
  no web search; Codex's sandbox enforces it, and its commands are shown, not
  re-checked. Codex's App Server and the plain-text harness have no read-only
  mode yet and are refused with why.

Also recorded: the split between drafting and rewriting. **Draft the tour**,
**Draft from PR context** on an empty field and **Suggest an outline** read the
repository; **Enhance**, **Enhance selection**, **Generate comment** and the
Settings test stay BPR-1375's tool-free writing, fast and in an empty folder.
Without a clone, drafting reads the pull request's own files
([[BPR-1873]]'s "This pull request only") and says so. A whole-tour proposal
never replaces the author's text without the author's Accept on that part,
says so on a field the author already wrote, and leaves out a stop whose lines
are not lines of a head file of the pull request: owner look v002 ("What's up
with this?") rejected showing it flagged with Accept disabled, so the proposal
says only "1 suggested stop pointed outside this pull request and was left
out." (nothing when none were), and the run's transcript keeps each one.

Stop placement, owner 2026-11-06: "It should strongly prefer on hunk. I'm okay
with high value off-hunk that will work on live or i can relocate to on a hunk
or in the PR". The request gives each file's hunks (head lines of each `@@`
header of GitHub's own patch, kept on the file record) and asks for stops
wholly inside one, allowing a one-line stop off them only when it adds high
value and its first sentence says why. The core places every anchored stop
(`core::place_tour_stop`), as tour publication will: **on the change** (wholly
inside one hunk, context lines included, or the file's hunks are unknown,
which publication also places natively), shown as it is; **near the change**
(one line off every hunk: published as a line comment where GitHub takes one
off the diff, otherwise as a file comment with a permalink, so it works on
GitHub as it is), shown with a small quiet "Outside the change"; **off the
change** (several lines not wholly inside one hunk: a range has no file
comment, so it would not publish as it is), shown with one plain line saying
it won't appear when published until it is on changed lines, and a one-click
**Move to nearest change** (`core::relocate_tour_stop`: onto the closest hunk
of its file, keeping as many lines as fit). Accepted as it is, it says the
same plainly, and the author can still choose its lines in the code with Use
selected lines. A stop outside the pull request is left out as above. The
proposal lists stops on the change first, then near, then off, each group in
the agent's order. Nothing in the proposal is red or disabled.

### Amendment [[BPR-1914]], [[BPR-1915]] (2026-11-06): the tour conversation, and formatted drafts

The owner, 2026-11-06: "having used it for a while I want to do more than just
enhance the tour text with my agent. I want to ask questions, get back and
forth and have it update it. For example right now I have Solution section
where the draft said something I don't understand, but I don't really have a
way to ask for clarification. It also feels like the output of the draft is
entirely devoid of any formatting. It feels just like big walls of text."

- **T1 superseded: one conversation per tour, replayed.** The tour session
  keeps one conversation per tour (`core::tour_agent_session`, in memory for
  the open review; a run started on another tour draft lets the earlier
  tour's turns and proposal go). An **ask** turn is a fresh read-only run in
  the review's workspace, as a drafting run is, and is given the tour as it is
  now (each stop's number, place and explanation), the pull request, this
  turn's question and the passage it is about, and the earlier turns
  (`core::tour_conversation_of`: every answered ask turn and every answered
  Draft the tour run by what it proposed, oldest first, the newest 12 within
  48 KiB, with how many were left out). Failed and stopped turns and the small
  rewrites are not replayed.
- **The resume mechanism, per agent kind: a replayed transcript for every
  kind.** Claude Code: read-only runs keep `--no-session-persistence`, so
  there is no session to `--resume`, and a resumed session would be bound to
  the run's own workspace folder, which is new for each run, and would carry
  tool results from a head the review may have moved past. Codex `exec`: its
  read-only runs are one-shot `exec` calls, and `exec resume` would keep a
  rollout in Codex's own store. The App Server and the plain-text harness have
  no read-only mode and are refused with why, in the conversation. What is
  stored where: the turns (question, passage, reply as written, what it
  proposed, the steps it took) live in the app's memory for the review; no
  agent keeps a session, and nothing about the conversation is written to
  disk. T1's reasons still hold for the tour text itself: the tour so far is
  the author's current text, and the request says to trust it over earlier
  replies.
- **Where it lives: the transcript window is the conversation** (T2's window,
  renamed **Tour conversation**). The turns on the left (newest first, as
  T2's runs were); the whole conversation on the right as one selectable
  document, oldest first: the passage quoted, the question, each step quietly,
  then the reply under **Tour agent**, rendered as Markdown, and what it
  proposed. The ask box is under it (Return asks; Shift+Return starts a new
  line), with the passage quoted above it on one quiet line that can be
  dropped; **Ask** wears the agent accent and becomes **Stop** while a turn
  runs. **Copy turn** and **Copy conversation** copy plain text that reads as
  Markdown, the reply as written. **Conversation (n)** on the Tour tab replaces
  Transcript (n) and is always offered, so a question can be asked before any
  run. A compact ask box beside the selected field was not chosen: it would
  take room from the editing column the owner asked to keep clear
  ([[BPR-1913]]), and a back-and-forth needs the earlier turns in view.
- **Ask about this.** Beside each field's Enhance control (Problem, Solution,
  a stop's explanation), **Ask about this** appears while text is selected and
  opens the conversation with that passage quoted and the keyboard in the ask
  box.
- **Changes are proposals.** An ask turn answers in `bpr.tour-ask.v1`: a reply,
  and a changed Problem or Solution, new stops, or a new explanation for a stop
  of the tour by its number (`stop_texts`), each empty when unchanged. The core
  makes the proposal (`core::propose_tour_changes`): Problem, Solution and new
  stops as a whole-tour draft's, anchored, placed and left out by the same
  rules; a stop's new explanation names that stop and says accepting replaces
  its words. It joins what is still pending (`core::merge_tour_proposal`: a
  new Problem or Solution, or a stop's new explanation, supersedes the pending
  one; decided parts go). Nothing changes without Accept.
- **Formatting ([[BPR-1915]]).** Tour fields are GitHub-flavoured Markdown: the
  Tour tab's preview and stop reader render them with the same reader GitHub
  parity is tested against, and publishing posts the same bytes. Every
  built-in tour prompt (Draft the tour, ask turns, and the writing hooks for
  the Problem, the Solution and a stop's comment) carries one named style
  section (`application::tour_writing_style`): short paragraphs, a bullet list
  for parallel points, inline code for names and paths, a fenced block only
  for a short excerpt, no headings, tables, HTML or images, and the length
  bounds kept. It is the default a reviewer's own writing guidance
  ([[BPR-1916]]) can replace. A proposed text shows rendered; **Edit text**
  shows its Markdown source to change before Accept, and **Show formatted**
  renders it again. A message that holds a fenced block inside a field is read
  as the JSON it is before any fence is looked for.

### Amendment [[BPR-1916]] and [[BPR-1917]] (2026-11-06): skills in prompts, and the prompt editor

The owner, 2026-11-06: "I wonder if I can't have a hook to let me reference a
custom skill in my prompts if the skill is available in as a personal user
skill or a skill in the repo being reviewed", and "the settings agent tab
where we're editing the built-in prompts its a bit weak".

- **S1. `/skill-name` in a prompt.** In a built-in prompt (Settings), an
  Agent tab question and a Tour conversation question ([[BPR-1914]]), `/name`
  names a skill; typing `/` completes the names
  found, each with its source. Sources, bounds, precedence (personal over
  repository unless `/repo:name`) and the quiet note for an unknown name are
  in the [AI agent boundary](../architecture/ai-agent-boundary.md#skills-a-prompt-names-bpr-1916).
- **S2. Inline, for every run (owner decision).** The named `SKILL.md` rides
  in the request's supplement as escaped `skill.N.*` fields; no run gains the
  `Skill` tool or a setting source. Native loading was evaluated on Claude
  Code 2.1.310 and rejected because a wrapper can replace this application's
  settings (owner: "my custom wrapper right now seems to throw away all my
  settings on load its own, so we can't rely on that").
- **S3. Referenced only; head allowed (owner decision).** Only skills a
  prompt names reach a run. The owner allowed repository skills from the
  pull request's head on that condition; the reader takes them at the base
  from the linked clone, and shows "this pull request's repository" as their
  source. The [threat model](../architecture/security-threat-model.md#skills-a-prompt-names-bpr-1916)
  records the rule.
- **S4. The built-in prompts' editor ([[BPR-1917]]).** Settings, Agents &
  context lists every built-in prompt under where it is used (Tour tab:
  Problem statement, Solution summary, Tour outline, Tour-stop comment;
  Comments: Review feedback; Agent tab: Review skill), each with what it is
  for. The editor is fixed-pitch and wraps; it says Built-in or Changed from
  built-in, Reset to built-in restores the default (Undo reset brings the
  text back unsaved), Preview shows what a run receives with the named
  skills brought in, and unsaved edits are marked and kept per prompt until
  saved or discarded. The owner's look on a frozen build decides it.

### Amendment [[BPR-1925]] (2026-11-07): the chat inside the Tour tab

The owner: "I'm starting to think it's kind of weird that the sidechat is so
decoupled from the tour when it's really about the tour." With the drawer on
Comments, BPR-1914 v004's pane at the window's edge still showed a stop's
chat. From three mockups (`docs/design/BPR-1914-tour-chat-directions-v1/`)
the owner chose A, the chat inside the Tour tab. This supersedes v004's
window-level pane and its tab strip, and keeps everything else of v003 and
v004: one replayed chat per part, Update the part from this chat with its
[[BPR-1900]] proposal decided in the chat, Copy chat, "Worked for Ns",
[[BPR-1919]]'s highlighting and [[BPR-1916]]'s `/skill` completion.

- **Where it lives.** The Tour tab is two columns while a chat is open: the
  tour on the left, the chat about its marked part on the right, with a line
  of the agent's accent between them. The window's dock is gone; no chat
  shows outside the Tour tab. While the chat is open the drawer widens to
  hold both: 740 pixels at first (the mockup's 420 to 740), between 600 and
  960, and a drag while the chat shows sets that width for as long as the
  window is open. The drawer's own narrow width stays the one the host keeps,
  and closing the chat gives it back. On the tour's own page the chat sits
  beside the tour there.
- **The part under discussion.** It carries the agent's rule: a faint wash of
  the accent with its line down the left, on the Problem's or the Solution's
  panel, or on the stop's row of the route. Each part's glyph is pressed while
  its chat is open and shows how many turns the chat has; the route shows a
  stop's count beside its title.
- **Switching, no tabs.** A part's glyph, or Ask about this, brings that
  part's chat; while a stop's chat is open, going to another stop on the route
  brings that stop's chat, without taking the keyboard. The chat's title row
  names the part (Problem, Solution, the whole tour, or the stop's number and
  title), with Show (Show in code), Copy chat and close. Every part's chat is
  kept for the review.
- **Leaving the Tour tab.** The chat goes with the tab: on Comments, Notes or
  Agent it is gone and the drawer is narrow again. The Tour tab carries a count
  of the parts with a chat, in the agent's accent
  (`core::chat_threads`), and coming back shows the chat as it was.
- **Narrow drawers.** Conversation and the tour's chat glyph keep together at
  the right of Draft the tour, and take their own row under it when the drawer
  is too narrow for both at their own widths.

v002 (2026-11-07). The owner on v001: "The mockup had the chat button on the
right side of each of the sections and I prefer that, I also want the chat
button to toggle it open and closed."

- **A toggle at each part's right.** Each part's chat button sits at the right
  of its header, as in mockup A:
  - the Problem's and the Solution's step rows, before Change;
  - the open stop's title row, after its pencil;
  - every other stop's row on the route;
  - the tour's own, by Draft the tour.

  Its fields' helper rows no longer carry it. The button is `part_chat_toggle`
  in the agent accent with its chat's turns. Off, it is a speech bubble
  outlined in the accent; on, a pill filled with the accent.
- **What it does.** A click opens that part's chat in the column. On the part
  whose chat is open it closes the column, and on another part it switches.
  It is a checkable button that takes the keyboard: Space toggles it, it shows
  focus, and its accessible name says "Open the chat about the Solution" or
  "Close the chat about the Solution".
- **Coloured code, whatever the timing.** A text the code highlighter has
  classified before is answered inside the request. A chat reader is among
  those an answer reaches before its body asks, so a chat rebuilt for a new
  presentation keeps its colours (the owner saw v001's plain).
- **A calm edit box for a stop's text.** The owner on the in-place editor:
  "feels cramping, I dont like it how it is". It had a thick blue focus border
  hugging the words.
  - The stop's text now has no frame, no native focus rectangle and no focus
    ring.
  - The card paints a quiet box round it, 8 pixels out into its own margin at
    the sides and 6 above and below. It is the ground a shade lighter or darker
    with a hairline edge, and that edge takes the focus colour, still one pixel,
    while the keyboard is in it.
  - The words keep their place and width, where they were read, so opening the
    pencil moves and rewraps nothing.
- **The open stop's card fits.** The tour and the chat share the drawer evenly
  at first. The tour keeps 340 pixels while the chat is open (the drawer
  between 660 and 960), and the open stop's card takes the route's new width
  when the chat narrows it.

v003 (2026-11-07). The owner on v002: "Overall fantastic. Missing syntax
highlights in this part. In fact I think this whole section probably just goes
away and we move the accept into the stops directly. probably where it says
ready, next right now. In fact ready, next doesn't seem to do anything. Let's
put a different color on the stop rails for stops that aren't accepted, maybe
grey and then turn them back yellow once accepted." This amends
[[BPR-1900]]'s proposal box. Its rules hold: each part is decided on its own,
nothing changes until it is accepted, and nothing is silently overwritten.

- **The proposal waits in the tour, on what it changes.** The separate list
  under Draft the tour is gone. Each pending part's box (heading, text as it
  will read, Edit text, Discard, and Accept at the right, where Ready, next
  sits) is placed:
  - a proposed stop on the route, after the tour's own stops, numbered on from
    them, under a grey dashed ring and rail. Accepted, it is a stop of the tour
    and wears the amber of the rest. Move to nearest change and the placement
    notes come with it;
  - a stop's new explanation on that stop's card, standing where Ready, next
    is while it waits: one primary action at a time;
  - the proposed Problem or Solution under the part's own text, with a quiet
    "Proposed" pill on its step's header; the Stops step's header carries one
    too while a stop waits.

  What stays under Draft the tour is one line, "N parts wait in the tour,
  marked grey", with Accept all, and the existing left-out and outside-list
  lines. A part with no place in the tour (a new explanation for a stop since
  removed) stays there as before.
- **Highlighting.** A proposed text's inline code reads in its part's
  language, as the side chat's does: a stop's own file, or the tour's commonest
  language. Fenced blocks were already coloured.
- **Ready, next.** It did nothing visible because it was refused. A stop is
  Ready only once its lines are confirmed in the code (`core::ready`), and a
  stop the agent proposed carries its lines as a place to look, not yet
  confirmed. Its handler also looked for Ready before the host had answered.
  Now:
  - Ready, next is enabled only for a stop that can be Ready, and otherwise
    says "Confirm the stop's lines first: Show in code, select its lines in
    the code, then Use selected lines". Show in code is then the card's
    default.
  - The move to the next stop waits until the host shows the stop Ready.
  - The card's primary action goes Accept (while proposed), then Show in code
    and Use selected lines (lines unconfirmed), then Ready, next.
- **For whole-tour proposals** ([[BPR-1930]]). The placer gives each kind of
  part its place on the route. A proposed removal or merge can take the same
  form as a stop's new explanation, a box on the stops it changes with Accept
  where Ready, next is. A proposed reorder can show as grey positions on the
  route. All of them stay grey until accepted.

### Amendment [[BPR-1914]] v004 (2026-11-06): quieter controls, the diff window's tabs, no scroll jump

The owner on v003: "can we simplify the enhance buttons to just the '<symbol>
Enhance' and leave the tooltips in place. The side chat buttons should be in
the agent color. The side chat tabs behave weirdly ... I can't seem to close
the first tab or even select it ... Clicking things in the tour tab is
disorienting ... a single left-click to activate (1) but it scrolls almost all
the way off the top of the screen. Also I can't tell but it seems like inline
code isnt being highlighted only blocks." And on the tabs: "I liked the tabs I
just wanted them to behave more like our diff window tabs."

- Each field's writing helper reads "Enhance" beside the agent spark when it
  rewrites; its tooltip still says what it rewrites, and the request keeps its
  longer name ("Enhance problem statement") in the transcript. The side chat's
  glyphs on the tour, its + and its Update and Send wear the agent accent.
- The side chats' tabs are the diff window's own tab strip
  (`document_tab_strip`): equal tabs stretching across the pane (a narrower
  minimum, `set_minimum_tab_width`), a close button on every tab, the tab menu
  (Close Tab, Close Other Tabs), and + for a chat about another part.
- A click on a stop or on Show in code keeps the Tour tab where it is. Going
  to a stop's code from the open Tour tab no longer reopens the Problem step
  first (`open_review_tour(true)`), and for a moment after the click the
  editor puts back any scroll the person did not make
  (`review_tour_editor::hold_scroll`). A drag or the wheel ends that at once.
- Inline code is coloured in the language of the part it is about: a stop's
  chat and the stop reader in the stop's file's language; the tour's, the
  Problem's and the Solution's chats in the language most of the tour's stops
  are in. With no language it stays plain monospace on its wash, as before.

### Amendment [[BPR-1914]] v003 (2026-11-06): side chats, and Update from the chat

The owner rejected v002's inline threads: "This is much closer than the
previous version but it's too in the way and noisy. I think I need something
more like a side chat ... On each part I can pop open a side chat, ask some
questions and then the other part the current version is missing is being
able to ask the agent to update the section based on our chat. I dunno if each
side chat is a session or what, you figure out the best way." v003 replaces
v002's threads under the parts (described below for the record) and keeps its
thread model, code passages, proposals carried by their run and [[BPR-1919]]'s
highlighting.

- **Session model: one side chat is one thread, replayed on its own.** It
  supersedes the v001 replay of the whole conversation. Each part of the tour
  (the tour as a whole, the Problem, the Solution, each stop) has at most one
  side chat, which is its `core::tour_thread`. A turn in a chat is a fresh
  read-only run given the tour as it is now (so the part's current text) and
  that chat's own answered turns alone (`core::tour_conversation_of(session,
  thread)`, the newest 12 within 48 KiB); the tour's chat also has the
  whole-tour drafts. Chats are independent: what was said about one stop never
  reaches another's. Turns live in the app's memory for the review (closing a
  chat's tab keeps them; opening it again shows them); a run on another tour
  draft lets them go. No agent keeps a session: Claude's read-only runs keep
  `--no-session-persistence`, and the same request works for every agent kind.
- **Where it lives: docked at the window's right edge.** The side chats dock on
  the main window's right edge, past the drawers (a `QDockWidget` with no title
  bar, so the tour, the code and the comments keep their layout; closed, it
  takes no room). Its own top row is one tab per open chat, + for another
  (the tour, the Problem, the Solution or any stop), Copy chat and close. Under
  it, what the chat is about: the part's current text quoted, or a stop's
  title and file and lines, with Show (Show in code for a stop) to bring the
  tour there and a stop's lines into the diff.
- **Opening one.** Each part has one quiet glyph (a small speech bubble) beside
  its own helpers (Problem, Solution, a stop's actions), and the part's own
  context menu has Open side chat. With text selected, the chat opens quoting
  it. A stop's chat opens with the stop's lines shown in the diff, and Ask
  about its code ([[BPR-1914]] v002's code passage) quotes the lines selected
  in the diff or the stop's own. Draft the tour's row has the glyph for the
  tour as a whole. Nothing is added to the tour's column: v002's threads and
  Ask buttons are gone.
- **A plain chat.** The author's words are right-aligned bubbles (with the
  quoted passage); the agent's are left-aligned, rendered as Markdown with
  highlighted code, under a folded "Worked for Ns" that opens to the steps it
  took (the host measures the run's time, `core::time_tour_run`), with Copy
  under each reply and Copy chat for the whole chat. The composer at the
  bottom (Return sends, Shift+Return is a new line) is the one place a chat's
  words are typed: every chat sends a `qt::tour_ask` through the editor's one
  `set_tour_ask_dispatch`, where a reviewer's `/skill` references
  ([[BPR-1916]]) are resolved.
- **Update this section from our chat.** One clear action above the composer,
  "Update the Solution from this chat" (the Problem, this stop, the tour),
  enabled once the chat has an answer. It sends an Update turn
  (`tour_agent_run::Update`): the request asks for that part rewritten as its
  whole new text from the chat and the code, and the core keeps only that
  part of the answer (`core::keep_thread_part`). It comes back as the part's
  proposal (BPR-1900's, with Accept, Edit text and Discard) in the chat, and
  Accept applies it to the part. Nothing changes without Accept.
- A question sent while the session is still finishing its last run, or with
  no agent chosen, lands in its chat as a turn that did not run, saying why.

### Amendment [[BPR-1914]] v002, [[BPR-1919]] (2026-11-06): asking in context, highlighted code

v002's threads under the parts were replaced by v003's side chats above; its
thread model, code passages and highlighting stand.

The owner rejected v001's conversation window: "I don't understand this
editing interface. Like I want to ask questions about the specific parts of
the tour, how do I do that if I can't see the tour or the code?" The window
above stays, but only as a secondary, copyable record; nothing is asked there.

- **Threads under the part they are about.** Each ask turn belongs to a thread
  (`core::tour_thread`): the tour as a whole, the Problem, the Solution, or one
  stop. A thread hangs under its part as a margin note in the agent accent:
  under the Problem or Solution field, under the open stop in the route, and,
  for the tour as a whole, under Draft the tour (**Ask about the tour**). Each
  turn shows the passage it was about, the question, one quiet line of what
  the agent read, the reply rendered as Markdown, and any change it proposed.
  The thread's ask box takes follow-ups, so "rewrite that" stays in the same
  thread, and the request names every turn's thread so a follow-up refers to
  its own.
- **Where asking starts.** Beside each field's Enhance control, **Ask about the
  Problem**, **Ask about the Solution** or **Ask about this stop** is always
  offered; with text selected it reads **Ask about this** and quotes the
  passage. Under an open stop, **Ask about its code** asks about the lines
  selected in the code, or else the stop's own lines (a `stop_code` passage
  with the path, lines and, where the app has them, the lines' text). Asking
  in a stop's thread shows the stop's lines in the diff (Show in code), so the
  code is beside the thread while it is read. A new answer scrolls the drawer
  to it.
- **Proposals in the thread.** An ask turn's proposed parts carry their run
  (`tour_proposal_item::Run`) and are decided in that turn's thread, with
  Accept, Edit text and Discard beside the part they change. The proposal box
  under Draft the tour shows only a whole-tour draft's parts.
- **Highlighted code ([[BPR-1919]]).** Replies and proposed texts in threads
  and the proposal box, and the Tour tab's stop reader and previews, set fenced
  code with the app's highlighter (the Markdown code-syntax dispatch the diff's
  comments use), the profile's code font, and the panes' code colours for the
  appearance (`review_tour_editor::set_code_theme`, from the appearance
  profile). A block in a language the highlighter does not know stays plain
  monospace. The published GitHub form is unchanged.

## Acceptance criteria

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
  `git worktree prune` never runs. A head the clone lacks is fetched from the
  application's cache by local path on a complete review, and on a sparse
  review nothing is fetched and the step offers the command to copy.
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
  an edited copy by itself; the skill's text is in every review request's
  brief and nothing is written to a harness's configuration.
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

## Milestones

| Milestone | Card | Scope | Acceptance |
|---|---|---|---|
| M1 Core setup and agent finding | BPR-1748 | The setup state, rows, policy filtering, failure words and check decoding in core; the Codex search and program path; the plain-text harness only by a chosen path, so a shipped build starts both | AC02, AC03, AC13 (core share) |
| M2 Host: detection, check and saved agent | BPR-1749 | Detection when setup shows, the bounded status check, the saved agent per profile and Send to Agent's default | AC01, AC10 (agent row), AC11 (agent), AC12 (agent) |
| M3 Clone folders and links | BPR-1750 | The core's remote URL parsing and matching, the adapter's bounded `.git/config` and `.git/HEAD` reads, the stored clone folders and per-repository links | AC04, AC05 |
| M4 Agent workspaces on the clone's history | BPR-1751 | Done as a private workspace borrowing the clone's objects (see [the implementation note](#the-agents-worktree), accepted by the owner in M8's look v001); only what the clone lacks copied from the application's copy; the workspace check row | AC06 and AC07 as noted, AC10 (workspace row) |
| M5 Brief and review skill | BPR-1752 | Context supplement version three with its bounds, the built-in `bpr-pr-review` skill, its profile copy and versions (the Settings editor is M6's; see the notes above) | AC08, AC09 |
| M6 Qt setup in the Agent tab | BPR-1753 | Every frame's state in the tab, light and dark, the History entries, the partly set up callouts and the Settings rows | AC01, AC11, AC12 |
| M7 Documentation | BPR-1754 | Done: the AI agent boundary amendment (Q2) with the agent search, the check, the borrowed history, the brief and the review skill; the Git and epoch model's reviewer's clone; Settings; the workbench specification's setting up state | All, as documented |
| M8 Frozen build and the owner's look | BPR-1755 | A frozen build, a short attended session and the owner's look | AC14 |
| Closeout | BPR-1756 | Verify every criterion and close the RFC | All |
