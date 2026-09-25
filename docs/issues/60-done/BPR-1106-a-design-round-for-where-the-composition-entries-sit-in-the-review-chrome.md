---
schema: "implement-rfc.card.v1"
id: "BPR-1106"
title: "A design round for where the composition entries sit in the review chrome"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin":"owner note on attended candidate BPR-0653-entry-row-v001, session cbd48b91000b00f1e72e1ad62c7c384c, decision 007a6eb5-adf3-427c-8473-547f9d0c4d9e, 2026-09-24","origin_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0037"],"related_cards":["BPR-0653","BPR-0929","BPR-0940","BPR-0795","BPR-1036","BPR-1021","BPR-0737"],"reservation":"docs/issues/_reservations/BPR-1106--BPR-1110--72437bc3b205.json","disposition":"Done: the owner's scaffolding decision is recorded here; RFC-BPR-0052 (BPR-1129 to BPR-1135) implements it; RFC-BPR-0028 closes on what it built","owner_decision":{"date":"2026-09-25","session":"7083474240820608cdf9ff551ddd298f","candidate":"BPR-1106-placement-v001","decision":"ece7a6f3-3e54-459f-9c41-eba06732a91d","selected":"Other: discuss outside the attended review flow in a separate session"},"owner_design_decision":{"date":"2026-09-24","session":"Claude Code design session e76905d2-b5a2-4e58-8edd-c07bdcacd7a8, the separate session decision ece7a6f3 asked for","design":"docs/design/BPR-1106-scaffolding-v8.html","rounds":["docs/design/BPR-1106-composition-entries-v2.html","docs/design/BPR-1106-scaffolding-v4.html","docs/design/BPR-1106-scaffolding-v5.html","docs/design/BPR-1106-scaffolding-v6.html","docs/design/BPR-1106-scaffolding-v7.html","docs/design/BPR-1106-scaffolding-v8.html"],"open":"none: the default fold (folded), the keys (as proposed) and where the work lives (an RFC of its own) were answered on 2026-09-24","revised":"2026-09-24, twice: the review bar reads Updates and the comparison first, then the tab's controls (v7); open, the timeline sits just below the bar (v8)"},"successor_rfc":"RFC-BPR-0052"}
acceptance: [{"criterion":"AC1 The design page shows at least three placements with the current row as the baseline, each with its stated costs, rendered and inspected headless at both sizes in both appearances","outcome":"pass","revision":"4e9037e6","evidence":{"kind":"artifact","detail":"docs/design/BPR-1106-composition-entries-v1.html: five placements on the accepted RFC-BPR-0028 full review surface (A the current row as baseline, B the strip's review menu, C the Updates line's right end with the readiness wording in its track, D the conversation drawer's foot, E a Compose control in the strip), each with its height from the comparison, Windows strip width at the 902 floor, clicks to each entry, keyboard reach and readiness line, drawn to scale and side by side. 50 frames from docs/design/BPR-1106-composition-entries-frames-v1.html (every placement at 1280x852 and 1920x1080, light and dark, macOS and Windows chrome, plus loading at 1280x852 on macOS in both appearances) and 8 close-ups under docs/verification/BPR-1106-composition-entries/, rendered with Chrome --headless=new --screenshot at exact window size and inspected; the page itself rendered headless at 1360 px and inspected. No product source changed.","path":"docs/design/BPR-1106-composition-entries-v1.html"}},{"criterion":"AC2 (form amended by the owner's decision ece7a6f3, 2026-09-25: recorded from the owner's own design session rather than one attended candidate) The owner's choice is recorded here","outcome":"pass","revision":"053abefe","evidence":{"kind":"observation","detail":"The owner's answers in design session e76905d2-b5a2-4e58-8edd-c07bdcacd7a8, 2026-09-24, verbatim in the section 'Owner decision, 2026-09-24: the review workbench's scaffolding': modality right; a review bar with the active tab's controls on its left and a collapsible Updates line fully above it when expanded; T; icons with tooltips for Compose review, Comment and the progress control; Comment in the review bar; and RFC-BPR-0025's tabs brought back. Drawn as docs/design/BPR-1106-scaffolding-v6.html from 13 frames under docs/verification/BPR-1106-scaffolding-v6/. Not fingerprint-bound: decision ece7a6f3 took the question out of attended review. The default fold and the keys were answered afterwards, with where the work lives (section 'Where it lives, the default fold and the keys').","path":"docs/issues/60-done/BPR-1106-a-design-round-for-where-the-composition-entries-sit-in-the-review-chrome.md#owner-decision-2026-09-24-the-review-workbenchs-scaffolding"}},{"criterion":"AC3 Whatever the owner chooses is reflected in RFC-BPR-0028 AC-01 and its chrome paragraph, and any implementation is carded, not done on this card","outcome":"pass","revision":"053abefe","evidence":{"kind":"artifact","detail":"The owner chose an RFC of its own. RFC-BPR-0052 carries the decided scaffolding: eleven acceptance criteria, a closeout card and six Backlog slices, BPR-1129 to BPR-1134 (with BPR-1135 closing it). It supersedes RFC-BPR-0028's chrome where they conflict and schedules RFC-BPR-0025's tab contract. RFC-BPR-0028 AC-01 keeps the owner's 2026-09-24 wording, so RFC-BPR-0028 closes on what it built; its current-state section, chrome paragraph and non-goals name the successor. RFC-BPR-0025's status and docs/ux/workbench-specification.md say the same. No implementation was done on this card.","path":"docs/rfcs/RFC-BPR-0052-review-workbench-scaffolding.md"}}]
---

