# Review Cycles workflow

A Review Cycle reviews a folder, a single file or a Git checkout of your own
over repeated rounds, on this computer, with no account, remote, commit or
agent. You read the code, write notes and exact suggested edits, and hand them
over as one round. Someone (you, a colleague or an agent) changes the code; you
capture it again, see what changed, record what they said and decide for
yourself what is done; then the next round. A finished result can become a new
pull request. This page is the reviewer's guide to the desktop. The
[terminal guide](../development/cycle-cli.md) covers `bpr cycle`, and the
[product contract](../architecture/product-contract.md) the scope.

Nothing in a cycle leaves the computer unless you copy, save, export, grant,
send or publish it, and nothing is ever written into the folder you review.

## Open a project

Choose **File > Open Project…**, or **Open Project…** in Home's **Review
Cycles** section, and pick a folder or a file. The first time, BetterPullReview
makes a cycle titled after it, binds the cycle to that folder and captures it as
the first revision, `R1`; opening the same folder again resumes its cycle. Home
lists the profile's cycles, most recent first, to resume. The page's title row
reads **Local cycle**, so a cycle is never mistaken for a pull request or a
Review Tour.

A capture reads the files as they are, uncommitted work included. In a Git
checkout, files the checkout's own ignore files exclude are left out, and
version-control and tool folders, caches and common credential stores are
left out of a plain folder; each exclusion is listed with its reason. Binary
files, files that are not UTF-8 and files over 128 MiB are listed and can be
noted as whole files, but not read line by line.

## Read the code

The drawer lists the captured files; **Previous file** and **Next file** walk
them, and a large capture is listed about 4,096 files at a time. The button
beside them names the view shown and opens the chooser:

- **Full source**: the selected revision of the file, whole. This is where a
  cycle starts, and you can always come back to it.
- **This round**: what changed since the chosen frozen round's starting point.
- **Since my last review**: what changed since the revision you last marked
  reviewed with **Mark reviewed** (the tick). Opening files never counts as a
  review.
- **All cycle changes**: from the first capture to the latest.
- **Choose revisions**: any two captures.

The comparisons are read from the captured bytes, so a file edited or deleted
on disk since still shows exactly what was captured. **Open in New Tab** in the
drawer's menu keeps a file open beside the others. The **Updates** line folds
out the captured revisions; **Show newer** moves the view to a newer capture.

## Write notes

- **Comment** (or Return in the code) notes the selection, or the caret's
  line. With nothing selected the button notes the whole file; **Note on the
  cycle** notes the whole change, and the drawer's **Note on folder/** a
  folder.
- Write in Markdown and choose **Save note**, or press ⌘↩ on macOS (Ctrl+Enter
  on Windows and Linux). Escape cancels; so does Control-C on macOS.
- **Link selection to note** adds the selected code to the selected note as a
  related location, for one discussion across several places.

A note keeps the exact code it was written on. When a later capture changes or
moves that code, the note says so (changed, found elsewhere, deleted) rather
than pointing at whatever is there now.

## Suggest an exact change

**Suggest change** (or Shift+Return in the code) opens the whole file for
editing in the main pane, starting at the caret. Edit it as you would in an
editor and choose **Save suggestion**; the removed and added lines are shown in
the comparison's colours. A suggestion records exact replacements against the
captured bytes. It never edits your folder, and a suggestion that no longer fits
the file is shown as stale rather than applied somewhere else.

## Share a round

**Share notes** opens the round as a sheet over the code:

1. Choose what to ask for: **Implement** ("Make the changes the notes
   describe."), **Discuss** ("Answer the notes; change nothing.") or **Plan**
   ("Reply with a plan before changing anything.").
2. Include or leave out each note, and order them by dragging, or with
   Option+Up and Option+Down (Alt elsewhere). **Note for the whole round** is
   optional.
3. Hand it over. **Copy round N** puts the whole assignment on the clipboard,
   to paste into an agent's chat, an issue or a message; this is the usual way.
   **Save as File…** writes it as Markdown, and **Save as Bundle…** writes the
   text, the exact round and its selected source as a `.bpr-review.zip`.
   **Exact Text** shows exactly what Copy and Save send.

The sheet says most of this with marks rather than sentences. A chip beside
the title shows the round's state (**Not shared yet**, **Changed since round
N**, **Shared**), and the info glyph beside it explains it. What each choice
and route is for is its tooltip, which a screen reader reads too. A route the
project's policy refuses stays listed, marked **Not allowed**, with the reason
in its tooltip. Colours come from the appearance's state colours and always
sit beside words or a glyph.

