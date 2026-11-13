---
id: "RFC-DRAFT-0001"
title: "Comments absorb notes: one comment that is Local, Draft or Published"
status: "Proposed"
metadata: {"origin":"owner request, 2026-11-12 (verbatim below)","inspected_revision":"47a5ed625","inspected_lane":"claude/v0-5-0-prerelease-targets-52265d","companions":["inventory.md","index.html"],"unadopted":true}
depends_on: ["RFC-BPR-0007", "RFC-BPR-0023", "RFC-BPR-0085"]
---

# Comments absorb notes: one comment that is Local, Draft or Published

A portable draft. Nothing here is imported, numbered or adopted; the live RFCs
it would amend are unchanged. The inventory it rests on is `inventory.md` and
the mockups are `index.html`, both beside this file.

## Problem and grounded current behavior

The owner, 2026-11-12, verbatim:

> And additional thing I want to design: I think we should collapse comments
> and notes into just comments. Comments right now have published and draft
> states, this would just be adding a local state. Maybe there's some minor
> complication with a local comment landing on the same row as a draft or
> published ocmment but I think we could deal and then I think we get to
> simplify a lot by dropping notes. You could demote a draft comment to a note
> / local comment so every place that asks both can just become draft.

Observed at `47a5ed625`:

- **Two records for one act.** A private note is a `local_note_record`
  (`src/application/include/bpr/application/review_ledger_port.hpp`) stored as
  a one-turn private thread with side tables for its path and line
  (`private_legacy_note`, `private_note_path`, `private_note_line`). A comment
  being written is a `review_comment_draft`
  (`src/core/include/bpr/core/review_round_trip.hpp`) in `private_draft`. Each
  has its own identity, revision, anchor type, bounds, store calls, panel and
  tests.
- **Two tabs for one place.** The right drawer has Comments and Notes. The
  same line can carry a thread on one tab and a note on the other, with two
  gutter marks.
- **The states already exist in the design language.** RFC-BPR-0085,
  principle 3: "Solid is published. Dashed is yours and not yet published. A
  lock is private and never published." But a draft's card wears the lock
  today (`conversation_open_card.cpp:2004`, `:2714`), and a saved draft that
  is not in the review is captioned "Your note, not published"
  (`conversation_cards_view.cpp:2801`).