# BPR-1106: A design round for where the composition entries sit in the review chrome

## Why

On 2026-09-24 the owner answered [[BPR-0653]]'s ask 3 in attended session
`cbd48b91000b00f1e72e1ad62c7c384c` (packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
candidate `BPR-0653-entry-row-v001`, decision `007a6eb5`, 18:54:09Z). He chose
to amend RFC-BPR-0028 AC-01 so that it admits the entry rows between the strip
and the epoch line, and added, verbatim:

> We should do a UX design iteration with the /frontend-design skill to figure
> out where best to put these. For now you can ammend the card but open a
> follow-up for the ux design.

The amendment is landed in
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md) AC-01 and
its chrome paragraph. This card is the design iteration.

## What sits there today

`build_central_surface` (`src/adapters/qt/src/workbench_window.cpp`) stacks,
between the strip and the epoch line:

- the composition entry row from RFC-BPR-0037 (`cc038748`, [[BPR-0929]]):
  Compose review and Draft comment or reply, 20 logical pixels on Linux and
  Windows and 22 on macOS;
- while a pull request loads, [[BPR-0795]]'s readiness line.

The inputs to the design:

- The owner's [[BPR-0246]] space budget: every surface other than the code
  panes and their gutter is on a strict budget in the review state. The
  UIforETW course measures the comparison at 842/1000 of the window on Linux
  and 835/1000 on macOS with the row, and 862 and 857 without it
  ([[BPR-1036]]).
- On Windows the strip is the title row and fits at its 902-pixel floor with
  nothing to spare ([[BPR-0653]]), so it cannot simply take two more controls.
- RFC-BPR-0037's own layout study put the Compose review entry "in toolbar"
  (`conversation-surface-layout-study.md`, its Read row).
- [[BPR-0737]]'s direction put the epoch line directly under the strip, above
  the drawers and their headings. The amendment keeps it above the drawers.
- The strip's existing review controls menu was one option on the ask. It
  costs the strip no width.

## Scope

- Run a design round with the `frontend-design` skill: at least three
  placements drawn on the accepted full review surface
  ([RFC-BPR-0028 full review surface](../../design/RFC-BPR-0028-full-review-surface-v1.html)),
  light and dark, at 1280x852 and 1920x1080, on macOS and Windows chrome. The
  current row is one of them, as the baseline.
