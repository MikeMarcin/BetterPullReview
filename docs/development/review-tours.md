# Prepare and maintain a Review Tour

A Review Tour walks reviewers through a pull request. It has a short
**Problem**, a **Solution**, and an ordered route of stops, each a comment on
selected source lines. This page is for pull-request authors: how to write a
tour, preview it, publish it to GitHub, keep it up to date and remove it.

Writing, previewing, navigating and saving a tour all work without AI.

> [!NOTE]
> Publishing a tour writes to GitHub only after you confirm the exact plan.
> Some publication cases have not yet been tried against live GitHub:
> off-hunk and file-level batches, standalone fallback comments, pending
> edits to the Start here comment, reconciling an ambiguous result, and
> GitHub Enterprise Server. Treat them as untested.

## Start a tour

Choose **Prepare review tour** in Overview or the **Pull Request** menu
(`Alt+T`). The **Tour** page opens beside **Comments** and **Notes**.

## Write the brief and route

1. **Write the brief.** In **Problem**, say what was wrong, missing or
   difficult. In **Solution**, describe the central approach and its limits.
   A sentence can be enough. Keep lasting design rules in source and
   documentation; keep change-specific explanation here.
2. **Add stops.** Select code lines and choose **Add tour stop**
   (`Alt+Shift+T`). Give the stop a meaningful title. Explain a decision,
   invariant, tradeoff, uncertainty or point that needs scrutiny. The new stop
   goes after the current stop and is selected. Related source links can add
   context.
3. **Arrange stops.** Right-click a stop to **Insert after**, **Duplicate**,
   **Merge**, replace its source or **Remove** it.
   - Shift-click selects a range. Command-click (macOS) or Control-click
     (Windows and Linux) selects individual stops.
   - **Merge** needs two or more adjacent stops. It joins their explanations in
     route order and keeps their related sources.
   - Drag the dotted handles to reorder the selected stops, or press
     `Alt+Shift+Up` / `Alt+Shift+Down`.
   - With the outline focused, `Cmd+D` (macOS) or `Ctrl+D` (Windows and Linux)
     duplicates the selection and Delete removes it.
   - Each of these actions is one Undo step.
4. **Check each stop.** Read the explanation against its source, then choose
   **Ready and next** (`Alt+Return`). Ready is private to you. It does not mark
   code reviewed, resolve a thread or approve the change. **Previous**,
   **Next** and **Return to tour** let you explore the code and pick the route
   back up.
5. **Preview.** Check the brief, explanations and source links in **Preview**.
   The preview uses a read-only Markdown reader with limited HTML and
   disclosure support. The raw Markdown is what gets published; the preview
   does not promise to look exactly like GitHub.

### Stop status icons

Each stop shows a small icon before its number. The dotted drag handle is
separate.

| Icon | Status |
|---|---|
| Pencil | Editing |
| Circled check | Ready |
| Sparkle | Suggested / unvisited |
| Warning triangle | Needs attention |

Hover a row to see its full status. Screen readers announce the number, title
and status.

### Saving, keyboard and Undo

- **Saving is automatic** and kept per profile and account. If a save fails,
  the tour says the text is not yet saved. Keep the editor open and use the
  offered retry or recovery. If two windows conflict, both versions are kept.
- **Tab** and **Shift+Tab** move between controls; they do not insert tabs
  into your text. The focused control scrolls into view and shows a focus
  outline.
- **Undo** and **Redo** (`Cmd+Z` / `Cmd+Shift+Z` on macOS) reverse the latest
  change to the route, such as adding or dragging a stop, while that history is
  available. Once you edit text, that route history is cleared and Undo/Redo
  go to the text editor, so restoring an older arrangement can never erase
  newer writing. The normal shortcuts work throughout the Tour panel.
- Undo after publishing changes only your local draft, not GitHub.

### Size limits

| Limit | Value |
|---|---|
| Stops per tour | 256 |
| Text per field | 64 KiB (UTF-8) |
| Related sources per stop | 32 |
| Saved draft, including baselines and metadata | 4 MiB |
| Saved drafts per profile | 128 (never removed automatically) |

An edit that would exceed a limit is refused but stays available, so you can
shorten it, copy it or Undo.

GitHub's limits are smaller. One pending review can hold at most 100 line
comments and 64 KiB of request data, including JSON overhead. A tour that fits
the local limits can still be too large to publish as one review.
BetterPullReview never silently truncates a tour or splits what it promised as
a single submission.

## Publish a tour

1. Choose **Prepare publication**. It shows the account that will publish, the
   current head commit, each stop's source and text, the GitHub operations it
   will perform, and an optional pull-request description edit, listed
   separately.
2. Choose **Publish**. This confirms only the plan you saw.

For stops on supported lines and line ranges, BetterPullReview creates them as
pending review comments, links each one to the next, checks the result, and
then submits one **Comment** review.

Reviewers see ordinary GitHub comments plus a **Start here** index with
**Previous**, **Next** and overview links. They need no BetterPullReview
installation, hidden marker or special syntax to read and reply. The index and
each stop's **Previous** and **Next** link to the comment on the **Files
changed** tab (`…/pull/N/files#rID`). The tour overview link stays on the
**Conversation** tab.