- **Many places ask both.** The Agent tab's answers and finding cards offer
  Draft comment beside Save as note; the owner already called that out on
  2026-10-27 (RFC-BPR-0094, Q2: "we may as well just let you turn anything
  into a note or draft comment directly"). Fifteen such places are listed in
  `inventory.md`, section 6.
- **Promotion copies.** Draft comment on a note copies its text to a draft and
  leaves the note (RFC-BPR-0085, "Promoting private material"). Nothing turns
  a draft back into a note; the only ways to quiet "N drafts aren't in this
  review" (BPR-2094) are Add and Delete.
- **Drafts are found; notes are not.** Threads, Overview's Comments column and
  Finish your review list my drafts (BPR-2092 to BPR-2094). Notes appear in
  none of them; Overview has a separate Note column and a Private notes count.

What a note can do that a draft cannot: sit on the pull request or on a whole
file, keep its line across comparisons by the blob it was written on
(`core::place_note`), and stay out of every count that asks to be sent. What a
draft can do that a note cannot: be a reply, cover a range, be in the review,
be published.

## Proposed behavior and boundaries

### Decision

There is one object, a **comment**, and it is in one of three states. Private
notes cease to exist as a separate kind: every note becomes a Local comment,
the Notes tab goes, and every surface that offered a note offers a comment.

| State | Meaning | Drawn |
|---|---|---|
| **Local** | Only on this computer. No action that sends anything ever sends it. | Dashed outline, lock, neutral ink |
| **Draft** | Mine, not sent yet, meant to be. In the review, not in the review, held by a submitted review, or a reply (today's `draft_standing`). | Dashed outline, pencil; amber "Not in review" as today |
| **Published** | On GitHub. | Solid |

The lock means Local and nothing else. A draft stops wearing it.

### Transitions

| From | To | Action | Rule |
|---|---|---|---|
| nothing | Local | **N** on a line; the composer with Local chosen; a keep action where no draft has a place | Saved with Save (⌘↩). |
| nothing | Draft | Comment…, a click in the comments gutter, Reply; the composer with Draft chosen | As today. |
| Local | Draft | **Make draft** | A move: the same comment, text, anchor and identity, now a draft that is not in the review. It replaces today's copying Draft comment. |
| Draft | Local | **Keep local** | A move: it leaves the review if it was in it, keeps its text, line or range and reply target, and stops being counted as a draft anywhere. |
| Draft | Published | Add single comment, the review's Submit, Send for a reply | Unchanged. |
| Published | anything | none | Published is GitHub's. Edit and delete on GitHub stay what they are. |
| Local | Published | none in one step | Make draft first. No Submit, Add single comment, Add all, Send or Publish response sends a Local comment. |

Refusals, each with its reason shown and nothing changed:

- Keep local on a draft a submitted review holds, or one being published:
  refused until that submission's answer is known (the existing held rule).
- Make draft on a Local comment whose line the shown comparison does not place
  (outdated, on another file, on the whole file or on the pull request): not
  offered at its own anchor. With a line selected in the code the action reads
  **Make draft on line N** and re-anchors it there on that press; otherwise it
  says to select a line. This keeps today's rule for a note with no line of
  its own (`private_notes_panel::set_promotion_target`).
- Make draft where the same line (or the same thread, for a reply) already
  holds a draft of mine with words: refused, and it says so. Nothing is
  merged.

Make draft and Keep local are undoable by pressing the other; neither asks.

### Where a Local comment can be

A Local comment has every shape a draft has, and three a draft does not:

| Anchor | Shown | Source of the rule |
|---|---|---|
| A line, or a range, on either side | At its line in Comments, in the line's group | Draft anchor; `place_note` |
| A reply in a thread | Inside that thread's card, after the published posts | Draft reply target |
| A line no longer in the shown comparison (outdated) | After the placed cards, with the sentence saying why, as an outdated thread is | `note_placement_kind::outdated` and its four reasons |
| A whole file | In the Comments drawer's edge chip for the file | `whole_file` |
| The pull request | In the "on the pull request" edge chip (RFC-BPR-0085 D8), and in the Overview drawer's Comments tab | `pull_request` |
| Another file | In the "other files" edge chip; choosing it goes to the file, then the line | `other_file` |

A Local comment is never moved to a nearby line. A line anchor holds in the
comparison it was written on and in any other whose side shows the same blob,
as a note's does today. When a draft is kept local, the blob its side shows is
recorded if its comparison is shown; if not, it keeps its comparison and is
outdated elsewhere with reason `unmatched`.

A draft that is a comment on the pull request or on a whole file is not added
by this RFC (see Q10).

### A Local comment on a line that has a draft or a published thread

The complication the owner named, settled by giving a Local comment exactly a
draft's two shapes:

1. **It is about the thread, or about the line.** Written from a thread's
   reply field, it is a Local reply: it shows inside that thread's card, and
   Make draft makes it a reply to that thread. Written on the line, it is its
   own card in the line's group, and Make draft makes a new comment there. A
   draft kept local keeps which of the two it was.
2. **Order in a card:** published posts in thread order, then my Local
   replies oldest first, then my draft reply, then the reply field. In a
   line's group: published threads, then Local comments, then my draft.
3. **Any number of Local comments** may share a line. Drafts keep today's
   rule (the latest draft at a line, or in a thread, is the one reopened;
   `core::reopen_draft_at`).
4. **One gutter mark a line.** A thread's mark wins, then a draft's, then the
   dashed Local mark. Its tooltip and accessible text name everything on the
   line ("Line 22: 1 thread, 1 local comment, 1 draft reply").
5. **Local never counts as new, unread, open or resolved.** Thread counts,
   the Comments tab's new count and label counters ignore it
   (`feedback_unit_origin`, already excluded).
6. **Following the thread.** A Local reply collapses, resolves from view and
   goes outdated with its thread. If its thread is gone it becomes a Local
   comment at the thread's last known file and line, saying the thread is
   gone, by the rules an unsent reply already follows
   (`core::decide_reply_draft`).

### The composer

One composer writes all three. Its foot keeps one row: where "Private until
you publish" stands today there is a two-segment state control, **Local** and
**Draft** (in a thread: **Local** and **Reply**).

- Draft chosen: the row is today's (Delete, Add single comment, Start a review
  or Add review comment; where both do not fit, the main action's split arrow
  carries Add single comment).
- Local chosen: Delete and **Save** (⌘↩), and the sentence "Only on this
  computer. Never sent."
- Choosing the other segment on a saved comment is Make draft or Keep local.
- Comment…, the gutter and Reply open it on Draft; **N** opens it on Local.
  The choice is not remembered between comments (Q2).
- Enhance, Ask agent, the label helper, Suggest the change (RFC-BPR-0118) and
  Maximize are the same in both states: it is one field.

### One action where there were two

| Place | Today | Proposed |
|---|---|---|
| Agent answer footer | Draft comment, Save as note | **Draft comment**; then Go to draft |
| Agent finding card | Draft comment, Save as note | **Draft comment**; Draft all loses its "noted" case |
| Finding queue | Add to private notes | **Draft comment** |
| A finding, or an answer, that no line can take | Save as note is the only choice | **Keep local** (on the file, or the pull request); then Go to comment |
| Explain this file card | Keep as note | **Keep local** (a Local comment on the file) |
| What changed since my review (RFC-BPR-0116 mockups) | Save as note | **Keep local** (on the pull request) |
| Local PR draft finding card | Keep as note, Keep all as notes (N) | **Keep local**, **Keep all local (N)** (Q8) |
| A note's card | Save, Draft comment (copies) | Save, **Make draft** (moves) |
| A draft's card | Add single comment, Start a review; Delete | the same, and **Keep local** from the state control and its menu |
| Finish your review, a draft not in the review | Add; Delete | Add; **Keep Local** in the row's menu, which takes it off the list |
| Threads row menu | Open in Review, Open in Discuss | adds **Make Draft** or **Keep Local** on my own rows |
| N and Comment… | two keys into two tabs | two keys into one composer, preset Local or Draft |
| Agent private context | Include my private notes; drafts | **Include my local comments**; **Include my drafts** (two opt-ins kept, Q7) |
| Ask agent | on a thread; on a note, recorded by `record_note_press` | on a thread; on my own comment in either unsent state, recorded by one path |
| Enhance settings | "Comments, replies and notes" | "Comments and replies"; `enhance.writing` unchanged |

A draft made from an agent answer is not in the review, so Finish your review
lists it until it is added, published, deleted or kept local. That is the cost
of "every place that asks both can just become draft", and Keep Local on that
row is its one-press answer.

### Finding and counting

- **Drawer tabs** are Comments, Tour and Agent. The Overview's drawer loses
  its Notes tab; pull-request Local comments are in its Comments tab.
- **Threads** lists my Local comments in their files' groups, marked with the
  lock and "Local". The My drafts chip becomes a state filter with four
  choices: **All**, **Published**, **Draft**, **Local**, each with its count;
  Draft carries the amber dot while any draft is not in the review. Like My
  drafts it is a way to find, so it is not remembered, and under Draft or
  Local no other filter hides one of mine. "Find a thread" searches Local
  comments too.
- **Overview's Comments column** shows, for a file, its published chips, the
  drafts chip as today, and a Local chip (lock and count). A folder adds up
  its files. A click or Enter on the Local chip opens Threads on Local at that
  file's first. The Note column and My review's Private notes row go (Q4);
  My review says how many comments are in the review, not in it, and Local.
- **Finish your review** lists pending comments and drafts not in the review
  as today. Local comments are never in either list. One quiet line under
  them says "N local comments stay on this computer" with Show, which opens
  Threads on Local; it is absent at zero (Q5).
- **Insights** renames its Private notes tile to Local comments and keeps
  counting (Q13).

### Migration

One ledger schema step, inside the existing all-version migration transaction
(`migrate_private_review`), from schema 11:

- Every private note becomes one Local comment with the same text byte for
  byte, the same file identity, comparison, path, side, line and blob, and the
  same order. Nothing is re-anchored, merged or dropped.
- **Anchors that no longer map** are kept as they are and shown by the rule
  above: a note with no path says so as it does today; a line note whose blob
  no comparison shows is outdated with its reason; a note written before
  schema 10 is on its whole file.
- **Identities.** A note's identity is kept as the migrated comment's legacy
  identity, so what points at a note still arrives: a finding card's or an
  explain card's Go to note (`agent_finding_card_memory::Note`), and an
  Agent conversation asked about a note. Those read Go to comment.
- **History.** Private history turns recorded from a note (Ask agent) stay
  where they are.
- **Bounds.** Local comments keep the note bound of 4,096 a review and do not
  count against the 64 active drafts a review may hold. A comment's body bound
  is the draft's (64 KiB) in every state, so Keep local never truncates; the
  agent supplement's own 16 KiB bounds apply to what it is sent, as today.
- **One authority.** After the step, one store answers what unsent comments I
  have. `store_note`, `erase_note` and `private_ledger_observation::Notes` are
  gone; RFC-BPR-0023's rule that two stores "may not remain competing
  authorities" holds. Which tables carry it is the implementation's.
- An interrupted step repeats safely, and a build older than the step treats
  the newer ledger as it treats any newer schema today.

### Export, import and published contracts

- No private note is exported or imported today, and none is added. Profile
  reset's copy is a copy of the store files and follows the schema.
- Agent and prompt sharing (RFC-BPR-0107) carries no comment text. Its retired
  purpose ID `enhance.note` stays readable as `enhance.writing`.
- The tour context's `context_omissions` keys (`review_notes_not_selected`,
  `comment_drafts_not_selected`) and `bpr draft open`'s JSON `notes` count are
  published; they keep their names (Q11).
