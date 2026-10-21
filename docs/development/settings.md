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

## Notifications

A notification carries only the repository, pull-request number, attention
category and unread count, never titles, code or comment text.

> [!NOTE]
> At most one notification is shown each time the app runs.

## Agents

Agent choices and private context are picked per review, for the next request
only. Private context is sent only when you select it.

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