- For each placement, state what it costs: height taken from the comparison,
  strip width at the Windows floor, clicks to Compose review and to Draft
  comment or reply, keyboard reach, and where the readiness line goes.
- Put the placements to the owner as one attended question: which placement,
  or the current row.
- If the owner picks a placement other than the current row, amend
  RFC-BPR-0028 AC-01 and the chrome paragraph again, and card the
  implementation, including [[BPR-1036]]'s floor and [[BPR-0940]]'s window
  course assertion. If the owner keeps the row, record that here and close.

## Acceptance

- AC1 The design page shows at least three placements with the current row
  as the baseline, each with its stated costs, rendered and inspected headless
  at both sizes in both appearances.
- AC2 The owner's choice is recorded here, one question on its own candidate,
  with the fingerprint-bound verdict.
- AC3 Whatever the owner chooses is reflected in RFC-BPR-0028 AC-01 and its
  chrome paragraph, and any implementation is carded, not done on this card.

## Boundaries

Design only: no product source changes on this card. The core decides, the
host performs, Qt presents; a placement that needs a new command or state is
named as such for the implementation card. This card is not linked to
RFC-BPR-0028 and does not hold [[BPR-1021]]'s closeout.

## Design round v1, 2026-09-24 (bpr-main-mac)

The round ran with the `frontend-design` skill. Nothing under `src/` or
`tests/` changed.

- **The design page**:
  [BPR-1106-composition-entries-v1](../../design/BPR-1106-composition-entries-v1.html).
  It holds the question, the five placements at a glance and close up, the
  costs to scale, the costs side by side, each placement's frames and what
  it would need, and the proposal.
- **The frames**:
  [BPR-1106-composition-entries-frames-v1](../../design/BPR-1106-composition-entries-frames-v1.html).
  It is one window drawn with the accepted full review surface's tokens,
  strip, Updates line, drawers and threads, and the query picks the placement,
  appearance, platform, size and state. Any two frames therefore differ only in
  the placement.
- **The renders**: `docs/verification/BPR-1106-composition-entries/`, 58
  files. Each placement is drawn at 1280x852 and 1920x1080, light and dark,
  macOS and Windows chrome: 40 frames. Each is also drawn while a pull request
  loads, at 1280x852 on macOS in both appearances: 10 more. The other 8 are 1:1
  close-ups cut from those frames. Headless Chrome (`--headless=new
  --screenshot`) rendered every frame at exactly its window size, device scale
  1. The frames were then reduced to 256-colour PNGs (12.8 MB became 4.9 MB).
  The frames and the page itself were rendered and looked at. Two fixes came
  from that inspection: the strip's left group overran the modality on
  Windows at 1280, and D's foot did not fit the 280-pixel drawer with icons.

### The five placements

- **A, the row as built (baseline).** Compose review and Draft comment or
  reply on their own row between the strip and the Updates line.
- **B, the strip's review menu.** The Private progress button keeps its place,
  its 55 pixels and Alt+Shift+4. Its menu gains a "Your review" section above
  Seen, Read, Accept and Exclude.
- **C, the Updates line's right end.** The two labelled buttons follow
  "Comparing … ▾" after a divider. While a pull request loads, the readiness
  wording and its bar take the track's place.
- **D, the conversation drawer's foot.** A bar at the bottom of the drawer.
- **E, a Compose control in the strip.** Pen and arrow before Review details.
  A click composes a review, and the arrow offers Draft comment or reply.

### What each costs

