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
question field. **Set up agent…** (here, and in the Agent tab's History menu)
runs it again. See the [workbench specification](../ux/workbench-specification.md).

Under **Agents & context**:

- **Codex** is found on `PATH` and in its usual install folders. A program
  path saved here is used instead. **Protocol** chooses App Server (the
  default) or JSONL.
- **A harness that reads plain text** is for a tool with no Claude or Codex
  interface. It runs only the program path you choose here and is offered in
  setup once one is saved.
- **Review skill** is how the agent reviews: the `bpr-pr-review` skill, sent
  with every review request. Edit it and **Save**; your copy is kept through
  updates. When a newer built-in version ships you can **Compare**, **Use
  version N** or **Keep mine**. **Restore default** saves your copy to
  Downloads first unless you untick it.

Under **Agents & context > Claude**:

- **Process wrapper** is the full path to a program to run instead of
  `claude`, for example one that sets up your provider first. Left empty,
  BetterPullReview uses the first `claude` on `PATH` or in the usual install
  folders, and says which.
- **Model** is passed to Claude as `--model`. Review Cycles **Send to Agent**
  does not use it yet.
- BetterPullReview never handles Claude's sign-in.

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
