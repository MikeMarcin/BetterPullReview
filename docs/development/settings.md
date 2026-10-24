# Settings

Open Settings with `Command+,` on macOS (`Ctrl+,` on Windows and Linux), and
find any setting with `Command+F` (`Ctrl+F`). Each profile has its own
settings, and changes apply at once.

## Signing in

- **Device sign-in** (GitHub.com) is remembered across restarts.
- **Manual token entry** works with any server, including GitHub Enterprise
  Server, but lasts only until you quit.
- **GitHub CLI access** (macOS only) borrows your existing `gh` sign-in and
  saves no token of its own.

Adding an enterprise server only remembers its hostname; you still sign in.
You can read a public GitHub.com pull request without signing in, read-only.

## Repositories and the queue

Home finds your authored and requested pull requests only in **saved
repositories**. **Observer rules** (in **Home & queue**) add other pull
requests to your queue by repository, author or path. Both are local: nothing
changes on GitHub.

**Repositories > Clone folders** lists the folders your clones live in, such
as `~/Documents/GitHub`. The list works as Visual Studio's folder lists do:
**New folder** adds a row to type a path into, with **…** at its end to
browse; **Remove**, **Move down** and **Move up** act on the selected row,
and the order is the order clones are looked for in. Double-click a row to
edit it. When the Agent tab sets up an agent, it looks in each
folder, two levels down, for a clone of the pull request's repository. It reads
only each clone's `.git/config` and `.git/HEAD`, follows no link, runs nothing
and writes nothing. **Linked clones** lists the clone, or the private copy,
each repository's agent works with; **Forget** one to choose again.

## Appearance

**Appearance > Syntax > C++ namespaces** lists the names that read as
namespaces before `::` in C++, even where a file does not declare them. It
starts as `std`, which brings the namespaces the standard puts in it
(`std::chrono`, `std::ranges` and the rest). Add your own, such as `Qt` or
`boost`, separated by commas or spaces, and write a nested one as
`boost::asio`. A class or enum a file declares with the same name still reads
as a class there. Open files recolour when you press Return or leave the field.

## Notifications

A notification carries only the repository, pull-request number, attention
category and unread count, never titles, code or comment text.

> [!NOTE]
> At most one notification is shown each time the app runs.

## Agents

Agent choices and private context are picked per review, for the next request
only. Private context is sent only when you select it.

The first time you open the Agent tab, it sets up an agent in place of the
conversation: the agent, the repository's clone and one check, then the
question field. **Reset agents…** (here, and in the Agent tab's History menu)
asks first, then removes every agent and its settings, the clone folders and
linked clones, and an edited review skill (saved to Downloads first), and
starts the setup again; notes, drafts and agent conversations stay (owner look
v006). See the [workbench specification](../ux/workbench-specification.md).

Under **Agents & context**:

- **Agents** is the list the Agent tab offers, in its order; an agent you
  have not added is never offered (owner amendment, 2026-10-23). **Add
  agent…** offers Claude Code and Codex when they are found on this computer
  and not yet added, and **Custom agent…**: a name, what the program speaks
  (Claude Code's stream JSON, Codex App Server or JSONL, or plain text), the
  program's full path, fixed arguments one per line (such as `--codex`), and
  a default model and effort. Two agents may run the same program: a work
  wrapper given nothing for Claude models is "Work Claude", and given
  `--codex` is "Work Codex". The arguments are given exactly as written,
  before the request's own, every time; nothing is put into them and no
  shell runs them. **Edit…**, **Remove**, **Move up** and **Move down** change
  the list. The first time, the Claude process wrapper and Codex program
  settings below move into the list (the chosen agent first) and are not
  moved again.
- **Review skill** is how the agent reviews: the `bpr-pr-review` skill, sent
  with every review request. Edit it and **Save**; your copy is kept through
  updates. When a newer built-in version ships you can **Compare**, **Use
  version N** or **Keep mine**. **Restore default** saves your copy to
  Downloads first unless you untick it.

The Claude process wrapper and Claude model rows are gone (owner decision,
2026-10-23): a profile's saved wrapper and model moved into its Claude Code
agent, whose program and model the list's **Edit…** changes. Tour writing
asks the agent chosen in the Agent tab when it speaks Claude Code's stream
JSON, otherwise the first one in the list that does, and Review Cycles **Send
to Agent** offers the list's agents, the Agent tab's current one first. Each
agent's model and effort show in the Agent tab's question field, where the
chip and `/model` and `/effort` set them. BetterPullReview never handles an
agent's sign-in.

- **Check** (in Add agent… and Edit…, for a custom agent only) is an optional
  check of the agent's own: fixed arguments setup runs its program with, which
  must exit 0. Empty, setup checks only that the program can be launched.
  Claude Code and Codex keep their sign-in status check.
- **Stop a run after N min** is off by default: a run goes on until the agent
  finishes or you press Stop. On, it is one limit for every agent; a run it
  ends says so.

### Review preparation writing hooks

**AI assistance > Writing instructions** holds the instructions an agent
follows when it helps write a Review Tour's Problem, Solution, outline and
stop comments. **Test saved hook** tries one on a sample without publishing.

> [!NOTE]
> Writing assistance works only with the **Approved Claude stream JSON**
> profile, which runs Claude with every tool turned off. Other profiles,
> including Codex, report it as unavailable.

#### Review feedback guidance

The **Review feedback** hook explains comment-label conventions. It is sent
with comment workshop requests only while **Comments > Feedback guidance** is
on. You can also edit it with `bpr tour hooks show|edit|reset feedback`.

See the [AI agent boundary](../architecture/ai-agent-boundary.md) for exactly
what an agent receives and may do.

## Storage & privacy

> [!WARNING]
> **Reset review data** and **Reset everything** delete private notes and
> unsent comments; a copy goes to Downloads unless you untick it. Uninstalling
> the app keeps your data and stored sign-ins, so sign out first.

See [Storage, diagnostics and removal](storage-and-diagnostics.md) for what
each reset and purge removes.