| | A | B | C | D | E |
|---|---|---|---|---|---|
| Height from the comparison | 22 px macOS, 20 Windows and Linux, every mode | 0 | 0 | 0 (the drawer's thread column gives 36) | 0 |
| Strip width at the Windows 902 floor | 0 | 0 | 0 | 0 | +59 (55 + 4): floor to 961 or a new yield |
| Clicks to Compose review | 1 | 2 | 1 | 1, or 2 with the drawer hidden | 1 |
| Clicks to Draft comment or reply | 1 | 2 | 1 | 1, or 2 with the drawer hidden | 2 |
| Keyboard reach | Tab stops after the Updates line's controls | Alt+Shift+4, then C or D | A's Tab stops, after "Comparing" | Alt+Shift+2, then the drawer's last Tab stops | a strip Tab stop before Review details |
| Readiness line | its row above the entry row, about 28 px | its own row, as today | inside the Updates line, no row comes or goes | its own row, as today | its own row, as today |

Where the numbers come from:

- **Heights.** These are the mock's own heights, and its row heights match the
  product: 22 on macOS and 20 on Windows (BPR-1036, BPR-0653). At 1280x852 on
  macOS the comparison is 746 with the row (875 per mille) and 768 without it
  (901).
- **Windows width.** The floor numbers are BPR-0653's measurements of the
  product. E's 55 pixels are the Private progress button's width as measured
  by BPR-1036's capture.
- **The track.** C takes 303 pixels of the Updates line's track at every
  width, measured on the mock: 1000 becomes 697 at 1280.

### Proposal and the prepared ask

The page proposes **C**:

- the comparison gets its pixels back;
- nothing is added to the Windows strip;
- each entry keeps one click and a visible label;
- the readiness line gets a home that does not move the window when loading
  ends.

The runner-up is D. B is the cheapest to build but hides both entries. E
spends width the Windows strip does not have.

The ask is prepared for one attended candidate, `BPR-1106-placement-v001`.
Nothing has been asked yet, and no session was started. The page is the
candidate's evidence. The question, verbatim:

> Where do Compose review and Draft comment or reply go? A, keep the row as it
> is; B, the strip's review menu; C, the right end of the Updates line (the
> loading line moves into it too); D, the foot of the conversation drawer; or
> E, a Compose control in the strip. Proposed: C.

Whatever the owner picks, it is recorded here with the fingerprint-bound
verdict (AC2). If the pick is B, C, D or E, RFC-BPR-0028 AC-01 and its chrome
paragraph are amended again, and the implementation is carded (AC3). That card
carries BPR-1036's floor (+22 on macOS, +20 on Linux) and BPR-0940's window
course assertion, plus the placement's own needs listed on the page.

## Owner answer, 2026-09-25: settle it outside attended review (attended session 70834742)

Source: attended session `7083474240820608cdf9ff551ddd298f`, packet
[owner-decisions-20260924-scoped-v001](../../verification/reviews/owner-decisions-20260924-scoped-v001/operator-packet.md),
hand-back at sequence 32. Candidate `BPR-1106-placement-v001`, revision
"4e9037e6 (parked a7dd1cd2)", fingerprint `a115198c…` equal to the
prepared candidate's in
[session-identity.json](../../verification/reviews/owner-decisions-20260924-scoped-v001/session-identity.json),
decision `ece7a6f3-3e54-459f-9c41-eba06732a91d`, sequence 28,
2026-09-25T00:14:58Z, `actor: human`: **select** "Other: my answer is in the
note", with this note, verbatim:

> Let's discuss this outside of the attended review flow, give me a handoff
> prompt to tackle this properly in a separate session thread.

No placement is chosen: not A to E, and not the proposed C. AC2 stays
pending, and AC3 waits on it. RFC-BPR-0028 AC-01 stays as the owner amended it
on 2026-09-24. The design page, its frames and the costs above are the
material for the separate session; the coordinator gives the owner its
handoff prompt. The placement is not asked again through attended review.

The card leaves `human-review` and is Parked on
`owner-design-discussion`: it waits on the owner's own design session, not on
an attended queue. Whatever that session decides is recorded here as an owner
decision, and AC3's amendment and implementation card follow from it.

## Owner decision, 2026-09-24: the review workbench's scaffolding