- The MCP context helper has no note or draft tool and gains none.

### Boundaries

- **Review Cycle notes** (`I<n>`, rounds, Share notes) are a cycle's feedback
  items and are not touched (Q9).
- **The `note:` label** of Conventional Comments is a label and stays.
- **Local PR drafts** (RFC-BPR-0108) keep their own record and storage. This
  RFC changes their vocabulary and card only, as a last milestone (Q8), and
  does not carry them to the pull request (Q12).
- **Discuss** shows no Local comment, as it shows no line draft today.
- No new GitHub call, no new provider type, no change to what a review sends.

### Architectural allocation

The core decides, the host performs, Qt presents.

- **Core:** the three states and the two moves as revisioned draft commands
  with typed refusals; one anchor that admits a line, a range, a reply, a
  whole file and the pull request; placement generalised from `place_note`;
  the roster's Local count, with `drafts_outside_review` never listing Local;
  the keep action's choice of state from whether a draft has a place.
- **Host:** one store port and the migration; one recording path for Ask
  agent; the supplement admits by state under two opt-ins.
- **Qt:** the Comments drawer, the state control, Threads' filter, Overview's
  chip, Finish your review's line, and the wording.

### Suggested milestones

1. Model, store and migration; the Notes tab reads the new store, nothing
   else looks different.
