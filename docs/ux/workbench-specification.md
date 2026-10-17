# Workbench Specification

## The daily-workbench redesign

The daily-workbench design registered on 2026-09-04 is implemented and closed
for the scopes reconciled under RFC-BPR-0028:
[Settings and direct sign-in](../rfcs/RFC-BPR-0018-robust-settings-center.md),
[BPR profiles and independent windows](../rfcs/RFC-BPR-0024-bpr-profiles-multiple-windows.md),
[review drawers and navigation](../rfcs/RFC-BPR-0025-review-workspace-navigation-browser-tabs.md)
(document tabs are implemented by
[RFC-BPR-0052](../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)),
and [personal Home and PR URL navigation](../rfcs/RFC-BPR-0026-personal-review-home-pr-url-navigation.md).
Their entry and closeout cards, [[BPR-0592]] through [[BPR-0599]], are Done.
Home, the Accounts & hosts card page, the empty desktop and the Profiles sheet
are a later revision of those surfaces and are described as they behave
(RFC-BPR-0032, RFC-BPR-0033, RFC-BPR-0034). The list on Home shows exactly what
the queue projection supplies; personal discovery fills it with the viewer's
authored and directly requested pull requests of the saved repositories
([[BPR-0988]]–[[BPR-0990]]).

## Architectural interaction contract

- Navigation, epoch selection, current file/change, command legality, and review semantics are functional-core state and policy.
- GitHub/Git/ledger/diff observations and background work are supplied by the application host.
- The main window, drawers, toolbars, actions, Qt models, focus, splitters, scrolling, painting, and accessibility are Qt-presentation-adapter responsibilities.
- Every user gesture becomes a semantic intent, and every bounded state change reaches Qt as an incremental projection or delta. No workbench widget directly calls an effectful service.
- Epoch rows and comparison endpoints use stable authority, catalog-incarnation,
  and anchor IDs. Qt never derives an epoch cause, resolves an alias, checks
  object residency, opens a cache, or infers a fallback.

## Primary layout

```text
┌ Title row: Home | Repositories | Files | name ↗ ······ Overview Review Discuss Threads | Refresh | Comments (n) | Hide drawers | Account | Settings ┐
├ Review bar: ▸ Updates  Comparing …, dates ▾ | File 3 of 40 « » | Changes 3 of 11 ‹ › | progress ▾ ···· (● Review · 2 pending │ Finish review) Compose review | Comment ┤
├ Updates line, while open: the timeline, full width ┤
├──────── file drawer ────────┬──────── comparison workbench ────────┬──── conversation drawer ────┤
│ filters and review map       │ natural-layout panes + DiffLinkMap      │ tabs: Comments | Notes      │
│                              │ one shared vertical scrollbar          │                              │
│                              ├ Line details: current line, base/head ┤                              │
└──────────────────────────────┴────────────────────────────────────────┴──────────────────────────────┘
```

[RFC-BPR-0052](../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)'s
chrome is built in part ([[BPR-1129]], [[BPR-1130]]). Above the comparison sit,
in order, the title row, the review bar and, while it is open, the Updates
line's timeline, with no scroll area and no separate title bar. The **title row** carries the window, the pull
request and the modality: Home, the repositories and files toggles and the pull
request's name at its left; the modality at its right beside refresh, the
comments toggle (which carries the current file's count of threads with new
comments), the
hide-drawers icon, account and settings; on Windows the application menu and
the caption buttons.