Source: the separate design session that decision `ece7a6f3` asked for,
Claude Code session `e76905d2-b5a2-4e58-8edd-c07bdcacd7a8`, run with the `frontend-design` skill on
2026-09-24. The owner widened the question in the session from where two
buttons sit to the workbench's outer scaffolding, designed around the review
tour pack (RFC-BPR-0039 to 0046) and the Conventional Comments pack
(RFC-BPR-0047 to 0051). Nothing under `src/` or `tests/` changed.

### The rounds

- [Round v2](../../design/BPR-1106-composition-entries-v2.html), with 88
  renders under `docs/verification/BPR-1106-composition-entries-v2/`. It traced
  what the two entries do in the product and drew four placements over the
  rows the product builds: a 50-pixel Updates line with its caption, and the
  comparison's own status line. v1 drew the asset's 34-pixel line, so its
  heights ran about 39 pixels high.
- Round v3 drew tray headers without tabs. It was superseded in the session
  by v4 and is not kept.
- [Round v4](../../design/BPR-1106-scaffolding-v4.html), with 66 renders under
  `docs/verification/BPR-1106-scaffolding-v4/`. Tray headers, RFC-BPR-0025's
  document tabs, and two ways to hold the Updates line: full width (T) or a
  vertical rail (V).
- [Round v5](../../design/BPR-1106-scaffolding-v5.html), with 59 renders under
  `docs/verification/BPR-1106-scaffolding-v5/`. The modality's place, icons
  with tooltips, three homes for the active tab's controls, and a fold for the
  Updates line.
- [Round v6](../../design/BPR-1106-scaffolding-v6.html), with 13 renders under
  `docs/verification/BPR-1106-scaffolding-v6/`. The owner's answers, drawn.

Every frame was rendered by headless Chrome (`--headless=new --screenshot`) at
its exact window size, device scale 1, from the round's frames file and its
query. Each was reduced to 256 colours and looked at. Every height quoted is
read back from the rendered page's own layout at the same viewport. The
Windows frames use the Mac's type, so their widths are indicative.

### What the owner said, verbatim

After v2, choosing to widen the question:

> Now let's take a step back because I feel like we've got a UX that is, yes,
> basically what I asked for but I think it's too busy in that title bar now
> and it's getting distracting and overwhelming. I do love maxing out the
> space available to the review surface itself but let's consider what can be
> done. And also consider that the review tour creation and wizard rfc pack
> and the conventional comments rfc pack is coming up so we may as well design
> the UX around both of those.

> From what was shown I like C best but I want to consider what can happen if
> we open up the design to the whole outer UX scaffolding (including possibly
> moving things into tray headers like codex desktop see top right side of
> screen grab)

> We could also consider a vertical epoch, there is some precedent for that
> also in the codex desktop app

On RFC-BPR-0025's browser-style tabs, which RFC-BPR-0028 had made a non-goal:

> Let's update the design and incorporate the tabs, i think I set that aside
> to get something up and working but now that we have something more or less
> functional I want to bring them back.

After v4:

> So I think it's clearly between T without caption, T comment in tray, and V
> Tabs

The same message went on to ask for the modality at the left, the right or
the centre rather than floating; for icons and tooltips on Compose review and
Comment; for other places for the tab's controls; and for a disclosure arrow
that folds the Updates line.

After v5, the decision:

> 1. For modality lets do Right.
> 2. Tools I like the idea of Review bar with tools and collapsable updates
> line, but controls should be on the left and updates line should be fully
> above tools when expanded.
> 3. T
> 4. I like the icons + tooltips, I think mark as read in tab tools can also
> be an icon with tooltip.
> 5. Review bar

That message ended with "So I". The owner then said, "there was no rest of
the message, that was a snippet i forgot to delete".

### What is decided

- **The title row** keeps Home, the drawer toggles and the pull request's
  name on the left. The modality sits at the right, beside refresh, the
  comments toggle, account and settings. The new-comment count becomes a badge
  on the comments toggle.