2. Comments absorbs Notes: cards, the state control, N, the gutter mark.
3. Threads' state filter, Overview's chip and My review, Finish your review.
4. One keep action on the agent surfaces; the wording sweep.
5. Local PR drafts' vocabulary (only if Q8 is yes).

Each of 2 to 4 ends in an owner look (Accept or Reject) on a frozen build
whose fixture holds Local, Draft and Published comments on one line.

## Proposed amendments to adopted RFCs

Live sources are unchanged; these are proposals against the revision
inspected. Each keeps every obligation not named.

| RFC | Clause | Before | After |
|---|---|---|---|
| RFC-BPR-0085 | Principle 3; "Notes (D3)"; "Promoting private material"; the tab list in "Agent" and AC05; Status drawer item 3 | Notes is a tab of private notes; Draft comment copies a note to a draft; My review counts private notes | No Notes tab. D3's margin-note behaviour is Comments' Local state. Make draft and Keep local move. The lock is Local only. My review counts comments by state |
| RFC-BPR-0085 (cards BPR-2092, BPR-2093, BPR-2094, Done) | Threads' My drafts; Overview's drafts chip; Finish your review's line | Drafts only | Threads' state filter; a Local chip beside the drafts chip; Local never listed as "not in this review", Keep Local on its rows, one quiet Local line |
| RFC-BPR-0007 | "Persist local notes"; owner amendments 2026-10-06 (path) and 2026-10-17 (line) | Notes are a ledger record with path and line side tables | The same facts are a Local comment's anchor; the path and line rules are kept word for word |
| RFC-BPR-0023 | "Migration of local notes"; acceptance item 14; the `store_note` allowance | A legacy note is a one-turn private thread; `store_note` may stay | Notes migrate once more into comments; the note store calls are removed; one authority |
| RFC-BPR-0012 | Finding promotion choices | draft comment, defect, private note | draft comment and defect; an unplaceable finding is kept as a Local comment |
| RFC-BPR-0011 | Context supplements: selected notes and drafts, two opt-ins | Notes; Drafts | Local comments; Drafts (same two gates, same bounds) |
| RFC-BPR-0049 | Counter population | Private notes and unsent drafts are excluded | Local and Draft are excluded (wording) |
| RFC-BPR-0094 | Q2 as amended (Draft comment beside Save as note); Q3 and "Notes." (Ask agent on a note) | Two footer actions; a note is recorded by its own press | One footer action; Ask agent on my own unsent comment, one recording path; the chip reads "Your local comment on …" |
| RFC-BPR-0101, RFC-BPR-0102, RFC-BPR-0125 | Save as note; "private notes/drafts" by explicit action; the prompt "Explain the code this note is on" | note wording | comment wording; the explicit-action rule is unchanged |
| RFC-BPR-0116 | A private note is never asked "was this addressed"; Save as note | Note | A Local comment is never asked; Keep local |
| RFC-BPR-0117 | AC03, Keep as note (card BPR-2076, parked on its look) | A private file-level note | A Local comment on the file; Keep local |
| RFC-BPR-0118 | Suggest the change on a draft (card BPR-2078, in progress) | A draft comment's field | The same field in either unsent state |
| RFC-BPR-0086, RFC-BPR-0121 | Private notes tile; "kept as a note" | Notes | Local comments |
| RFC-BPR-0107 | Purpose IDs | `enhance.note` read as `enhance.writing` | Unchanged; wording only |
| RFC-BPR-0108, RFC-BPR-0110, RFC-BPR-0111, RFC-BPR-0113, RFC-BPR-0115 (card BPR-2069, backlog) | The Notes area; "private deliberation (notes, …)"; "what stays private"; Keep as note, Fix on a note, Check the fix | Local-draft notes | Only if Q8 is yes: the same record, called Local comments, with Keep local; Fix and Check the fix unchanged |

