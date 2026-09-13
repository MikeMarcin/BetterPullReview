# Design notes: the surfaces before a review

These notes record the design plan behind `shell-surfaces-v1.html` and the two
RFCs, and the self-critique that shaped it. They are working notes for the
implementer, not a contract; the RFCs are the contract.

## Subject, audience, job

BetterPullReview is a native desktop tool for someone who reviews pull requests
all day. The three surfaces in scope are the seconds *before* a review: signing
in, naming a pull request, choosing a profile. Their whole job is to get out of
the way and put the reviewer into a review in one gesture. Nothing on them is
worth looking at for its own sake, so the design spends its one bold move on the
one control that does the job (the bar on Home) and makes everything else quiet.

The audience is one reviewer at a wide macOS window who uses the keyboard, and
later the same person on Windows and Linux. The app is native Qt and RFC-BPR-0018
forbids a second stylesheet or a hard-coded palette, so the design is written in
palette roles and native type, not in hex.

## What is wrong today, named

| Surface | Symptom in the screenshots | Cause in the source |
|---|---|---|
| Accounts & hosts | Three eras of chrome stacked: account card, then the old session strip (host picker, Connect, Sign out, Read without signing in, Details, host and transport lines, login again), then the Owner/Repository/PR locator, then a pull-request load banner, then a servers editor made of an empty combo box | BPR-0646 moved the session panel into Settings whole; BPR-0652 put the account card above it; BPR-0669 and BPR-0732 moved naming and public reading to Home but left the locator and the button behind. `workbench_window.cpp` still adopts `SessionPanel` entire. |
| Accounts & hosts, signed out | A form of two fields with the address truncated, a two-by-two button grid with a disabled "Sign in again", and the same three stacked panels beneath | `device_authorization_panel` is a standalone form laid above the session strip rather than the account row's own state |
| Home | A card whose two buttons only move focus; a centred "Pull request unavailable" page; a second "Open a pull request" heading over the bar; "Resume reading" jammed beside a heading; an empty black box and six filter combos for an empty list; a status bar that says nothing is open | The landing card, the pull-request stack's empty page, the bar, Recently viewed and the queue landing were added one card at a time (BPR-0646, 0650, 0669, 0733) and each kept its own heading and controls |
| Manage Profiles | A bare list, a permanent "Profile name" field, a status sentence, four buttons and a full-width "Reload Profiles"; it appears every time the last window closes | `profile_desktop.cpp` presents the manager when `Runtimes.empty()` (BPR-0696's fix for an invisible desktop, because `setQuitOnLastWindowClosed(false)` is set on every platform) |

## Tokens

Colour is the platform palette. The mockup maps each role to a plausible dark
and light value for illustration only.

| Role (QPalette) | Use | Mockup dark | Mockup light |
|---|---|---|---|
| Window | Page ground | #1f2023 | #f2f2f4 |
| Base | Cards, the bar, rows' hover ground | #27282c | #ffffff |
| Mid | The one hairline: card edge, row separators | #3a3b41 | #d6d6db |
| Text | Ink | #ececee | #1b1b1e |
| PlaceholderText | Quiet ink: consequences, timestamps, reasons | #9a9ca4 | #6b6d75 |
| Highlight | The single filled control on a page; selection | platform accent | platform accent |
| status: connected | The word "Connected" only | #4cc06c | #1f8a3b |
| status: needs me | The reason word on a row that needs me | #e0a23c | #a86a06 |

Type is the interface font at the accepted interface size, and the code font
for one thing only: the device code, which is code. Scale from the base size:

| Level | Size | Weight | Where |
|---|---|---|---|
| Page title | 1.25× | demibold | Settings page heading (exists) |
| Section | 1.0× | demibold | "Recently viewed", "Needs me", card header host |
| Row label, identity | 1.0× | regular; identity demibold | "Rate limit", `owner/name#123` |
| Consequence | 0.92× | regular, quiet ink | Under every label |
| Device code | 1.6× code font | regular, +0.08em tracking | The code in the account row |

Rhythm: an 8-pt grid. Page inset 24. Card inset 16. Row height 44, row gap 0 with
a hairline between rows. Section gap 24. Home's content is one left-aligned column
of at most 720 logical points, centred in the window; the bar spans the column.
No shadows, no gradients, one 8-pt radius on cards and the bar, none on rows.

## Layout

Accounts & hosts, connected:

```
Accounts & hosts
One account per server. Credentials are never shown here.
┌────────────────────────────────────────────────────────────────┐
│ github.com                                          Connected   │
│ GitHub.com, native HTTPS                                        │
├────────────────────────────────────────────────────────────────┤
│ (M) MikeMarcin                                        Sign out  │
│     Signed in with a device grant on 2026-09-13; renews itself  │
│     and is remembered on this Mac.                              │
├────────────────────────────────────────────────────────────────┤
│ Access        Reads and writes the repositories the account     │
│               can reach; publication uses this account.         │
│ Rate limit    4,807 of 5,000 requests remaining, resets in 38m  │
│ Details ▸     Transport, capabilities, last check               │
└────────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────────┐
│ ghe.example.com                                     Signed out  │
│ GitHub Enterprise Server                                        │
├────────────────────────────────────────────────────────────────┤
│ (?) No account                                        [Sign in] │
│     A token for this session only; asked for again after restart│
├────────────────────────────────────────────────────────────────┤
│ Remember this server                                    (●  )   │
│ Keeps the hostname and account name across launches.            │
│ Forget server                                                   │
└────────────────────────────────────────────────────────────────┘
[ Add enterprise server ]
```

Accounts & hosts, signing in (the account row is the only thing that changes):

```
│ (?) Sign in to GitHub                                  Cancel   │
│     Enter this code at github.com/login/device                  │
│     A416-42ED ⧉                                                 │
│     [Open browser]                     Valid for 14 min 59 s    │
│     Waiting for the browser to authorize this Mac.              │
```

Home, signed in (the accepted set is drawn in `home-v2.html`; the first draft's
narrow column and plain lists were rejected on 2026-09-13):

```
 ⌂                          [Group: None ▾] [State headers] [Filter ▾] ↻ │ ◯ ⚙
 ┌ Open  paste a link, or write owner/name#123 ───────────────────── ⌘L ┐
 ┃▾ Needs me 3                                                          ┃  amber header, amber rail
 ┃ EpicGames/UnrealEngine#14366  Hair strand lighting fixes for LSS     ┃
 ┃ Alex Chen pushed 2 updates since my last review; 1 reply… Changed    ┃  (M MikeMarcin)  2 h ago
 ┃ …                                                                    ┃              3 of 40 read
 ┃▸ Waiting on others 1                 1 of mine, waiting on 2 reviewers┃  blue, collapsed
 ┃▾ Following 3                                                         ┃  green
 ┃▸ Recently viewed 2                                                   ┃  neutral
 Signed in as MikeMarcin on github.com and mmarcin on ghe.example.com.
```

Group by Organization with State headers on nests a slim tinted band per state
inside each neutral owner panel; with it off the rows sit directly in the panel
in state order with their marks. The owner's avatar heads the panel; the
account chip sits at the panel's right when the group is one account, else at
the right of each row.

First launch: the strip with Home, account and settings only; the bar relabelled
for public reading; one card with Sign in (filled) and Add enterprise server;
the status bar. Nothing else.

Profiles:

```
Profiles
┌────────────────────────────────────────────────┐
│ (D) Default            Open in 2 windows        │
│ (W) Work               Closed                   │
│ (P) Personal           Closed                   │
└────────────────────────────────────────────────┘
New profile                       Rename   [Open]
```

## Principles

1. One filled control per page, and it is the page's job: Sign in on a signed-out
   card, Open on the bar, Open on the chooser. Everything else is bordered or flat.
2. A row is label, consequence, control. No colons, no "Label: value" pairs, no
   all-caps eyebrows, no numbering.
3. Nothing repeats. A fact appears once on a page. The host is the card header;
   the login is the account row; the transport is under Details.
4. Empty is a sentence, never a box. An empty list says what would fill it and
   where that is configured. Controls that act on nothing are not shown.
5. A refusal sits beside what was refused: a pull request that cannot load is
   said under the bar; a server that cannot connect is said in its card.
6. A gate is never a screen. The profile chooser opens when asked, or when the
   app has no window to carry a notice. Closing a window closes a window.
7. On Home colour means state and nothing else: amber Needs me, blue Waiting
   on others, green Following, at low strength on headers and rails and at full
   strength on the reason word. Rows are always in state order; Group changes
   the panels around them, never the order. A group header, not a row, carries
   an avatar.
8. Words are the reviewer's: first person as RFC-BPR-0028 fixes it ("Needs me",
   "my last review"), sentence case, verbs on buttons, the same name for an
   action through its whole flow (Sign in, Signing in, Signed in).

