# Review Cycles workflow

A Review Cycle reviews a folder, a single file or a Git checkout of your own,
over repeated rounds, on this computer. It needs no account, remote, commit or
agent.

Each round works like this:

1. You read the code, write notes and exact suggested edits, and hand them over
   as one round.
2. Someone (you, a colleague or an agent) changes the code.
3. You capture the code again, see what changed, record what they said and
   decide for yourself what is done.
4. The next round begins.

A finished result can become a new pull request.

This page is the reviewer's guide to the desktop app. The
[terminal guide](../development/cycle-cli.md) covers `bpr cycle`, and the
[product contract](../architecture/product-contract.md) defines the scope.

> [!NOTE]
> Nothing in a cycle leaves the computer unless you copy, save, export, grant,
> send or publish it. Nothing is ever written into the folder you review.

## Open a project

Choose **File > Open Project…**, or **Open Project…** in the **Review Cycles**
section of Home, and pick a folder or a file.

- The first time, BetterPullReview creates a cycle named after it, binds the
  cycle to that folder and captures it as the first revision, `R1`.
- Opening the same folder again resumes its cycle.
- Home lists the profile's cycles, most recent first.
- The page's title row reads **Local cycle**, so a cycle is never mistaken for
  a pull request or a Review Tour.

A capture reads the files as they are on disk, uncommitted work included.

- In a Git checkout, files excluded by the checkout's own ignore files are left
  out.
- In a plain folder, version-control and tool folders, caches and common
  credential stores are left out.
- Each exclusion is listed with its reason.
- Binary files, files that are not UTF-8 and files over 128 MiB are listed and
  can be noted as whole files, but cannot be read line by line.

## Read the code

The drawer lists the captured files. **Previous file** and **Next file** walk
through them. A large capture is listed about 4,096 files at a time.

The button beside them names the current view and opens the view chooser:

| View | Shows |
|---|---|
| **Full source** | The selected revision of the file, whole. A cycle starts here, and you can always return to it. |
| **This round** | What changed since the chosen frozen round's starting point. |
| **Since my last review** | What changed since the revision you last marked with **Mark reviewed** (the tick). Opening a file never counts as a review. |
| **All cycle changes** | Everything from the first capture to the latest. |
| **Choose revisions** | Any two captures. |

Comparisons use the captured bytes. A file edited or deleted on disk since
then still shows exactly what was captured.

- **Open in New Tab**, in the drawer's menu, keeps a file open beside the
  others.
- The **Updates** line expands to list the captured revisions. **Show newer**
  moves the view to a newer capture.

## Write notes

- **Comment** (or Return in the code) notes the selection, or the caret's line.
  With nothing selected, the button notes the whole file.