Consequences for active work: BPR-2078 keeps its design and gains nothing to
do until milestone 2. BPR-2076's parked look would be asked with Keep local if
this is adopted first, or relabelled after. BPR-2069 is unstarted and would be
written in the new words. RFC-BPR-0116's mockups under the owner's look show
Save as note and would change the same way.

## Open questions for the owner

Each has a recommended default. A default is not an answer; the scope it
governs is unready until answered.

| # | Question | Recommended default |
|---|---|---|
| Q1 | What is the private state called? | **Local**, with the lock and "Only on this computer" (not Private, not Note) |
| Q2 | Which state does a new comment start in? | Draft from Comment…, the gutter and Reply; Local from N; not remembered |
| Q3 | Is Make draft a move or a copy? | A move; today's copy leaves two texts to keep in step |
| Q4 | Overview's Note column and Private notes row? | Retire both; the Local chip and its tooltip carry it |
| Q5 | Does Finish your review mention Local comments? | Yes, one quiet line with Show and no Add |
| Q6 | May a Local comment sit inside a published thread as a private reply? | Yes; a draft reply kept local stays under its thread |
| Q7 | Agent opt-ins: one or two? | Two, as today (local comments; drafts) |
| Q8 | Do local PR drafts' notes join? | Yes, vocabulary and card only, last; storage unchanged |
| Q9 | Review Cycle notes? | Out of scope; they are the cycle's items |
| Q10 | Should a Local comment on a whole file or the pull request be able to become a draft file comment or pull-request comment? | Not in this RFC; Make draft on line N is the way |
| Q11 | Rename the published keys (`review_notes_not_selected`, `notes`)? | Keep the names |
| Q12 | After Create draft PR, do a local draft's comments follow to the pull request? | No |
| Q13 | Insights' Private notes tile? | Rename to Local comments; the history continues |
| Q14 | How many may share a line? | Any number Local; drafts as today; Make draft refused where my draft already stands |