## Review against the brief

Before drawing, the plan was checked against what a generic pass would produce
for "clean up a dark settings page and a home screen":

- The generic answer is a card kit: every section in an identical rounded card
  with a soft shadow. Rejected. Only things that *are* one object are cards (a
  server with its account; the sign-in offer; the bar). Lists are rows with
  hairlines and no container, so a long queue reads as a list, not a wall of tiles.
- The generic answer is a near-black ground with one acid accent. Rejected; the
  palette is the platform's and the only accent is the platform's own, used once
  per page. Green and amber are status words, not decoration.
- The generic answer keeps the six filter combos "for power users". Rejected;
  the same typed preferences survive behind one Filter menu, and the state shows
  as chips only when it is not the default. Nothing is lost, and an empty Home
  shows nothing to filter.
- The generic answer centres a big empty-state illustration. Rejected; the
  empty state is one sentence and a link to where the list is configured.
- The generic answer for a profile picker is a launcher screen of tiles
  (Chrome). Rejected; the picker is a menu item and a small sheet of rows,
  because the reviewer has one or two profiles and opens them by name.
- The mockup uses the interface font, not a display face, and the code font
  only for the device code. Deliberate: these screens must look like the
  platform, and a distinctive typeface here would read as a web page in a
  native window.

The one place boldness is spent: Home's bar carries its own heading inside a
tall bordered field, at the top of a narrow centred column, so the page is
visibly "one field and some rows". The screenshots today are the opposite: a
bar lost between three headings and an empty box.