- **Note on the cycle** notes the whole change. **Note on folder/**, in the
  drawer, notes a folder.
- Write in Markdown, then choose **Save note** or press `Cmd+Return` (macOS) /
  `Ctrl+Enter` (Windows and Linux). Escape cancels, as does `Control+C` on
  macOS.
- **Link selection to note** adds the selected code to the selected note as a
  related location, so one discussion can cover several places.

A note keeps the exact code it was written on. When a later capture changes or
moves that code, the note says so (changed, found elsewhere or deleted) rather
than pointing at whatever is there now.

## Suggest an exact change

**Suggest change** (or Shift+Return in the code) opens the whole file for
editing in the main pane, starting at the caret. Edit it as you would in any
editor, then choose **Save suggestion**. Removed and added lines are shown in
the comparison's colours.

A suggestion records exact replacements against the captured bytes. It never
edits your folder. A suggestion that no longer fits the file is shown as stale,
never applied somewhere else.

## Share a round

**Share notes** opens the round as a sheet over the code.

1. **Choose what to ask for:**
   - **Implement**: "Make the changes the notes describe."
   - **Discuss**: "Answer the notes; change nothing."
   - **Plan**: "Reply with a plan before changing anything."
2. **Choose the notes.** Include or leave out each note. Reorder them by
   dragging, or with `Option+Up` / `Option+Down` (`Alt` on Windows and Linux).
   **Note for the whole round** is optional.
3. **Hand it over:**
   - **Copy round N** puts the whole assignment on the clipboard, to paste into
     an agent's chat, an issue or a message. This is the usual way.
   - **Save as File…** writes it as Markdown.
   - **Save as Bundle…** writes the text, the exact round and its selected
     source as a `.bpr-review.zip`.
   - **Exact Text** shows exactly what Copy and Save send.

The sheet uses marks rather than sentences where it can:

- A chip beside the title shows the round's state: **Not shared yet**,
  **Changed since round N** or **Shared**. The info glyph beside it explains
  the state.
- Each choice and route explains itself in a tooltip, which screen readers
  also read.
- A route the project's policy refuses stays listed, marked **Not allowed**,
  with the reason in its tooltip.
- Colours come from the appearance's state colours and always sit beside words
  or a glyph.

The first hand-over freezes the round as `F1`, `F2` and so on. A frozen round
never changes, and the sheet then says how it was shared. **Shared so far**
lists each observation in its own words, with a glyph for its kind. Its info
glyph notes that none of them means the round was received, read, acted on or
accepted.

A recipient needs nothing but the text: every note, suggested change and code
excerpt is in it. Notes you write later go into the next round.

### Send to an agent

**Send to Agent…** appears only in builds with agent integrations. It hands the
same text to one of the profile's agents, working in the cycle's folder, under
that agent's own settings and sign-in. The dialog names the agent and what it
will do; **Details** says the rest.

For a local folder, sending stays refused until the profile allows it. Allow it
from a terminal:

```sh
bpr cycle policy --classification unclassified --channel agent_adapter --permission allowed
```

Alternatively, a harness that runs `bpr` can read one round you grant it with
`bpr cycle round grant`. See
[A harness reading one round](../development/cycle-cli.md#a-harness-reading-one-round).

## Capture, compare, reply and verify

### Capture a new revision

When the code has changed, choose **Capture revision**. It reads the folder
again in the background while you keep reading, and records the next revision.

- **Cancel capture** stops it and records nothing.
- If no bytes changed, nothing is recorded, unless you choose **Record another
  observation of the same bytes** from its menu.
- No callback, receipt or Git commit is needed.

### Move a project to a new folder

If the project's folder was moved, renamed or replaced by another checkout,
**Capture revision** is refused, because capturing again would read the old
place. **Rebind Folder…** appears beside it; it is also always in **Capture
revision**'s menu.

1. Choose where the folder is now, or the checkout that replaces it.
2. The cycle compares it with the latest capture and shows how many files are
   the same, changed, missing and new.
3. Choose **Rebind** to bind the cycle there, or **Cancel** to leave it as it
   was.

Nothing is bound until you choose **Rebind**. A folder that already belongs to
another cycle is refused. If the folder changes between the comparison and
**Rebind**, the new comparison is shown for you to confirm instead.

Captures, notes and rounds stay as they are. The next **Capture revision**
reads the new location. From a terminal, use `bpr cycle rebind` (see
[Moving a project](../development/cycle-cli.md#moving-a-project)).

### Record a reply

Record what the recipient said with **Add reply**, and say who it is from:

- **Paste a reply to FN…** for prose.
- **Open a response file for FN…** for a structured response. The format is in
  the bpr-cycle skill's
  [responses reference](../../packaging/agent-skills/bpr-cycle/references/responses.md).

A reply is the recipient's claim. "Addressed" means the note needs your
verification; a response can never mark anything verified.

### Decide each note

Decide each note yourself from the **Notes** list's context menu:
**Verified in RN**, **Needs work**, **Withdraw**, **Defer** or **Clear my
decision**.

If the note or its code changes after you verify it, the verification says so
and asks you to check again.

**Next round** prepares the next round from the last round's assignment and the
notes still open. Nothing is sent until you share it.

## Publish the result

When the result is ready, choose **Share notes > Publish as Pull Request…**. It
proposes one new commit on a new branch of a GitHub.com repository, plus a pull
request.

1. Choose the base commit and target branch in your checkout.
2. Choose what to publish: **Only the cycle's improvements**, or **The starting
   work the cycle touched, and its improvements**.
3. Give the commit author, the title, and the Problem and Solution that become
   the description.
4. Choose **Review Plan** to record exactly what would go out. **Read the Exact
   Plan** shows it with its digest.
5. Choose **Publish Pull Request** to confirm that plan and send it.

Publishing uses the signed-in account and BetterPullReview's own connection.
Nothing is pushed from your checkout.

- If an answer from GitHub is lost, **Check GitHub** reads what GitHub holds
  before anything is sent again.
- **Cancel Publishing** stops and keeps what was already sent.

Other options:

- **Save Patch…** saves the same change for your own Git tools.
- **Link Existing Pull Request…** links a pull request you published yourself.
- Once a pull request is linked, **Share notes > Add Notes to Review Tour…**
  adds chosen notes, in their own words, as stops in that pull request's
  Review Tour. This is a local tour edit; it is published only from the pull
  request.

A cycle's notes, rounds and replies are never uploaded. GitHub Enterprise
Server pull requests cannot be published or linked yet.

## Builds without agents or automation

- A build without agent integrations has no **Send to Agent** and no agent
  controls.
- A build without local automation also has no `bpr` and opens no command
  endpoint.

Every cycle still works with Copy, Save, capture, replies and your own
decisions, and cycles made by a full build open unchanged. See
[release packages](../development/release-packages.md#packages-without-agent-integrations-or-local-automation).

## Where a cycle lives

A cycle is kept in the profile's private storage and cannot be recovered from
GitHub. Rounds you saved are your own files. See
[storage, diagnostics and removal](../development/storage-and-diagnostics.md)
for the store, its limits and removal, and the
[threat model](../architecture/security-threat-model.md#review-cycles) for what
the boundaries do and do not protect.
