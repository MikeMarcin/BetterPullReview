# Settings

Open Settings with `Command+,` on macOS (`Ctrl+,` on Windows and Linux), and
find any setting with `Command+F` (`Ctrl+F`). Each profile has its own
settings, and changes apply at once.

## Profiles

**Profiles** comes right after General. It lists every profile, with the one
this Settings window belongs to selected and named in bold, and each one's
state at the right (Open in 2 windows, Closed). **Profiles › Manage
Profiles…** opens this page.

- **Open** opens the selected profile; for a profile that is already open the
  button says **New window**. Return or a double-click opens or focuses it.
- **Rename** and **New profile** edit the name in the row; **Save** keeps it
  and **Cancel** or Escape leaves the name as it was.
- When a profile change fails, one notice says why with **Try again**.

With no window open (macOS keeps BetterPullReview running), **Manage
Profiles…** and a failure that no window can show open a Settings window
with Profiles alone: the same list and actions, without the sharing below,
because no profile is open to import into.

**Share profile settings** holds **Import Profile Settings…** and **Export
Profile Settings…**: a `.bprsettings` file with this profile's agents,
prompts, workflows and theme, to move to another computer or share. The file
has six sections, each ticked or unticked as a whole or item by item:

- **Agents**, **Prompts** and **Workflows**: what each is set to. A
  program's location on this computer is never written; an imported custom
  agent asks for its program.
- **Identity** (unticked to begin with): the profile's name and the accounts
  it uses, each by host and login.
- **Repositories** (ticked): saved and followed repositories, by host and
  owner/name. No local folder or clone location is written.
- **Theme** (ticked): the colours of the theme on screen and the built-in
  theme they were made on ([[BPR-2283]]). Importing it replaces that
  theme's colours on the theme on screen here. **Settings › Appearance**'s
  Export… writes a file holding only this section, and its Import… is this
  import.

Every export is a `.bprsettings` file, and only a version 3 file is read: an
older Profile Settings `.json` export or a `.bprtheme` theme is not Profile
Settings ([[BPR-2283]], owner, 2026-11-19: "don't keep legacy support").

The file never holds a sign-in: no token, keychain item, `gh` sign-in, cookie
or other secret. **Import Profile Settings…** imports into the profile whose
Settings you opened it from, and its preview names that profile. Each item
says whether it is new, updated or the same as yours, and you choose what to
save. Importing Identity renames the profile and remembers its enterprise
servers; it signs in to nothing, so each account says **Sign in to use** until
you sign in on Accounts & hosts. A saved repository whose host has no account
signed in here also says **Sign in to use**; a followed one needs no account.
Nothing runs, and an imported agent keeps strict run checks on and Build and
run tests off unless you tick them.

Dropping a `.bprsettings` file (a prompt pack or a theme is one too) onto a window
opens the same import preview over that window, read the same way; nothing
is imported until you confirm it. The window shows a quiet highlight while
one file hovers; two or more files are refused ("Drop one file at a time."),
and a file that is not Profile Settings says why in the status bar. What a
file is comes from its content, not its name ([[BPR-2282]]).

## Signing in

- **Device sign-in** (GitHub.com) is remembered across restarts.
- **Manual token entry** works with any server, including GitHub Enterprise
  Server, but lasts only until you quit.
- **GitHub CLI access** (macOS, Windows and Linux) borrows your existing `gh`
  sign-in and saves no token of its own.

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