## Acceptance

- **AC01:** The right drawer's tabs are Comments, Tour and Agent, and the Overview drawer has no Notes tab; N on a selected line opens the composer on Local in Comments.
- **AC02:** On a line that has a published thread, a Local reply and a draft reply of mine, one card shows all three in the order defined, each with its state's mark, and the line has one gutter mark whose accessible text names all three.
- **AC03:** With Local comments present on lines, in threads, on a file and on the pull request, Add single comment, the review's Submit, Add all, Send and Publish response write nothing of them: the fixture provider receives no Local body, and each is still Local afterwards.
- **AC04:** Keep local on a draft in the review lowers the review's pending count by one, removes it from every "not in this review" list, and keeps its text, line or range and reply target; Make draft on it returns a draft of the same kind at the same place, not in the review.
- **AC05:** Keep local on a draft held by a submitted review or being published, and Make draft where my draft already stands, are refused with their reasons and change nothing; no action offers a state change on a published comment.
- **AC06:** A Local comment on the pull request, on a whole file, on another file, and on a line the shown comparison does not place each appears where "Where a Local comment can be" says, the last with its reason; none is shown at a line it was not written on, and Make draft on it is offered only as Make draft on line N with a line selected.
- **AC07:** Threads lists Local comments in their files' groups and its state filter's four choices show exactly the rows of that state with matching counts; the filter is not restored on reopening; under Local, Hide resolved and Only new hide no Local comment.
- **AC08:** A file's Comments cell on Overview shows a Local chip with its count beside the drafts chip, a folder shows the sum, the chip's activation opens Threads on Local at that file's first, and Overview has no Note column or Private notes row.
- **AC09:** Finish your review never lists a Local comment as pending or as not in the review; Keep Local on a not-in-review row removes that row; the Local line shows the count and is absent at zero.
- **AC10:** The Agent answer footer, finding card, finding queue and Explain this file card each offer one keep action; it makes a draft where a line can take one and a Local comment where none can, and its follow-up goes to what it made.
- **AC11:** A profile whose ledger holds notes written before and after the path and line side tables opens after migration with one Local comment for each: equal text bytes, equal anchor facts, equal order; a note-era Go to note arrives at its comment; a migration interrupted at any statement and run again gives the same result.
- **AC12:** An agent request carries a Local comment only when the session's Local opt-in is set and a draft only when its drafts opt-in is set, each independently of the other.
- **AC13:** Label counters, thread counts, the Comments tab's new count and Home are unchanged by creating, moving or deleting Local comments.
- **AC14:** Each state is announced in words to assistive technology ("Local comment, only on this computer", "Draft, not in your review", "Draft, in your review"), and the state control, Make draft and Keep local are reachable and operable by keyboard alone.
