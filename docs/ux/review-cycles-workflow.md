# Review Cycles workflow

A Review Cycle reviews a folder, file or Git checkout on this computer over
repeated rounds. Start one with **File > Open Project…**. It needs no account or
agent. The [terminal guide](../development/cycle-cli.md) covers `bpr cycle`.

> [!NOTE]
> Nothing leaves the computer unless you copy, save, export, grant, send or
> publish it. Nothing is ever written into the folder you review.

## How a cycle works

- **Captures.** A capture (`R1`, `R2`, …) records the files as they are on
  disk, uncommitted work included. Ignored files, tool folders and common
  credential stores are left out. Opening the same folder again resumes its
  cycle; **Capture revision** records a new capture, and unchanged bytes record
  nothing.
- **Comparisons** use the captured bytes, not the disk, so a file edited or
  deleted since still shows what was captured. **Since my last review** counts
  from the revision you marked with **Mark reviewed**; opening a file never
  counts.
- **Notes** keep the exact code they were written on. When a later capture
  changes or moves that code, the note says so rather than pointing at
  whatever is there now. A suggested change records exact replacements and is
  shown as stale, never applied elsewhere, once it no longer fits.
- **Rounds.** Handing notes over freezes them as a round (`F1`, `F2`, …) that
  never changes. Notes you write later go into the next round. The round's text
  carries every note, suggestion and code excerpt, so a recipient needs nothing
  else.
- **Replies** are the recipient's claims. "Addressed" still needs your
  verification; you decide each note yourself, and a later change to the note
  or its code asks you to check again.

> [!WARNING]
> A cycle lives only in the profile's private storage and cannot be recovered
> from GitHub. See [storage and removal](../development/storage-and-diagnostics.md).

## Hand a round to someone

**Share notes** offers **Copy round N** (the usual way, to paste into an agent's
chat or a message), **Save as File…** (Markdown) and **Save as Bundle…**
(`.bpr-review.zip`). **Exact Text** shows exactly what is sent. The delivery
marks under **Shared so far** record how a round left; none of them means it
was received, read or accepted.

### With an agent

**Send to Agent…**, in builds with agent integrations, runs one of the
profile's agents in the cycle's folder under that agent's own settings. For a
local folder it stays refused until you allow it:

```sh
bpr cycle policy --classification unclassified --channel agent_adapter --permission allowed
```

A harness that runs `bpr` can instead read one round you grant it; see
[A harness reading one round](../development/cycle-cli.md#a-harness-reading-one-round).

## When the folder moves

If the folder was moved, renamed or replaced, **Capture revision** is refused.
Use **Rebind Folder…**: nothing is bound until you confirm the comparison it
shows, and captures, notes and rounds are kept. From a terminal, see
[Moving a project](../development/cycle-cli.md#moving-a-project).

## Publish the result

**Share notes > Publish as Pull Request…** proposes one new commit on a new
branch of a GitHub.com repository, plus a pull request. **Review Plan** records
exactly what would go out before **Publish Pull Request** sends it.

- It uses the signed-in account and BetterPullReview's own connection. Nothing
  is pushed from your checkout, and the cycle's notes, rounds and replies are
  never uploaded.
- If an answer from GitHub is lost, **Check GitHub** reads what GitHub holds
  before anything is sent again. **Cancel Publishing** keeps what was already
  sent.
- GitHub Enterprise Server pull requests cannot be published or linked yet.
