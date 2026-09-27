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
┌ Title row: Home | Repositories | Files | name ········ Overview Read Review Discuss | Refresh | Comments (n) | Account | Settings ┐
├ Review bar: ▸ Updates  Comparing …, dates ▾ | File 3 of 40 « » | Changes 3 of 11 ‹ › | progress ▾ ········ Compose review | Comment ┤
├ Updates line, while open: the timeline, full width ┤
├──────── file drawer ────────┬──────── comparison workbench ────────┬──── conversation drawer ────┤
│ filters and review map       │ natural-layout panes + DiffLinkMap      │ tabs: Comments | Notes      │
│                              │ one shared vertical scrollbar          │                              │
│                              ├ full-width selected-change detail pane ┤                              │
└──────────────────────────────┴────────────────────────────────────────┴──────────────────────────────┘
```

[RFC-BPR-0052](../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)'s
chrome is built in part ([[BPR-1129]], [[BPR-1130]]). Above the comparison sit,
in order, the title row, the review bar and, while it is open, the Updates
line's timeline, with no scroll area and no separate title bar. The **title row** carries the window, the pull
request and the modality: Home, the repositories and files toggles and the pull
request's name at its left; the modality at its right beside refresh, the
comments toggle (which carries the count of threads with new comments),
account and settings; on Windows the application menu and the caption buttons.
The **review bar** is full width under it and always shown with a review: from
its left the Updates disclosure, the comparison's menu naming the comparison and its
dates, File and its position before the file arrows, Changes and its position
before the change arrows, and private progress as an icon whose tooltip names the file's
state; at its right **Compose review** (Option-R, Alt+R) and **Comment**
(Option-C, Alt+C) as icons whose tooltips name the command and its key. Compose
review carries an amber mark while a review draft is kept. Review details is
reached from the comparison's menu. Both composition commands are also in the
Pull Request menu, and Esc returns from either composition page. Each navigation
label and its pair share a horizontal accent rail. File and Changes stay named
at every width and when their position is unavailable; narrow layouts shorten
only the position wording.

**Comment and Reply** ([[BPR-1133]]). Comment answers the selected review
thread when there is one (the thread the reviewer chose, or the thread on the
line they selected) and otherwise starts a comment on the current line. It
names that target wherever it is offered: its tooltip ("Comment on line 431
(⌥C)", "Reply to tbutler's thread (⌥C)"), the Pull Request menu, and the menu
its arrow opens on the review bar, which offers that plain comment alone; it is
where RFC-BPR-0048's Conventional Comments labels will be offered, and no label
is offered before then. Either way it opens the inline composition page on the
line's draft or on the reply draft; nothing is published without the page's
explicit Publish. [[BPR-1207]] requires an admitted editable draft, or an explicit recovery for its
retained text, at the named current-head target before Comment opens. Otherwise the tooltip and
menu state why it is unavailable. The inline page hides empty workshop panels
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
Overview and Discuss show the window's comparison; Read and Review show the
active tab's. When no tab is open, changing the comparison sets the window's
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

## Overview files

Overview groups files under their complete folder hierarchy. Each folder has
the native tree disclosure control; Left collapses and Right expands it.
Collapsing an ancestor hides every descendant, while reopening retains nested
folders' expansion choices and the same file identities and order. These
gestures never open a comparison. If the focused file becomes hidden, focus
moves to the collapsed folder without changing the active source selection.

Folder expansion is local to this page. Incremental same-authority refreshes
retain surviving items, selection and expansion; newly appearing folders start
expanded. Exact authority invalidation clears that local state. The native tree
exposes folder names, descendant file counts and expansion state to accessibility.
[[BPR-0934]] records the focused and rendered public evidence.

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
  shows and how long GitHub asked to wait, or "Save a repository to find
  the pull requests in it that need me." It says a clock time, never how
  long ago, and nothing without an account session, whose Home already
  carries the sign-in. Rows found before a failure stay.
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
  (`#diff-<SHA-256 of the path>`) selects exactly that changed file in Read;
  its `R<n>` line is selected when the comparison on screen ends at the pull
  request's head. `#pullrequestreview-<id>` reveals that review in Discuss, and
  `#discussion_r<id>` reveals the comment when the read identified it by that
  number (a signed-out read does; an authenticated read names comments by
  node identity only). Everything else — a commit page, a base-side `L<n>`
  line (GitHub numbers it against a merge base the comparison does not use),
  an `#issuecomment-` link, a digest naming no changed file, an unread
  fragment — opens the pull request and says, under the bar and in the status
  bar, what was asked for and why it is not shown. Nothing nearby is chosen
  instead, query parameters are ignored, and a link that names no pull
  request (a repository commit, an issue) opens nothing.