- **The Updates line** stays full width (T) and drops its permanent caption.
  "Known updates · display order is not push order" becomes the line's
  tooltip, and the line shows a mark only when history is incomplete, which is
  what RFC-BPR-0029 requires it to say. It folds. Open, it sits fully above the
  review bar, and it shows the active tab's pair.
- **A review bar**, full width and always shown, sits under the Updates line.
  On its left are the active tab's controls: previous and next change with the
  position, previous and next file with the position, and private progress as
  an icon whose tooltip names this file's state. After them is the Updates
  fold. Folded, the bar names the comparison and its dates. On its right are
  Compose review, as an icon with the kept-draft mark and a tooltip, and
  Comment, as an icon whose arrow offers the Conventional Comments labels.
  Review details moves into the comparison's menu.
- **Document tabs**, RFC-BPR-0025's, fill the comparison's header. Each tab
  holds a file, a pair of updates and a reading position. New tabs take the
  window's comparison, and a tab with its own pair says so on its label. That
  is RFC-BPR-0025's behaviour, which the owner approved on 2026-09-04; the
  session drew it that way from v4 on and the owner kept it.
- **The composition entry row goes.** "Draft comment or reply" becomes
  Comment, and every thread card offers Reply. Beside the code, a card offers
  Comment here today, which starts a second thread rather than answering this
  one.
- **The right tray** takes tabs in its header: Comments, Tour and Findings.

### Measured, at 1280×852

| | macOS | Windows |
|---|---|---|
| Today, the entry row as built | 707 | 712 |
| Decided, the Updates line open | 710 | 712 |
| Decided, the Updates line folded | 744 | 746 |

At 1920×1080 on macOS the three are 935, 938 and 972. At the Windows 902
floor the title row shows the pull request's name ("UnrealEngine #14366") and
the whole modality words. Today it shows neither.

### Not decided

- **Whether a new window starts with the Updates line open or folded.** It
  was asked and not answered. The proposal is folded, since the bar still
  names the comparison and its dates. Each window then remembers its choice,
  and nothing folds or opens on its own.
- **The keys** proposed in the rounds: ⌥R Compose review, ⌥C Comment or
  Reply, ⌥] and ⌥[ the next and previous tour stop, ⌃Tab and ⌃⇧Tab the next
  and previous tab, ⌘W Close tab, and Esc back from a page. They were put to
  the owner in v2 to v4 and never answered. They stay proposals for the
  implementation.

### What follows (AC3)

AC-01 now admits the entry row. The decided scaffolding removes that row and
adds tabs, which RFC-BPR-0028 names a non-goal. So where the implementation
lives decides what RFC-BPR-0028 says:

- amend AC-01 and the chrome paragraph now, and card the work under
  RFC-BPR-0028, so that its closeout ([[BPR-1021]]) waits for the build, tabs
  included; or
- keep AC-01 as amended on 2026-09-24, record this decision in RFC-BPR-0028
  as its successor, and put the scaffolding in an RFC of its own that
  supersedes RFC-BPR-0028's chrome where they conflict, as RFC-BPR-0028 did for
  RFC-BPR-0025.

That question was put to the owner next; the answer follows.

## Where it lives, the default fold and the keys, 2026-09-24

After the decision was recorded (`666daf1c`), three questions went to the
owner in the same session. Answers, verbatim:

- "Where should the decided scaffolding (review bar, fold, tabs, modality
  right) be implemented?" **"Its own RFC (Recommended)"**. The option read:
  RFC-0028 closes on what is built (AC-01 as amended 2026-09-24) and records
  this decision as its successor. A new RFC, ordered after RFC-0038 and before
  the tour pack, supersedes RFC-0028's chrome where they conflict and holds
  the cards.
- "When a window opens for the first time, should the Updates line start
  open or folded?" **"Folded (Recommended)"**.
