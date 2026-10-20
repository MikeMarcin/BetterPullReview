# Settings

This guide walks through the Settings window page by page, in the order the
pages appear in its sidebar. It is for people using BetterPullReview; the
engineering record of where each setting is stored and how it is verified is
the [Settings authority inventory](settings-authority-inventory.md).

## Open Settings

Open Settings from the application menu, the **Settings** button, or
`Command+,` on macOS (`Ctrl+,` on Windows and Linux).

- The window title names the profile you are editing. Each BetterPullReview
  profile has its own preferences.
- All windows in one profile share one Settings window. Opening Settings from
  another review brings that window forward and uses the review you opened it
  from as its context.
- Choose a page in the sidebar. Each page scrolls on its own and keeps its
  position when you switch pages. `Tab` reaches controls below the visible area.
- `Escape` first goes to whatever is active (an editor, a picker or a
  confirmation). When nothing else handles it, `Escape` closes Settings and
  returns focus to the window you came from.

To find a setting quickly, see [Search settings](#search-settings).

## General

New windows start on Home. There is no startup preference here: restoring
profiles and windows is handled by the app when it starts.

This page holds the release-update panel. Its preferences apply to the whole
application, not just the current profile. If updates are unavailable on your
platform or installation, the panel says why. See
[Application updates and recovery](application-updates.md) for the defaults,
the release-access account, checks, downloads, installation and cancelling.

## Accounts & hosts

Each configured server has a card showing its connection state, the signed-in
account and the access available. **Details** lists capabilities, rate limits
and diagnostic facts for the current session; these are information only, not
settings. A card never shows or copies credentials.

### Sign in

Choose **Sign in** to start the sign-in flow.

- **GitHub.com device sign-in** starts the code flow straight away. The code and
  the button that opens your browser stay available while you wait. **Cancel**
  discards the request.
- **Manual token entry** is always available, including for enterprise servers
  that do not offer device sign-in.
- **GitHub CLI access** (macOS only, where supported) asks you to trust it
  explicitly, naming the account and host. BetterPullReview uses your existing
  `gh` sign-in while it needs it and saves no token. It keeps reusing that
  sign-in until you sign out or replace it. The details disclose which
  executable, certificates, redirects and helpers are trusted. Publishing a
  review, comment or tour through it asks for no extra warning.

If you were opening a pull request when sign-in started, it continues once that
host accepts your sign-in. Cancelling sign-in leaves the review you already had
open usable.

### Add or forget an enterprise server

**Add enterprise server** checks the hostname and remembers it, with an
optional account choice, as local configuration. This does not sign you in or
grant access.

**Forget server** asks for confirmation and removes that local configuration.
It does not remove anything on the server or sign out a session.

### Sign out and how long sign-ins last

- **Sign out** removes the remembered device sign-in for that server and its
  stored token.
- Manual-token sign-ins last only for the current session.
- Device sign-ins can survive an orderly **Quit** and are checked again at
  startup.
- Switching servers never reuses another server's credentials.

### Public preview

You can read a public GitHub.com pull request without signing in on macOS,
Windows and Linux. Public preview is read-only: there is no signed-in account
and nothing can be published. It is not available for GitHub Enterprise
Server.

Which sign-in methods are available depends on your platform and the server.
When one is unavailable, the page says so; it never quietly switches to
another method.

## Repositories

This page lists your saved repositories. Saved repositories let Home find pull
requests you wrote or were asked to review.

### The repository list

One card names the account and host the list belongs to ("Saved for @account
on host") and lists that account's repositories as owner/name. It grows with
its rows up to ten and then scrolls. A row's tooltip repeats its host and
account. With nothing saved, the card says "No repositories yet."

When you are signed out there is no current account, so every saved record is
listed with its account and host, and **Remove** is unavailable until you sign
in.

### Add a repository

1. Choose **Add repository** under the card. A **New repository** card opens
   on the page, with its **Repository** field focused. (Home's **Add a
   repository** buttons open this same card.)
2. Check the destination the card shows ("Save for @account on host").
3. Enter an HTTPS repository URL or owner/name for that host.
4. Choose **Add** or press `Return`.

Progress and problems are shown beside the field. If the repository cannot be
added, your text stays so you can fix it and try again; a repository that is
already saved gets its own message. Once saved, the card closes into the list,
"Added owner/name." appears under **Add repository** and in the status bar, and
the keyboard returns to **Add repository**.

Saving a repository starts a search for your authored and requested pull
requests in it. It changes no GitHub subscription or review and opens no pull
request. The saved record belongs to the current profile and account.

To save a repository on a different host, choose **Choose account or set up
server…** first. Your text stays on the page while you do this. Cancelling
saves nothing; after you sign in, check the destination and choose **Add**
again.

**Cancel**, or `Escape` in the field, discards the text and returns the
keyboard to **Add repository** without closing Settings.

From an open pull request, **Save Repository** saves its repository directly,
in the same way.

### Remove a repository

Select a row and choose **Remove**, or press `Delete` or `Backspace`. This
removes only the local saved record. If removal fails, the list stays as it
was. Removing the last repository brings back Home's **Add a repository**
button.

## Home & queue

These choices change what Home shows on this machine. They never change GitHub
assignments or subscriptions.

### Queue defaults

| Setting | Default |
|---|---|
| Role, observer reason, attention bucket, repository | Any |
| Quiet/current state | Any |
| Sort | Priority order |

**Reset** on this group resets only the queue filters and sort.

### Observer rules

Observer rules add pull requests to your queue by matching a repository,
author or path. Adding or removing a rule changes only the local rule and the
pull requests it matched. Rules apply across all repositories, not only saved
ones.

**Open Repositories** under **Saved repositories** takes you to the
[Repositories](#repositories) page.

## Review

How a pull request opens and reads.

| Group | Setting | Default |
|---|---|---|
| Opening | Open on | Overview |
| Opening | Return comparison | Whole PR |
| Reading | Mark read | By hand |
| Reading | Comments drawer filter | All |
| Layout | Drawer controls | Hidden |
| Layout | Line details | Shown |
| Layout | Center ribbon width | 28 px |

Each group's **Reset** restores only that group's settings.

**Center ribbon width** is the space between the two source panes, in logical
pixels, from 16 to 256. A change updates every window of the profile and is
kept after restart.

## Comments

### Labels

| Setting | Default |
|---|---|
| Label marks | Icons |
| Overview counts | Open |
| Overview chips | None |

- **Label marks** shows each comment label as an icon or as **Letters** (the
  label's two letters). A six-mark preview sits beside the choices. A label
  typed out in full or chosen from the chip is recognised and counted either
  way.
- **Overview counts** sets whether a pull request's Overview starts by counting
  **Open** (unresolved) threads or **All**. Switching it on an Overview changes
  only that view.
- **Overview chips** lists the twelve labels. Tick a label to give it its own
  count in the Overview's Comments column. Drag a ticked label, or press
  `Alt+Up` / `Alt+Down` (`Option+Up` / `Option+Down` on macOS), to change the
  order.
- **Mark colours** opens **Appearance > Colours** filtered to "label".
- **About Conventional Comments** opens conventionalcomments.org, where the
  labels come from, in your browser. The comment label menu ends with the same
  item.

The Comments drawer's own filter (on the [Review](#review) page) is separate
and is never changed by these settings.

If your saved chips name a label this version does not know, or the same label
twice, the row says how many were left out. The rest keep their order. Changing
a chip or choosing **Reset** saves the list as shown.

### Comment workshop

**Feedback guidance** ("Send with comment workshop requests") is off by
default. When it is on, each comment workshop request includes the
[Review feedback](#review-feedback-guidance) guidance. Its info icon explains
what the guidance is. **Guidance text** opens the writing instructions on
**Agents & context** with the Review feedback hook selected. **Reset** turns
Feedback guidance off and changes nothing else.

### Descriptions and comments

Images in descriptions and comments load by default.

## Appearance

### Mode and type

The mode defaults to **System**. You can also choose **Light**, **Dark**,
**High contrast system**, **High contrast light** or **High contrast dark**.

Interface and code fonts start from your platform's native defaults. You can set
family, size and weight separately for interface and code, plus interface line
height and code options. Code defaults:

| Code option | Default |
|---|---|
| Tab width | 4 spaces |
| Ligatures | Off |
| Bold and italic syntax emphasis | On |

The preview shows the values in effect.

### Colours

Each theme keeps its own custom colours.

1. Filter the colour-role list and choose a swatch to preview a change.
2. Choose **Keep changes** to save that theme's palette, or **Cancel** to
   discard the preview. Closing Settings also discards it.

**Import** checks a theme file (size-limited) before previewing it. **Export
saved theme** writes the saved palette for the theme on screen; unsaved
previews are not included. Theme files hold colour roles only, never
credentials, review content or other app data.

### Dif

Dif is shown by default and follows the system's motion setting. When the
system asks for reduced motion, or the setting cannot be read, Dif uses its
still pose.

### Resets

| Reset | Restores |
|---|---|
| Reset appearance mode | System |
| Reset type | Platform font defaults and code options |
| Reset all colours | Removes custom colours for the current theme only |
| Dif reset | Visibility and motion |

Other themes and unrelated preferences are not touched.

> [!NOTE]
> If appearance settings cannot be saved, you can still make changes for the
> current session (they are labelled as such). They return to the defaults
> after restart while the problem lasts.

## Notifications

Review notifications are on and not paused by default. These preferences are
separate from your operating system's notification permission. **Reset
notification preferences** changes only on/off and pause; resetting queue
defaults does not change them.

**Preview for selected Home review** sends a test notification and reports
whether it was handed to the system. That does not prove the system showed a
banner.

A notification contains the repository, review number, attention category and
unread count. It never contains titles, source code or comment text. Clicking a
notification opens only the review it is about.

> [!NOTE]
> System notifications and clicking them may be unavailable or limited on your
> platform. The current delivery shows at most one notification each time the
> app runs, and only the first click on it opens its review, because the
> system does not say which notification was clicked.

## Agents & context

### Per-review agent choices

Open a review to see the agent profiles allowed for it. The profile, the
private notes and draft you select, and whether to use the existing working
copy apply only to your next request in that review; they are not saved as
defaults. Private context is sent only when you select it. Start and cancel
agent work from the review's agent panel.

Settings cannot create an agent credential, loosen what an agent is allowed to
do, or publish an agent's output. Launch options a profile does not support
are explained on the page.

### Claude

**Process wrapper** is saved for the profile. It is the full path to a
program run in place of `claude` for every Claude request. It gets Claude's own
arguments, folder and environment, and is never run through a shell. Set it to
the `claude` you installed (for example, the path `which claude` prints) or to
a program that prepares your provider and then runs Claude.

When the wrapper is empty, BetterPullReview runs the first `claude` it finds on
the app's `PATH`, then in the usual install folders:

- `~/.local/bin`, `~/.claude/local`, `/opt/homebrew/bin`, `/usr/local/bin`,
  `~/.npm-global/bin`
- On Windows, `%USERPROFILE%\.local\bin\claude.exe`

A read-only line under the field says which one it found, for example "Using
/opt/homebrew/bin/claude (found on PATH)." or "claude not found on PATH or in
the usual install folders; set a wrapper." A saved wrapper always takes
priority.

To set it, choose **Choose…** to pick the file, or type a path and choose
**Save** or press `Return`. Saving an empty field clears it. The path is checked
when you save and again before every launch. A rejected path is not saved, and
the row says why: not absolute, starts with `~`, nothing there, a folder, not
executable, or on Windows not an `.exe` (a `.bat` or `.cmd` is refused because
only `cmd.exe` runs those).

**Model** is the Claude model saved for the profile, for example
`claude-sonnet-5-5`. It is passed to `claude` as `--model` for tour writing
and **Ask agent** requests; empty uses Claude's own default. Choose **Save** or
press `Return` to save it. A name is refused if it contains a space, a quote,
a leading `-`, any character other than letters, digits and `. _ - : @ / [ ]`,
or is longer than 128 bytes. Review Cycles **Send to Agent** does not use this
model yet.

The **Sign-in** line reads "Claude and your wrapper handle sign-in.
BetterPullReview never asks."

For how Claude is launched, see the
[AI agent boundary](../architecture/ai-agent-boundary.md#claude-process-wrapper).

### Review preparation writing hooks

**Agents & context > AI assistance · Writing instructions** holds the
instructions an agent follows when you ask it to help write a review tour.
There are four tour hooks:

| Hook | Helps write |
|---|---|
| Problem statement | The tour's problem statement |
| Solution summary | The tour's solution summary |
| Tour outline | Suggested tour stops (no comment text) |
| Tour-stop comment | The explanation for one tour stop |

For each hook you can edit the instructions and choose **Save instructions**,
or choose **Restore default** to follow the shipped text again. Labels show
which text is in effect and its version. Your saved text is kept across
upgrades. Changes apply to the current profile. Instructions are text only:
they give an agent no tools, no publishing and no wider editing.

Saving or restoring never runs an agent. **Test saved hook (no publishing)**
runs the selected agent once, in the open review, on a sample passage. The
result is private, changes no draft and no tour field, and publishes nothing.

**Using a hook while writing a tour.** A writing button sends your selection
(including its whitespace), or the whole field when nothing is selected.

- An empty Problem or Solution field first picks up a matching section from
  the existing pull-request description, exactly as written.
- Other empty fields need existing material to work from; an empty tour stop
  also needs its pinned source to be available.
- Context is gathered only when you press the button: the author's description
  and comments (size-limited), your private brief, surrounding text and the
  selected stop's code. Linked pages are never fetched.
- An accepted result is a single undo step. Undo restores your text and
  selection; redo reuses the result without running the agent again.
- Notes the agent marks as recovered, inferred or unclear stay private and out
  of the tour text.
- **Tour outline** returns suggestions without comment text. Insert the stops
  you want from the **Tour stops** page, then visit each one and write its
  explanation against its source.

If no supported agent or source is available, you can still write by hand.

| Limit | Size |
|---|---|
| Selection sent | 6,000 bytes (UTF-8) |
| Returned replacement text | 16,384 bytes |

If your selection is too large, you are asked to select less.

> [!NOTE]
> Writing assistance works only with the **Approved Claude stream JSON**
> profile, which runs Claude with every tool turned off. Other profiles,
> including the Codex profiles, report writing assistance as unavailable.
> BetterPullReview's launch options do not change your own Claude
> configuration. Installed-provider and model quality have not yet been
> qualified.

See [Prepare and maintain a review tour](review-tours.md) for the tour workflow,
and the [AI agent boundary](../architecture/ai-agent-boundary.md#review-tour-writing-boundary)
for how the request is restricted.

#### Review feedback guidance

Below a separator, the same list holds a fifth hook, **Review feedback**. Its
default text explains the conventions behind comment labels: the twelve
labels, the decorations, how a `suggestion` label differs from a GitHub
suggested change, and how a label differs from a review decision.

Comment workshop requests include it only while **Comments > Comment workshop
> Feedback guidance** is on; a line under the source label says On or Off and
links there. It writes no tour field, so **Test** is unavailable for it.
**Save instructions** keeps your text across upgrades; **Restore default**
follows the shipped text again. From the command line,
`bpr tour hooks show|edit|reset feedback` reads and changes the same text.

## Storage & privacy

### Local data

The local data location is shown read-only. It holds repository caches,
retained history, drafts, notes, receipts and preferences. You cannot move it,
purge all profiles or edit automatic retention here.

### Prune history and delete caches

**Prune selected epochs** and **Delete repository cache** apply to the current
review and ask for confirmation, with **Cancel** as the default. Pruning can
make an older comparison unavailable. Neither changes GitHub or your working
copy, and neither resets preferences.

### Diagnostics

**Help > Diagnostics** opens this page, even when you are signed out.

- Capture detail is **Standard** or **Verbose**.
- Identifying details are **Anonymized** or **Include locally**, chosen
  separately.
- **On**/**Off**, **Open logs** and **Clear** are nearby. **About local
  diagnostics** explains what recent context is kept and the recording limits.

The **Help** menu also has **Export anonymized diagnostics…** and **Export with
identifying details…**, which work without opening Settings. Each opens a
preview of exactly what will be saved, with **Save…** and **Cancel**; options
and the full records expand on request. The identifying preview lists what
capture could not omit, and **Save with identifying details…** asks you to
confirm before you choose a file. Nothing is uploaded. See
[Preview and export diagnostics](storage-and-diagnostics.md#preview-and-export-diagnostics).

### Profile maintenance

Expand **Profile maintenance** for the cleanup controls.

1. **Prepare diagnostic cleanup…** or **Prepare profile purge…** warns that
   the profile and everything writing to it will stop, then lists exactly what
   will be removed. Preparing deletes nothing.
2. **Clear prepared diagnostics…** or **Purge prepared profile data…** asks
   you to confirm that list, then removes it.

**Cancel maintenance** does not restart a stopped profile and cannot undo an
operation that has already started removing data.

> [!WARNING]
> Purge removes only this profile's data. Shared updater and registry data,
> other profiles, your exports and some empty folders and lock files remain.
> Purge is not uninstall and not secure erasure.

### Troubleshooting

The page ends with four resets, each with a short description and a button:

| Reset | Removes | Keeps |
|---|---|---|
| **Clear caches…** | Downloaded repository data, images in descriptions and comments, pull-request summaries on Home, context snapshots kept for agents | Notes, drafts, read marks, settings |
| **Reset settings…** | Every choice in Settings, Home's queue filters and sort, notification choices, window layout and which windows reopen | Saved repositories and servers, observer rules, review data and caches |
| **Reset review data…** | Read and seen marks, private notes and their threads, unsent comments, replies and reviews, review tours, Review Cycles | Everything posted on GitHub, settings and caches, files you saved or exported |
| **Reset everything…** | All three of the above, including saved repositories and observer rules, plus Insights statistics and history | Other profiles, your Git checkouts and exported files |

> [!WARNING]
> Clearing caches can make comparisons with earlier pushes of a force-pushed
> pull request impossible to rebuild, because GitHub may no longer have them.

Each asks for confirmation, listing what it removes and what it keeps;
**Cancel** is the default and `Escape` cancels. **Reset review data** and
**Reset everything** also need "I understand my private notes and unsent
comments will be deleted" ticked, and offer to save a copy of the removed data
in Downloads (on by default).

A confirmed reset closes the profile's windows, runs, and reopens the profile
on Home, whose status bar says what was reset. No reset signs you out or
touches diagnostic logs. See
[Reset this profile's local data](storage-and-diagnostics.md#reset-this-profiles-local-data).

### Uninstalling

> [!WARNING]
> Uninstalling the app keeps your local data and system credential entries. To
> remove remembered device sign-ins, sign out on **Accounts & hosts** before
> uninstalling. Native vault, crash-reporting and full-removal behaviour are not
> yet qualified.

The [storage, diagnostics and removal guide](storage-and-diagnostics.md) gives
the full scope, recovery rules and what remains after each operation.

## Search settings

Type in **Search settings** at the top of the sidebar, or press `Command+F` on
macOS (`Ctrl+F` on Windows and Linux).

While the field has text, the sidebar lists matching settings, grouped by page
in sidebar order and by section within a page. Each result shows the setting's
name with its page and section below in smaller type. A matching page or
section is a result of its own. Matched letters are shown in bold.

A setting matches when your text:

- appears in its name, at the start of a word or inside one;
- or, failing that, matches its letters in order, where each word you enter
  starts at the beginning of a word ("lnum" finds Code line number text);
- appears in its section;
- starts a word in its description (descriptions are not shown, and count only
  when no name, section or page matches).

If nothing else matches, a small typing slip is forgiven.

| Key | Action |
|---|---|
| `Up` / `Down` | Choose another result while typing |
| `Tab` | Move into the result list |
| `Return` or click | Open the result's page, scroll to the setting and focus it |
| `Escape` | Clear the field; from the list, return to the field; with the field empty, close Settings |

Opening a result changes nothing. A setting that cannot take the keyboard is
shown and its page takes focus. When nothing matches, the sidebar says so and
offers **Clear**. Search covers only the pages currently shown.

## How changes apply

- Changes apply as soon as they are accepted; there is no **Apply** button.
- A change that is refused goes back to the previous value and shows why.
- Closing Settings cancels an unsaved colour preview or a pending theme import.
  It does not undo a saved change or cancel a storage operation that has
  started.
- If one kind of setting cannot be saved, only that kind is affected. Where
  changes still work for the current session, the page labels them
  session-only.