- **A refusal under the bar**: one sentence from the existing diagnostic
  with **Copy details** beside it. A first load still in progress is said in
  the same place and carries no Copy details, because it is not a refusal.
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
  rules appear here." — whose link opens Settings › Reviews & queue. No empty
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
  footer says that nothing there changes GitHub and offers the way to
  Settings › Reviews & queue. These are the same typed controls Settings
  presents as queue defaults, not a mirror of them.
- **Recently viewed** is the last panel in every arrangement, and Resume is
  its rows' action: on hover, on focus and in the row's menu. It is governed
  by the entries the composition gave the window rather than by the account,
  so a signed-out window still lists its public reading.
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
Conversations are the title row's toggles. The navigation, drawer and
private-progress presenters are icon-only, but each retains a stable visible
tooltip, accessible name and description, strong-focus stop, and keyboard
shortcut. No text toolbar appears at a narrower width: as the window narrows
the review bar drops the comparison's dates and then the positions' words
("3/40" for "file 3 of 40"), and the title row drops the name's title, its
owner and then the modality's shortcuts, keeping the name's number. No control
folds into an overflow menu at any width down to the platform's floor.

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
| Previous file | `Alt+Up` |
| Next file | `Alt+Down` |
| Previous review change | `Alt+PgUp` |
| Next review change | `Alt+PgDown` |
| Toggle files drawer | `Alt+Shift+1` |
| Toggle conversations drawer | `Alt+Shift+2` |
| Toggle review details | `Alt+Shift+3` |
| Open private review progress | `Alt+Shift+4` |
| Toggle source line numbers | `Alt+Shift+5` |
| Compose review | `Alt+R` (Option-R) |
| Comment, or Reply to the selected thread | `Alt+C` (Option-C) |
| Back from a composition page | `Esc` |

The drawer toggles use `Alt+Shift` rather than plain `Alt+1`/`Alt+2` because
the composed epoch control already holds `Alt+1` through `Alt+3` with
widget-with-children context. Both bindings match whenever that control has
focus, and Qt resolves an ambiguous shortcut by triggering neither, so the
plain-`Alt` form produced two silently dead toggles. No key sequence may be
duplicated anywhere in the assembled window.

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
- The lower pane is a full-width line-over-line detail view for the selected change group.
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
one-pane toggle and, after a separator, Settings…. The bar was added by
BPR-0457 because the owner asked for a "view menu option" and, offered the
choice between a real menu bar and a checkable entry in the existing toolbar
popup, chose the menu bar. On macOS it merges into the system menu bar and
costs the window no height; on Windows and Linux it takes a row, which is the
price of the same answer being true on every platform. The existing toolbar
popup keeps the private progress actions it already had. **File** holds Quit,
and **Profiles** lists the profiles by name with the current one checked, then
New Window and Manage Profiles…. On Windows the strip's application menu
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
  **View** — Line numbers, One pane and Settings… — offered and disabled,
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
Comments tab holds the conversation exactly as described below, with its scope
and filters, and Next new comment beneath the tabs; the tab carries the count
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
Next new comment under Comments. The private tab contains none of them. Choosing Notes is RFC-BPR-0031's explicit
move into the AI tools: when its content is shown, it permits the existing
compare_glance cue, dispatches no agent or review action, and never opens a
collapsed drawer. Under [[BPR-1209]], the single Dif rests at 28 logical pixels in the
status bar's leading corner on every ordinary surface, stepping aside for a
transient message. The first-launch Home and [[BPR-0966]]'s exact offered
Compose workshop remain prime spots. No Dif slot belongs to the private tab. Each tab scrolls on its own, so switching tabs keeps each one's scroll
position and the selected conversation. The tabs are one focus stop and the
arrow keys move between them. RFC-BPR-0039's Tour joins them when it exists.

Replies read as a linear feed, not a staircase. A reply sits one step in from
the root it belongs to, and a reply to a reply sits in the same place — the
grouping by root stays visible, SmartBear Collaborator style, while depth stops
spending the width the comment text needs. Depth is still carried by the model
and is read on the thread map above, which is the product owner's own stated
reason for accepting a flat feed here.