**Appearance > People > Avatars** chooses what a person's avatar shows in a
pull request's window. **GitHub pictures**, the default, shows each person's
own picture from the pull request's server (GitHub.com or your GitHub
Enterprise Server), and their coloured initials while it loads or when it
cannot be fetched. **Coloured initials** shows only the initials and fetches
no picture. Pictures are kept with the profile's other images, in memory and
on disk.

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
question field. Agent and Repository have their own **Find agents again** and
**Find matching clones** actions; these read metadata and do not retry a check.
Check it works shows progress until both the agent and workspace finish, and
**Stop** cancels outstanding workspace work too. A failed agent has **Show
details**; a failed workspace has **Show the log**, each a local plain-text
disclosure. A custom agent's passed check says **Program check passed**
rather than signed in. **Use BetterPullReview's files for now** (or **This
pull request only, for now**, when the review holds only the pull request's
files) retries immediately without a clone; it may still need the same head
download. **Reset agents…** (here, and in the Agent tab's History menu) asks
first, then removes every agent and its settings, the clone folders and
linked clones, and an edited review skill (saved to Downloads first),
forgets which agent each pull request was last asked with, clears the legacy
wrapper/model migration fields, and starts the setup again; notes, drafts and agent conversations stay (owner look v006). See the [workbench specification](../ux/workbench-specification.md).

Under **Agents**:

- **Agents** is the list the agent picker in a pull request's footer offers,
  in its order; an agent you
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
  the list. The first time, a profile's earlier Claude process wrapper and
  Codex program settings move into the list (the chosen agent first) and are not
  moved again.

The Claude process wrapper and Claude model rows are gone (owner decision,
2026-10-23): a profile's saved wrapper and model moved into its Claude Code
agent, whose program and model the list's **Edit…** changes. Tour writing
asks the agent chosen in the agent picker in the window's footer, and only
when that agent speaks Claude Code's stream JSON; otherwise it says so and asks
nothing. The footer's picker shows and sets each agent's model
and effort; `/model` and `/effort` in the Agent tab's question field set them
too. BetterPullReview never handles an agent's sign-in.

Add agent… and Edit… also set, for each agent:

- **Check** (a custom agent only): optional fixed arguments setup runs the
  program with, which must exit 0. Empty, setup checks only that the program
  can be launched. Claude Code and Codex keep their sign-in status check.
- **Scratch folder starts** (on by default): whether a pull request starts
  with the agent's scratch folder, a folder of its own outside your repository
  where it may write and run scripts. A pull request remembers a change made
  in the agent picker. Only agents that speak Claude Code's or Codex's
  protocols get one, and no scratch folder is made on Windows.
- **Build and run tests** (off by default): lets the agent build and run tests
  in a disposable checkout of the pull request, never in your own checkout.
  Each pull request asks before the first build, because building runs its
  code with your account's rights.
- **MCP servers** (off by default): off, the agent starts with no MCP servers;
  **Use the agent's own configuration** loads the servers its own
  configuration names in conversations. A pull request's own servers never
  load, and one-off runs such as Enhance keep them off.
- **Instructions**: whether a built-in prompt's instructions go in the system
  prompt (the default) or **In the message**, for a wrapper that ignores the
  system prompt.
- **Strict run checks** (on; only a custom agent may turn it off): stops a
  tour run when the agent loads or uses a tool it was not given. Off, the app
  reports what the agent did instead of stopping it.

The rest of the page applies to every agent:

- **Stop a run after N min** is off by default: a run goes on until the agent
  finishes or you press Stop. On, it is one limit for every agent; a run it
  ends says so.
- **Follow-up behaviour** is **Queue** (the default) or **Steer**: what a
  message typed while the agent is answering does. `Command+Return` on macOS
  (`Ctrl+Return` on Windows and Linux) does the other for one message.
- **Scratch folder limit** is 256 MiB by default (16 to 65,536 MiB). A
  question that starts with the folder full may run what is there but not
  write; one that fills it is stopped. **Remove scratch folders unused for**
  30 days is on by default and is checked when BetterPullReview starts; off,
  a folder stays until you clear it. **Clear scratch folders** removes every
  agent's scratch folder now, except those of the open pull request's agents,
  and keeps the agents' conversations.
- Agents are shared with the rest of the profile from **Profiles › Share
  profile settings** ([Profiles](#profiles)).

## Prompts

**Prompts** has two parts. **Surfaces** lists where prompts appear, in order:
the pull request, file and thread **Ask** menus and their suggested chips (up
to 5 show), the tour's stop, part and whole-tour chats, and the Commits tab's
**Selection menu**, which asks about the selected commits (**Explain these
commits** and **Review these commits** ship there; a prompt there may use
`{commits}`, their short IDs). Drag a prompt up
from the **Prompt library**, or right-click it and choose **Add to**. The
library holds every prompt and filters by **All**, **Built-in**, **Yours**,
**Not on a surface** or **System**. **New prompt** adds one of your own (the
library holds at most 64), and a prompt may carry conditions that decide
where it is offered. **Restore built-ins…** gives every built-in prompt its
shipped text, turns it on and puts the surfaces back as they shipped; your own
prompts are kept. **Prompts for** switches between all repositories and one
repository, whose changes apply only to its pull requests. **Ask > Edit Quick
Asks…** opens this page. Every change saves as it is made.

The **System** prompts are the ones the app sends itself; they can be edited
and reset but not placed on a surface. They include the review skill, the
prompts in the sections below, and the Agent tab's **What changed**, **Was
this addressed**, **Explain this file** and **Suggest the change**.

**Review skill** is how the agent reviews: the `bpr-pr-review` skill, sent
with every review request. Edit it and **Save**; your copy is kept through
updates. When a newer built-in version ships you can **Compare**, **Use
version N** or **Keep mine**. **Restore default** saves your copy to Downloads
first unless you untick it.

### Review preparation writing hooks

The **Tour tab** system prompts (**Tour outline** and **Tour-stop comment**)
hold the instructions an agent follows when it helps write a Review Tour's
outline and stop comments. **Test saved prompt** tries one on a sample without
publishing.

> [!NOTE]
> Writing assistance works only with an agent that speaks Claude Code's
> stream JSON, which it runs tool-free. With any other agent, including Codex,
> the Tour tab says writing is unavailable.

#### Review feedback guidance

The **Review feedback** hook explains comment-label conventions. It is sent
with comment workshop requests only while **Comments > Comment workshop >
Feedback guidance** is on; it is off by default. You can also edit it with
`bpr tour hooks show|edit|reset feedback`.

#### Enhance prompts

**Enhance** has two system prompts: **Enhance a stop** (a tour stop with
text) and **Enhance writing** (a comment or a reply, local or draft). Each has
a built-in default and **Reset to built-in**. A press uses the prompt saved at
that moment. On a comment or a reply the result is a suggestion under the
field, used only with **Use suggestion**; Enhance writes nothing to GitHub.
`bpr tour hooks show|edit|reset` takes `enhance-stop` and `enhance-writing`.

#### Suggested comments

The **Suggested comments** system prompt (under **Agent tab**) says
how the agent writes the comments it suggests for you to post (a finding's
suggested comment, or a reply written to be posted). Its built-in default asks
for a short comment you could post as it stands: the problem in one sentence,
then the fix, code in code spans, no severity or label prefix, and no
restating of the finding. Every Agent tab question carries it, and the review
skill follows it; **Reset to built-in** restores the default. Add as draft
comment and **Draft all** on a reply start from what it writes. `bpr tour
hooks show|edit|reset suggested-comment` edits it too.

#### Local drafts

The **Local drafts** system prompts are the ones a local PR draft's agent
questions carry (RFC-BPR-0110): **Description writing**, sent when you ask
about the draft's title or description and by **Draft with agent** beside a
pull request's description in Author ([[BPR-2281]]), and **Change discussion**, sent with
every other question in its Tour and Description areas. Both cite code as
`path:first-last` in the candidate and keep moved behaviour described as kept.
A third, **Commit message**, is sent when you ask the agent to fill in a
commit; its answer fills the fields for you to edit and commits nothing. Each
has **Reset to built-in**, and all three are in Export Profile Settings… and
Import Profile Settings… ([Profiles](#profiles)).

See the [AI agent boundary](../architecture/ai-agent-boundary.md) for exactly
what an agent receives and may do.

## Storage & privacy

> [!WARNING]
> **Reset review data** and **Reset everything** delete private notes and
> unsent comments; a copy goes to Downloads unless you untick it. Uninstalling
> the app keeps your data and stored sign-ins, so sign out first.

See [Storage, diagnostics and removal](storage-and-diagnostics.md) for what
each reset and purge removes.