### Stops GitHub cannot hold as line comments

A stop on historical, unchanged or unsupported source may have to be published
as a file comment or a pull-request conversation comment, with an exact link to
the commit. The preview says so when publishing will take several separate
visible writes. If an outcome is unknown, denied or ambiguous,
BetterPullReview does not switch to another kind of comment on its own.

### The pull-request description

A description edit changes only the description body. It updates the existing
Problem and Solution sections, or, when neither exists, previews inserting both
at the beginning. Other template content is kept. If only one section exists,
or a section appears twice, you must map them before publishing.

The description edit is separate from the review. If it fails, the tour can be
published while the description still needs attention.

Opening or publishing a tour does not mark a draft pull request ready, assign
reviewers, merge it or close it.

## Reopen and update a published tour

Reopening a tour reads the current content from GitHub.

### Recover a tour into a fresh profile

In the import choices, select the tour's **Start here** comment explicitly. Its
index supplies the stop order, and its links identify the comments. Custom or
ambiguous Markdown needs you to map it by hand. Private preparation and AI
history cannot be recovered. Importing or previewing sends no edits.

### Edits made on GitHub

Edits made on the website are compared with the last version
BetterPullReview saw and with your local text. When changes overlap, you see
**Baseline**, **Local** and **Website** versions and choose how to resolve
them. Unrelated description and template text is kept.

A missing page or a network failure leaves the last known text visible; it
does not mean the comment was deleted.

> [!WARNING]
> Someone can edit on GitHub at the same moment you publish an update. Checks
> before and after publishing cannot rule this out, because GitHub offers no
> conditional update for these comments.

### Change the route

- **Reorder** updates the affected navigation and the index. Comment IDs and
  replies stay the same.
- **Insert** creates the new comments first, then updates the index.
- **Remove** takes the stop out of the route and leaves its discussion on
  GitHub.
- **Retarget** a stop to different source creates a linked replacement,
  because a published comment cannot be moved. The old comment and its
  replies stay.
- **After the head changes**, review the original and proposed mapping, then
  reaffirm or retarget each stop. Identical selected lines do not prove the
  surrounding code is unchanged.

### Stop, resume and recover

- **Stop** halts the remaining writes and keeps what has been confirmed. It
  reports what state was reached: pending, partly updated or published,
  including a description update that still needs attention.
- After a restart, BetterPullReview reads what may have happened. Check the
  fresh preview and choose **Resume** to finish the remaining writes.
- Cancelling does not by itself mean there is a conflict on GitHub.
- An unknown outcome is not proof of failure. Do not repost blindly.
- While an update is partly done, every stop keeps a link back to the overview,
  and the existing **Start here** comment remains the route until its final
  update.

## Optional writing assistance

**Settings > Agents & context > AI assistance > Writing instructions** holds
four separate tour instructions: **Problem statement**, **Solution summary**,
**Tour outline** and **Tour-stop comment**. A fifth, **Review feedback**, is
for the comment workshop, not tour fields. Each one has a versioned source,
**Restore default** and a **Test** that you run yourself and that publishes
nothing. Changes apply to the current profile. See
[Settings](settings.md#review-preparation-writing-hooks).

### Rewrite a field

The field's assist button rewrites the selected text, including whitespace, or
the whole field when nothing is selected and the field is not empty.

- An empty Problem or Solution field first takes a matching section from the
  pull-request description, when there is exactly one, without guessing.
- Other drafting needs approved context, within limits.
- A successful result is one undoable edit. Redo reuses the text that was
  received.
- If you edit, navigate, switch account or window, use an input method, or the
  result is stale, the result is not applied to the newer text.
- A failure leaves the field unchanged.

### Suggest an outline

**Suggest tour outline** proposes stop titles, sources and private reasons, but
no explanations. Insert the stops you want, then visit each one with its
source. Write the explanation yourself or choose **Generate comment**, then
choose **Ready and next** for that text. There is no command that generates or
completes every stop at once.

> [!NOTE]
> AI text is never published automatically. The assistant does not fetch
> linked tickets or run repository instructions. Check any claims yourself:
> passing format checks and tests does not prove the model's quality. Model
> quality and installed-provider restrictions are not yet qualified. If the
> selected provider's protocol is not supported, manual editing still works.

## Retention and removal

BetterPullReview keeps these on this computer, in the profile's private
storage:

- private drafts, the last-seen GitHub versions, conflict versions and the
  record of which stops were generated;
- a bounded record of publication operations and the comment IDs GitHub
  returned, for recovery.

Purging the profile removes these, so unsent work and private history can be
lost. Published comments stay on GitHub. Uninstalling the program does not
purge the profile. Backups may keep copies; protect them like your source and
review text. See [Storage and diagnostics](storage-and-diagnostics.md).

Related: the [tour CLI](tour-cli.md), the
[qualification course](../verification/RFC-BPR-0039-0043.manual-course.md) and
the [evidence ledger](../verification/RFC-BPR-0039-0043.md).