Filters include New, Unresolved, Defects, Needs Verification, Pending Drafts,
Outdated, and All. The current comparison filters roots to the exact file. A
reviewer-selected source line narrows that forest to the exact diff side and
line; selecting a root or reply drives the reciprocal code reveal described
above. The first thread the drawer makes current by itself, so that its detail
has something to show, is not a selection: it moves no code, and a comparison
published again reveals only a thread the reviewer chose ([[BPR-0985]]).
Otherwise every file's first thread would take the reader's line, and a
resumed reading would lose its saved line to it. Unplaceable threads remain
readable at file scope and state why no code marker is drawn. Retained
historical navigation requires a revision identity and remains separate work;
an outdated thread is never placed against the current panes by reusing its
original line.

### The thread map

Above the tree, the drawer draws the same forest as a map of its reply
structure. It is the Treeverse-style view the product owner chose on
2026-08-21, and it exists because a reply chain deep enough to matter stops
being followable as an indented list — which is the case the linear
presentation loses, not the ordinary one.

Nodes are boxes labelled with the author of a comment, or with the anchored
file and line for a thread's root, laid out one row per depth. Edges run
parent to child. Slots within a depth are assigned in pre-order, so within one
row every node of an earlier subtree precedes every node of a later one and no
two edges can cross. A box is sized to its own label, so no author name is
elided into an ambiguous prefix.

Selecting a node highlights **the whole branch it belongs to**, in two weights.
The root-to-selection path is drawn heaviest: that is the floor the owner set
— "it still needs to show the full branch that you clicked on, or at least up
to the node you selected". Everything downstream of the selection is drawn
lighter, so the chain's continuation is visible without competing with the
path; that lighter set is the whole subtree, sibling sub-branches included,
because all of it is downstream of the comment that was clicked. Anything
neither above nor below the selection is left plain. A caption below the map
states the path and how many replies follow.

Highlighting is not the same claim as showing. The drawer's band is short, so
selecting a node scrolls the map to it and keeps as much of the path above it
as fits; when the whole branch fits, all of it is shown. A band that changes
height re-reveals rather than leaving the selection scrolled away. Without
that, a reviewer gets a highlighted branch whose selected end is off-screen —
which is what the surface shipped as until an independent review measured it.

Selection has one owner. Pressing a node moves the drawer tree's current index,
and the code reveal, the anchor marker, and the defect surface keep reading
that selection from where they always did. The map holds no selection authority
of its own, performs no effect, and adds no projection: it is a second view of
the drawer's forest, never a second source of it.

There is deliberately **no read or unread state on the map**. The accepted
mockup legends one, and nothing in this product carries it — the review
ledger's unread notion is a per-conversation cursor rather than a per-comment
flag. Showing it would mean inventing the state or approximating it, so the
distinction is absent rather than faked.

An unexpected shape costs nesting and never a row. A parent naming a comment
that is not present, and a parent chain that never terminates, both promote the
node to a root — every node on the chain, decided in a pass that writes nothing
so the outcome cannot depend on visit order. That is the same answer the
drawer's tree gives, and the drawer shows both views at once, so a shape they
answered differently would be a disagreement the reviewer could see.

The map maps whichever threads the drawer is currently showing, so its summary
counts across them. A projection equal to the one already mapped is skipped
rather than relaid, because the drawer re-adopts on every anchor change.

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

Owner hand-back 49 amends the review bar's last width yield: when compact
positions still cannot fit, their numbers yield into the tooltips. File and
Changes remain before their arrows, joined by a separate accent rail.

## Source-line comments outside a hunk

Use the same line gesture, draft editor, Publish control and pending announcement
on changed and unchanged current/right lines of a changed text file. Publication
selects the canonical returned conversation at its exact semantic source line
after refresh. The reviewer does not select a representation. A required file
fallback for line-only suggestion syntax reports typed incompatibility before
any file POST; it does not silently remove suggestion semantics.

A compatibility conversation normally shows reviewer prose. Its collapsed
**Support details** explains that GitHub stores a file review comment and the
visible code link supplies placement. The disclosure is keyboard operable and
accessible; expanded text can require scrolling in a narrow drawer. Native and
file subjects use the same focus and selection behavior. Three-scale, three-theme
rendered evidence is in the [RFC-0022 ledger](../verification/RFC-BPR-0022.md).
Production fallback remains unavailable until an exact unsupported response
contract is admitted; the UI does not infer support from host name or version.
