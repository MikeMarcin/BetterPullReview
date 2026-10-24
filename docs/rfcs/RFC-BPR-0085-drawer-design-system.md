---
id: "RFC-BPR-0085"
title: "One drawer design system: Notes, Tours, Agent, Compose review and the Overview's Status"
status: "Auto-Verified"
area: "BPR"
depends_on: []
metadata: {"rfc_base_sha":"4317330bad446c4c1577b6d6bf09d458cf8edb9a","mandate_revision":"drawer-design-system-r1","closeout":"BPR-1595 (2026-10-24)","design_card":"BPR-1574","owner_session":"9b6359b5e66fa4d761e1acb025c4697b","milestone_plan":{"id":"drawer-design-system-v1","path":"docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}}
---

# One drawer design system: Notes, Tours, Agent, Compose review and the Overview's Status

## Execution status

Mandate `drawer-design-system-r1` adopts RFC-DRAFT-0023 as decided by the
owner in attended session `9b6359b5e66fa4d761e1acb025c4697b` (hand-back at
event 26, 2026-10-17; every decision `actor: human`). Immutable RFC_BASE_SHA:
`4317330bad446c4c1577b6d6bf09d458cf8edb9a`. The
[live import](assets/drawers-and-agent-redesign-v1.LIVE_IMPORT.md) keeps the
draft's bytes; the [milestone plan](plans/drawer-design-system-v1/milestone-plan.json)
groups the cards:

| Card | Scope |
|---|---|
| [[BPR-1587]] | Shared drawer parts (AC01) |
| [[BPR-1588]] | The Overview's Status becomes a real drawer; no Start review (AC02) |
| [[BPR-1589]] | Status content: attention first, details on demand, my role, unread (AC03) |
| [[BPR-1590]] | Compose review as a sheet over the code, expanding to a focused page (AC04) |
| [[BPR-1591]] | Private notes anchored to lines (AC05, data) |
| [[BPR-1592]] | Notes as margin notes, and the promote label (AC05) |
| [[BPR-1593]] | Tours as the route, expanding to the tour page (AC06) |
| [[BPR-1594]] | Agent as a drawer tab (AC07) |
| [[BPR-1603]] | Pull-request comments wait at the Comments drawer's edge (AC10, amendment) |
| [[BPR-1595]] | Closeout (AC08, AC09) |

Milestone 1 ([[BPR-1587]], [[BPR-1588]]) landed on 2026-10-17 after the
owner's Accept (session `e471b4b8…`). Its settings step was renumbered from
schema 22 to 23 at landing, because [[BPR-1584]]'s reading width took 22 first.

Milestone 2 ([[BPR-1589]]) landed on 2026-10-18 after the owner's Accept of
v002 (session `82f52ad4…`); v001 was rejected for a verbose role line and a
"you may be a member" hedge, so v002 asks GitHub which requested teams the
viewer is in. The owner then decided that pull-request comments get read
state; that is [[BPR-1601]], outside this RFC, and the Status drawer's
conversation line waits for it.

Milestone 3 ([[BPR-1590]]) landed on 2026-10-18 after the owner's Accept of
v003, with the hover words removed as the owner's note asked; v001 and v002
were rejected (dead space; rows larger than the mockup's one line, no way to
the real comment, and a summary that read as the last comment).

Milestone 4 ([[BPR-1591]], [[BPR-1592]]) landed on 2026-10-18 after the
owner's Accept of v001 with no note. The agent's material moved unchanged to a
provisional Agent tab until [[BPR-1594]].

Milestone 5 ([[BPR-1593]]) landed on 2026-10-18 after the owner's Accept
of v001 with no note (session `9a9c8a15…`). Tours read as a route in the
drawer and expand to their page.

Milestone 6 ([[BPR-1594]]) landed on 2026-10-18 after the owner's Accept
of v001 with no note (session `5547b234…`). Agent has its own drawer tab,
opened by its shortcut or Ask about these lines. The accepted candidate's
known gaps remain recorded on its handoff.

The AC10 amendment ([[BPR-1603]]) landed on 2026-10-18 after the owner’s
Accept of direction A built v001 with no note (session `d28b9f71…`, decision
6, handback 7). PR comments and review summaries open from the top-edge
chip into the drawer’s chronological list. Replies use durable private
drafts and explicit Send through the existing witnessed publication path.
[[BPR-1595]] remains open; this amendment does not close the RFC.

The closeout attempt on 2026-10-23 ([[BPR-1595]]) found AC08 unmet. AC01 to
AC07, AC09 and AC10 have their evidence, and the milestone courses still
pass. The new AC08 course renders every rebuilt surface in the four themes
at its narrowest and widest width, at each scale. It finds the same four
defects in every theme. The Comments drawer's tab row hides Tour and Agent
at 760 pixels and at a restored 280. At 280 pixels the open review
summary's labels and the Tour stop's action buttons are cut. In the
900-pixel window the tour page's On GitHub column runs off the window.

Auto-Verified on 2026-10-24: [[BPR-1757]] repaired the four defects
as layout (the tab row takes its new width when the tray is shown, the
scope in the summary's header yields to the verdict, a stop's actions
with no room wait behind More, the tour page's side columns give way
at the window's floor), the AC08 course passes in the four themes at
the three scales, and [[BPR-1595]] closed the RFC with every criterion
recorded.

**Amendment, 2026-10-18 ([[BPR-1603]]).** The owner, while [[BPR-1593]] was
under way: "The notes-on-the-pull-request thing might actually be a good UX
for comments on the pull request too." D8 below adds it as a milestone after
the Agent tab, with its own design note and owner review.

The design round, its audit and the before renders are in
[`docs/design/BPR-1574-drawers-and-agent-v1/`](../design/BPR-1574-drawers-and-agent-v1/index.html)
and on [[BPR-1574]].

## Problem

The owner, 2026-10-17: "I think that comments drawer is beautiful now, but the
notes and tours drawers are pretty ugly, and I think agent probably deserves a
first-class tab, not to be tucked away." The same day: Compose review's left
side ("I don't think I've ever done more than try to ignore it") and the
Overview's side panel ("getting extremely busy … It looks like a drawer but
doesn't behave like the other drawers").

What the audit found, in short (sources on [[BPR-1574]] and in the
[retained draft](assets/drawers-and-agent-redesign-v1/RFC-DRAFT-0023-drawer-design-system-agent-compose-overview.source.txt)):

- **The Comments drawer** ([[BPR-1290]] v005) works because it does one job,
  its header is its tabs, every thread uses one card anatomy, each colour
  means one thing, its notes move with the code, and it uses few words and
  real keys.
- **Notes** nests tabs three deep (Notes › Private history › History, Sources,
  Write) and stacks stock fields and button columns; Agent tools sits inside
  it.
- **The Tour tab** squeezes RFC-BPR-0039's whole wizard into 375 pixels with
  no sight of the code a stop points at.
- **The agent** is reached only through Notes › Agent tools, with no key or
  menu item; its other faces are scattered (composer workshop, Private
  history, tour editor, Review Cycles, Settings).
- **Compose review** shows Discuss's whole thread list beside it, inherited
  from [[BPR-1212]]'s maximized-draft layout, while the page already lists its
  pending comments.
- **The Overview's side column** names a failed check three times, draws rows
  that say —, 0 or not available, uses full-width grey bars, and is a fixed
  widget rather than a drawer.

## Decisions

Each is the owner's, from session `9b6359b5`, verbatim notes quoted.

- **D1, Start review** (chat, 2026-10-17): the Overview has no Start review
  or Continue review button. "Start review is probably unnecessary now that
  the Review tab is so convenient, and double-clicking a file or clicking Next
  file also works." The button only entered Review at the first unseen
  change's file (`enter_review_at(EntryPath)`) and was the page's default
  button; it started nothing and recorded nothing. Both behaviours stay: the
  reviewer's first entry into Review (⌘2, the Review segment) from the Overview
  of a pull request they have not started (nothing read, no earlier review,
  where Start review stood) selects `EntryPath`; a started review keeps its
  restored reading place, and a link or a file row keeps its own target. Return
  on the Overview's Files list opens the selected file.
- **D2, principles:** Accept (`235de4db`).
- **D3, Notes:** A, margin notes (`566b134c`). Note: "I'm looking for something
  a bit better than 'Make it a comment' I initially thought 'Promote to
  comment' but that's probably too long."
- **D4, Tours:** A, the route, with C, the tour page, as its expanded view
  (`337ab961`): "I like A with C as expand view."
- **D5, Agent:** B, a drawer tab (`1f844f88`).
- **D6, Compose review:** C, a sheet over the code, with an expand to A, the
  focused page (`a2ad29f8`): "I think I like C with an expand to A." This
  supersedes the owner's 2026-10-06 answer to [[BPR-1212]]'s Q3 ("A: move to
  Discuss") for Compose review only; a maximized draft still opens in Discuss
  with its list beside it.
- **D7, Overview:** A, attention first (`7ab1ca4c`): "I like attention first,
  but I still want to be able to dig into those and get details, particularly
  people but also checks and the other things. Let's do some sort of
  progressive disclosure. It's also very important to know my role in this
  review. Is my review required? Am I just tagged? Am I the author? Do I have
  no role? Also as part of my review I want to include unread comments count
  and an easy way to go to them (threads or discuss whichever you think is
  better)."

## Behavior

### Principles (every drawer and side surface)

1. **The header is the tabs.** No caption, no purpose paragraph, no tabs
   inside a tab. Tools are icons at the tab row's right; each tooltip names
   its key. Purpose lives in the tooltip and the empty state.
2. **One card anatomy**, the Comments drawer's: a flat closed note (avatar,
   [[BPR-1083]] mark under it, rail in the label's tint, two lines); an open
   card on the panel colour with a shadow, a 3-pixel edge and a notch toward
   its line. Threads, notes, tour stops, findings and pending comments use it.
3. **Ownership at a glance.** Solid is published. Dashed is yours and not yet
   published. A lock is private and never published. A square avatar with a
   `>_` glyph is an agent; its output is dashed until a person promotes it. A
   numbered amber circle is a tour stop. Faces are [[BPR-1583]]'s one avatar
   rule once it lands.
4. **Colour means one thing each.** Amber is where you read (the open thread,
   the tour route); blue is new and where you write; green is done, passing or
   approved; red is blocking, failed or destructive only; everything else is
   neutral ink. Every colour is a theme role, so Light, Dark and both high
   contrast themes follow; no new hues.
5. **Actions.** At most one filled and one outlined button per card, sheet or
   page; the rest are in its More menu and its context menu (Shift+F10). No
   stacked button columns and no full-width grey bars.
6. **States.** Empty is a glyph, one sentence and one action. Waiting,
   loading and failing use [[BPR-1576]]'s one loading-state pattern
   (placeholders, the reveal after 400 ms, the slow wait, the veil, Retry)
   and [[BPR-1575]]'s refresh feedback; nothing new is invented. No row is
   drawn to say 0, — or not available.
7. **Every drawer behaves the same.** It resizes by its edge, collapses from
   its toggle and its key, and remembers its width per window as the files and
   comments drawers do ([[BPR-0662]]). Its tabs are one focus stop and
   Left/Right move between them; Up, Down, Return and Esc move through and open
   its cards; one letter starts its main verb. Each tab scrolls on its own and
   keeps its place. Its toggle is in the View menu (with [[BPR-1580]]'s menu
   once that lands).
8. D1.

### Promoting private material

A private note and an agent finding become a local draft through one shared
action labelled **Draft comment**: the owner's choice at [[BPR-1588]]'s review
(session `e471b4b8…`, decision `bcb5d520`, 2026-10-17, from To comment, Draft
comment and Use as draft). It copies the text to a local draft at the same line
or file, the source stays, and only the draft's own Publish or review
submission sends it; "Post as comment" and "Share" were left out because the
action publishes and shares nothing. [[BPR-1592]] (Notes) and [[BPR-1594]]
(Agent findings) use this label.

### The Overview's Status drawer (D1, D7)

The Overview's right column becomes **Status**, a drawer on the Overview's
right edge: 340 pixels by default, drags from 260 to 560, collapses from a
toggle in its header and ⌥⇧6 (Alt+Shift+6), remembers its width per window,
and keeps its tab row's tools: Prepare review tour (⌥T) and Open on GitHub
(⇧⌘O). It opens with, in order and only when they apply:

1. **My role**, one line: Author; Review requested from you (marked code owner
   when GitHub says so); Review requested from a team (named; "you may be a
   member" when membership is not observed); Mentioned in the description or a
   comment; Reviewed (with the state of my latest review); or No role. Several
   apply at once and the strongest leads (author, then direct request, then
   team request, then reviewed, then mentioned). It never claims my review is
   required beyond what GitHub's accepted assessment says (RFC-BPR-0069's
   rule: configured minimums produce no remaining count); "required" appears
   only as GitHub's code-owner marking and Merge's own conditions.
2. **Needs attention**: conflicts, failed checks (one line with their count
   and names), outstanding requests I am waiting on, replies to me.
3. **My review**: files read of total with a bar, private notes, pending
   comments, and **N threads with new posts** with **Go to them**, which opens
   Threads ([[BPR-1293]]) with Only new on, because Threads lists every
   thread with new posts and opens the picked one in place at its line. New
   pull-request comments that are not on a file are a second line whose Go to
   them opens Discuss on the pull request.
4. **Checks**: one bar of passed, failed and skipped.
5. **People**: faces with their state glyphs and one summary line.

Every section is a disclosure that expands in place to its details:
Needs attention to Merge's conditions and Requirement details; Checks to the
full result list, the selected result's card and Open check details
(RFC-BPR-0068's existing behaviour); People to the full roster with each
person's request kind, latest review state and code-owner marking (RFC-BPR-0069);
My review to Since my last review's facts that have values. A disclosure
remembers its state for the pull request in the window. Each failed check is
named in exactly one place at a time.

### Compose review (D6)

Compose review (⌥R) and Finish review open **Finish your review** as a sheet
from the window's right edge over the current surface, 540 pixels wide, with
the code (or whatever the modality shows) visible beside it. It holds, in
order: the title; [[BPR-1541]]'s waiting panel, unchanged, while GitHub has not
confirmed; the pending comments as rows (file, lines, mark and first line,
Delete that asks first) where choosing a row enters Review at that line with
the sheet still open; the summary; the three verdicts; "Nothing is published
until you submit." with Discard review; "Pull request and account". Esc or ×
closes the sheet and changes nothing else. **Expand** (its header button, and
⌘⇧M as for drafts) opens the same review as the focused page in Discuss: one
column of comfortable reading width ([[BPR-1584]]'s setting when it lands), no
thread list, with an unsent draft or unread replies said in one sentence with
one fix. Restore returns to the sheet with text, caret and scroll kept. All
submission, refusal and unknown-answer behaviour of [[BPR-1345]] and
[[BPR-1541]] is unchanged.

### Notes (D3)

Notes holds only private notes; the promote action reads Draft comment. Each note hangs from its line beside the code
and opens in place, exactly as a thread does, drawn dashed with a lock; notes
on the whole file or the pull request, and on other files, wait in the edge
chips. Writing a note uses the blue edge (where you write). The tab's tools:
New note on the selected line (N) and Notes on the pull request. A note's main
action is Save (⌘↩); its other is the promote action above. The gutter marks a
noted line with a dashed cell. Existing file and pull-request notes keep their
anchors; a new note on a selected line records its side and line against the
comparison it was written on ([[BPR-1591]]), and a note whose line no longer
exists in the current comparison follows the placed ones with a sentence
saying so, as an outdated thread does. Notes' agent material moves to the
Agent tab ([[BPR-1594]]); Notes never shows agent tools.

### Pull-request comments at the edge (D8, amendment)

Conversation comments and review summaries on no file wait as one chip at the
Comments drawer's top edge ("3 on the pull request", its unread count in the
new blue once any is unread), as Notes' notes on the pull request do. The chip
lists them in the drawer, newest last; each opens in place as a thread does,
reads as read once seen ([[BPR-1601]]), and offers Show in Discuss. Discuss
keeps every comment as today. A short design note with a Light and Dark
mockup precedes the build ([[BPR-1603]]).

The owner selected direction A (2026-10-18, session `537cac7b…`) and
confirmed replying from this drawer later that day. A reply first saves a
private draft, retaining the exact post quoted. Explicit Send creates one
pull-request (issue) comment quoting that post: GitHub's conversation is
flat. It reuses the staged-publication transport witness and recovery
journal, rereads the exact PR immediately before POST, and refuses changed
base/head/actor/PR authority (BPR-1560/BPR-1572). Drafts survive restart;
failures keep the text and offer retry. An uncertain send is reconciled
without a second POST. This is the owner's explicit extension of the
original presentation-only scope, not an inferred reply permission.


### Tours (D4)

The Tour tab is the route: the brief (Problem, Solution) as one compact card
that opens in place, then the stops in tour order as numbered amber circles on
an amber route line, a file divider where the route changes file. The current
stop opens in place to write (title, Markdown explanation, related source,
Use selected lines, Ready and next ⌥↩), and its lines wash amber in the code.
Readiness reads from the stop's ring: Ready (green tick), Editing, Needs
attention (amber warning), Suggested (dashed). Drag and Alt+Shift+Up/Down
reorder. The foot holds the private-draft state and Preview and publish.
**Expand** opens the tour page in the main pane (outline with drag handles,
the stop editor with its code excerpt under the same amber bracket, a live
preview of the GitHub comment, Preview and publish), and Back to the code
returns. A reviewer following a published tour sees the same route read-only,
with Previous and Next stop. RFC-BPR-0039 to RFC-BPR-0045's model, publication
and recovery are unchanged; their controls move into this layout.

### Agent (D5)

**Agent** is the right drawer's fourth tab, after Comments, Notes and Tour,
with ⌥⇧A (Alt+Shift+A) to open the drawer on it. ⌘I (Ctrl+I) on selected
lines opens it with those lines attached to the question; the code's context
menu offers "Ask about these lines". The tab shows: a context row (the agent
profile menu, the head it reads and its state, the scope); the conversation
with finding cards (severity, likelihood, category, claim, evidence, the line
it anchors to, the promote action, Ask follow-up, Add to notes, and More with
Draft a defect and Dismiss); and the question field with its context chips.
Sessions, sessions on earlier heads and private runs on drafts (RFC-BPR-0023)
are in its History menu. A finding's lines wash amber when it is selected and
its gutter mark is a square. The four states: not set up (Set up an agent
opens Settings › Agents & context), working (BPR-1576's pattern with Stop),
head moved (Start a new session, Continue on the old head: the core's two
continuations), stopped (what happened, the question kept, Ask again, Show the
log). The agent stays read-only (RFC-BPR-0011, RFC-BPR-0012, RFC-BPR-0060);
nothing it says is published without a person's press. Below 340 pixels the
tabs show their icons only.

### Boundaries

The core decides, the host performs, Qt presents. Roles, unread counts, note
anchors and sheet state are core decisions over accepted facts; widgets gain no
GitHub, Git, ledger, agent, process or network effect. Only [[BPR-1591]] changes
stored data (a backward-compatible line anchor on private notes). Publication,
review submission, check admission, tour publication and agent authority are
unchanged. The accepted Comments drawer and BPR-1541's panel are not changed.
Every slice loads `frontend-design:frontend-design`, renders Light and Dark
offscreen, runs one full suite before landing, and ends in a frozen build and
one attended Accept or Reject per criterion. A slice that touches files another
parked card is changing lands after that card or merges with it.

## Acceptance criteria

- **AC01 Shared parts:** the drawer header, card, ownership marks and empty
  state are one set of Qt components used by every surface below; Qt courses
  render each in the four themes at 100, 125 and 200 percent with negative
  controls (a nested tab, a stacked button column, a non-role colour).
- **AC02 Status is a drawer:** the Overview's side column resizes within its
  bounds, collapses by toggle and key, remembers its width per window across
  restart, and is reachable by keyboard; there is no Start review button;
  ⌘2 from a fresh Overview selects the first unseen change's file and Return on
  Files opens the selected file; the owner answers the promote-label question.
- **AC03 Status content:** the drawer leads with my role and what needs
  attention as specified, names each failed check in one place, draws no
  empty row, expands each section to its details, and Go to them opens Threads
  with Only new (or Discuss for pathless comments); core courses cover every
  role, including several at once and unobserved team membership.
- **AC04 Compose review:** Finish review opens the sheet over the current
  surface with no thread list; a pending row reveals its line behind the sheet;
  Expand and Restore keep text, caret and scroll; BPR-1541's panel and every
  submission outcome behave as before.
- **AC05 Notes:** notes hang at their lines and open in place; a note written
  on a line survives restart and a new head (or is shown as outdated); the
  promote action creates a local draft at the note's line and publishes
  nothing; Notes shows no agent tools.
- **AC06 Tours:** a tour is written, reordered by pointer and keyboard,
  expanded to the tour page and back, previewed and followed in the route,
  with code and explanation visible together.
- **AC07 Agent:** ⌥⇧A, ⌘I and the context menu reach the Agent tab; the four
  states render; a finding becomes a local draft only on a person's press;
  sessions, earlier heads and private runs are reachable from History.
- **AC08 Themes and sizes:** every rebuilt surface renders without clipping in
  the four themes at 100, 125 and 200 percent and at its narrowest and widest
  width.
- **AC09 Owner looks:** the owner accepts each milestone's frozen build.
- **AC10 Pull-request comments at the edge (amendment):** conversation comments
  and review summaries on no file wait as a chip at the Comments drawer's top
  edge, open in place with their unread marks, and offer Show in Discuss;
  Discuss is unchanged; the owner accepts the design note's mockup and the
  frozen build.
