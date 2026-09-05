---
schema: "implement-rfc.card.v1"
id: "BPR-0163"
title: "Qt: landing model, ordered by the core, incremental by identity"
status: "Done"
rfcs: ["RFC-BPR-0010"]
depends_on: ["BPR-0158"]
---

# Qt: queue model, filters, sorting, and empty states

The landing page. Models update incrementally by stable pull-request identity,
filters and sorting are presentation, and every empty state says what would put
something there.

## Required direct evidence

- Replacing a projection with one differing in a few items produces exact
  insert, remove, move, and data signals, and no routine reset.
- Persistent indexes survive a replacement or are invalidated legally, by
  identity rather than by row.
- The model has no mutating entry point: the presentation cannot decide a
  bucket, and there is no method by which it could.
- Every state reads without colour and through a screen reader, as the defect
  card does.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What was done

`qt::review_landing_model`, not `review_queue_model` — that name is taken by
RFC-BPR-0007's model of ranked actionable *files within one review*. This is
pull requests *across* reviews, and reusing the name would have made two very
different things look like one.

### Diffing by identity, not by position

The RFC asks for incremental updates by stable pull-request identity, and that
is a stronger requirement than it first sounds. RFC-BPR-0007's model diffs
positionally: truncate or extend, then report the differing prefix changed.
Applied here, an item that becomes urgent and moves to the top would be
reported as a long run of data changes on rows that did not change.

That is correct only in the sense that a reset is correct. The view re-reads
everything, the selection means nothing, and assistive technology is told the
list it was reading no longer exists.

So `apply` matches rows by `pull_request_ref`: removals first from the back so
every index stays valid while they are issued, then each item placed where the
core put it, moving rather than replacing. A row that changes bucket keeps its
persistent index and a view keeps the reviewer's place. The test holds three
persistent indexes across a reorder and requires all three to survive and to
follow their rows.

### Decisions worth stating

**Re-applying an unchanged queue emits nothing at all.** A model that re-signalled
on every refresh would make a periodic update look like constant activity, and
a view would repaint on a schedule for no reason.

**The reason is the core's sentence.** A model that composed its own explanation
would be deciding policy while looking like presentation.

**The rule is named only when it is why the item is here.** Saying "you watch
src/parser" about a review someone asked for would bury the fact that they were
asked, which is the one thing the reviewer most needs to know.

**Clearing removes rather than resets**, for the same reason `apply` moves
rather than replaces.

**There is no mutating entry point** beyond `apply` and `clear`. No row is
editable and `setData` is not implemented, so the presentation cannot enroll,
depart, or re-bucket anything — there is no method by which it could.

### A wrong assertion, corrected

The first version of the removal test expected the survivor of a removal to
emit a move. It does not: removing row 1 from three rows already shifts row 2
into place. The implementation was right and the assertion was wrong, and a
model that emitted the move anyway would be telling the view about work it had
already done.

### Evidence

Eight tests. Four breaks, each landing on its own assertion:

| Break | First failure |
|---|---|
| a reorder becomes remove-and-insert | `nothing was inserted` |
| every row is reported changed | `and exactly one row's content changed, the one that did` |
| the rule is named on assigned items too | `an assigned one does not, so being asked is not buried` |
| clear resets instead of removing | `clearing does not reset` |

### What is not here

Filters and sorting presentation. The card names them; the model presents what
the core ordered, and a filter is a view concern that would sit above this
rather than inside it. Nothing was built that only pretends to filter.