**Open on GitHub** ([[BPR-1338]], the owner, 2026-10-08: "I think an open in
browser button next to the pr title would be nice", and "Open in GitHub from
the home page on a pr would also be useful"). A small borderless icon button
sits right after the pull request's name, tooltip "Open on GitHub (⇧⌘O)"
(Ctrl+Shift+O off macOS). It is the window's Open on GitHub action, also first
on the Pull Request menu. It is shown only while an accepted GitHub pull
request's name is, never for a local cycle, so nothing in the row reads as
disabled. On Home the same action and key act on the row the keyboard is in;
each queue row shows the icon on hover and while it is current in a focused
list, in a column the row keeps for it, and its right-click menu holds the
action, as does a Recently viewed row's. A gesture names a pull request,
never an address: the application host admits it against the window's current
publication, or the accepted session's host and account, and builds the
canonical page from the accepted hostname, owner, repository and number. The
system browser opener still refuses anything but an absolute HTTP(S) address
with a host and no userinfo.

Accepted content settles the name, browser button and their width allocation
from the current publication at the existing window width ([[BPR-1418]]).
This includes first acceptance after a loading surface, replacement,
republication and clear; neither a resize nor a Home round trip is required.
A content update that preserves navigation keeps Home, the chosen modality
and selected file; a retained local cycle keeps its own name without the button.

**Modalities** ([[BPR-1295]], owner direction 2026-10-06). The modalities are
Overview, Review and Discuss, on `Ctrl+1` to `Ctrl+3` (⌘1 to ⌘3), set per
modality rather than by position, and Threads on `Ctrl+4` (⌘4) since
[[BPR-1293]]. The owner, 2026-10-06, verbatim: "can we swap discuss to 3 and
conversations to 4". The strip reads in its keys' order: Overview, Review,
Discuss, Threads. Read is merged into Review: every path that entered
Read (an Overview file row, Continue review, Open in Review, Resume, a link to a
file, a tour) enters Review. Review remembers the drawer arrangement it last
had in the window, hidden drawers included, and applies its preset (Files and
Comments open, Repositories as they were) only on the window's first entry and
when ⌘2 is pressed while already in Review (the strip's Review button is the
same gesture). Entering Review any other way, while it shows, changes nothing.
"Continue review enters" is gone from Settings: Start and Continue review both
enter Review, and "A pull request opens in" offers Overview, Review and Last
used, a stored Read opening in Review.

**Threads beside Discuss** ([[BPR-1293]], owner decision 2026-10-06,
amended by the owner's answers the same day: "Hide drawers" and "just keep
discuss for now even after 1294"). The modality was built as Conversations;
the owner renamed it Threads on 2026-10-08 ([[BPR-1341]]: "I'm considering
rebranding conversations as threads", then "Go ahead"), and the Comments
drawer and its "Comments (n)" toggle keep their names. Every other visible
or accessible word for a review thread says thread too ([[BPR-1355]]): the
drawer's list and its column header ("Threads"), Discuss's counts and file
scope, the Overview's counts, chips and Label key, Settings' Overview counts
help, the queue's attention reason and the tour CLI's count summary. Internal
names, persisted keys, fixture names and GitHub API terms keep
`conversation`, and the Notes tab's "AI conversations" (agent chats) are a
different thing. Threads is added and
Discuss is kept: the modalities are Overview, Review, Discuss and Threads, on
`Ctrl+1` to `Ctrl+4` (⌘1 to ⌘4); Discuss keeps ⌘3 and its behaviour and
Threads takes ⌘4 (the owner's swap, above). `Ctrl+J` (⌘J) goes straight to Threads from Overview, Review
or Discuss. The Threads tab provides the same entry; the redundant finder button
at the top of the Comments drawer was removed at the owner's request ([[BPR-1510]],
2026-10-14). It puts the finder of
[prototype v2](../design/BPR-1290-inline-drawer-v2/index.html) in the main
pane, for navigation only: one row per thread on a file, grouped by
file, with its line, its first post's label mark, its first words, an avatar
per post in posting order and its new count, resolved tick or draft lock; a
row branches only where a post has two or more replies that have replies of
their own. Search takes words, a person, a file or a line number; the filters
are Only new, Hide resolved and one label. Picking a row or an avatar enters
Review on that file, opens the Comments drawer if it is closed, and opens the
thread in place at its line with the picked post washed amber and scrolled
into view. ⌘J again, or Esc on an empty search, returns to where it was
entered from. Pull request comments and review summaries that are not on a
file are not listed; Discuss still reads them ([[BPR-1294]]). The drawers are
hidden while Threads shows, as Discuss hides them, and nothing changes
their states: Review comes back with the arrangement it last had, hidden
drawers included, and Discuss's preset is not applied. Its segment's tooltip
names both keys ("Find a thread in the pull request (⌘4, ⌘J)"); the
drawer toggle is named the Comments drawer, so only the modality is called
Threads. The finder's own words follow: its search reads "Find a thread", its
filters "Only threads with new replies" and "Hide resolved threads", and its
View menu item, on the window and on the empty desktop's menu bar, is Threads. Every other segment's tooltip names
its key too ("Read the pull request overview, with every drawer closed (⌘1).",
[[BPR-1324]]), so a segment that has dropped the key beside its name still
offers it. The segments drop those keys below 1100 points wide, and on Windows
below 1300, where the title row also holds the menu control and the caption
buttons: the whole pull request name, title included, fits at 1280 on every
platform.

**Open in Discuss from Threads** ([[BPR-1356]], the owner, 2026-10-09: "Do
it"). A click or Return still opens a thread in Review. Each thread row also
offers Discuss: a small **Discuss** button shows at the end of its words on
the row under the pointer and on the cursor's row, never on a file's header
or on my draft, and takes the words' room so the people never move. Its
tooltip, anchored right below it, reads "Open in Discuss (⇧↩)". Shift+Return
(keypad Enter too) opens the cursor's thread, or the post under the cursor,
in Discuss. A right click on a row, or the menu key or Shift+F10 on the
cursor's row, opens the row menu: **Open in Review** (the default, drawn
bold) and **Open in Discuss**, each showing its key (↩, ⇧↩ on macOS). The
finder reads those keys itself; the menu's shortcuts are bound to the menu
and never act from the window, and no window command holds them. In the
search, the menu key and Shift+F10 keep the field's own menu. Discuss opens
with the thread's card open at the picked post, washed amber as the Review
pick is, selected, scrolled into view and holding the keyboard; Review's file
does not change. The core routes the pick (`route_workbench_thread`): the
window reports the destination asked for, whether the row is my draft, and
which of Discuss's filters would hide the thread (File scope on another
file, Only new on a thread with nothing new, Hide resolved on a resolved
thread, an Overview count's list that leaves it out). Discuss turns off
exactly those and no other, and its notice says so: "To show this thread,
Discuss turned off File scope, Only new and Hide resolved." The notice goes
when the reviewer changes a filter or picks again. My draft is written beside
its line and Discuss does not list it, so its menu offers Open in Review
alone, and Shift+Return on it opens Review and says why in the status bar.

As built ([[BPR-1293]]): the page is `conversation_finder`, which reads the
forest the Comments drawer reads, the ledger's read state and [[BPR-1087]]'s
label lists, and decides nothing. A conversation's label is its root's, as the
Overview's chips count it; the label menu counts the conversations on a file,
and a post carrying the chosen label is ringed in its tint while the others
fade. A row folds its oldest read posts into "+n" past nine; new posts, the
path to the picked post, the search's and the label's never fold, and "+n"
unfolds the row. The keyboard is the prototype's: typing goes to the search, Up
and Down move between rows (Up from the first returns to the search), Left and
Right walk a row's posts with a preview of each or fold and unfold a file, and
Return picks; hovering a post's avatar previews it. Entering focuses the search
with the cursor on the conversation open in Review, its path from the first
comment to the picked post traced in amber; the search, the filters, the folded
files and the cursor are kept in the window, in memory only. A pick waits for
its file's comparison before it opens the thread, so the drawer's notes are that
file's; the picked post is then brought into view once the code has settled,
the foot of a thread taller than what is left below its line coming from the
drawer's own overflow. My unpublished comment is a row with a lock on its
file's group. Three entries land the same way a pick does: a
`#discussion_r<id>` link the read identified, the reveal of an inline comment
just published (Review stays, the thread open at the published post), and
Previous or Next defect for a thread the drawer does not hold. A
`#pullrequestreview-` link, an older-pair tab's Show them and the review page's
preflight rows land where they did.

**Open in Review from Discuss** ([[BPR-1383]], [[BPR-1384]], owner request,
2026-10-10). An open thread has an explicit **Open in Review** button, with
its filename:line beside it and the full path in the tooltip and accessible
name. The same action is in the collapsed or open thread's context menu.
It enters Review on the thread's file, opens the Comments
drawer and selects its Comments tab. The exact thread opens at its first post
beside the source line once that file's comparison is ready, using the same
pending pick as Threads. This also works when the Comments drawer was closed,
Notes was selected, or a different file was shown. The menu keeps unavailable
anchors and PR-wide comments disabled with their existing explanation.
Right-clicking an individual comment or reply body, or opening its keyboard
context menu, also offers **Open in Review** in rendered and source view.
That gesture selects the exact post that was clicked, including a reply,
rather than the first post. Its disabled action explains a missing current
code anchor; draft bodies do not offer it. Review's own body menu stays the
native reader menu.
The **review bar** is full width under it and always shown with a review: from
its left the Updates disclosure, the comparison's menu naming the comparison and its
dates, File and its position before the file arrows, Changes and its position
before the change arrows, **Accept** as a named one-click file action, and
private progress as an icon whose tooltip names the file's state. Accept uses
the selected exact file material and becomes **Revoke acceptance** after the
saved receipt arrives. It is disabled while no comparison is selected, private
progress is unavailable, or the file is excluded. The adjacent state menu keeps
Seen, Read, acceptance and exclusion actions; use Include there before accepting
an excluded file. **Option-A** on macOS (**Alt+A** elsewhere) accepts the
current file without opening the menu ([[BPR-1515]]; the owner replaced
Command-Shift-A with it on 2026-10-15, [[BPR-1531]]). The menu and Accept
tooltip show the key. It works outside text input and never revokes an
accepted file; use Revoke acceptance explicitly. Wherever the reviewer can
type (a field, a comment, reply or note editor, an editable list, the Review
Cycle suggestion editor) the key is the editor's, so Option-A still types
`å` there. On macOS the key is the one labelled A in the current layout; a
non-Latin layout whose A key types another letter does not offer it.
Opening or seeing
a file does not accept it. At the bar's right
are **Compose review** (Option-R, Alt+R) and **Comment**
(Option-C, Alt+C) as icons whose tooltips name the command and its key. Compose
review carries an amber mark while a review draft is kept. Review details is
reached from the comparison's menu. Both composition commands are also in the
Pull Request menu. Compose review opens the whole-review summary in Discuss,
beside the conversations (owner's Q3 answer on [[BPR-1212]]: "A: move to
Discuss"), and Esc or Back returns to where it was opened. While a review is
in progress ([[BPR-1345]]) a chip before Compose review says "Review · N
pending", N exactly the drafts pending in it (or "Review · submitting" while a
submitted review holds them), with **Finish review**, which opens the same
summary in Discuss. Finish review is also Command+Option+Return
(`Ctrl+Alt+Return`), from anywhere in the window, while a review is in
progress and not being submitted; the chip's tooltip names the key
([[BPR-1345]] v002). Each navigation
label and its pair share a horizontal accent rail. File and Changes stay named
at every width and when their position is unavailable; narrow layouts shorten
only the position wording.

**Comment and Reply** ([[BPR-1133]]). Comment answers the selected review
thread when there is one (the thread the reviewer chose, or the thread on the
line they selected) and otherwise starts a comment on the current line. It
names that target wherever it is offered: its tooltip ("Comment on line 431
(⌥C)", "Reply to tbutler's thread (⌥C)"), the Pull Request menu, and the menu
its arrow opens on the review bar, which offers that plain comment alone. Labels
are chosen in the comment editor's chip, never in this menu (owner, BPR-1083
`comment-menu-v001`, 2026-10-05). [[BPR-1212]]: a new comment opens as a draft
thread in the drawer at its line, and a reply in place at the foot of its
thread, so the code stays in view; nothing is published without the draft's
explicit Publish. [[BPR-1207]] requires an admitted editable draft, or an explicit recovery for its
retained text, at the named current-head target before Comment opens. Otherwise the tooltip and
menu state why it is unavailable. A draft whose retained text is in conflict, and a comment-workshop proposal,
open in Discuss's full composer instead, which hides empty workshop panels
and inactive revision-recovery controls; refused draft text stays available
for explicit recovery. Every thread card offers **Reply**, beside the code as in
Discuss, through the one Reply path: the card's thread becomes current, its
line is selected (even when another line was), and the draft on that line
answers it. No card offers a comment that would start a second thread on the
same line.

Nothing else sits between the title row, review bar and Updates line.
Loading, preparation and unavailable-comparison words occupy the open Updates
track, or the review bar's summary when folded ([[BPR-1201]]). The track stays
34 pixels high; complete readiness words remain selectable there and available
in the summary's tooltip and accessible description. The separate readiness
and composition entry rows are gone. Home has no review bar.

**The Updates line folds** ([[BPR-1130]]). The disclosure at the bar's left,
a triangle and the word Updates, folds the timeline away and opens it again;
the View menu's **Updates Line** does the same from the keyboard, and the
disclosure itself takes focus and answers Space. The fold takes no key of its
own. Open, the timeline is one 34-pixel row across the whole window directly
under the bar; folded, it leaves no row, and the bar, which names the
comparison and its dates either way, does not move. The fold is window state
the core decides, like a drawer: a new window starts folded, each window keeps
its own choice in its own layout record (never in the profile's defaults), and
nothing folds or opens on its own: no modality, drawer or refresh changes it.
Review details still open under the bar while the line is folded. The line has
no permanent caption: its history statement ("Known updates · display order is
not push order") is its tooltip away from a node and its accessible
description. When history is incomplete or ambiguous, as RFC-BPR-0029
defines it (a cut-off or refused read, a provisional current snapshot, an
occurrence that cannot be matched, or local records alone), a mark, a ring
with an exclamation point, sits at the line's start before the track, and the
disclosure carries the same mark after its name, so a folded line still says
so; both tooltips carry the statement. The line's "pair ready" words left its
row with the caption; their sentence is the comparison menu's tooltip, and
loading or an unreadable pair uses that summary when folded and the track when open.

**The line starts with the pull request's base** ([[BPR-1557]]; the owner,
2026-10-16, on both handles stacked on the newest update: "Surely the left
side should be at whatever the PR's merge base is, no?"). Every comparison
resolution names, whatever is selected, the anchor Whole PR compares from,
from the same core function Whole PR's left side takes: today the newest live
epoch's `Branch base` (below), which follows the base when the base branch
moves or a force push rebases the branch, and is never an imported epoch's.
The line only presents it: when the base Whole PR uses changes, the line's
base changes with it. The line draws it as a solid post in the ink at the
track's start, labelled "Base", and spreads the updates over the rest, so one
update sits at the far end. The handles are solid brackets, `[` on the left
and `]` on the right, 15 pixels tall, their arms reaching round a filled span
in the accent (v002; the owner, on v001's hollow handles: "I find these
handles quite difficult to see"). The theme's accent is a selection colour
that stands only 1.2:1 to 2.1:1 off the ground in the built-in themes, so the
brackets and the span carry an outline: the accent moved towards the ink only
as far as it must to stand 4.5:1 off the ground and 3:1 off the track. On a
dark ground (Dark, High Contrast Dark) that kept the selection navy's family,
and the owner asked for "a different color so they stand out" (v003): there
the brackets are the accent's hue turned towards cyan, saturated and at full
value, at least 7:1 off the ground and the track, with a rim darker than the
ground that draws their arms; the span is that colour tinted into the ground,
a step below the brackets; the focus ring is the bright colour. Amber stays
"my last review"'s. Each
bracket stands on a ground-coloured halo, so neither the track nor a node
runs into it, and a held update's node is its bracket's stem. Under the
pointer, or while dragged, a bracket fills with its outline colour (the ink on
a dark ground) and the
pointer shows sideways arrows; with the keyboard on the line, the handle the
arrow keys move wears a ring. Whole PR puts the left bracket on the base and
the right on the newest update. An update's own base sits 18 pixels before its
node, so "Show this update alone" draws two brackets that never touch, and two
handles on one place are drawn either side of it. It is the left handle's first
stop for Left and Right and a drag; a click on it, its label or its menu's
Compare from here compares from it, and the menu copies its commit; the
right handle never takes it, and an older history page keeps it at the start.
An older update's own base, which Show this update alone compares from, keeps
its place just before that update. The tooltip names the commit and that
Whole PR compares from here, and claims nothing about how the base was
chosen; the line's accessible name says where both handles are. It is
labelled Base, not Merge base, because today it is the base GitHub records,
not a computed merge base; which base Whole PR should compare from is
[[BPR-0944]]'s decision. The core decides the places, stops and steps
(`bpr/core/update_line.hpp`); the line draws.

**Document tabs fill the comparison's header** ([[BPR-1132]]), replacing
its status row. Each tab keeps an exact file, comparison and semantic reading
position. Clicking a file or using previous/next file changes the active tab;
Cmd-click (Ctrl-click elsewhere) and **Open in New Tab** open a background tab
on the window's comparison. **Duplicate Tab** and the strip's **+** copy and
activate the exact view. Names elide from the left, retain enough path to tell
same-named files apart, and expose the full path in the tooltip. A tab with its
own comparison carries a date pill and names the pair accessibly. When the
strip has room, tabs share it equally; at a 220-pixel minimum they scroll
sideways with the mouse wheel or trackpad. Activating a tab brings all of it
into view. The plus stays at the right outside the scrolling region and
focuses its duplicate. There is no count or overflow menu at any width.
Loading, refusal and approximate-alignment status appear beside the tabs.
That single line can elide; its tooltip and accessible description keep the
complete text. When there is no source text to read, the empty comparison also
shows the complete, wrapping explanation with text selection and Copy
([[BPR-1197]]). A successful text comparison uses the compact header alone.

The tab menu offers Duplicate, Close, Close Other Tabs, **Use the Window's
Comparison**, and **Make New Tabs Use This Comparison**. Ctrl+Tab and
Ctrl+Shift+Tab step between tabs on every platform; Cmd-W (Ctrl+W elsewhere)
closes the active tab, choosing the next neighbour or the previous one at the
end. Closing the last tab leaves the pull request and its file chooser open.
Overview, Threads and Discuss show the window's comparison
(Threads keeps it underneath the finder); Review shows the active tab's. When no tab is open, changing the comparison sets the window's
pair; the first file opened takes that pair. Resume restores the retained tab order, active tab, exact pairs
and reading positions. Unavailable material stays unavailable on its own tab.

An older-pair tab places only threads belonging to its revisions and disables
new Comment when neither endpoint is current. The Comments tray counts current
threads on later updates of that file and offers **Show them**. It activates an
existing same-file tab on the window's pair, returns this tab to that pair, or
opens Discuss. Later threads remain listed with unavailable code anchors; the
owner's choice about hiding them until Show them is pending, as is whether
**+** should duplicate the active view.

The chrome is inside the review state's space budget ([[BPR-0246]]). In Review
at 1280x1000 with every drawer collapsed, the comparison takes 872/1000 of the
window's height on macOS with the Updates line folded and 838/1000 with it
open (offscreen, where the menu bar takes a row), measured with the 28-pixel
review bar, the 28-pixel tab strip and the 34-pixel timeline; the Linux figures, 876 and 842, are
derived from the Linux stack [[BPR-1036]] measured until the Linux station
measures them, and Windows keeps the owner's 850 for the open line, so 884
folded. `application_uiforetw_selected_diff_contract` holds those floors both
ways: its window starts folded, it opens the line through the window's own
command to measure the open stack, and a further row that takes height from the
comparison fails that course, whose failure line names every row stacked with
the comparison, folded and open ([[BPR-1036]]).

The file and conversation regions are true collapsible drawers. Their open widths are independently resizable and persisted. Collapsing one returns its width to the central review surface. Their toggles remain at the opposite edges of one compact icon-only row, so both surfaces stay reopenable without a second drawer toolbar.

Files and Search form the file drawer's header, using the same compact native
document tabs as Comments and Notes ([[BPR-1509]]). The drawer keeps its
accessible name without a separate visible heading. Switching pages retains
the file selection, Files filter and Search query/results; native tab focus,
arrow navigation and disabled states remain available.

**Hide and show every drawer** ([[BPR-1295]]). One checkable window command,
Hide Drawers, on `Ctrl+B` (⌘B on macOS) and an icon immediately right of the
comments toggle. While any drawer is open it closes every open drawer
(repositories, files and comments) and remembers which were open; pressed
again it reopens exactly that set at the saved widths, or Review's preset when
nothing is remembered. Opening a drawer by hand while they are hidden, or ⌘2 in
Review, drops the remembered set; leaving Review and coming back keeps it. The
core decides it, as it decides every drawer gesture. Overview refuses it with
its existing reason, and Discuss and Threads disable it, because there
the conversation or the finder is the page ([[BPR-1293]] for Threads). The icon is a window outline with a column on each side, filled while
the drawers show and outlined and checked while they are hidden; its tooltip
names what it hides or brings back with the key ("Show Files and Comments
(⌘B)").

## Overview summary

RFC-BPR-0063 replaces the four-line preview with a resizable description above
Files ([[BPR-1312]], [[BPR-1313]]). A long body starts with half their shared
height; a short body fits its content; an empty body says **No description**
and gives Files the room. Every nonempty body has a visible, focusable
**Resize summary** grip. Drag it, or press Up/Down on it to move one current
text line. Double-click or choose **Reset summary size** from its context menu
to return to content-aware sizing. Font-relative limits retain three summary
lines and three file rows when the available height permits; smaller windows
share their existing room without adding a window minimum.

Description and Files scroll independently. Native Markdown and source fallback
remain selectable and copyable. The reader's top-right icon control offers
**Enter full view** / **Exit full view**, without an action row below the body.
Right-click inside the reader for **View source** or **Rendered**. Ordinary
selection Copy yields Markdown text rather than HTML; source mode preserves
exact selected bytes, while rendered partial selections may normalize source
formatting. The same reader documents and Files selection survive full-view
changes and local Escape. Refresh
and reflow retain the reading place; a changed body or authority clears old
reading state before the replacement appears.

An explicit resize belongs to this window, survives navigation, other PRs and
orderly restart, and is stored only as a fraction of shared height. A temporary
small-window clamp or empty body does not erase it; Reset does. New windows
use content-aware sizing. No description text is stored as a layout preference.
Owner acceptance of the changed native surface remains in RFC0063's ledger.

## Overview check details

RFC-BPR-0068 adds a native **Check details** disclosure in Overview’s summary
column, after the review entry action. The first accepted failure expands it;
a reviewer’s later collapse and selected result survive the same PR’s refresh.
The failure count stays visible while collapsed. The ordered result list shows
failures/action-required results, waiting results, then settled results. Selecting
a result exposes its kind, producer, actual outcome, supplied explanation and
**Requirement unknown** until separate requirement evidence is accepted.

Check explanations are bounded literal text. **Open check details** is an
explicit host-admitted browser handoff for that exact current result. Missing
output or a missing/refused address leaves the result readable and explains the
unavailable action. Current check runs and newest commit-status contexts are
separate rows even when names match. Totals are partial when either collection
is unavailable. The current implementation labels the accepted head commit and
states that test-merge evaluation is unobserved. RFC-BPR-0069 supplies separately
observed target policy for required/optional classification; an unavailable
policy or unknown required app keeps the classification unknown. Green head
totals do not establish merge readiness. These reads and controls neither
rerun jobs nor execute repository code.

## Overview merge requirements and outstanding requests

RFC-BPR-0069 puts **Merge** at the top of the native summary column.
[[BPR-1405]] keeps it short. GitHub's accepted assessment is one toned phrase
(Blocked, Conflicts, Behind base, Mergeable, Draft, Merge state unknown), and
each unmet condition follows on its own line: review required with its
configured minimum, changes requested, each failed check, and each expected
required result that has not reported. They remain visible together.
**Waiting on** lists outstanding requests as roster rows in Participants'
anatomy, code-owner requests first and marked "code owner", ordinary requests
marked "requested". A team has a square tile and its organization-qualified
slug; its supplied name is in the row's tooltip and accessible name. Names
elide in the middle, so teams of one organization stay distinguishable. A
complete empty roster shows nothing; a partial or last-known one says so. A
prior approval stays in Participants while a re-request stays in the roster.
Accepted removals, dismissals and replacements update both observations
without turning review history or an unpublished review into a request.

**Requirement details** is a native keyboard disclosure, closed by default. It
opens with GitHub's assessment as a sentence and holds the evidence limits the
short section leaves out. It shows the exact active applicable rule source, classic protection, approval restrictions,
conversation-resolution and other returned obligations, plus the observed
auto-merge state. Team review policy retains its separate numeric identity and
minimum; file-pattern applicability is not evaluated locally. A request roster
never asserts that every code-owner team must separately approve. Configured
minimums never produce a remaining count, and positive server assessment grants
no viewer merge authority. No GitHub destination for policy is fabricated from
an API address; check detail handoffs retain their existing exact admission.

Names and explanations wrap as literal text and share the inherited palette,
font and summary scrollbar. Accessibility receives the same waiting names and
one combined announcement for changed description/requirements. Field failures
remain explicit; same-head/base observations retained after a failed refresh
are labelled stale. Required results missing on the accepted head are separate
from the observed totals; test-merge evaluation remains explicitly unknown.
There are no merge, rerun, assignment, reminder or policy-write controls.

## Overview files

Overview groups files under flat folder rows ([[BPR-1213]]). A folder that
holds files takes one row named by its whole path, so a folder with no file of
its own takes no row, and every file label starts at the same left column. Each
folder row draws the native disclosure mark before its path. Clicking the row,
or Left and Right, hides and shows that folder's own files only. Reopening
restores the same file identities and order. These gestures never open a
comparison. If the focused file becomes hidden, focus moves to its folder
without changing the active source selection.

Beside the Files heading, two glyph buttons fold every folder row at once
([[BPR-1269]]). Collapse all shows two chevrons closing upward, and expand all
two chevrons opening downward. Each is named in its tooltip and to
accessibility, and each takes the keyboard. Each folder keeps its files in
order, and a per-folder toggle works as before afterwards. Like a single
folder's, the gestures open no comparison and move a hidden focused file's
focus to its folder. The buttons are shown only while the list has a folder
row.

Folder expansion is local to this page. Incremental same-authority refreshes
retain surviving items, selection and expansion; newly appearing folders start
expanded. Exact authority invalidation clears that local state. The native tree
exposes each folder's path, file count and expansion state to accessibility.
[[BPR-0934]] recorded the earlier nested hierarchy, which the owner's
clarification on [[BPR-1213]] replaced with these flat rows.

## Home

Home is the window's other page, shown whenever no pull request is open and
reachable at any time through Home in the strip (`Ctrl+Shift+H`, which macOS
presents as ⌘⇧H; `Ctrl+0` until the owner moved it on 2026-10-15,
[[BPR-1535]]). The Home button's tooltip includes that shortcut in the
platform's native notation. It is the window ground edge to edge rather than
a narrow column, and RFC-BPR-0032 gives it one job per row.

- **The strip.** Home at the left; at the right, before a divider, the
  account and settings buttons. While Home is shown and an account can fill
  its list, **View** and the refresh glyph stand before them ([[BPR-1580]]:
  View replaced Group, State headers and Filter). Signed out those two act on
  nothing, so they are absent rather than disabled, which is how RFC-BPR-0028's rule that the strip carries only
  Home, account and settings still holds there. They are absent from the
  review strip for the same reason. On Home the refresh glyph is the
  profile's personal discovery (RFC-BPR-0026, [[BPR-0990]], answering
  [[BPR-0801]]): it asks for the authored and directly requested pull
  requests of the saved repositories again, is disabled while that refresh
  runs and says so, and is its own action, so the review strip's refresh
  and its shortcut stay the open pull request's. Arriving at Home, or
  activating a window that shows it, asks discovery to catch up; discovery
  reads only when its list is more than five minutes old.
- **What discovery last did** is one sentence under the bar, where Home
  already reports what the queue refused: "Found at 14:05 in 2 saved
  repositories.", "Finding my pull requests in 1 saved repository…", a
  partial list with why ("…but the list may be missing some: more matched
  than one search returns."), a stale one with its failure, what it still
  shows and how long GitHub asked to wait. It says a clock time, never how
    long ago. Rows found before a failure stay. **Add repository** occupies the
  same row, above the lists, whenever Home has an account and at least one
  saved repository: populated, empty, filtered, refreshing or unavailable. The
  row stacks at narrow widths.
- **First repository** (RFC-BPR-0062; owner verdicts of 2026-10-07 and
  2026-10-08). Once the queue is ready and the exact displayed account/host
  has no saved repository, the row gives way, in the same place under the bar,
  to one default-styled **Add a repository** button and nothing else. It opens
  Settings › Repositories with the new-repository field focused, and disappears
  with the first durable save, handing the keyboard to the ordinary row's Add
  repository. While nothing is saved and the queue is empty, Needs me is not
  shown: without a saved repository only an observer rule or a follow can
  enroll a row, and when either does its lane shows as usual. Recently viewed
  and Review Cycles (local projects, with Open Project…) are unaffected. A
  filtered or empty queue with saved repositories, or a queue still restoring,
  keeps the ordinary row. Signed-out Home keeps its sign-in and public-reading
  entry without either.
- **Add repository** ([[BPR-1308]], [[BPR-1309]], RFC-BPR-0062) everywhere
  opens Settings › Repositories with a New repository card in its edit state
  and its field focused, the way Accounts & hosts adds an enterprise server;
  there is no dialog. The card names the exact destination account and host
  and accepts a repository link or owner/name for that host. Add (or Return)
  reports progress, duplicate and persistence refusal beside the field; a
  refusal keeps input for a fresh retry. A durable success closes the card into
  the page's list with a confirmation, then discovery reports its own empty,
  current, offline, limited or unavailable result on Home without undoing the
  save. A different host requires explicit setup/sign-in while retaining the
  draft; returning requires a new Add after checking the destination. Cancel
  or Escape discards the draft. Long identities wrap; progress and refusal are
  announced politely. The open-PR shortcut saves its known repository directly
  through the same host and durable store.
- **The bar**, alone in the first row: one bordered field labelled Open,
  with the placeholder "Paste a link, or write owner/name#123", its shortcut
  printed at the trailing edge (`Ctrl+L`, ⌘L on macOS), and Return to open.
  It has focus when Home appears — on arrival only, so a later projection
  cannot take focus from wherever the reviewer has since moved it.
- **What a link names** ([[BPR-0987]], RFC-BPR-0026 AC-07/AC-08). A link to
  the pull request the window already shows reads it fresh from GitHub, then
  goes to the location, and a Recently viewed row of that pull request reads
  it fresh and lands on Overview ([[BPR-1475]], the owner, 2026-10-13: a read
  per reopen over a stale view; BPR-0987 read nothing again). `/files` and `/commits` are
  views of the whole pull request and say nothing. A file fragment
  (`#diff-<SHA-256 of the path>`) selects exactly that changed file in Review;
  its `R<n>` line is selected when the comparison on screen ends at the pull
  request's head. `#discussion_r<id>` reveals the comment when the read
  identified it by that number (a signed-out read does; an authenticated read
  names comments by node identity only): Review opens on its file with the
  thread open in place at that post ([[BPR-1293]]; it was Discuss before).
  `#pullrequestreview-<id>` reveals that review in Discuss.
  Everything else — a commit page, a base-side `L<n>`
  line (GitHub numbers it against a merge base the comparison does not use),
  an `#issuecomment-` link, a digest naming no changed file, an unread
  fragment — opens the pull request and says, under the bar and in the status
  bar, what was asked for and why it is not shown. Nothing nearby is chosen
  instead, query parameters are ignored, and a link that names no pull
  request (a repository commit, an issue) opens nothing.
- **A refusal under the bar**: one sentence from the existing diagnostic
  with **Copy details** beside it. A first load still in progress is said in
  the same place and carries no Copy details, because it is not a refusal.
  A pull request the current connection cannot read is refused in a card of
  its own there instead ([[BPR-1200]]): the style's warning icon, a heading
  that names the pull request, a sentence that says why in terms of the
  connection, and the steps that could open it, then Copy details. Signed
  out it says to sign in and offers **Sign in** and, where the composition
  can inspect gh, **Use GitHub CLI**; signed in it names the account and
  offers **Use another account** and GitHub CLI unless the account already
  is GitHub CLI's. The core decides the words and the steps from the typed
  reason and the connection; Qt lays them out. The card is announced once,
  politely, from Home in its own words. Home's spare height is a fill below
  its last row, so the card and the loading sentence sit directly under the
  bar.
  A refusal the reviewer has already begun rewriting stays dismissed until
  the next load. The pull-request stack's empty page is not part of Home; it
  stays parented in the hidden pull-request column so what writes to it keeps
  its shape.
- **The list** is in section order — Needs me, My pull requests, Waiting on
  others, Following, then Recently viewed — from the core's row-scoped Home
  decision ([RFC-BPR-0066](../rfcs/RFC-BPR-0066-home-authored-pull-requests.md)).
  Accepted author-role or authored-discovery membership never appears or counts
  in Needs me. Authored rows with the waiting-only bucket remain in Waiting on
  others; every other authored row is in My pull requests, including actionable,
  quiet and unknown. Unread activity still outranks waiting, so it moves an
  authored PR to My pull requests and retains its unread count. Other people's
  unknown rows sit in Following. Unknown detail is labeled and never treated as
  Quiet. Group changes the panels around the rows; section order comes first
  and the chosen sort orders rows inside each section. Notifications and the
  repository drawer retain the same attention policy and counts.
- **A row** leads with the PR title, a second line that says why and ends
  in its one-word reason, and at the right the time and my reading progress.
  Recently viewed uses the same anatomy, so a row reads the same way wherever
  it appears. [[BPR-1414]] (owner, 2026-10-12) puts the title in the row weight
  and moves repository/number into the quieter second line. An unknown title
  falls back to the full identity. Under Repository grouping the second line
  needs only #number; under Organization it needs name#number. Each queue row
  names its author beside that identity, including the viewer's own PRs; the
  login uses ordinary text ink. Assistive text keeps the full identity and
  author. Recently viewed stores no author. [[BPR-1407]]: a queue row's right holds
  a column of facts, each on the line it is about: "+1,204 / −318" beside
  the first line, "21d" and a comment icon with "14" beside the second in
  its quiet ink. Tabular figures line the column up from row to row; the
  hover action keeps its own place to the column's right. Age is whole days
  to the clock the host supplies ("<1d" under one); comments count the
  conversation and inline comments together; an unread fact draws nothing;
  assistive text says the facts in words. The owner's one exception to the
  colour rule: lines added are green and removed red (the theme's accepted
  and defect inks). Otherwise colour means state and nothing else: the reason word, and
  either a 3-point mark at the row's left or the rail of the panel holding
  it. The rail and header of My pull requests are the one section colour. Row text wraps rather than eliding, and the style paints its own
  selection, hover and focus.
- **One selection, and the open review** ([[BPR-1365]], the owner,
  2026-10-09: "Separate problem: the color of recent is the same as the
  color of selected."). Home shows one selection, in the list that last took
  its keyboard: the queue (in every arrangement), Recently viewed or Review
  Cycles. Clicking or tabbing into a list moves the selection there. The
  others keep their current row, so they still know which row Return and the
  row menu mean, but select nothing, and a publication of Recently viewed or
  Review Cycles never selects a row in a list that does not have the
  keyboard. The selection is the style's own, in the theme's selection
  colour. The review this window has open (the composition marks its
  Recently viewed entry; the queue row with the same exact identity follows)
  is drawn quietly, never with the selection's fill. Unselected, its ground
  is the theme's current-line tint (AlternateBase) and it carries a
  **Reviewing** tag, the word Home's title uses for it, in neutral ink in a
  thin outline beside the time. Selected, it is the style's selection like
  any other row, and the tag takes the selected ink. Its accessible
  description begins "Reviewing in this window". With a review cycle
  selected, Home's pull-request actions (Open Review, Open on GitHub) name no
  row.
- **Panels.** With State headers on — the default — each state is a bordered
  card with a rail in its colour and a tinted header carrying the name and
  the count. My pull requests has its own plum header and rail ([[BPR-1528]],
  the owner, 2026-10-15: "Can we add a header and rail color for my reviews on
  the home"), a fourth built-in Home tone per variant that, like the other
  three, is neither stored nor customisable. It marks the section, not a
  state: its rows carry different states, their reason words keep their
  existing state colours and their sentences identify the PR as mine
  alongside the observed activity.
  Its collapsed summary names actionable and unread activity, retaining unknown
  detail distinctly. Its own collapse starts expanded on upgrade and then
  persists per window independently of Waiting on others. A header sticks to the top of the scroll area while its panel is
  in view. Every panel collapses to its header by chevron, Space, Return,
  Left and Right, keeps its count and a one-line summary while collapsed, and
  announces its expanded or collapsed state. An empty state is not shown, and
  when a signed-in window's ready queue is empty Needs me holds one sentence
  — "Nothing needs me. Pull requests from saved repositories and observer
  rules appear here." — whose link, on "saved repositories", opens Settings ›
  Repositories; observer rules are reached from View's Open Reviews & queue.
  No empty
  box is drawn in any arrangement.
- **Group by** offers None (the default), Organization, Repository and
  Account. A group is a neutral panel headed by an initial tile — round
  for an account, a small radius for an owner that is not one — and its name,
  with its count; no avatar image is cached, so the tile is all there is.
  With State headers on, a slim tinted band per non-empty state nests inside
  each group and collapses on its own; with it off, the group's rows sit
  directly in state order carrying their marks.
- **Account chips.** A row carries its account's chip at the right when the
  queue names more than one account. A group whose rows are all one account
  carries the chip in its header instead and its rows drop it; under Account
  grouping no row carries one; with a single account none is drawn anywhere.
  A chip uses neutral palette roles only, because on Home colour means state
  and a chip in a state's colour would claim a state its account does not
  have.
- **View** ([[BPR-1580]], the owner, 2026-10-17: "I wonder if we should
  collapse these 3 into one"; "I don't understand what half these options on
  Filter are even meant to do") is the one strip menu over how Home arranges
  and filters its list. It replaced three controls, Group, State headers and
  Filter, whose four Filter groups were `QMenu::addSection` titles that the
  native macOS menu draws as bare separators. Every group is now a submenu
  whose item names the group and its current value, so the native menu
  draws every label: **Group by: None** ▸, **Sort by: Priority** ▸ (the
  order inside each state), **State headers** (checkable), then **My role:
  Any** ▸, **Added by: Any** ▸ and **Quiet items: Show** ▸, then **Reset to
  defaults**, then the footer "Nothing here changes GitHub." and **Open
  Reviews & queue**. Each submenu is one exclusive choice listing every value of its
  vocabulary, so an item cannot be unticked into nothing. A native macOS menu
  shows no tooltip, so each choice explains itself in its own words: My role
  reads Any role, I opened it, I reviewed it, My review is requested, I took
  part in it and None of these (a rule or a follow added it); Added by opens
  with the line "Observer rules are mine, kept in Settings › Reviews & queue"
  and reads Any way, A rule on a repository, A rule on an author and A rule
  on a path;
  Quiet items opens with the line "Quiet: nothing new for me and nothing
  waiting on anyone" and reads Show, Hide and Only quiet. The parent item
  shows the value's short name (Author, Requested, No role, Path rule, Only
  quiet), and every choice also carries a tooltip and status tip where the
  platform draws them. Added by is
  offered only when the profile has an observer rule, or when this window is
  already filtered by one. Repository and Bucket are not Home filters because
  Group by and the state order do their work. These are the same typed
  controls Settings presents as queue defaults, not a mirror of them.

  The button reads **View** at the defaults. When any choice differs from
  its default it carries the strip's count pill with how many do, and its
  tooltip and accessible description name each one ("2 changed: Group by
  Repository; My role Requested."). **Reset to defaults** is enabled exactly
  then and restores Group by None, Sort by Priority, State headers on, My role
  Any, Added by Any and Quiet items Show in one gesture; the profile hears its
  filter part as it hears a filter choice. When the queue has rows and the
  view's filters hide all of them, the list holds one sentence, "Filters hide
  all 5 pull requests. Clear filters or change them under View.", whose
  **Clear filters** link (Tab, Return or Space) puts My role, Added by and
  Quiet items back to their defaults and leaves the arrangement alone; the
  line under the bar does not repeat it.
- **Recently viewed** is the last panel in every arrangement, and Resume is
  its rows' action: on hover, on focus and in the row's menu. It is governed
  by the entries the composition gave the window rather than by the account,
  so a signed-out window still lists its public reading.
  It retains at most ten destinations per profile, with no time expiry.
  **Remove from Recently Viewed**, in a recent row's menu, or Delete/Backspace
  while the list has focus removes that exact destination from the profile's
  list and persists it. Other windows update too. It does not close an open
  review, remove its saved reading, unfollow it or change GitHub. Opening it
  in another window or navigating away and back can list it again; a refresh
  of the already-open PR leaves it removed ([[BPR-1414]]).
- **Opening a row** ([[BPR-1342]], the owner, 2026-10-08: "I should be able
  to open a review and open a review in a new window from the home, including
  from right click on a pr"). A single click only selects a row: it becomes
  current, takes the keyboard and is highlighted ([[BPR-1364]], the owner,
  2026-10-09: "I don't like a single click on home card opening the pr."). A
  double-click or Return opens the row's pull request in this window, on
  Overview, as the row menu's Open Review does; this holds on every platform,
  including where the style makes a single click an activation. Every Home
  list (the queue in each arrangement, Recently viewed and Review Cycles)
  says so in its accessible description: "Double-click or press Return to
  open". Changing Group or State headers keeps the keyboard on the selected
  row. ⌘Return (Ctrl+Return off macOS), ⌘-click
  (Ctrl-click) and middle-click open it in a new window of the same profile.
  On a Recently viewed row ⇧Return still resumes. A right click on a queue row
  or a Recently viewed row makes it current and opens Home's one row menu:
  **Open Review** (the default, drawn bold), **Open Review in New Window**,
  Resume for a Recently viewed row, then a separator and Open on GitHub. Each
  item shows its key: ↩, ⌘↩, ⇧↩ and ⇧⌘O on macOS. The lists read the keys
  themselves; the menu's shortcuts never act from the window. Open Review in
  New Window is offered exactly when Profiles › New Window is. It is that New
  Window, in the same profile under its one runtime owner, carrying the one
  pull request the window's host admitted: the new window opens Home, as
  every new window does, and then that pull request by its own bar's path, on
  Overview. A refusal is said in the status bar.
  **The same pull request already open.** Open Review in New Window always
  opens another window, even when this or another window shows that pull
  request: an explicit New Window always creates one, and two windows of one
  profile may show the same pull request at different positions
  (RFC-BPR-0024). Open Review in this window reads it fresh even when the
  window already shows it ([[BPR-1475]]; [[BPR-0987]] returned to it without
  reading again).
- **After publishing** ([[BPR-1475]], the owner, 2026-10-13). When this
  window publishes a review, a comment or a reply, it reads the pull request
  again by itself, as Refresh does, so what was posted shows without a manual
  refresh. An unknown outcome is not reread: a review's is settled by Check
  status, which rereads once it finds the review published, and a comment or
  reply's draft stays held for its Retry. A Refresh pressed meanwhile keeps
  that draft held, locked, for the same Retry ([[BPR-1477]]); a reread the
  window started by itself would land while the reviewer presses Retry and
  drop that retry's answer. The reviewer's place stays: the modality, the
  selected file and line, the status line, and a note being written or
  awaiting its retry, which is held again and reopened in the drawer. A read
  already in flight is not doubled: the reread waits for it to settle. A file
  the reviewer picks while this reread or a Refresh is in flight is the one
  shown once the read lands ([[BPR-1478]]): the latest pick wins, and the
  read never takes the window back to the file it showed before.
- **While a row opens** ([[BPR-1357]], the owner, 2026-10-09: "There's no
  obvious immediate indication you actually tried to open a review if there
  is real network latency on the action."). A first read stays on Home until
  GitHub answers ([[BPR-1200]]), so the row that asked says so in the same
  turn as the double-click or Return ([[BPR-1364]]): its second line gives
  way to a turning mark in the line's own ink and "Opening #42…", and a
  second double-click or Return on it asks for nothing. After the core's wait (`core::home_open_wait_milliseconds`,
  800 ms, measured by the application's clock) the line names the step the
  read is on ("Opening #42: asking GitHub for the pull request…") and the
  row's action place holds **Cancel** (Esc on the row presses it), which
  stops the read on the wire and leaves the row as it was. A failure keeps
  the row's line, in the theme's failure ink behind a ringed exclamation,
  saying why ("Couldn't open #42. The GitHub host could not be reached…"),
  with **Retry** where asking again can help; the line under the bar does not
  repeat it. What is shown follows the read's request identity as the core
  decided it (`core::home_open_state`), so a late answer for another read
  changes nothing. Each change is announced once, politely.
- **Home's status band** ([[BPR-1357]], the owner, 2026-10-09: "I think the
  general status text on the home page is still a bit lacking, it's kind of
  hard to see."). Docked at Home's foot, under the list ([[BPR-1381]], the
  owner, 2026-10-10: it "needs at least a background plate", and "I'm not
  sure that is the best place"), at body size in full ink on a plate of the
  window's own ink over its ground, behind a mark that says its tone: busy
  (the turning mark on the neutral plate), problem (the ringed exclamation on
  a tint of the failure ink) or notice (no mark, the neutral plate). Shown
  or hidden there it moves nothing above it; under the bar it pushed the
  list down under the pointer. What the bar is opening is also said in the
  bar, where its shortcut stood: the turning mark and "Opening…". Only a problem is coloured, because on Home colour
  means state and blue is Waiting on others. It says, in this order: a setup stage's
  notice, a review restoration's, a load's refusal ([[BPR-0771]]); what the
  bar asked to open, with Cancel after the wait and Retry on a failure; a
  problem the window has; a connection under way; then what was just done,
  for as long as the status bar says it. It is hidden while Home is quiet,
  so nothing is added to an idle Home, and each new sentence is announced
  politely.
- **The status bar** on Home carries the session line ("Signed in as
  MikeMarcin on github.com.") and never "No pull request is open in the
  workbench.", because Home is what the window shows when none is. The
  session projection carries one account on one host and no refresh time, so
  the line says that much and no more.

View's choices (Group by, Sort by, State headers and the filters) and the collapse of the panels that carry no
name are remembered per window, in the per-window table of the workbench
layout store (Home view introduced in schema 8; schema 19 adds the
independent My pull requests collapse bit). A group panel's own collapse is not: its key would
name an owner, a repository or an account, which that store has never held
([[BPR-0189]]), so it lasts only while its window is open.

First launch and signed out: the strip with Home, account and settings only;
the bar relabelled "Paste a public pull request link to read it without an
account"; one card, **Sign in to GitHub**, with a sentence, Sign in as the
page's one filled button and Add enterprise server beside it as a bordered
one — the two ways in RFC-BPR-0018 requires, without opening Settings; and
the status bar line "Not signed in. Public pull requests open read-only."
No list, no panels, no filters.

Home over an open review keeps the way back: **Back to the review** (⌘⇧H),
the filled button at the bar's right on the bar's own line, its description
naming what is being reviewed and how far through it. Home beneath it is
unchanged, and whatever had focus in the review has it again when the review
returns. [[BPR-1381]]: it was a row of its own above the bar, and it came
and went with the review, so the bar and the list moved under the pointer
when a new pull request was opened (the owner, 2026-10-10). Beside the bar
only the bar's width changes.

Home waiting facts under RFC-BPR-0070 use the same row anatomy. [[BPR-1405]]:
an authored row's reason says who is awaited once, replacing the generic
"waits on reviewers" clause. [[BPR-1407]] (the owner, 2026-10-12: "just show
waiting on # reviewers") makes that a count of people and teams, code owners
among them: "Waiting on 3 reviewers; Branch Freshness failed". Overview has
the whole roster. An
aggregate unmet requirement with no request says Required review outstanding.
Someone else's row keeps its failed checks or merge block but not its author's
roster. Unread authored activity still goes to My pull
requests, and clearing it restores Waiting on others when review work remains.
Known check/merge blockers say Blocked and stay visible when Quiet items are hidden;
Only quiet excludes them. Collapsed summaries count blocked rows, and
awaiting-review rows outside Waiting on others. Last-known observations end
"last known", and opening uses the exact row's
Overview without changing enrollment or notification categories.

## GitHub host and pull-request presentation

A server is configured on the Accounts & hosts page of Settings, and its
pull request is read on the review surface. RFC-BPR-0032 separates the two:
the page carries the servers, and naming a pull request is the Home bar's
job.

- The page is a title, the sentence "One account per server. Credentials are
  never shown here.", one card per configured server, and one bordered **Add
  enterprise server** button. There is no host-profile picker. A host is
  chosen by its own card's **Connect**, which selects that host through the
  existing selection intent and then connects.
- A card's header carries the hostname, the server kind and transport beneath
  it as one quiet line, and the connection state at its right as one word:
  Connected, Connecting, Signing in, Signed out, or the short refusal "Could
  not connect". Connected is drawn in the accepted green and is the only
  colour on the page.
- The account row is a letter avatar, the login, and one consequence sentence
  built from the existing grant projection; signed out it reads "No account"
  with the terms of signing in instead. **Sign out** stands at its right
  while a session exists, **Sign in** while the card speaks for the chosen
  host, and **Connect** for a host that is not chosen. Where a host offers
  GitHub CLI the offer stands beside them and carries the configured-CLI
  trust warning RFC-BPR-0015 requires, before the opt-in and after it.
- The rows are Access and Rate limit, each a value from an existing
  observation; an enterprise server's **Remember this server** switch and
  flat **Forget server** action; and a **Details** disclosure holding the
  capability lines, the per-resource rate limits, and the last problem with
  **Copy details**. Each row is one line of label, info glyph and control,
  and the consequence sentence is the glyph's popover and its accessible
  description.
- Add enterprise server opens a card in its edit state — hostname, an
  optional account, Add and Cancel — through the existing saved-server
  intents. It creates no credential authority: a hostname and an account name
  are metadata, and the token is still asked for.
- The host-profile picker row and its buttons, the Owner/Repository/PR
  locator with Open and Refresh, the "Active host", transport and login
  lines, the pull-request status banner with Copy message, and the servers
  combo editor are absent from the page and from its accessibility tree.
  Their jobs moved rather than disappearing: a pull request is named in the
  Home bar, a load refusal is said under that bar, Refresh is the strip's,
  and a session failure is the card's own state and its Details.
  A first load stays on Home, said under the bar, until GitHub has answered
  for both the repository and the pull request (or the read has shown its
  overview, the review or a cached overview), so a refused read never passes
  through an empty Overview ([[BPR-1200]]); a long read still shows its
  progress on Overview from those first answers ([[BPR-0795]]).
  If the first load fails or is cancelled before any review is accepted,
  the window returns to Home with its Open bar available. A failed load shows
  the existing diagnostic and Copy details there. Enabled drawer commands
  cannot keep an empty Overview or comparison visible; a failed refresh
  retains the previously accepted review.
- Every gesture on the page emits a semantic application intent. Show Review
  Threads is Qt-local presentation behavior that selects, expands, scrolls
  to, and focuses the first available review thread.
- Command legality, capability tri-state, rate-limit observations, normalized
  metadata, conversations, status, and bounded diagnostic text remain
  distinct accessible fields.
- The pull-request title, author, branch, state, and counts establish the
  primary hierarchy. Exact commit identities remain available behind a
  Technical Details disclosure rather than competing with the review
  content.
- The viewport has explicit `disconnected`, `connecting`, `empty`, `loading`,
  `error`, `cancelled`, `loaded`, `refreshing`, `loaded-error`, and
  `loaded-cancelled` presentation states. The last three nonterminal/failure
  variants retain accepted content. A failed or canceled refresh cannot
  replace the last accepted content with an indeterminate spinner.
- Remote title, body, author, path, and comment values render as literal plain
  text. Provider records, requests, URLs, transports, vaults, credentials, and
  authentication material never enter the Qt adapter.
- Selecting another identity or opening another target immediately clears
  stale identity-sensitive content. Refresh keeps the last accepted content
  visible while a replacement is loading or if that replacement fails,
  cancels, or becomes stale.
- Conversation rows have stable semantic identities and update by insert,
  remove, minimal move, or data change without resetting the complete model.
- Conversation rows use a wrapped, non-eliding delegate that preserves native
  selection and focus painting. The first accepted projection expands the
  initial hierarchy; later refreshes preserve local expansion and restore the
  current selection when its identity remains.
- Each published status/diagnostic update produces at most one combined,
  polite accessible announcement, with repeated identical announcements
  suppressed.

The window remains usable through a minimum-size scroll surface, exposes
keyboard shortcuts and accessible names/descriptions, and is rendered at
1.0x, 1.25x, and 2.0x scale in the Qt harness. It inherits the active native
palette, style, and font without a forced palette, style, or QSS, and responds
to application palette, font, style, and theme changes. Offscreen
forced-style renders verify layout only. The reusable RFC-BPR-0003 comparison
control follows this same native-System inheritance. User-selectable System,
Light, Dark, and high-contrast profiles remain RFC-BPR-0006 work, with a fresh
installation defaulting to System.

## Public preview

Public preview is the credential-free reading mode for public GitHub.com pull
requests. macOS enables it; CLI selections still report unavailable. It is
reached through the Home bar, which relabels itself for it while no account is
signed in, and it is stated on the github.com card's account row while such a
session exists: "Reading public pull requests without an account." There is no
Public preview button beside Connect and no session strip to label the mode —
RFC-BPR-0032 retired both, and the status bar's "Not signed in. Public pull
requests open read-only." says the same thing where the reviewer is looking.
The same file drawer and exact selected-file comparison show public content.
REST inline comments appear as read-only root/reply groups, with no invented
thread resolution. Unobserved and observed-empty conversations have different
wording.

Queue role, private review progress, review-thread resolution, and publishing
remain visible but disabled with explanations. A budget ring sits on the
reading host's card, in its Rate limit row: it has no consumed fraction before
a budget was observed, and after a read it shows the reported limit, remaining
units, and reset. Exhaustion and private-or-missing failures offer Sign in, as
the ring does, and both reach the one sign-in already mounted — the device
sign-in in that card's account row — so a flow can never begin against a host
the reviewer is not looking at. GHES refuses preview and Linux/Windows report
unavailable rather than silently trying a credentialed read. Switching mode
clears stale identity-sensitive UI.

## Signing in with a device code

RFC-BPR-0016 adds a second way to enrol a profile beside the token field, and
the token field stays. `qt::device_authorization_panel` is the surface:

- A heading, two read-only fields — the user code and the address to enter it
  at — a remaining-validity label, and a browser-handoff label. The reviewer
  has to read the code aloud or type it elsewhere, so both fields are
  selectable text rather than a status line.
- Four actions: Get a code, Copy code, Open browser, and Cancel. Get a code
  becomes Get a new code once the flow has left `idle`, because offering
  "Get a code" after a denial reads as though nothing happened.
- Get a code and Cancel are enabled from the core's command projection rather
  than from a rule the panel keeps for itself. An absent command is a disabled
  command, so a projection that lost one fails closed. Copy and Open are
  enabled from what is on screen, because neither moves the flow
  ([[BPR-0283]], `qt_device_authorization_scale_100`).
- The handoff label reports what became of the last attempt to open a browser
  and never an authorization outcome. A refused, failed, or unavailable
  handoff leaves the flow exactly as live as an opened one, with the code and
  the address still presented ([[BPR-0280]], [[BPR-0516]]).
- Every terminal, including the successful one, clears the code and the
  address. A user code that outlives its flow is a code the reviewer can still
  be tricked into typing.
- Each published change produces at most one combined polite announcement
  carrying the stage, the code, and the handoff together, under the same rule
  as the rest of this window, and the panel is rendered at 1.0x, 1.25x, and
  2.0x in the Qt harness.

The server's card offers **Sign out** in its account row. Device grants are
remembered across orderly quits; manual tokens remain session-only. Sign out
removes both device-token slots and disables the stored binding. Startup
restores only an exactly validated session and reports unusable retained
material through the existing diagnostic surface. These effects belong to the
application host, not the button or panel ([[BPR-0541]]).

The panel receives a projection and never a token, a refresh token, a device
code, a client identifier, or an endpoint; those fields do not exist on the
type it is handed.

**Where it is mounted.** The panel is laid into a server card's account row on
the Accounts & hosts page, because a profile has two ways in — the token prompt
Connect opens and a code read aloud into a browser — and a reviewer looking for
either looks at the server's card ([[BPR-0526]], [[BPR-0773]]). While a flow
runs, that row *is* the sign-in: the login, the terms and the ways in give it
the whole row, and a renewal that needs a new code sits beneath a signed-in
account instead of replacing it. The panel is bound to the profile the flow was
asked for, and it follows a newly chosen host only once a running flow has
settled, because the core forbids a second flow while one runs; so the token
prompt and the device code can never mean two different hosts. The strip's
account button is what opens the page, and a flow the core begins presents it,
so the code is seen.

The application constructs the device host on every platform and binds its
projection port to a fan-out: every publication reaches the recording sink the
public self-test reads, and then the mounted panel. The panel's own contracts
run at the adapter boundary (`qt_device_authorization_scale_100`, `_125`,
`_200`); its placement, profile binding and gestures are proven in the composed
window (`qt_composed_github_scale_100`, `_125`, `_200`); and that beginning a
flow from the mounted control reaches the device host is proven end to end
(`application_device_authorization_panel_course`, with
`application_device_authorization_panel_absent` as its construction cut).

## Primary actions

Previous/next change and previous/next file are always visible on the review
bar ([[BPR-1129]]), in that semantic order after the comparison's menu, each
pair followed by its position; private progress follows them. Files and
Comments are the title row's toggles, and Hide drawers sits after the
Comments toggle ([[BPR-1295]]). The navigation, drawer and
private-progress presenters are icon-only, but each retains a stable visible
tooltip, accessible name and description, strong-focus stop, and keyboard
shortcut. No text toolbar appears at a narrower width: as the window narrows
the review bar drops the comparison's dates and then the positions' words
("3/40" for "file 3 of 40"), and the title row drops the name's title, its
owner and then the modality's shortcuts, keeping the name's number. Open on
GitHub goes with the name and is counted in its room, so the title elides
around it; it stays beside the number down to the floor ([[BPR-1338]]). No control
folds into an overflow menu at any width down to the platform's floor. The
floor follows the bar ([[BPR-1255]], the owner's choice of 2026-10-06). When a
larger font makes the fully yielded review bar wider than the platform's 900
pixels (902 on Windows), the window's minimum width rises to the bar's width.
It returns to the platform's floor at the default font, and it is measured
again on a font, style, theme or scale change. A restored geometry narrower
than the floor opens at the floor.

At an unchanged window width and appearance, readiness, comparison names, file
positions and change positions keep the review bar's hit targets stationary
([[BPR-1385]]). The comparison summary and both counters occupy reserved slots;
the Updates disclosure reserves its warning mark's space even when no mark is
shown. The slots follow the native font and the bar's width mode. Longer summaries
and positions elide within their slot and retain the complete words in their
tooltip and accessible description. Actual resizing still yields dates, then
position words and finally numbers in the existing order.

A local Review Cycle's review bar (RFC-BPR-0061) keeps the same rule at the
default font ([[BPR-1182]]). One button names the view shown and opens the view
chooser, as the comparison does in the pull request's slot. Show newer is the
refresh glyph and Mark reviewed the private-progress tick, each named in its
tooltip and accessible name. Capture revision, Suggest change and Share notes
keep their words. As the window narrows the view's revisions go first ("Full
source" for "Full source · R2"), then the position's words ("File 2/4"), then
its numbers; the tooltips keep the whole text. At 900 pixels the widest bar,
a named comparison with newer material and open notes, needs 875 pixels on
macOS with nothing yielded. The owner's verdict on this bar is pending.

The file and change arrows are a bounded previous and next that are always
enabled ([[BPR-1328]]). The owner rejected greyed-out arrows ("It doesn't feel
good for next change arrows to get greyed out when there's only 1 change") and
then arrows that crossed into the next file: "I want the arrow keys to be there
and selectable, pressable so the ui doesnt change but I want them to just try
to move to the next change and realize there are no more and effectively do
nothing. Just let it be a bounded prev and next that you can always use." So
while a pull request is open the four arrows and their keys stay enabled. At
the first or last change of the open comparison, Previous or Next change does
nothing: no wrap and no move to another file. At the first or last file,
Previous or Next file does nothing. A press with no comparison open, or in a
comparison with no changes, does nothing too. The core decides it: each
navigation command's projection is enabled with a `Bound` (`at_first_change`,
`at_last_change`, `at_first_file`, `at_last_file`, `no_open_comparison`,
`no_changes`, `no_actionable_files`). RFC-BPR-0076 extends all four commands:
from Overview, Discuss or Threads, a toolbar, View-menu or admitted shortcut
press enters Review through its normal remembered-drawer entry, even at a bound.
The command resolves the active tab's exact file, pair and reading before taking
one ordinary step or showing its exact bound; it never steps the other page's
window comparison and then restores a different tab over it. A bounded command
sends no source-navigation gesture and leaves that source position unchanged.
Inside Review, navigation preserves the current drawer arrangement. Only an
inactive workbench disables the arrows. Their size and place stay fixed. A press at a bound shows
its reason in the status bar for three seconds ("Last change in this file.",
"First change in this file.", "Last file in this pull request.", "First file
in this pull request."). Each arrow's tooltip names it and its first key in the
platform's notation ("Next change (⌥↓)", "Previous file (⌥⌘↑)"); a second line
names the bound, and the status tip carries that line. The stop-versus-wrap
wrap policy, used only by a fixture probe, still wraps inside the comparison.

A clicked source line is the origin for Previous/Next change ([[BPR-1518]]).
Inside a change, either direction steps from that change. On an unchanged line,
Next chooses the nearest following change and Previous the nearest preceding
change in the same file and exact comparison. The clicked side is resolved in
the canonical alignment, so insertions, deletions and unequal line counts cannot
borrow the other side's numeric line. Before the first change Previous keeps the
clicked line; after the last change Next keeps it. Those bounds preserve both
panes' visible positions and show the ordinary bound message. A later click
replaces an earlier change-navigation origin. With no source selection, Next
still enters at the first change and Previous at the last; the fixture-only
wrap policy remains explicit.

An exact tab view that must load retains at most the latest navigation press,
bound to that PR, active tab and exact pair. Review shows immediately. The step
runs once after that view returns; a later file, tab, comparison, modality, Home
or composer gesture cancels it. Unavailable material retains its normal reason
without borrowing a newer pair. Sibling tabs, discussion reading/filter state
and unsent work survive entry. With no tab, navigation uses its ordinary file
order/bounds and entry alone opens nothing. Keyboard entry focuses the visible
comparison when present or the originating arrow when empty/unavailable;
editable multi-line input keeps its native shortcut handling.

Once Review is on the active tab's exact comparison and selected file,
Previous/Next file use its accepted file order even when that file's source is
unavailable. A preceding tree choice becomes the starting file for an arrow
pressed before its load settles. Source failure cannot strand ordinary file
navigation; change navigation still needs the file's comparison material
([[BPR-1517]]).

A press refused because the active tab's exact view is unavailable shows
"This tab's file is unavailable. Choose another file or comparison." for the
same three seconds as a bound, so an arrow never silently does nothing. The
document tabs belong to the pull request they were opened in: opening another
pull request, from Home or a link, starts its tabs again even though the open
first retires the shown pull request ([[BPR-1539]]).

Every tooltip in the application shows directly below the control it names,
left edges aligned and four pixels apart, or directly above it where below
would leave the screen, moved left only to stay on the screen
([[BPR-1354]]). A line longer than about eighty characters wraps. The owner
rejected Qt's placement, at the pointer plus the cursor's height, three times:
"The tooltip feels strangely far away." ([[BPR-1083]] v004), "Whys this
tooltip so far from the button?" ([[BPR-1328]], the title row and review bar)
and "bug: Why is this damned tooltip far away again. This is a consistent
problem." (BPR-1354, the diff's "+"). The control's rectangle is the anchor:

- a widget's own tooltip anchors to the widget, and a child without one shows
  its parent's, anchored to the parent;
- an item of a list or tree anchors to the item, a tab to the tab, a header
  section to the section, and a menu action, in a menu that shows its
  actions' tooltips, to the action's row;
- a control painted inside another widget anchors to its painted rectangle:
  the diff's "+" button, a thread's tinted number, a scroll-bar tick, a mark
  and its rail, an avatar, an Overview chip, a node on the updates line (and
  the line's history statement to the whole line).

A tooltip the pointer asked for goes when the pointer leaves its control, on a
press, a key other than a modifier, or the wheel. One a keyboard reviewer asked
for (an info glyph focused from the keyboard, an Overview chip reached with
Tab, a link reached with Tab, "Nothing new in this file.") stays while the
control keeps focus and goes on the next key or press. Both go after ten
seconds. The system tray icon's tooltip is the operating system's and is not
placed by the application.

The owner settled the keys on 2026-10-08: "Okay final choice: option cmd down
next file. Option down next change", after "let cmd+down scroll to the bottom
and cmd+up to the top". Option-Down and Option-Up (Alt+Down and Alt+Up) are
Next and Previous change, with Option-Page Down and Option-Page Up kept;
Option-Command-Down and Option-Command-Up (Ctrl+Alt+Down and Ctrl+Alt+Up) are
Next and Previous file. They work everywhere in the window except an
editable multi-line editor, which keeps Option-Up/Down (its paragraph moves on
macOS) and Command-Up/Down (its document start and end). The Updates control's
Load earlier epochs and Return to newest epochs, which held Ctrl+Alt+Down and
Ctrl+Alt+Up inside the control, are `Ctrl+Alt+PgDown` and `Ctrl+Alt+PgUp`
(⌥⌘PgDn, ⌥⌘PgUp), so no key is offered twice in the window. In the code
panes the platform's document end and start scroll the comparison to its
bottom and top: Command-Down and Command-Up on macOS, Ctrl+End and Ctrl+Home on
Windows and Linux (`QKeySequence::MoveToEndOfDocument` and
`MoveToStartOfDocument`).

**First and Last change** ([[BPR-1532]], the owner, 2026-10-15).
Option-Shift-Up and Option-Shift-Down (Alt+Shift+Up and Alt+Shift+Down) go to
the first and the last change in the current file. A change is what
Option-Up and Option-Down step through: one change group of the file's exact
comparison, in its order; the comparison shows the whole file and folds or
hides no change, in two panes and in one. The core decides the end through the
change-navigation transition with an outermost reach, from a change, an
unchanged line or no selection; pressed on that end it keeps the selection and
brings the change back into view, with no bound message. The commands are
**View > First change** and **Last change** (mnemonics R and H) after Next
change, in the window and on the empty desktop's bar, and have no review-bar
button. Unlike the four
arrows they are disabled in Review where nothing can be reached (no open
comparison, or one with no changes), the tooltip naming why. From Overview,
Discuss or Threads they enter Review first, as the arrows do
(RFC-BPR-0076), and a Review file without changes shows the three-second
message. The keys are unbound while the focused widget takes typed text (an
editable line or multi-line editor, an editable combo box, a spin box, the
suggestion editor), where they extend the selection on macOS; the suggestion
editor claims them as its box gesture, read-only too. They are unbound as well
while the focus is in a widget that binds the same key itself: the tour
outline reorders stops on them.

The four navigation arrows use the native style's silhouettes with foreground
pixels derived from the row's live active and disabled `ButtonText` roles.
Palette, font, style, and theme changes rebuild those modes. This presentation
refresh changes neither command identity nor projected enablement: disabled
arrows (only with no pull request open, [[BPR-1328]]) remain readable and
visually distinct, while the Files, Comments,
Review details, and private-progress symbols retain their separate native
icons.

`Next actionable` remains available as a secondary command and uses the local actionability projection.

RFC-BPR-0005 implements this shell. The four primary actions are `QAction`s
owned by the window itself, not by the toolbar, which is what keeps their
shortcuts alive when the toolbar reorganises. Their enablement, bounds and disabled
reasons come from the core's command projections; a widget never decides
navigation legality locally, and a bounded or disabled action carries its exact
reason in both tooltip and status tip.

File navigation resolves through the retained accepted Files order and the
existing workbench host selection path. The application supplies the core only
the bounded cursor facts of the exact accepted diff and semantic selection;
the core projects change-command legality. A toolbar change gesture then calls
the comparison surface's narrow `navigate_change(direction, policy)` bridge,
which uses its existing semantic index and the exact projected stop-versus-wrap
policy. The comparison surface has no second Previous/Next presenter, and Qt
never invents enabled state.

| Command | Shortcut |
|---|---|
| Previous file ([[BPR-1328]]) | `Ctrl+Alt+Up` (⌥⌘↑) |
| Next file ([[BPR-1328]]) | `Ctrl+Alt+Down` (⌥⌘↓) |
| View > Go to header/source ([[BPR-1416]]) | `Alt+O` (⌥O) |
| View > Find > Find ([[BPR-1422]]) | `Ctrl+F` (⌘F) |
| View > Find > Next / Previous Match ([[BPR-1422]]) | `F3` / `Shift+F3`; macOS also ⌘G / ⌘⇧G |
| In the pane Find query ([[BPR-1422]], [[BPR-1503]]) | `Enter` / `Shift+Enter` navigate; `Ctrl+Enter` inserts a newline; `Escape` closes; `Alt+W` (⌥W) toggles Whole Word and `Alt+L` (⌥L) Find in Selection, claimed only while the query has focus; `Alt+C` and `Alt+R` stay Comment and Compose review (owner, 2026-10-14) |
| Previous review change ([[BPR-1328]]) | `Alt+Up` (⌥↑), or `Alt+PgUp` |
| Next review change ([[BPR-1328]]) | `Alt+Down` (⌥↓), or `Alt+PgDown` |
| View > First change in the file ([[BPR-1532]]) | `Alt+Shift+Up` (⌥⇧↑) |
| View > Last change in the file ([[BPR-1532]]) | `Alt+Shift+Down` (⌥⇧↓) |
| Scroll the comparison to its top or bottom, in the code ([[BPR-1328]]) | `Ctrl+Home` / `Ctrl+End` (⌘↑ / ⌘↓) |
| Load earlier epochs / Return to newest epochs, in the Updates control ([[BPR-1328]]) | `Ctrl+Alt+PgDown` / `Ctrl+Alt+PgUp` (⌥⌘PgDn / ⌥⌘PgUp) |
| Toggle files drawer | `Alt+Shift+1` |
| Toggle comments drawer | `Alt+Shift+2` |
| Toggle review details | `Alt+Shift+3` |
| Open private review progress | `Alt+Shift+4` |
| Toggle source line numbers | `Alt+Shift+5` |
| Compose review | `Alt+R` (Option-R) |
| Comment, or Reply to the selected thread | `Alt+C` (Option-C) |
| Move the selected line; with Shift, extend the selection ([[BPR-1212]]) | `Up`, `Down`; `Shift+Up`, `Shift+Down`, in a code pane |
| Add comment on the selected lines, in the code ([[BPR-1212]]) | `Return` (keypad `Enter` too) |
| Suggest change on the selected lines, in the code ([[BPR-1212]]) | `Shift+Return` |
| The drawer draft's main action: Start a review, Add review comment while a review is in progress, or keep a pending draft pending ([[BPR-1345]] v002) | `Ctrl+Return` (⌘↩) |
| The drawer draft's Add single comment: post this one comment now ([[BPR-1345]] v002) | `Ctrl+Shift+Return` (⌘⇧↩) |
| Finish review: open the review's summary in Discuss ([[BPR-1345]] v002) | `Ctrl+Alt+Return` (⌘⌥↩) |
| Open the drawer's draft in Discuss, and back to the code ([[BPR-1345]] v002; ⌘⇧↩ before) | `Ctrl+Shift+M` (⌘⇧M) |
| Back from Discuss's composer; leave a drawer draft (kept when written, dropped when empty) | `Esc` |
| Hide or show every drawer ([[BPR-1295]]) | `Ctrl+B` (⌘B) |
| Overview, Review, Discuss ([[BPR-1295]]) | `Ctrl+1`, `Ctrl+2`, `Ctrl+3` (⌘1-⌘3) |
| Threads ([[BPR-1293]], [[BPR-1341]]) | `Ctrl+4` (⌘4) |
| Find a thread: go to Threads, or back ([[BPR-1293]], [[BPR-1341]]) | `Ctrl+J` (⌘J) |
| In Threads, open the cursor's thread in Review, or in Discuss ([[BPR-1356]]) | `Return`; `Shift+Return` (⇧↩) |
| In Threads' list, the cursor's row menu ([[BPR-1356]]) | `Menu`, `Shift+F10` |
| Zoom In on the comparison's code ([[BPR-1336]]) | `Ctrl+=` or `Ctrl++` (⌘= or ⌘+); Ctrl+wheel (⌘-scroll) or pinch out over the code |
| Zoom Out on the comparison's code ([[BPR-1336]]) | `Ctrl+-` (⌘−); Ctrl+wheel (⌘-scroll) or pinch in over the code |
| Actual Size: the comparison's code at Code size ([[BPR-1336]], [[BPR-1535]]) | `Ctrl+0` (⌘0) |
| Home: show Home, or leave it for the review ([[BPR-0650]], [[BPR-1535]]) | `Ctrl+Shift+H` (⌘⇧H) |
| Help > Keyboard Shortcuts ([[BPR-1567]]) | `Ctrl+/` (⌘/) |

Every key in this table, and every other key a window, widget or the
resident desktop menu bar binds, is written once, in the shortcut catalogue
(`src/adapters/qt/src/keyboard_shortcuts.cpp`, [[BPR-1567]]). The bindings
take their keys from it, and Help > Keyboard Shortcuts is generated from it;
see [Keyboard Shortcuts](#keyboard-shortcuts). A new or changed key is a
catalogue entry first.

Actual Size is ⌘0 (`Ctrl+0`), as in browsers and editors. Until the owner
moved Home to ⌘⇧H on 2026-10-15 ([[BPR-1535]]), ⌘0 was Home and Actual Size
had ⌘⌥0 (`Ctrl+Alt+0`); ⌘0 replaced it rather than adding to it, so `Ctrl+Alt+0`
is bound to nothing. ⌘⇧H is Home in Safari and Finder, and no text widget or
macOS system command takes it; ⌘-backtick was considered and rejected because macOS
cycles an application's windows with it. Inside the open label menu ⌘0 is
still Plain ([[BPR-1080]]): the menu is a popup and claims the key before the
window's Actual Size. On Windows, AltGr arrives as `Ctrl+Alt`, and AltGr+0
types `}` on German and Nordic layouts. An editable text field that receives
`Ctrl+Alt` with a typed character claims it in `ShortcutOverride`, so the
character is typed and no window shortcut on that key fires. The read-only
code panes leave such a key to the window.
The three zoom commands sit in View after Updates Line, between separators,
and the resident desktop menu bar offers them disabled with the same keys
([[BPR-0796]]).

View > Go to header/source uses one window action, available in PR Review with
an accepted catalogue and a supported selected file. Its `Alt+O` (Option+O)
shortcut works from read-only source, Files and the tab strip. Editable fields
and composers disable the native menu equivalent while they have focus, keeping
their ordinary Option/Alt input. Home, local Review Cycles, Overview, Discuss
and Threads cannot navigate a hidden retained source. The resident desktop menu
mirrors the item disabled when no window is open.

View > Find searches the complete accepted source in PR Review. Focused split
source chooses Earlier or Later; Review chrome recalls the most recently focused
available source, initially Later or the sole available side. Unified Find
uses actual versions in the displayed order. Find focuses a native multiline
query in a floating panel modelled on VS Code's find widget ([[BPR-1503]]): a
rounded panel with a hairline border, anchored to the focused pane's top-right
corner over the code and in no layout, so opening it moves no line. Its query
field carries the Aa (Match case), underlined ab (Whole word) and .\* (Regular
expression) toggles at its right end; then the count ("2 of 14", "No results"),
previous, next, a Find in Selection toggle and close. An active toggle is an
accent tint with an accent outline. An invalid or unsupported pattern or an
oversized query is an error state on the field, its words in the tooltip and
the accessible status. Where a pane is too narrow, the panel extends left over
its neighbour, then tightens its spacing, then lets Find in Selection give way
to ⌥L, keeping every other control at the supported minimum window width.
Native UI font and inherited palette roles supply the panel. ⌥C and ⌥R stay
Comment and Compose review (owner, 2026-10-14); Match case and Regular
expression take no key. Inside the query, as in every text field, ⌥C and ⌥R
type `ç` and `®` ([[BPR-1536]]); ⌥W and ⌥L stay the query's own toggles.

Selected source text seeds the query, then a word at a real text caret, then
the retained query. As in VS Code, the seeded occurrence is the current match
("1 of N" at a selected first occurrence), not the next one after it. A semantic line cursor supplies only the reading origin.
Regex seeding escapes literal punctuation; an oversized selection retains the
query and reports the 4 KiB input limit. The captured original selection remains
fixed when Find in Selection is enabled. Unified ranges crossing versions are
unavailable with an explanation. Counts, scope, invalid/unsupported patterns,
incomplete coverage and wrapping have accessible wording.

Quiet visible occurrence washes, a current outline and marks on the existing
scroll tracks accompany exact source selection through the shared reveal
controller. The washes are the theme's `find_match_background` and, stronger,
`find_current_match_background`, composited at the selection's opacity and held
to the same contrast floors; the outline is the current-match hue carried to the
indicator ratio, and the text selection is not stacked on the current match. Match traversal uses the accepted immutable record collection.
Escape clears decorations and restores target source focus; Next can reopen
and advance the retained query without another scan. Unrelated editors, modal
dialogs, Home and other modalities retain their keys. The resident desktop menu
mirrors all six Find commands disabled.

View > Find in Files (`Cmd+Shift+F` on macOS, `Ctrl+Shift+F` elsewhere) enters
PR Review from Overview, Discuss or Threads, expands Files and selects its
Search tab. It seeds from selected source text, then the current pane query,
then its own retained query. Pane and PR options stay independent. The native
query and status stay visible; Search Details discloses include/exclude globs,
full comparison OIDs and skipped-side reasons. Search uses all full changed
sources under the active tab's exact pair, independently of Files filters and
private excluded receipts. It never autoopens a typed result.

Click/Return activates an occurrence through ordinary active-tab source opening.
F4 and Shift+F4 navigate the accepted ordered prefix, with exact source-side
focus and explicit wrap. Unrelated editable fields and modal dialogs retain
their keys. Sibling tabs, review drafts, receipts and pinned comparisons remain.
Unavailable material offers Retry under the original exact authority; partial,
cancelled and capped results qualify coverage, and the cap asks for a narrower
query or file filter. Escape returns source focus while retaining PR results;
Close cancels pending query/filter work and restores the prior drawer tab and
collapse state. Counts, scope, invalid input and coverage share accessible
wording. See the [source search contract](../architecture/source-search.md).

The command opens the unique closest same-basename opposite-family file through
ordinary exact file selection, replacing the active tab's file and retaining its
comparison. Siblings, filters, private receipts and actionable order stay as they
were. Exact-directory candidates win; otherwise the greatest number of matching
trailing directory components wins uniquely. A tie or absent match changes no
reading state and announces a brief nonmodal reason. Missing target versions
report the ordinary selected-comparison failure. See the
[navigation contract](../architecture/header-source-navigation.md).

The code-pane keys act only while a code pane itself has keyboard focus
([[BPR-1212]]; owner Q1, 2026-10-06). They are not window shortcuts: the pane
claims them in `ShortcutOverride`, so a text field, the drawer, a menu, Home
and the link map keep their own Return, and Alt+C keeps its reply-first
behaviour. No modifier but Shift is taken; keypad Enter is Return. Each pane's
accessible description names the keys, as Review Cycles' source view does.
The local project's Return and Shift+Return are unchanged ([[BPR-1157]]).

The drawer toggles use `Alt+Shift` rather than plain `Alt+1`/`Alt+2` because
the composed epoch control already holds `Alt+1` through `Alt+3` with
widget-with-children context. Both bindings match whenever that control has
focus, and Qt resolves an ambiguous shortcut by triggering neither, so the
plain-`Alt` form produced two silently dead toggles. No key sequence may be
duplicated anywhere in the assembled window, with one exception: the tour
outline's widget-scoped reorder keys are First and Last change's, and the
window unbinds those two while the outline has focus ([[BPR-1532]]), so the
two never match at once.

`Ctrl+B` is Qt's standard Bold key, which no Qt text widget acts on, so a
window shortcut would fire inside a composer. An editable multi-line editor
(a comment, reply, review or note composer, the suggestion editor, the tour
editor) therefore claims `Ctrl+B` in `ShortcutOverride` and does nothing with
it, as the label body claims ⌘L. Hiding the drawers never takes a composer away
mid-sentence, and the key stays free for Markdown Bold. Single-line fields do
not claim it; when hiding removes the focused field, focus moves to the
comparison ([[BPR-1295]]). On macOS the View menu's item carries ⌘B as a
native key equivalent, and the same claim keeps it from firing there, because
Qt asks the focus object before AppKit's menu takes the key.

The bar contains one presenter for each semantic action at every supported
width. Review details is an item of the comparison's menu. Seen, Reviewed, Accepted, and
Excluded remain the same receipt `QAction` instances and appear in one
keyboard-operable private-progress popup; the popup adds no duplicate policy,
shortcut, or persistent text width. Closing the popup leaves the actions owned
by the window, so their projected state and receipt semantics remain intact.

## Epoch selection

Each upper pane has its own selector. The default semantic comparison is
`Whole PR`: the latest live epoch's exact `Branch base` anchor on the left and
exact head (`Latest`) anchor on the right. Full OIDs remain non-elided and
copyable.

Named comparisons resolve as one operation:

- `Whole PR` selects the latest live epoch's exact base and head;
- `Previous upload -> latest` selects the nearest earlier live epoch with a
  different head and the latest live head;
- `My last reviewed -> latest` selects the exact newest chronologically
  ordered, non-pending current-viewer review commit and the latest live head.

Alias resolution chooses that semantic candidate before checking object
availability. If the exact previous upload or newest review is pruned,
cache-deleted, corrupt, or never observed, the alias remains visibly
unavailable with its reason; it never falls back to an older resident
candidate. A named selection re-resolves only when accepted review facts or
the catalog revision changes.

An explicit pair remains on its exact anchor IDs and OIDs. Any two resident
anchors in the same PR authority and catalog incarnation are legal, including
reverse and identical pairs; identical OIDs represent an empty comparison.
Changing one side of a resolved alias converts the selection to an exact pair.
Cross-authority, cross-incarnation, mixed-object-format, or unavailable pairs
are rejected with a visible reason rather than remapped.

Epoch rows show ordinal, availability, and both the primary label and
independent base/head transition facts. A simultaneous base/head change keeps
both facts, and unknown lineage is shown as `Head change; lineage unavailable`
rather than guessed as an upload or force-push. Append, availability, and
selection changes update stable rows incrementally without a routine model
reset.

The selectors retain one bounded page at a time: at most 100 epochs and 200
base/head anchor rows, independent of the durable catalog size. `Load earlier
epochs` follows only the exact continuation authorized by the core, while
`Return to newest epochs` requests the newest page without a cursor. Replacing
a page uses stable-ID row signals, not a model reset. If the exact selected pair
leaves the resident page, both selector indices become nonresident while the
complete selected OIDs, availability, comparison identity, and accessible
explanation remain visible. No pinned or synthetic epoch row is created.

A live catalog revision does not mix a newly accepted epoch into an older
resident page. The visible page is retained as read-only and identified as
stale; earlier navigation and selector gestures are disabled until the
core-authorized newest request succeeds against the new revision.

The interim loaded pull-request surface now hosts this reusable control. Each
selector lists the exact base and head anchor supplied for a stable epoch, and
read-only text fields expose the complete selected SHA-1 or SHA-256 OIDs for
keyboard or pointer copying. A distinct review anchor selected by `My last
reviewed` remains visible through its exact OID without being presented as a
fabricated epoch. Loading, retained refresh, empty, failed, canceled, pruned,
cache-deleted, corrupt, unavailable, and never-observed states use text as well
as enabled state; color is not their only distinction.

A single secondary `Local history maintenance` menu contains prune selected
epochs, delete repository cache, and cancel maintenance. The core controls
enablement, and prune stays disabled unless the accepted pair holds an epoch
anchor that can actually enter a prune operation, so an already pruned,
cache-deleted, or never-observed selection is never offered a destructive
confirmation it would only fail. A disabled command carries its exact core
reason in the tooltip and status tip rather than only losing its enabled
state. Destructive commands open keyboard-accessible, window-modal
warnings with Cancel as the default and Escape action; the exact stable target
is captured before the dialog opens and revalidated after confirmation. The
dialogs state the retained-history or repository-cache consequence and that
neither GitHub nor a developer worktree is modified.

This RFC-BPR-0003 control is the reusable semantic and accessibility surface,
not final visual acceptance. RFC-BPR-0005 still owns the integrated workbench
presentation-polish and native visual-acceptance slice. RFC-BPR-0006 owns
selectable themes, fonts, syntax presentation, and high-contrast profiles.

## Comparison surface

- Upper panes show both source documents in natural layout with no synthetic
  alignment blanks or invented source rows.
- Each pane numbers its own lines in a gutter at its left edge, on by default.
  The numbers are one-based on screen while every internal coordinate stays
  zero-based, and a pane is never renumbered to agree with the other side — so
  past a one-sided block the same screen row legitimately carries different
  numbers in each pane. The gutter is sized to the document's digit count
  rather than to what is on screen, because a column that grew while scrolling
  would shift the code sideways under the reader. It is the longest number's
  digits times the font's widest digit, with 4 pixels either side and nothing
  else: no column for threads ([[BPR-1335]]). Its colours are the
  `code_gutter_background` and `code_line_number_text` roles, which the
  appearance contrast rules already validate in every profile. The conversation
  anchor marker stays at the text viewport's inner edge, so it sits beside the
  code rather than behind the numbers.
- The numbering is a durable reader preference, reached from **View > Line
  numbers**. Turning it off returns the gutter's whole width to the code rather
  than leaving an empty margin, and turning it back on restores the exact
  geometry it had. The setting survives a restart. Its state belongs to the
  core and its action's checked mark comes from the published projection, so
  the menu entry cannot disagree with what the panes are drawing.
- One retained comparison-space position drives both pane origins and one
  shared vertical scrollbar. Wheel/trackpad input over either pane or the
  center divider, Page/Home/End, scrollbar drag, and programmatic reveal all
  route through that controller; horizontal scrolling remains pane-local.
- The center `DiffLinkMap` paints filled monotonic interval ribbons and
  insertion/deletion wedges attached to actual pane boundaries. Moved-code
  metadata uses markers or a temporary overlay, never permanent crossing
  ordinary ribbons. Its live-palette ordinary fill remains readable against
  the gutter in every built-in profile, while the selected highlight plus
  contrasting edge stays distinct. The composed capture verifies these as
  actual selected and unselected path pixels without changing endpoints.
- A one-sided block pins the empty side at the synchronization rail while the
  populated side advances. Unequal replacements advance proportionally and
  meet at one segment endpoint; reverse input retraces without accumulated
  drift or an input-receiver handoff jump.
- Selecting a line, hunk, ribbon, comment, or defect synchronizes all relevant
  surfaces through the one semantic alignment model.
- A keyboard line cursor ([[BPR-1212]]). With a code pane focused, Up and
  Down move the selected line on that pane's side, and Shift+Up and
  Shift+Down extend the selection from where it began; the cursor is the
  selection's moving end and the semantic selection follows it. With no line
  on that pane, the first press selects the first line in view. The cursor
  keeps itself in view through the shared comparison scroll, never by
  scrolling one pane alone. The mouse and the keyboard share one selection: a
  click selects a line, a drag or Shift-click selects lines in the pane's own
  text selection, and Shift+Up and Shift+Down extend either. The selected
  line wears the current-line band and extended lines the pane's text
  selection. A text selection is the top layer: it is filled solid in the
  theme's selection colour over the line's band, the selected line's wash and
  the intraline emphasis, as Line details fills one, and each selected glyph
  keeps its syntax colour, moved only as far
  as it must to read on the fill at the variant's text floor ([[BPR-1212]]
  v002; the owner, on v001: "are we meant to lose all syntax highlighting when
  we select something?"; [[BPR-1533]], replacing v002's translucent wash, which
  the owner found "quite hard to see", and which the emphasis hid inside a
  change). In the light variant the colour is mixed three parts to one with
  the code background, because at full strength the owner found it "slightly
  overdone on the light" ([[BPR-1533]] v002); Dark and both high-contrast
  variants use it at full strength. A move is announced once, politely ("Line
  12 of the later version selected", "Lines 9 to 13 selected"). In one pane
  the same keys move by rows, and each end keeps its row's side.
- Return comments on the selected lines and Shift+Return suggests a change to
  them, as a draft in the drawer ([[BPR-1212]]; see "Writing a new comment").
  Return, Shift+Return, the code's right-click Add comment and Suggest change,
  and the "+" button take one path. The core decides the selection first
  (`core::decide_comment_range`): several lines must be on one side and inside
  one hunk of the pull request's diff (the change groups with three lines of
  context, as GitHub's diff draws them). A base (left) line takes a new
  comment, GitHub's `side: LEFT`, only once the epoch host has answered that
  the comparison's base document is the merge base's version of the file,
  which GitHub numbers `LEFT` lines in, and only inside a hunk; Suggest change
  stays on the head side ([[BPR-1212]] v002). A refusal opens nothing and says
  why in the status line: a base line whose numbering is not known to be
  GitHub's, a base line outside every hunk, a suggestion on a base line, lines
  on two sides, lines outside one hunk, or, with no line selected, "Select a
  line first: click it, or move to it with Up and Down while the code has
  focus." Otherwise the range's last line is selected, its draft takes the
  range's first line in one edit through the draft editor, and the draft opens
  in the drawer; when the line was selected just now its draft may arrive a
  moment later, and the status line says "Opening a draft on line N…" until it
  does, or why none came. Return always starts on the selection, so Return on
  one line narrows a range draft ending there; Alt+C keeps reopening the
  line's draft as it is.
- The code's right-click ([[BPR-1212]] v002). A right-click on any line
  selects it first, without a left click before it, unless it is inside the
  selection, which it keeps, with its selected line. The menu is headed "Line
  N", "Lines a–b" or "Base line N" and offers Add comment (Return) and Suggest
  change (Shift+Return). A line selected by the right-click is not refused for
  a draft still on its way. An item that cannot apply is unavailable, and a
  row under the items says why in a few words ("This file's base is not
  GitHub's", "Base lines need a changed region", "Suggest change: head side
  only", "Lines must be in one changed region"), with the full reason as its
  tooltip.
- The lower pane is **Line details** ([[BPR-1334]]), RFC-BPR-0005's Beyond
  Compare–inspired line-over-line pane. It shows the current line twice, full
  width: the base version above the head version, each with its line number
  and a −/+ mark in a gutter, and the changed characters highlighted in the
  intraline colours. Rows do not wrap: each is one line, and a row too long
  for the strip scrolls sideways under the strip's one horizontal bar, so the
  base and head rows move together and their columns stay aligned. The gutter
  does not scroll. The bar never covers a row or the gutter: a legacy bar
  takes its own height, and for an overlay bar (macOS's default, which floats
  over the content while it scrolls) the strip keeps the same band free under
  its rows ([[BPR-1334]] v003). A newly shown line opens scrolled to its first changed
  character, with a little of the text before it ([[BPR-1334]] v002; the owner
  rejected v001's wrapping). Each row is coloured as its side's pane colours
  that line ([[BPR-1334]] v004; the owner rejected v003: "there's no syntax
  highlighting"): the base row from the base blob's classification and the
  head row from the head blob's, the spans the panes already hold, painted
  through the same format ranges. The change highlight is a background under
  them, so a changed token keeps its own colour, moved only as far as it must
  to stay readable on the fill, as in the panes. A cut row is coloured only as
  far as it is shown. A text selection in Line details is drawn as the panes
  draw theirs, solid in the selection colour with each glyph keeping its
  syntax colour ([[BPR-1534]]). A text
  selection across lines that holds the current line shows each selected
  line's pair, up to sixteen, then how many more were selected. An unchanged
  line shows both versions plainly. A removed or added line shows its own row
  and says the other side has none ("Not in head"). An unpaired line that
  replaces exactly one line shows that line as its other row, without
  emphasis, because no character pairing was made; one that replaces several
  names them. A line longer than 4,096 bytes is
  cut at a character boundary and says how many bytes follow. The core decides
  the rows, the cut and the highlight spans from the comparison's existing
  mapping and intraline edits; Qt draws them. It is on by default. **View >
  Line Details** and the Comparison row on Reviews and queue hide or show it
  for the profile. Its top edge is a resize seam with the shared cursor hold:
  drag it, or press Up/Down on it to move one text line; double-click it or use
  its context menu to reset the height. The height a reader chose is kept per
  window and survives a restart; a clamp in a small window does not erase it.
  Review details no longer shows or hides it. At its default height it takes
  about eight text lines from the panes; the UIforETW focused-review capture
  hides it through View > Line Details beside the drawers and Review details,
  so that capture's height floors still measure the panes alone.
- Comments are shown beside code in the right drawer, not inserted into the source layout.

Conversation synchronization is exact rather than proximity-based. Selecting a
current root thread, or any reply below it, resolves the root's path, current
line, and GitHub diff side. When the selected comparison proves that endpoint's
revision identity, Qt converts the provider's one-based line to the
comparison's zero-based source index and uses the same semantic line-selection
transition as a source-pane gesture. The shared comparison controller reveals
that line in the correct natural-layout pane. A narrow four-pixel mark at the
pane's inner margin distinguishes the conversation anchor from the ordinary
selected-line wash; the comment body remains in the drawer and creates no
source row.

The reverse path uses the same identity. A reviewer-selected source line filters
the conversation drawer to roots on the exact current file, side, and line;
replies remain attached to their root. A group, connector, or file-level
selection clears the line filter and selected conversation with its marker
while retaining the current-file scope.
Automatic selection created while a diff is first published does not pretend
the reviewer selected that line.

Only a current anchor whose endpoint identity matches the selected comparison
is placeable. The composed GitHub path proves the right endpoint by matching
the selected right commit to the accepted `headRefOid`. It leaves `LEFT`
unplaced because GitHub's pull-request diff uses a merge-base coordinate and the
current provider snapshot does not carry that merge-base identity. Outdated,
file-level, endpoint-mismatched, and selected-comparison-out-of-bounds roots stay
visible at file scope with an explicit reason.
`OriginalLine` is retained as history but is never treated as a current
comparison coordinate, and Qt never guesses a side or revision. Missing or null
provider side and a line thread with neither current nor original line are
malformed provider responses rejected before this presentation boundary;
defensive Qt fixtures still prove their unplaced rendering.

The live GitHub window now composes this selected-file comparison surface with
the production natural-layout panes, link map, shared controller/scrollbar,
semantic previous/next selection reached through the single top-row presenter,
source-line highlight, and lower detail.
Loading, failure, file/epoch/options replacement, authority invalidation, and
whole-cache deletion clear stale geometry by exact request context. Font,
style, palette, screen, DPI, viewport, and divider changes remeasure Qt-local
geometry while retaining valid semantic selection.

A metadata-only accepted pull-request refresh preserves an exact selected diff
and semantic selection when every authority and anchor-driving revision fact
is unchanged. A changed head, base, branch, origin, identity generation,
current-review candidate, file, options, or comparison clears it before late
geometry can appear. Loading, ready, failure, invalidation, and semantic
selection changes expose one combined polite accessible announcement per
presentation change; consecutive identical statuses are suppressed.

This defines functional and rendered-composition acceptance for
RFC-BPR-0004, not the final visual hierarchy. BPR-0083 retains the final-source
public-composition replay and target-native System-theme transition.
RFC-BPR-0005 retains the planned integrated presentation-polish and native
visual-acceptance pass. Target-native System-theme evidence must use the real
platform style and a full selected composition; forced offscreen palettes
remain layout evidence only.

The complete normative interaction is in
[Diff Link Map and Alignment-Space Scrolling](diff-link-map-and-alignment-space-scrolling.md).

## File drawer

Filters include All, Unread, Changed Since Review, Changed Since Accepted, Defects, Needs Verification, and Excluded. Status is represented by label/icon as well as color.

The files drawer and the repositories drawer each carry a text filter above
their tree ([[BPR-0981]], RFC-BPR-0025). Both are resident: they narrow the
rows the drawer already holds and never send a search to a provider. A filter
matches when its letters appear in order, case-folded, in the file's path or
in the repository's name or host, the pull request's title or its number;
each space-separated term must match. A matching file keeps its folders and
a matching pull request keeps its repository row, so parent context is never
lost, and a matching repository keeps every row beneath it. When nothing
matches the drawer says so, names the filter, and offers Clear filter; the
open pull request and the selected file stay exactly where they were, and
clearing makes the selected file and the open pull request's row current again
by their identity without a second open or select. The repositories tree's
current row is the window's open pull request however it was opened, from the
tree, Home or a URL, so an earlier row is never left current to swallow the
gesture that would open it. The filters, the files order selector and Clear
filter are declared focus stops ahead of their trees.

In the files tree a folder's name is DemiBold in the muted ink Overview's
folder rows draw their paths in, the palette's placeholder text, so folders
read apart from files on both surfaces ([[BPR-1529]]); file names keep the
ordinary text ink. A folder draws its file count at the row's right edge only
while it is collapsed: an open folder shows its files instead, and the count
returns when it closes by mouse, keyboard or collapse-all ([[BPR-1530]]). The
folder's accessible text names its count either way. Overview's folder rows
keep their count slot ([[BPR-1363]]).

Both drawers are independently collapsible and resizable. Collapsing hides the
panel rather than resizing it to zero, because a splitter distributes a hidden
child's space among its remaining children; that is what actually returns the
width to the centre. Reopening restores the persisted width, and a drawer
refuses to be dragged below the width floor the host is willing to persist.

Ownership is split by meaning. Collapsed and expanded state is semantic, so the
core owns it and a toggle emits a core intent. Pixel widths carry no semantics,
so the host owns them and a drag emits the host's persistence path. Neither
meaning has a second carrier. Source-pane line-number visibility is split the
same way and for the same reason: the core owns it, the View menu emits an
intent, and the panes draw what the projection says. Drawer contents update
through incremental model signals; a bounded row or state change never resets
the model.

## Menu bar

The window carries a menu bar. **Pull Request** ([[BPR-0990]]) holds the
open pull request's two local gestures: **Follow**, checked while the queue
keeps it because it is followed, and **Save Repository**, offered while its
repository is not saved for the account. Neither reaches GitHub; a pull
request opened by link is only Recently viewed until one of them is used,
and one read signed out offers neither. **View** holds the line-number toggle, the
one-pane toggle, Line Details ([[BPR-1334]]), Hide Drawers ([[BPR-1295]]) and, after a separator, Settings…. The bar was added by
BPR-0457 because the owner asked for a "view menu option" and, offered the
choice between a real menu bar and a checkable entry in the existing toolbar
popup, chose the menu bar. On macOS it merges into the system menu bar and
costs the window no height; on Windows and Linux it takes a row, which is the
price of the same answer being true on every platform. The existing toolbar
popup keeps the private progress actions it already had. **File** holds Quit,
and **Profiles** lists the profiles by name with the current one checked, then
New Window and Manage Profiles…. Home's Open Review in New Window
([[BPR-1342]]) is the same New Window, carrying one pull request. On Windows the strip's application menu
lists the same menus, Pull Request among them. There the bar is hidden, so
its Alt mnemonics are gone; instead `Alt` pressed and released alone, or
`F10`, opens the application menu with File highlighted, as a menu bar
entered from the keyboard would be ([[BPR-1022]]). `Alt` used as a modifier
(any chord above, `Alt+Space`, `Alt+F4`, AltGr text entry or an Alt-click)
opens nothing.

**Option keys and typing** ([[BPR-1536]]). On macOS Option with a character
key types (`®`, `ç`, `†`, `¡`) or starts a dead key (`˜` before `n`). While
the keyboard is in a widget that takes typed text (a field, a comment, reply,
note or composer editor, an editable list, the Review Cycle suggestion editor,
the find query), every Option+character window, button and menu key is that
widget's: ⌥R types `®` instead of composing, ⌥⇧1 types `⁄` instead of hiding
Files. The widget claims the key when it arrives, and the menu bar lifts its
Option+character keys until the keyboard leaves, because AppKit fires a
native menu item's key before Qt sees it ([[BPR-1295]]); tooltips keep naming
the key. Elsewhere, including read-only code, the keys work as listed. Option
with an arrow, Page Up or Down or Return, and Command-Option keys, are not
text and are unchanged. On Windows and Linux Alt is a command key and keeps
these commands in editors; text input there is AltGr ([[BPR-1336]]).

### Keyboard Shortcuts

**Help > Keyboard Shortcuts** ([[BPR-1567]]; owner, 2026-10-16: "Can we add
a help page to list keyboard shortcuts") is Help's first item, before
Diagnostics…, on `Ctrl+/` (⌘/), the key Slack and Google's editors list
their shortcuts with. ⌘? is macOS's own Help-menu search and is not taken;
no Qt standard key, text widget or other binding in the window uses `Ctrl+/`.
It opens one non-modal Keyboard Shortcuts window per workbench window; the
same key or item again raises it, and Esc or Close hides it. The resident
desktop menu bar offers the item disabled, like Diagnostics….

The window lists every shortcut, grouped by area: Navigation, Review and
files, Comments and labels, Find, Views and drawers, Comparison and history,
Zoom, Review tours, Home, Review Cycles and Application. Each row is the
command in words and its keys as key caps, in the platform's notation
(`QKeySequence::NativeText`: ⌘⌥⇧ glyphs on macOS, `Ctrl+Alt+…` elsewhere),
with "or" between alternatives; a keypad Enter that only repeats Return is
bound but not listed twice. A key that acts in one place says where under
its command ("In the tour outline", "In a code pane", "On Overview"), and a
key that a text field takes while you type says "Not while typing": on every
platform Accept file, the change arrows, First and Last change and Hide
Drawers, and on macOS each window-wide Option+character key
([Option keys and typing](#menu-bar)). A footer states that rule. A key with
no binding on the platform (Quit on Windows, the Review Cycle sheets'
Control-C off macOS) is not listed.

The search field has the keyboard when the window opens. Every word typed
must appear in a row's command, place, group or keys (as written natively,
spoken, or in portable text such as `Ctrl+0`); a group with no matching row
is hidden, and when nothing matches, "No shortcut matches “…”." stands in
place of the list. Down Arrow or Return moves from the search to the first
row shown; Tab reaches the list and Close. The list, the search and each row
carry accessible names, and a row's keys read as words ("Option-Command-Up
Arrow" on macOS). The window is resizable, sized from the font, and draws
only from the inherited palette and font: a key cap is a light tint of the
list's text over its base, so it reads as raised in Light, Dark and both
High Contrast themes.

The rows come from the catalogue the bindings are made from, so the page
cannot list a key the application does not bind. `qt_keyboard_shortcuts_scale_{100,125,200}`
enumerates every `QAction`, `QShortcut` and explicit button key in the
assembled window, the resident menu bar and a Review Cycle view, and fails on
a key the catalogue lacks, a key that differs, a binding that lost its key or
a catalogue entry with nothing bound. Keys a widget reads in its own key
handling (Return in the code, ⌘L in a comment editor, the Find query's ⌥W
and ⌥L) are catalogue entries too, and the menu items that only show them
(the code's Add comment, Threads' and Home's row menus) take their keys from
it. No binding object carries those keys, so the enumeration cannot see the
handlers themselves: a change to such a handler changes its entry by hand.

The line-number entry is disabled until a pull request is open, because
line-number visibility is workbench-surface state and an inactive workbench
disables every workbench command. A reader who opens **View** on a freshly
launched window sees the entry greyed.

### The empty desktop

Closing a window closes a window. No chooser, and no other window, is
presented because a window closed (RFC-BPR-0033, amending the last-window
presentation [[BPR-0696]] added). What a desktop with no window does is a
named policy the composition injects, not a platform test the desktop makes
for itself, so both answers are drivable on one host:

- **macOS** stays resident, as macOS applications do, and the Dock icon
  remains. Activating the application, `Ctrl+N` (⌘N), or choosing a profile
  from the Profiles menu opens the most recently used profile's window at Home
  through the existing open intent.

  **The menu bar the close leaves behind.** The application's own menu bar
  carries the same menus a window's bar carries, so the close subtracts nothing
  a reviewer can see: a File menu holding Quit, a Profiles menu listing the
  profiles and then New Window and Manage Profiles…, and the window's own
  **View** — Line numbers, One pane, Hide Drawers ([[BPR-1295]]) and Settings… — offered and disabled,
  because no window's presentation is theirs to change. That is what macOS
  applications offer with no document open, and Settings keeping the
  preferences role is what keeps the merged Preferences item in the application
  menu. Measured from `[NSApp mainMenu]` by
  `application_profile_empty_desktop_native_menu_bar`, which snapshots the bar
  while a window is open and requires the windowless bar to equal it: both read
  `Apple | ~File | Profiles | View`. AppKit merges Quit into the application
  menu and hides the emptied File title — with a window and without, so nothing
  changes at the close. [[BPR-0796]], after the owner rejected the previous
  behaviour on 2026-09-16 with "Closing last window closes menu bar."; until
  then View and Preferences… went with the window.

  **The desktop's bar exists only while the desktop has no window.** Qt hands a
  parentless menu bar to any focused window that carries none of its own, so a
  desktop bar left alive after a window reopened would take the menu bar from
  the workbench the moment a reviewer focused Settings. It is built when the
  desktop empties and retired when a profile window becomes active — that
  moment rather than when the window opens, because that is when the window's
  own bar takes the macOS menu bar, and the course measures a 0 ms gap in both
  directions rather than assuming one.

  "Most recently used" is the last profile whose window was opened or
  focused in this process; nothing new is persisted for it, because an empty
  desktop exists only after a window has been open.
- **Windows and Linux** quit, through the existing desktop Quit path, so
  profile records are saved exactly as an explicit Quit saves them. The
  platform expectation is that a program with no window is gone. The request
  is refused while the registry is still saving the close that emptied the
  desktop, so it is retried until it lands; only a persistently refused
  request forces the shutdown, because a process with no window and no way to
  reach one is the defect [[BPR-0696]] fixed by presenting the chooser.

The restore records already tell the two apart: an explicit close retires that
window from automatic restoration, while Quit retains every live window in
focus order. A profile operation that fails while a window exists is reported
in that window — the Profiles menu grows a status item under a separator,
shown only after a refusal — rather than by presenting the chooser, which is
presented for a failure only when no window exists to carry it.

A refused agent workspace recovery is reported the same way ([[BPR-0817]],
RFC-BPR-0033's owner-delegated amendment). One cause is a folder around the
data root that other users can write; BetterPullReview neither uses nor
repairs such a folder. Whatever the cause, every window of the profile adds
"Disposable agent workspaces are unavailable: their folder needs repair or has
reached its limit." to its status bar. The sentence stays on Home and on every
other surface for as long as the profile runs, and a window opened later
carries it too. The agent panel still says "Not started". The refusal is not a
failure of the quit, so closing the last window on Windows or Linux still
exits 0.

### The Profiles sheet

**Profiles** opens from Profiles › Manage Profiles…, or at startup when no
profile can be opened. It is a sheet of rows: a letter avatar, the profile
name, and its state at the right ("Open in 2 windows", "Opening", "Closing",
"Closed") from the existing projection, which is also the row's accessible
description. The current profile's row is selected when the sheet opens.
Return or a double-click opens the selected profile, and the footer's filled
button says **Open** for a closed profile and **New window** for an open one,
because that is what the gesture does.

**Rename** and **New profile** are footer actions that turn the row into an
editor in place, with Save and Cancel replacing the footer's buttons, so a
name is edited where it is read; the field exists only while an edit is open.
Escape cancels and keeps the text, and a refused edit stays open on exactly
what was typed. A failure is one notice above the rows with a single **Try
again**, and is absent otherwise. There is no permanent name field, no
permanent status sentence and no Reload Profiles button, and nothing announces
"Profile saved.": the renamed row is the confirmation.

## Conversation drawer

The drawer is RFC-BPR-0052's right tray, and its header is its tabs:
**Comments**, then **Notes** ([[BPR-1204]]). It opens on Comments. The
Comments tab holds this file's threads exactly as described below; its filters
and Next new comment sit at the right of the tab row; the tab carries the count
of threads with new comments in the current file as an accent pill and says it
to accessibility ("Comments, 2 threads with new comments in this file"). Both
this pill and the title-row badge are absent without a selected file or when
that file has no unread threads, even if another file does ([[BPR-1511]]). The
count follows the accepted unread policy regardless of the drawer's filters.
Next new comment continues through unread threads across the pull request.
The Notes tab holds private ledger notes, AI tools, side chat replies and the
finding queue. It stays available with no agent configured. Its introductory
sentence says: "Private to you and never published. Write a note or ask an agent about this pull request, then promote what you want to say to a draft in Comments."
One note or selected reply is copied to a local draft at Comment's target; the
source stays, and nothing is published until the existing explicit Publish.
Finding conversions retain RFC-BPR-0012's existing actions. [[BPR-1208]] puts thread
defects on their own cards: right-click, the context-menu key or Shift+F10
opens the thread's actions. A classified thread also has a text badge with its
state, severity and category; its tooltip and accessible description explain
the state and retained context. Reclassify opens the existing severity and
category controls. Previous defect, Next defect and Defects N/M sit beside
Next new comment under Comments; a defect's thread the drawer does not hold
opens in Review on its file, in place, when it is on a file, and in Discuss
when it is not ([[BPR-1293]]). The private tab contains none of them. Choosing Notes is RFC-BPR-0031's explicit
move into the AI tools: when its content is shown, it permits the existing
compare_glance cue, dispatches no agent or review action, and never opens a
collapsed drawer. Under [[BPR-1209]], the single Dif rests at 28 logical pixels in the
status bar's leading corner on every ordinary surface, stepping aside for a
transient message. The first-launch Home and [[BPR-0966]]'s exact offered
Compose workshop remain prime spots. In the workshop Dif opens the header's
row at the corner's 28 pixels, before "Comment workshop" and its Accept and
Reject, and the row is as tall with him as without him, so the composition
starts right under the review bar ([[BPR-1347]]; the 64-pixel slot left "way
too much deadspace around him"). No Dif slot belongs to the private tab. Each tab scrolls on its own, so switching tabs keeps each one's scroll
position and the selected conversation. The tabs are one focus stop and the
arrow keys move between them. RFC-BPR-0039's Tour joins them when it exists.

The Comments tab is inline only (owner verdict on [[BPR-1290]]'s prototype,
2026-10-06: "I like it"). It shows this file's threads as notes in a margin
beside the code, each hanging from its line, and the drawer moves with the
code: it never scrolls on its own, and a wheel over it scrolls the code. Only
when the code can scroll no further does the drawer take the overflow, so the
foot of a tall open thread can still be read. A thread follows its line
however far the line scrolls above the code, so every wheel notch or scroll
bar move brings more of a thread taller than the drawer into view, as Discuss
shows it ([[BPR-1542]]). A closed note shows its first
post's author, avatar, label mark and rail ([[BPR-1083]]), two lines of the
body as written (one when resolved), the reply count with the people who
replied, the count of new posts and ✓ when resolved; a resolved note is faded.
My unpublished draft is a dashed note at its line.

**Writing a new comment** ([[BPR-1212]], owner, 2026-10-06: "I want add
comment to show up in the drawer instead of the full screen add comment
window"). Return on a selected line of the code, Add comment on the code's
right-click menu, Comment (Option-C) on a line no thread is on, and the "+"
button each open the line's draft as a draft thread in the drawer, pinned at
its line, writing. The "+" is GitHub's ([[BPR-1335]], owner, 2026-10-08: "I
don't like the + replacing the line number, i think github actual does it
better"): a small blue button with a white cross on a head line no thread is
on, at the start of the line's code, across the boundary with the number and
never over a digit. It is drawn only while the pointer is over its own
column, the strip across that boundary where it sits; elsewhere on the line's
code or on its digits nothing is drawn ([[BPR-1335]] v002, owner, 2026-10-09:
"i find it somewhat distracting always being drawn. Maybe we only draw it
when the cursor is over the column where it would go?"). Its tooltip is "Add
comment (↩)", directly below the button ([[BPR-1354]]). It takes no keyboard focus; Return
and the context menu are the keyboard's way to the same comment. Pressing it
never reaches the code, so it neither starts nor changes a text selection, and
a press anywhere else in the code is the text's. Dragging it across lines
washes them blue and, on release, comments on all of them, through the same
core decision as Shift and the arrows. Its edge and
notch are blue and a blue bracket marks its line in the gutter: amber is where
you read, blue is where you write. The header holds the line, Draft (or
Suggested change), Maximize and Close; under it the field with the label chip
and its codes as the reply has them; the foot holds a lock with "Private until
you publish", shown whole or not at all (the lock alone where the drawer is too
narrow, with "Only you see this draft until you publish it." as its tooltip;
[[BPR-1212]] v002, the owner: "this 'only you see it unti...' doesnt fit"),
then Delete and the two ways out, all on one row ([[BPR-1345]], the owner's
Accept of `start-a-review-v001`; v002, the owner: "stacked buttons is too much
space"): **Add single comment**, today's Publish, which posts this one comment
now, outlined beside **Start a review**, which keeps it pending, unpublished,
in a review finished later. While a review is in progress **Add review
comment** is the main action and the foot says "Pending until you finish the
review"; a draft already pending offers **Update review comment**, which
closes it still pending, its edits kept as typed. Command+Return is always the
main action (owner, v001: "Start a review, add to review should be the default
for cmd+enter"); Command+Shift+Return is Add single comment; each button's
anchored tooltip names its key. Where the drawer is too narrow for both pills,
the main action keeps the row with a split arrow joined to its right edge, and
the arrow's menu carries Add single comment with its key; nothing stacks or is
cut off. Delete asks first when the draft has words ("Delete this comment?",
or "… from your review?" for a pending one), and deletes the draft from this
computer. Every deletion in the compose flow asks in one shared destructive
question ([[BPR-1345]] v003, after the owner's v002 notes on Delete and
Discard review): a rounded panel in the asking window's own theme with the
question in bold and a close ×; under it, for a comment, its file and lines on
their own line and then the comment in a recessed box as the drawer reads it
(its label's mark in the left gutter, the header in semibold, the Markdown
rendered); then only "This cannot be undone." (owner, v003: "please say cannot
instead of can't"); then a neutral Cancel, the default and focused, beside the
deleting button, named for what it does and drawn in red words on a red tint.
Esc, Cancel and × keep everything; only the red button deletes. Each pending
draft is its dashed note at its line, named "Pending in your review", whichever
line is selected.

Finish review opens the summary in Discuss as "Finish your review", one column
no wider than a comfortable line (v002, the owner: "Make this page beautiful,
and remove some of this duplicate text and reduce the wall of text feel"): the
title; "N pending comments" with the blocking-feedback chip at its end; the
comments in one hairline box, a row each with its file (faint when it repeats
the row above), its lines in the code font and the highlight's ink, and its
first line of words, each row a button that goes to the comment at its line
in Review and each with a Delete that asks first, naming the comment; the
summary in the same box, optional while the review carries line comments; the
three verdicts **Comment**, **Approve** and **Request changes**, which submit
the review at once, each as its own verdict, and are each enabled only when
the core says the review can be submitted so (Request changes waits for a
summary, Comment for a summary or a line comment; a disabled one says why in
its tooltip); "Nothing is published until you submit." said once, with
**Discard review** quiet at the end of the same line, in red words with the
red tint only under the pointer; and "Pull request and account", a disclosure
closed by default, over the host, head and account. The status line shows only
an outcome; Check status shows only when it can act. No separate Submit review step remains: the verdict
pressed is the one submitted, and a verdict's own tooltip already says what it
does before it is pressed. Submit sends one GitHub review whose comments are
exactly the pending drafts (lines, ranges and both sides). Every carried draft
is held for that submission before anything is sent, and is marked published
only when GitHub's review list and comment collection show the review with
exactly those comments; a refusal returns each to pending with its text, and
an unknown answer keeps each held, with its text, until Check status settles
it. When GitHub confirms the review, on Submit or later through Check
status, the page closes and returns to the view Finish review was pressed in
(Review or Discuss), and the status bar says "Review submitted with N
comments" (v002, the owner: "once the review is submitted I just want it to
close and return to the PR"); a refusal or an unknown answer keeps the page
with its words. Discard review asks first in the shared question, naming how
many pending comments it deletes ("Discard your review and its N pending
comments?"); confirmed, it deletes every pending comment of the review and its
summary, and the page closes. A finished review, submitted or discarded, is
never the next one: the next review starts at once with its own comments.
Return in the field is a new line. Esc keeps a
written draft as its dashed note and drops an empty one, and focus returns to
the code on the same line; opening the note writes it again. A draft on
several lines (selected with Shift and the arrows, a drag or a Shift-click)
says "Lines a–b" in its header and the bracket spans all of them; one on the
base side says "Base line N". A right-click outside the selection selects its
line first, and the menu, headed "Line N", "Lines a–b" or "Base line N",
offers Add comment (↩) and Suggest change (⇧↩) above Copy and Add tour stop. Suggest change starts the draft as a
```` ```suggestion ```` block holding the selected lines exactly, the caret
at the end of its last line, set monospaced and unwrapped, and shows what the
block changes as it is edited. Neither key nor item acts on lines the core
refuses together, Suggest change does not act on the base side, and the menu
says why in a row under the items. Maximize (Command+Shift+M) opens the
same draft in Discuss, where the full composer lives: the conversations narrow
to a list at the left, and at the right the draft's lines with three either
side under the same bracket, and the full editor. Back to the code sits at the
top right, its arrows at the far right where the drawer's Maximize was
([[BPR-1212]] v002, the owner: "the restore (back to code) should still be in
the top right"). The same key, Esc or Back to the code
returns it to the drawer with its text and caret. The full-page comment
window is no longer a destination of its own (AC5, amended). Local projects
keep their own flow ([[BPR-1212]] Q2: B).

A thread opens in place: one at a time, pinned exactly at its line, with the
line washed amber in the code, its line number barred amber and the open card's
amber edge and notch pointing at it. The other notes make room above and
below. The open thread shows every post in posting order with its avatar,
mark, rail, unread dot and complete body, and a reply that answers anything
but the thread's first post names whom it answers. Each body reads through the
shared Markdown reader in place (RFC-BPR-0036; [[BPR-1290]] v005, after
[[BPR-1299]] found posts shown as written): headings, lists, tasks, quotes,
code, tables and links rendered, the post as tall as its content with no
scrolling of its own, wide code and tables scrolling sideways. A recognized
label header and its decorations stay as written at the front, in semibold
([[BPR-1083]]). Hovering a post or moving focus into it shows View source and
Copy source in its name row; links open through the application's admission.
Tab from the notes enters the open thread's first post, Tab walks its links
and then the next post, and in a post's text the arrows move the reading
caret, which the drawer keeps in view. What is new is
said in the header: beside the line number a blue "N new", the closed note's
count kept while the thread is open. A New rule is never a thread's first
element: it stands only where a read post is followed by the first new one, in
place of the hairline between them and inset inside the card, whose edge runs
through it ([[BPR-1290]], the owner's v002 note: a rule under the header "kind
of looks like it belongs to the thread above it"). When the first post is
already new, the header's count and the posts' unread dots say so and no rule
is drawn. Its header row has Show line, Resolve or Unresolve, Mark read, More
(the thread menu) and Close; my draft, opened, is its composer (above). Reply (the field, or R) opens an editor in place at
the foot of the thread; the reply is the same draft the composition entry
writes, and nothing is published until Publish (Command+Return). The reply
has the composer's own label controls ([[BPR-1083]]): the label chip and its
menu (Command+L). The decoration toggles stay in the chip's menu, since the
drawer is too narrow for them beside the chip ([[BPR-1290]]). A typed `:xx:`
code stays the text typed: the owner removed the codes on 2026-10-16
([[BPR-1565]]). Comment
(Option-C) on a thread's line writes its reply in place the same way. A file-level compatibility
comment states in the open thread why it is placed at its line (RFC-BPR-0022).

Threads above or below what the drawer shows are chips on its top and bottom
edges, nearest first, three at most and then a count; choosing one opens it at
its line. The code's scroll bar carries a tick per thread, amber for the open
one, blue for one with new posts, outlined for my draft. A plain press on the
bar pages or drags as any scroll bar does; Shift-click on a tick scrolls the
code to its line and opens its thread, as pressing its note does
([[BPR-1290]], the owner's v001 note). Where ticks crowd, the tick whose centre
is nearest the pointer, within a few pixels of its edge, is the one chosen; its
tooltip says "Shift-click to go to line N: …", and with Shift held over it the
pointer is a hand, the tick is ringed and the tooltip shows at once, without
the hover delay a tooltip otherwise has (the owner's v002 note). A modifier key
alone does not take a tooltip away. In the code's gutter a thread's line
number keeps its place and its cell is tinted ([[BPR-1335]], owner's option
1): amber, blue when the thread has new posts, lighter when resolved, and
outlined, dashed, for my unpublished draft. Pressing anywhere on the tinted
cell opens or closes the thread, and its tooltip describes it. A selected line
with a thread says so in its accessible description, and the code's context
menu offers **Open thread** for it. Keys in the drawer: Up and Down move between threads, Return opens,
Esc closes, R replies, N opens the next thread with new posts (Shift+N the
previous), and Shift+F10 or the context-menu key opens the thread's actions.
The drawer is 375 pixels wide by default and drags from 280 to 760.

The filters are **Only new** and **Hide resolved**; the open thread is never
filtered out. Finding a conversation anywhere in the pull request is
[[BPR-1293]]'s, and pull request comments that are not on a file are
[[BPR-1294]]'s. The current comparison filters roots to the exact file. A
reviewer-selected source line narrows the selection authority's forest to the
exact diff side and line; selecting a thread drives the reciprocal code reveal
described above. The first thread the drawer makes current by itself is not a
selection: it moves no code and opens nothing, and a comparison published
again reveals only a thread the reviewer chose ([[BPR-0985]]). Otherwise every
file's first thread would take the reader's line, and a resumed reading would
lose its saved line to it. Unplaceable threads remain readable at file scope,
after the placed ones, and state why no code marker is drawn. Retained
historical navigation requires a revision identity and remains separate work;
an outdated thread is never placed against the current panes by reusing its
original line.

Discuss borrows the same notes for the centre: every thread of the pull
request (or of one chosen file) in file order under its path, with Discuss's
scope, file and Unread and Unresolved filters, each note opening in place as
it does beside the code.

**Discuss's top is one band** ([[BPR-1337]], owner note 2026-10-08: "The top
section of discuss could use some beautification"). It is drawn in the language
of Threads' bar ([[BPR-1293]]): 30-pixel chips with a 7-pixel corner, a
hairline edge and the signal's soft fill when on, on the panel's base with a
hairline under the band. From the left: the scope as two segments, **Pull
request** and **File**, with the file chosen in a menu right after File while
File is on (hidden otherwise); Only new as a chip with the ring-and-dot glyph
and the count of threads with new posts in blue; Hide resolved as a chip with
the struck ring; an Overview count's drill-down ([[BPR-1089]]) as a pill with
the count's mark, its caption and a ×. At the right end are the counts, in
semibold numbers with quieter words ("39 threads  27 unresolved"), and
Clear filters as a funnel icon. The band wraps onto more rows when the list is
narrow, as it is beside the composer ([[BPR-1212]]). Every control keeps its
accessible name ("Whole pull request", "File scope", "Thread file
scope", "Unread only", "Unresolved only", "Clear filters", "Show every
thread", "Threads summary") and has a tooltip. The counts' accessible
text is the whole sentence, with the pathless-content note in File scope. Tab
walks scope, file, filters, the drill-down's ×, Clear, then the notes. The two
scope segments are one stop, as radio buttons are: the arrows choose. A focus
ring shows on keyboard focus only. The availability line's ordinary count
("34 review threads available.") moves into the counts' tooltip. Any other
availability line (threads unavailable, public preview) stays shown above the
band. Beside the code nothing changes: the filters are the tab row's two icons.

### The thread map

The drawer no longer draws a thread map (owner verdict on [[BPR-1290]]'s
inline drawer prototype, 2026-10-06: "I like it"). A deep thread is read in
place, every post in posting order, and a reply that answers anything but the
first post names whom it answers. Finding a conversation across the pull
request, which is what the owner wanted the map for ("Not to move around a
single conversation, I mean to find the right conversation"), moves to a view
of its own, [[BPR-1293]].

The Treeverse-style map the owner chose on 2026-08-21 ([[BPR-0418]]) was kept
as a widget, `github_thread_map`, for BPR-1293 to reuse. Threads draws a
thread's branches as rows instead, as prototype v2 does, so the map was
not reused and is removed with its tests ([[BPR-1293]]). What it held is kept
where it applies: the finder lays rows out in pre-order with an explicit stack,
traces the path from the first comment to the picked post in amber, and an
unexpected shape (a parent that is absent, later or itself) costs nesting,
never a post.

Selection still has one owner: the drawer's conversation model and its
current index, which the code reveal, the anchor marker, the reply target and
the defect surface read. The notes hold no selection authority of their own;
opening one sets that current index.

## Profile storage and privacy

The profile Settings shell mounts **Storage & privacy** independently of GitHub
sign-in. **Preview diagnostics** renders the exact frozen metadata document;
**Export preview…** chooses a new local file. Existing files are refused and
there is no upload action. Seven actions follow core command projections; status
updates preserve an unchanged preview's selection/scroll and never reset the
review models.

**Prepare diagnostic cleanup…** and **Prepare profile purge…** warn that all
work in that profile stops and unsaved work must be finished first. Preparation
deletes nothing. A second confirmation binds the displayed profile and complete
prepared inventory; Cancel is default/Escape at both stages. An authority or
selection change dismisses a stale dialog. Cancellation after preparation does
not restart the profile, and an irreversible failure can require recovery.

The terminal distinguishes local export, cleared diagnostics, purged/stopped
profile, cancellation, refusal and recovery-required cleanup. Residuals include
shared registry/updater data, exports and sibling profiles. Follow the
[operator steps](../development/storage-and-diagnostics.md) and
[typed maintenance contract](../architecture/release-maintenance.md). Component
rendering/public-path evidence does not supply the still-pending complete native
accessibility, vault, purge or uninstall qualification.

## Command placement

Review submission actions remain visible in a persistent action region but do not displace file/change navigation. RFC-BPR-0052 makes that region the review bar. Private receipt progress stays available through the compact keyboard-operable popup beside Review details. Destructive or shared mutations require clear target and state presentation.

## Empty, loading, and failure states

Every pane has explicit loading, canceled, permission-denied,
unsupported-capability, unavailable-revision, binary, oversized,
authorization-required cache, and retry states. Cached private content never
opens for an unauthenticated or different account. No spinner may conceal a
permanent error.

**No file open** ([[BPR-1287]]). With a pull request open and no file
comparison chosen, the comparison area shows no empty panes and no line
numbers. It says "No file open", how many files there are to choose from in
Files, and that ⌥⌘↓ (Next file) opens the next one, with two buttons: Open first
file, which is Next file's own gesture, and Show files, which opens the Files
drawer and focuses it. A pull request with no actionable file says "No files to
read" and offers no Open first file. The comparison tab and the status line say
the same; a reason the host gives (a refusal, an invalidated comparison) is
still shown as before. The review details still open below on request.

Owner hand-back 49 amends the review bar's last width yield: when compact
positions still cannot fit, their numbers yield into the tooltips. File and
Changes remain before their arrows, joined by a separate accent rail.

## Comments on several lines

[[BPR-1212]]. A pull-request draft anchors to a first line, a last line and a
side. The anchor stays the last line, which is GitHub's `line`; the first is
`review_comment_draft::StartLine`, GitHub's `start_line`, absent for one line.
A range is a new comment on the right side whose first line precedes its
last; a reply has none, and answering a thread drops a draft's first line.
The host refuses any other range before publication, and the GitHub adapter
refuses it before the wire.

Publication sends `start_line` and `start_side` after the single-line fields,
in the same sorted key order; a single line sends exactly the bytes it always
did, and its idempotency key is unchanged. A range's key and wire fingerprint
differ from its last line's. GitHub's answer is the range's receipt only with
the exact first line and side, and a single line's only without them. A range
never falls back to a file comment with one line's link: GitHub's refusal of
it is `incompatible_body`, with the text kept. A stored draft on several lines
is version 2 of the private draft record, with its first line after the
version 1 fields; single-line drafts keep their version 1 bytes, and every
earlier row reads as one line.

## Source-line comments outside a hunk

Use the same line gesture, draft editor, Publish control and pending announcement
on changed and unchanged current/right lines of a changed text file. Publication
selects the canonical returned conversation at its exact semantic source line
after refresh. The reviewer does not select a representation. A required file
fallback for line-only suggestion syntax reports typed incompatibility before
any file POST; it does not silently remove suggestion semantics.
Before Publish, a small mark beside it says whether a body's suggested change
goes to GitHub as its own suggestion on this line or is text only there, and
after such a refusal it marks that exact line text only (BPR-1084; the
Conventional Comments delivery contract, "Suggested changes").

A compatibility conversation normally shows reviewer prose. Its collapsed
**Support details** explains that GitHub stores a file review comment and the
visible code link supplies placement. The disclosure is keyboard operable and
accessible; expanded text can require scrolling in a narrow drawer. Native and
file subjects use the same focus and selection behavior. Three-scale, three-theme
rendered evidence is in the [RFC-0022 ledger](../verification/RFC-BPR-0022.md).
Production fallback remains unavailable until an exact unsupported response
contract is admitted; the UI does not infer support from host name or version.

## Draft status on Home

[RFC-BPR-0067](../rfcs/RFC-BPR-0067-home-draft-pull-requests.md) adds a neutral outlined **Draft** chip immediately before a draft PR’s identity/title in every queue section and Recently viewed. It uses the row’s native font and selection ink, remains complete while the identity/title wrap, and adds no action or keyboard stop. Accessible row text says draft pull request before its identity. Only last accepted exact-authority metadata establishes it: ready removes it, unknown (including older stored entries) has no chip, and cached rows retain it under existing freshness reporting. Account chips, Reviewing, attention, actions, opening and authored section placement retain their behavior.
