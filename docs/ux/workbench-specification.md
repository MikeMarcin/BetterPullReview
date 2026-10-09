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
├ Review bar: ▸ Updates  Comparing …, dates ▾ | File 3 of 40 « » | Changes 3 of 11 ‹ › | progress ▾ ········ Compose review | Comment ┤
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
comments toggle (which carries the count of threads with new comments), the
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

**Modalities** ([[BPR-1295]], owner direction 2026-10-06). The modalities are
Overview, Review and Discuss, on `Ctrl+1` to `Ctrl+3` (⌘1 to ⌘3), set per
modality rather than by position, and Threads on `Ctrl+4` (⌘4) since
[[BPR-1293]]. The owner, 2026-10-06, verbatim: "can we swap discuss to 3 and
conversations to 4". The strip reads in its keys' order: Overview, Review,
Discuss, Threads. Read is merged into Review: every path that entered
Read (an Overview file row, Continue review, Open in code, Resume, a link to a
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
drawer and its "Comments (n)" toggle keep their names. Threads is added and
Discuss is kept: the modalities are Overview, Review, Discuss and Threads, on
`Ctrl+1` to `Ctrl+4` (⌘1 to ⌘4); Discuss keeps ⌘3 and its behaviour and
Threads takes ⌘4 (the owner's swap, above). `Ctrl+J` (⌘J) goes straight to Threads from Overview, Review
or Discuss, as does the finder button at the top of the Comments drawer. It puts the finder of
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
`#pullrequestreview-` link, an older-pair tab's Show them, a Discuss card's Open
in code and the review page's preflight rows land where they did.
The **review bar** is full width under it and always shown with a review: from
its left the Updates disclosure, the comparison's menu naming the comparison and its
dates, File and its position before the file arrows, Changes and its position
before the change arrows, and private progress as an icon whose tooltip names the file's
state; at its right **Compose review** (Option-R, Alt+R) and **Comment**
(Option-C, Alt+C) as icons whose tooltips name the command and its key. Compose
review carries an amber mark while a review draft is kept. Review details is
reached from the comparison's menu. Both composition commands are also in the
Pull Request menu. Compose review opens the whole-review summary in Discuss,
beside the conversations (owner's Q3 answer on [[BPR-1212]]: "A: move to
Discuss"), and Esc or Back returns to where it was opened. Each navigation
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
reachable at any time through Home in the strip (`Ctrl+0`, which macOS
presents as ⌘0). It is the window ground edge to edge rather than a narrow
column, and RFC-BPR-0032 gives it one job per row.

- **The strip.** Home at the left; at the right, before a divider, the
  account and settings buttons. While Home is shown and an account can fill
  its list, Group, State headers, Filter and the refresh glyph stand before
  them. Signed out those four act on nothing, so they are absent rather than
  disabled, which is how RFC-BPR-0028's rule that the strip carries only
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
  the pull request the window already shows reads nothing again: the window
  returns to its review and keeps its view, and a Recently viewed row of that
  pull request lands on Overview the same way. `/files` and `/commits` are
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
- **The list** is always in state order — Needs me, then Waiting on others,
  then Following — from the core's attention projection. Qt invents no
  urgency, and Group only changes the panels around the rows. A pull request
  nothing has been read about yet sits in Following with the reason word
  Unknown and "Nothing about it has been read yet", never Quiet, and the
  Quiet-only filter does not show it ([[BPR-0989]]).
- **A row** is the identity, the title, a second line that says why and ends
  in its one-word reason, and at the right the time and my reading progress.
  Recently viewed uses the same anatomy, so a row reads the same way wherever
  it appears. Colour means state and nothing else: the reason word, and
  either a 3-point mark at the row's left or the rail of the panel holding
  it. Row text wraps rather than eliding, and the style paints its own
  selection, hover and focus.
- **Panels.** With State headers on — the default — each state is a bordered
  card with a rail in its colour and a tinted header carrying the name and
  the count. A header sticks to the top of the scroll area while its panel is
  in view. Every panel collapses to its header by chevron, Space, Return,
  Left and Right, keeps its count and a one-line summary while collapsed, and
  announces its expanded or collapsed state. An empty state is not shown, and
  when a signed-in window's ready queue is empty Needs me holds one sentence
  — "Nothing needs me. Pull requests from saved repositories and observer
  rules appear here." — whose link, on "saved repositories", opens Settings ›
  Repositories; observer rules are reached from Filter's Open Reviews & queue.
  No empty
  box is drawn in any arrangement.
- **Group** is a strip menu of None (the default), Organization, Repository
  and Account. A group is a neutral panel headed by an initial tile — round
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
- **Filter** is a strip menu of exactly four sections over the surviving
  typed queue preferences — My role, Added by, Quiet items, and Sort within a
  group — each an exclusive choice listing every value of its vocabulary, so
  an item cannot be unticked into nothing. Repository and Bucket are no
  longer filters because Group and the state order do their work. The menu's
  footer says that nothing there changes GitHub and that observer rules are
  in Settings › Reviews & queue, and offers the way there. These are the same typed controls Settings
  presents as queue defaults, not a mirror of them.
- **Recently viewed** is the last panel in every arrangement, and Resume is
  its rows' action: on hover, on focus and in the row's menu. It is governed
  by the entries the composition gave the window rather than by the account,
  so a signed-out window still lists its public reading.
- **Opening a row** ([[BPR-1342]], the owner, 2026-10-08: "I should be able
  to open a review and open a review in a new window from the home, including
  from right click on a pr"). A click or Return opens the row's pull request
  in this window, on Overview. ⌘Return (Ctrl+Return off macOS), ⌘-click
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
  (RFC-BPR-0024). Open Review in this window keeps [[BPR-0987]]: a window
  already showing it returns to it without reading again.
- **The status bar** on Home carries the session line ("Signed in as
  MikeMarcin on github.com.") and never "No pull request is open in the
  workbench.", because Home is what the window shows when none is. The
  session projection carries one account on one host and no refresh time, so
  the line says that much and no more.

Group, State headers, Filter and the collapse of the panels that carry no
name are remembered per window, in the per-window table of the workbench
layout store (schema 8). A group panel's own collapse is not: its key would
name an owner, a repository or an account, which that store has never held
([[BPR-0189]]), so it lasts only while its window is open.

First launch and signed out: the strip with Home, account and settings only;
the bar relabelled "Paste a public pull request link to read it without an
account"; one card, **Sign in to GitHub**, with a sentence, Sign in as the
page's one filled button and Add enterprise server beside it as a bordered
one — the two ways in RFC-BPR-0018 requires, without opening Settings; and
the status bar line "Not signed in. Public pull requests open read-only."
No list, no panels, no filters.

Home over an open review keeps the way back: one row above the bar naming
what is being reviewed and how far through it, with **Back to the review**
(⌘0) as the filled button at its right. Home beneath it is unchanged, and
whatever had focus in the review has it again when the review returns.

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
Conversations are the title row's toggles, and Hide drawers sits after the
Conversations toggle ([[BPR-1295]]). The navigation, drawer and
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

The change arrows continue across files ([[BPR-1328]]). At a file's last
change, Next change goes to the first change of the next actionable file; at
its first change, Previous change goes to the last change of the previous one.
The destination is the file Next or Previous file would choose, and a file with
no changes is passed through. A change arrow is disabled only at the first
change of the first file ("First change in the pull request.") and the last
change of the last ("Last change in the pull request."), so a file with one
change keeps both arrows live. The stop-versus-wrap wrap policy still wraps
inside the comparison. Each arrow's tooltip names it and its first key in the
platform's notation ("Next change (⌥↓)", "Previous file (⌥⌘↑)"); a second line
names the file a crossing goes to ("Continue to the next file: …") or says why
the arrow is disabled, and the status tip carries that line.

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

The four navigation arrows use the native style's silhouettes with foreground
pixels derived from the row's live active and disabled `ButtonText` roles.
Palette, font, style, and theme changes rebuild those modes. This presentation
refresh changes neither command identity nor projected enablement: disabled
arrows remain readable and visually distinct, while the Files, Conversations,
Review details, and private-progress symbols retain their separate native
icons.

`Next actionable` remains available as a secondary command and uses the local actionability projection.

RFC-BPR-0005 implements this shell. The four primary actions are `QAction`s
owned by the window itself, not by the toolbar, which is what keeps their
shortcuts alive when the toolbar reorganises. Their enablement and disabled
reasons come from the core's command projections; a widget never decides
navigation legality locally, and a disabled action carries its exact reason in
both tooltip and status tip.

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
| Previous review change ([[BPR-1328]]) | `Alt+Up` (⌥↑), or `Alt+PgUp` |
| Next review change ([[BPR-1328]]) | `Alt+Down` (⌥↓), or `Alt+PgDown` |
| Scroll the comparison to its top or bottom, in the code ([[BPR-1328]]) | `Ctrl+Home` / `Ctrl+End` (⌘↑ / ⌘↓) |
| Load earlier epochs / Return to newest epochs, in the Updates control ([[BPR-1328]]) | `Ctrl+Alt+PgDown` / `Ctrl+Alt+PgUp` (⌥⌘PgDn / ⌥⌘PgUp) |
| Toggle files drawer | `Alt+Shift+1` |
| Toggle conversations drawer | `Alt+Shift+2` |
| Toggle review details | `Alt+Shift+3` |
| Open private review progress | `Alt+Shift+4` |
| Toggle source line numbers | `Alt+Shift+5` |
| Compose review | `Alt+R` (Option-R) |
| Comment, or Reply to the selected thread | `Alt+C` (Option-C) |
| Move the selected line; with Shift, extend the selection ([[BPR-1212]]) | `Up`, `Down`; `Shift+Up`, `Shift+Down`, in a code pane |
| Add comment on the selected lines, in the code ([[BPR-1212]]) | `Return` (keypad `Enter` too) |
| Suggest change on the selected lines, in the code ([[BPR-1212]]) | `Shift+Return` |
| Publish the draft written in the drawer | `Ctrl+Return` (⌘↩) |
| Open the drawer's draft in Discuss, and back to the code | `Ctrl+Shift+Return` (⌘⇧↩) |
| Back from Discuss's composer; leave a drawer draft (kept when written, dropped when empty) | `Esc` |
| Hide or show every drawer ([[BPR-1295]]) | `Ctrl+B` (⌘B) |
| Overview, Review, Discuss ([[BPR-1295]]) | `Ctrl+1`, `Ctrl+2`, `Ctrl+3` (⌘1-⌘3) |
| Threads ([[BPR-1293]], [[BPR-1341]]) | `Ctrl+4` (⌘4) |
| Find a thread: go to Threads, or back ([[BPR-1293]], [[BPR-1341]]) | `Ctrl+J` (⌘J) |
| Zoom In on the comparison's code ([[BPR-1336]]) | `Ctrl+=` or `Ctrl++` (⌘= or ⌘+); Ctrl+wheel (⌘-scroll) or pinch out over the code |
| Zoom Out on the comparison's code ([[BPR-1336]]) | `Ctrl+-` (⌘−); Ctrl+wheel (⌘-scroll) or pinch in over the code |
| Actual Size: the comparison's code at Code size ([[BPR-1336]]) | `Ctrl+Alt+0` (⌘⌥0) |

Actual Size cannot take ⌘0 (`Ctrl+0`). That key is Home, and inside the open
label menu it is Plain ([[BPR-1080]]). Actual Size takes ⌘⌥0 (`Ctrl+Alt+0`)
instead. ⌘⌥ is the modifier pair the comparison control already uses for its
own commands, and the label menu matches ⌘0 exactly, so ⌘⌥0 never chooses
Plain. On Windows, AltGr arrives as `Ctrl+Alt`, and AltGr+0 types `}` on German
and Nordic layouts. An editable text field that receives `Ctrl+Alt+0` with a
typed character claims it in `ShortcutOverride`, so the character is typed and
the zoom is left alone. The read-only code panes leave the key to Actual Size.
The three zoom commands sit in View after Updates Line, between separators,
and the resident desktop menu bar offers them disabled with the same keys
([[BPR-0796]]).

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
duplicated anywhere in the assembled window.

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
  would shift the code sideways under the reader. Its colours are the
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
  selection; nothing new is drawn. A move is announced once, politely ("Line
  12 of the later version selected", "Lines 9 to 13 selected"). In one pane
  the same keys move by rows, and each end keeps its row's side.
- Return comments on the selected lines and Shift+Return suggests a change to
  them, as a draft in the drawer ([[BPR-1212]]; see "Writing a new comment").
  Return, Shift+Return, the code's right-click Add comment and Suggest change,
  and the gutter's "+" take one path. The core decides the selection first
  (`core::decide_comment_range`): a new comment needs the right side; several
  lines must be on one side and inside one hunk of the pull request's diff
  (the change groups with three lines of context, as GitHub's diff draws
  them). A refusal opens nothing and says why in the status line: a left line
  (the reason Comment gives), lines on two sides, lines outside one hunk, or,
  with no line selected, "Select a line first: click it, or move to it with Up
  and Down while the code has focus." The right-click items are unavailable
  with the same reason. Otherwise the range's last line is selected, its
  draft takes the range's first line in one edit through the draft editor,
  and the draft opens in the drawer. Return always starts on the selection, so
  Return on one line narrows a range draft ending there; Alt+C keeps reopening
  the line's draft as it is.
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
  rejected v001's wrapping). A text
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
of threads with new comments as an accent pill and says it to accessibility
("Comments, 2 threads with new comments"). The Notes tab holds private ledger notes, AI tools, side chat replies and the
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
Compose workshop remain prime spots. No Dif slot belongs to the private tab. Each tab scrolls on its own, so switching tabs keeps each one's scroll
position and the selected conversation. The tabs are one focus stop and the
arrow keys move between them. RFC-BPR-0039's Tour joins them when it exists.

The Comments tab is inline only (owner verdict on [[BPR-1290]]'s prototype,
2026-10-06: "I like it"). It shows this file's threads as notes in a margin
beside the code, each hanging from its line, and the drawer moves with the
code: it never scrolls on its own, and a wheel over it scrolls the code. Only
when the code can scroll no further does the drawer take the overflow, so the
foot of a tall open thread can still be read. A closed note shows its first
post's author, avatar, label mark and rail ([[BPR-1083]]), two lines of the
body as written (one when resolved), the reply count with the people who
replied, the count of new posts and ✓ when resolved; a resolved note is faded.
My unpublished draft is a dashed note at its line.

**Writing a new comment** ([[BPR-1212]], owner, 2026-10-06: "I want add
comment to show up in the drawer instead of the full screen add comment
window"). Return on a selected line of the code, Add comment on the code's
right-click menu, Comment (Option-C) on a line no thread is on, and the "+"
the gutter shows on a head line under the pointer each open the line's draft
as a draft thread in the drawer, pinned at its line, writing. Its edge and
notch are blue and a blue bracket marks its line in the gutter: amber is where
you read, blue is where you write. The header holds the line, Draft (or
Suggested change), Maximize and Close; under it the field with the label chip
and its codes as the reply has them; the foot says "Only you see it until you
publish" and holds Discard and **Publish** (Command+Return), whose tooltip says
it posts this one comment now. Return in the field is a new line. Esc keeps a
written draft as its dashed note and drops an empty one, and focus returns to
the code on the same line; opening the note writes it again. A draft on
several lines (selected with Shift and the arrows, a drag or a Shift-click)
says "Lines a–b" in its header and the bracket spans all of them. A
right-click outside the selection selects its line first, and the menu,
headed "Line N" or "Lines a–b", offers Add comment (↩) and Suggest change
(⇧↩) above Copy and Add tour stop. Suggest change starts the draft as a
```` ```suggestion ```` block holding the selected lines exactly, the caret
at the end of its last line, set monospaced and unwrapped, and shows what the
block changes as it is edited. Neither key nor item acts on the base side or
on lines the core refuses together, and the menu says why. Maximize
(Command+Shift+Return) opens the same draft in Discuss, where the full
composer lives: the conversations narrow to a list at the left, and at the
right Back to the code, the draft's lines with three either side under the
same bracket, and the full editor. The same key, Esc or Back to the code
returns it to the drawer with its text and caret. The full-page comment
window is no longer a destination of its own (AC5, amended). Local projects
keep their own flow ([[BPR-1212]] Q2: B).

A thread opens in place: one at a time, pinned exactly at its line, with the
line washed amber in the code, its gutter marker ringed and the open card's
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
menu (Command+L), and with the label helper on, a lone `:` opening the label
completion and a typed `:xx:` code becoming its label, as `:qu:` becomes
`question: `. The decoration toggles stay in the chip's menu, since the drawer
is too narrow for them beside the chip ([[BPR-1290]], the owner's v003 note:
"chortcodes arent working in the reply comments"). Comment
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
alone does not take a tooltip away. The code's gutter
carries a marker per thread, the root post's label mark or a plain square, a
blue dot when new, faded when resolved; pressing it opens or closes the
thread. Keys in the drawer: Up and Down move between threads, Return opens,
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
semibold numbers with quieter words ("39 conversations  27 unresolved"), and
Clear filters as a funnel icon. The band wraps onto more rows when the list is
narrow, as it is beside the composer ([[BPR-1212]]). Every control keeps its
accessible name ("Whole pull request", "File scope", "Conversation file
scope", "Unread only", "Unresolved only", "Clear filters", "Show every
conversation", "Threads summary") and has a tooltip. The counts' accessible
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