- "Should the proposed keys go into the implementation?" **"Accept as proposed
  (Recommended)"**: Option-R Compose review, Option-C Comment or Reply,
  Option-] and Option-[ the next and previous tour stop, Ctrl+Tab and
  Ctrl+Shift+Tab the next and previous tab, Cmd-W Close tab (the window moving
  to Cmd-Shift-W if Cmd-W closes it today), and Esc back from a page.

So:

- [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md) is
  registered, Proposed, after RFC-BPR-0038 in the index. Its slices are
  [[BPR-1129]] (the title row and the review bar), [[BPR-1130]] (the fold and
  the caption), [[BPR-1131]] and [[BPR-1132]] (the tabs, in the core and the
  host, then the strip), [[BPR-1133]] (Comment and Reply) and [[BPR-1134]]
  (the right tray's tabs). [[BPR-1135]] closes it.
- RFC-BPR-0028 AC-01 keeps the owner's 2026-09-24 wording, so this card no
  longer amends it again. Its current state, its chrome paragraph and its
  tabs non-goal now name RFC-BPR-0052 as the successor. RFC-BPR-0025's status
  and `docs/ux/workbench-specification.md` say the same. AC3 passes on that
  answer: the choice is reflected where the owner chose it to live, and the
  implementation is carded, not done here.
- One point is carried to [[BPR-1130]] rather than decided. The rounds moved
  the loading line's words into the Updates line, but the owner was never
  asked that separately, so BPR-1130 asks before it removes [[BPR-0795]]'s row.

The card is Done.

## Revision, 2026-09-24: Updates first in the review bar

Having seen round v6 (`830fa6a9`), the owner changed the review bar's order.
Verbatim:

> I changed my mind lets move it back to updates -> tab tools

Asked which arrangement that meant, the owner answered, verbatim:

> I just meant we have the review bar which includes the tab tools and the
> update labels is to the left of the tab tools and the expanded review bar
> still has the full width update line above the update summary and tab tools

So the review bar reads, from its left:

1. the Updates disclosure, with the comparison and its dates;
2. previous and next change, previous and next file, and private progress;
3. Compose review and Comment, at the right.

It reads the same open or folded. Open, the timeline runs alone across the
whole window above it. Its label and "Comparing …" menu are in the bar, so the
track gains the width they took: 1,256 pixels at 1280, from 961. The heights
do not change: 710 open and 744 folded at 1280×852 on macOS, 712 and 746 on
Windows. [Round v7](../../design/BPR-1106-scaffolding-v7.html), with 13 renders
under `docs/verification/BPR-1106-scaffolding-v7/`, draws it. RFC-BPR-0052's
AC-03 and its closeout checklist are reworded to match, and so are BPR-1129,
BPR-1130 and RFC-BPR-0028's successor note.

## Second revision, 2026-09-24: the timeline below the bar

Having seen round v7 (`a53ee9cc`), the owner asked, verbatim:

> hmm maybe it makes more sense if the epoch line is below that bar instead of
> above it, what do you think

The answer given was yes, for four reasons:

- The bar no longer moves when the timeline opens or folds. With the timeline
  above it, opening pushed the bar, its disclosure, the tab tools and both
  icons down 34 pixels, out from under the pointer.
- A disclosure opens what is beneath it.
- The bar's summary reads first, and the timeline under it is that summary's
  detail.
- The timeline that picks the active tab's pair sits right above the tabs it
  changes.

The costs named were none in height, and the tab tools sitting one row
further from the tabs while the timeline is open. The owner answered,
verbatim:

> ya do below

So the chrome, open, is the title row, the review bar, the timeline, then the
tabs. Folded, it is the title row, the review bar, then the tabs. The heights
are unchanged: 710 open and 744 folded at 1280×852 on macOS, 712 and 746 on
Windows. [Round v8](../../design/BPR-1106-scaffolding-v8.html), with 13
renders under `docs/verification/BPR-1106-scaffolding-v8/`, draws it.
RFC-BPR-0052's AC-01 and AC-03 and its closeout checklist are reworded to
match. BPR-1129, BPR-1130, RFC-BPR-0028's successor note, the RFC index and
the workbench specification follow.