The first hand-over freezes the round (`F1`, `F2`, …): it never changes after,
and the sheet then says how it was shared. **Shared so far** lists each
observation in its own words, with a glyph for its kind; its info glyph says
that none of them means the round was received, read, acted on or accepted. A recipient needs nothing but the
text; every note, suggested change and code excerpt is in it. Later notes go
into the next round.

**Send to Agent…** appears only in a build with agent integrations. It hands
the same text to one of the profile's agents, in the cycle's folder, under that
agent's own settings and sign-in; the box names the agent and what it will do,
and **Details** says the rest. For a local folder it stays refused until the
profile allows it, from a terminal:
`bpr cycle policy --classification unclassified --channel agent_adapter --permission allowed`.
A harness that runs `bpr` can instead read one round you grant it
(`bpr cycle round grant`); see the [terminal guide](../development/cycle-cli.md#a-harness-reading-one-round).

## Capture, compare, reply and verify

When the code has changed, choose **Capture revision**. It reads the folder
again in the background, while you keep reading, and records the next revision;
**Cancel capture** stops it and records nothing. Unchanged bytes record nothing,
unless you choose **Record another observation of the same bytes** from its
menu. No callback, receipt or Git commit is needed.

Record what the recipient said with **Add reply**: **Paste a reply to FN…** for
prose, or **Open a response file for FN…** for a structured response (the format
is in the bpr-cycle skill's
[responses reference](../../packaging/agent-skills/bpr-cycle/references/responses.md)),
and say who it is from. A reply is their claim: "addressed" means the note
needs your verification, and a response cannot mark anything verified.

Decide each note yourself from the Notes list's context menu: **Verified in
RN**, **Needs work**, **Withdraw**, **Defer** or **Clear my decision**. A
verification that the note or its code later changes says so and asks you to
check again. **Next round** prepares the next round from the last round's
assignment and the notes still open; nothing is sent until you share it.

## Publish the result

When the result is ready, **Share notes > Publish as Pull Request…** proposes
one new commit on a new branch of a GitHub.com repository, and a pull request.
Choose the base commit and target branch in your checkout, and whether to
publish **Only the cycle's improvements** or **The starting work the cycle
touched, and its improvements**; give the commit author, the title, and the
Problem and Solution that become the description. **Review Plan** records
exactly what would go out, and **Read the Exact Plan** shows it with its digest.
**Publish Pull Request** confirms that plan and sends it, as the account signed
in, through BetterPullReview's own connection: nothing is pushed from your
checkout. If an answer is lost, **Check GitHub** reads what GitHub holds before
anything is sent again, and **Cancel Publishing** stops with what was sent kept.

**Save Patch…** saves the same change for your own Git tools, and **Link
Existing Pull Request…** links one you published yourself. A cycle's notes,
rounds and replies are never uploaded. Once linked, **Share notes > Add Notes to
Review Tour…** adds chosen notes, in their own words, as stops of that pull
request's Review Tour; that is a local tour edit, published only from the pull
request. GHES pull requests cannot be published or linked yet.

## Builds without agents or automation

A build without agent integrations has no Send to Agent and no agent controls;
a build without local automation also has no `bpr`, and binds no command
endpoint. Every cycle keeps working with Copy, Save, capture, replies and your
own decisions, and cycles made by a full build open unchanged. See
[release packages](../development/release-packages.md#packages-without-agent-integrations-or-local-automation).

## Where a cycle lives

A cycle is kept in the profile's private store and is not recoverable from
GitHub. Rounds you saved are your own files. See
[storage, diagnostics and removal](../development/storage-and-diagnostics.md)
for the store, its limits and removal, and the
[threat model](../architecture/security-threat-model.md#review-cycles) for what
the boundaries do and do not protect.