## Home, second round

The first Home (a 720-point column, plain rows with thin text dividers, six
filters folded into one menu) was rejected: "the home redesign is not good",
and on the grouping, "it all runs together and doesn't look separated". The
second round tried three page shapes (inbox table, desk with rail, list with
preview), then four separations (panels, bands, columns, tabs), then four
colour strengths (tinted headers, rail, tinted panels, coloured titles). The
owner chose tinted headers plus the rail, then asked for collapsible panels,
grouping by organization, repository and account with the state order kept
underneath, avatars in group headers, the account visible per repository (a
chip, at the right), Group and Filter and refresh in the title strip, and
"Added by" for the observer-reason filter. Those decisions are the design;
they are recorded in RFC-BPR-0032 and drawn in `home-v2.html`.

## Removed on the way out (Chanel's rule)

- The "Resume reading" button beside the Recently viewed heading: it is a row
  action now, on hover, focus and the row's menu.
- Repository and Bucket as filters: Group and the state order do their job.
- The 720-point Home column: the owner's window is wide and the rows use it.
- The ghost "Sign in again" button shown disabled during a sign-in.
- A bordered "Copy code" button beside Open browser: the copy is a glyph at the
  end of the code itself, where the convention puts it (owner note, 2026-09-13),
  so the row has one filled button.
- The status bar's "No pull request is open in the workbench." on Home, which
  said what the page already showed.
- "Reload Profiles" as a permanent button; it is the retry inside a failure notice.
- The landing card's second button ("Accounts & hosts settings") on the signed-out
  Home, which opened the same page as the first.

## Open questions for the owner

- Whether "Connected" should keep its green. The accepted 0028 mockup shows it
  green; the design keeps that and adds no other colour.
- Whether the collapsed-panel summaries earn their place, or a collapsed header
  should carry only its count.
