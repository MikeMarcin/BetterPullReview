---
id: "RFC-BPR-0032"
title: "Calm Home and Accounts & Hosts: One Language for the Surfaces Before a Review"
status: "Proposed"
area: "BPR"
milestone: "Shell Surfaces v1"
depends_on: ["RFC-BPR-0016","RFC-BPR-0018","RFC-BPR-0028"]
metadata: {"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# RFC-BPR-0032: Calm Home and Accounts & hosts

## Owner amendment: shared sign-in (BPR-0915)

Home and pasted PR links now use one focused sign-in dialog, reusing the account
controls and authentication operations from Settings. Settings retains connection
management. This supersedes the requirement below to keep active sign-in only
inside Settings. A CLI action starts discovery immediately; one matching account
advances to the existing trust confirmation, and multiple accounts offer a choice.
A pending PR resumes after acceptance for its exact host. Cancel or a replacement
link retires pending setup and ignores late discovery completion. Credential
storage, CLI trust and separate publication approval requirements are unchanged.

## Summary and scope

Home and the Accounts & hosts page of Settings are the surfaces a reviewer
crosses in the seconds before a review. Today each is a stack of panels added
one card at a time, and the owner's verdict on 2026-09-13 was that they are
"ugly as heck and so dang cluttered". This RFC gives both surfaces one design
language and removes what no longer has a job there. It is a presentation
revision: it changes what the existing typed projections and intents look like
and where they sit, and it changes no credential, provider, publication, queue
or navigation authority.

The design follows the accepted RFC-BPR-0028 statements for Settings (one card
per server) and Home (the bar, Recently viewed, the grouped action list, the
signed-out sign-in card) and the RFC-BPR-0018 row language (label, consequence,
control; reset beside what it resets; nothing optimistic). Where the current
composition departs from those statements this RFC names the departure and the
card that closes it. It supersedes the compositions made by [[BPR-0646]],
[[BPR-0652]], [[BPR-0660]] and [[BPR-0650]] to the extent stated below, and no
RFC text. The design references are
`docs/rfcs/assets/bpr-shell-surfaces-v1/design/shell-surfaces-v1.html` (Settings
and the profile chooser) and `design/home-v2.html` (Home, the set the owner
accepted on 2026-09-13 after rejecting the first draft), with their notes; a
reference illustrates palette roles and is not runtime evidence.

## Problem, named from the source

| Surface | What the screenshots show | Where it comes from |
|---|---|---|
| Accounts & hosts, signed in | The account card; under it the whole former session strip (host-profile picker, Connect, Sign out, Read without signing in, Details, "Active host" and transport lines, the login a second time); under that the Owner/Repository/PR locator with Open and Refresh; a "The pull request could not be loaded" banner with Copy message; then an "Enterprise servers" editor whose first control is an empty combo box | `workbench_window.cpp` adopts `SessionPanel` whole into the page after `AccountCard` and `DevicePanel`, then `build_enterprise_servers_editor`. BPR-0669 made the Home bar the way a pull request is named and BPR-0732 made it the way a public one is read, but the locator and the button stayed |
| Accounts & hosts, signed out | A two-field form whose address is cut off, a two-by-two button grid with a disabled "Sign in again", and the same three stacked panels beneath, including "Not authenticated" twice | `device_authorization_panel` is its own form above the strip rather than the account row's own state |
| Home, signed in | A card whose two buttons only move focus elsewhere on the same page; the pull-request stack's empty page centred beneath it; a second "Open a pull request" heading over the bar; "Resume reading" beside the Recently viewed heading; a large empty box for an empty queue with six filter combos under it; a status bar that says no pull request is open | `workbench_landing` and the queue landing each kept their own heading and controls as BPR-0646, 0650, 0669 and 0733 added them; the queue landing is the drawer's list moved to Home with its filters |

Three facts appear twice on the signed-in Accounts page (host, login, connection
state) and one heading appears twice on Home. No control on either page is wrong;
too many of them are present at once.

## Accepted design

### Accounts & hosts

The page is a title, one sentence, one card per configured server, and one
button. The sentence is the accepted mockup's: "One account per server.
Credentials are never shown here."

A server card has a header, an account row, and rows.

- **Header.** The hostname in the section weight and, under it, the server kind
  and transport as one quiet line ("GitHub.com, native HTTPS"; "GitHub
  Enterprise Server"). At the right, the connection state as one word:
  Connected, Connecting, Signing in, Signed out, or a short refusal ("Could not
  connect"). Connected keeps the accepted green; nothing else on the page is
  coloured.
- **Account row.** A letter avatar, the login in the row weight, and one
  consequence sentence built from the existing grant projection ("Signed in with
  a device grant on 2026-09-13; renews itself and is remembered on this Mac.";
  "Signed in with a token for this session only; it is asked for again after a
  restart."). At the right, Sign out. Signed out, the row reads "No account" with
  the sentence "Sign in starts a device code; the session is remembered on this
  Mac." and one filled Sign in button, the page's only filled control. A server
  that is configured but not the active host offers Connect in the same place;
  Connect selects that host through the existing selection intent and then
  connects. There is no host-profile picker.
- **Signing in.** Sign in begins the device flow (RFC-BPR-0016, RFC-BPR-0018
  AC-12) inside the account row, which becomes: "Enter this code at
  github.com/login/device", the user code in the code font at 1.6× with the
  whole code selectable and a copy glyph at its end, Open browser as the filled
  button, the remaining validity, one status sentence from the existing typed state
  ("Waiting for the browser to authorize this Mac."), and Cancel at the row's
  right. Denied, expired, unavailable and cancelled states replace the sentence
  and offer one Sign in again. No button is shown disabled; no second field
  truncates the address; the manual-token path remains reachable for servers
  whose device flow is unsupported, in the same row. The copy glyph carries the
  accessible name Copy code, a tooltip, a visible focus ring and a brief
  "Copied" shown in place, so a glyph is never the only label.
- **Rows.** Access (consequence from the existing capability projection), Rate
  limit ("4,807 of 5,000 requests remaining, resets in 38 min."), and a Details
  disclosure that holds the transport, the capability and availability lines,
  the last check, and a "Last problem" line with Copy details when a session
  failed. Enterprise cards add a Remember this server switch with its consequence
  ("Keeps the hostname and account name across launches. The token is still
  asked for.") and a flat Forget server action that confirms before it removes
  the record and names exactly what it removes.
- **Add enterprise server.** One bordered button under the cards. Pressing it
  adds a card in its edit state: hostname, account (optional), Add and Cancel.
  It uses the existing saved-server intents; it does not create credential
  authority.

Removed from the page: the host-profile picker row and its Connect, Sign out,
Read without signing in and Details buttons; the "Active host", transport and
login lines beneath it; the Owner/Repository/PR locator with Open and Refresh;
the pull-request status banner and its Copy message; the servers combo editor.
Their jobs move as follows: naming a pull request is the Home bar; reading
without an account is the Home bar; the pull-request load refusal is the Home
bar's notice; a session failure is the card's state and Details; Refresh is the
strip's; selecting a host is the card's Connect. Public preview is stated on
the github.com card's account row while such a session exists ("Reading public
pull requests without an account.").

### Home

The owner reviewed a first Home draft on 2026-09-13 and rejected it, then
accepted the design below through nine rendered boards
(`design/home-v2.html`). Home is the window ground edge to edge, not a narrow
column. Signed in it holds, top to bottom:

- **The strip.** Home at the left; at the right, before the account and
  settings buttons and a divider, three controls and a glyph: Group, State
  headers, Filter and refresh. This is the owner's amendment (2026-09-13) to
  RFC-BPR-0028's rule that the strip shows only Home, account and settings
  while Home is open; the rule still holds while signed out, when the three
  controls have nothing to act on and are absent, not disabled.
- **The bar**, alone in the first row under the strip: an address field
  labelled "Open" with the placeholder "Paste a link, or write
  owner/name#123", ⌘L to focus, Return to open. It has focus when Home
  appears. The `workbench_landing` card's focus-only buttons are removed.
- **A refusal under the bar** when the last open failed: one sentence from the
  existing diagnostic with Copy details beside it. The pull-request stack's
  empty page is not shown on Home.
- **The list**, always in state order: Needs me, then Waiting on others, then
  Following, from the existing core attention projection; Qt invents no
  urgency. Every row is the identity in the row weight, the title in ink, a
  second line that says why in a sentence ("Alex Chen pushed 2 updates since
  my last review; 1 reply to me.") ending in the one-word reason (Changed,
  Requested, Replied, Waiting, Approved, Following), and at the right the
  time and my progress ("3 of 40 read"). Colour means state and nothing else:
  amber for Needs me, blue for Waiting on others, green for Following, on the
  reason word and on a 3-point mark or rail at the row's left.
- **Panels.** With State headers on (the default) each state is a panel: a
  bordered card with a rail in its colour down the left and a tinted header
  band carrying the state name and count. Headers stick to the top of the
  scroll area while their panel is in view. Every panel collapses to its
  header by chevron, Space or Return, Left and Right; collapsed it keeps its
  count and a one-line summary from the same projection ("1 of mine, waiting
  on 2 reviewers"). An empty state is not shown; when every state is empty
  the Needs me panel holds one sentence, "Nothing needs me. Pull requests from
  saved repositories and observer rules appear here.", whose link opens
  Settings › Reviews & queue. No empty box.
- **Group.** A strip menu: None (default), Organization, Repository, Account.
  A group is a neutral panel headed by the owner's or account's GitHub avatar
  (square for an organization, round for a user, an initial tile until the
  image is cached) and its name, with the count. With State headers on, a slim
  tinted band per state sits inside each group panel, each collapsible on its
  own; with it off, the rows sit directly in the group in state order with
  their marks. Under Organization and Repository a host is shown only where it
  differs from the account's default, per RFC-BPR-0028.
- **Account.** When the profile has more than one signed-in account, each row
  carries an account chip at its right before the time: the account's round
  avatar and login, plus "on host" when the host is not github.com. When every
  row in a group belongs to one account the group header carries the chip at
  its right instead and its rows drop it. Under Account grouping rows never
  carry it. With one account no chip is shown anywhere.
- **Filter.** A strip menu holding the surviving typed queue preferences: My
  role (Any, Author, Reviewer, Requested); Added by (Any, Rule, Follow,
  Request), which is the observer reason under its meaning; Quiet items (Hide,
  Show); Sort within a group (Priority, Updated, Repository), which orders
  rows inside a state since state is always the first key. Repository and
  Bucket are no longer filters because Group and the state order do their
  job. The menu's footer says nothing here changes GitHub and links to
  Settings for saved repositories and rules. Group, State headers, the state
  panels' and Recently viewed's collapse, and Filter are remembered per window,
  as RFC-BPR-0024 places window filters; a group panel's own collapse is not,
  by the owner's direction below.
- **Recently viewed**, last, as its own neutral panel in every grouping, with
  Resume as a row action shown on hover, on focus and in the row's menu.
- **The status bar** carries the session line ("Signed in as MikeMarcin on
  github.com and mmarcin on ghe.example.com. Refreshed 4 min ago.") and never
  "No pull request is open in the workbench." while Home is shown.

First launch and signed out: the strip with Home, account and settings only;
the bar relabelled "Paste a public pull request link to read it without an
account"; one card, "Sign in to GitHub", with a sentence, Sign in as the
filled button (opens Accounts & hosts with the device flow begun) and Add
enterprise server as a bordered one, the two ways in RFC-BPR-0018 requires
without opening Settings; and the status bar "Not signed in. Public pull
requests open read-only." No list, no panels, no filters. Home over an open
review keeps its return: one shaded row above the bar, "Reviewing
NVIDIA/stdexec#1846 …, file 9 of 12" at the left and Back to the review (⌘⇧H;
⌘0 until the owner's amendment of 2026-10-15, [[BPR-1535]])
as the filled button at the right; Home is unchanged beneath it.

**Dif on Home (owner direction, 2026-09-15).** The owner asked for Dif to
act mostly like AllTheThings' Nib: he shows up on Home for the first-time
sign-in experience, and otherwise sits in the very bottom-left corner, out
of the way. RFC-BPR-0031's Home placement therefore governs the first-launch
card alone: Dif stands beside the card's sentence at 96 points, 72 at the
compact breakpoint, and is hidden before the card's controls would be
squeezed. On every other Home, signed in or over an open review, the same
single Dif rests at 40 points on the leading edge of the status line in the
window's bottom-left corner. There he has no pointer or focus path, the line
begins after him, and he steps aside while the status bar shows a transient
message. RFC-BPR-0031's greeting, calm and blink cadence, its motion and
visibility preferences, its one sprite per window and its explicit AI tools
slot on review surfaces are unchanged. Resting in the corner while a review
is read lies outside this RFC's Home boundary and is carried as a follow-up.
On 2026-09-25 the owner extended the direction to every surface. Dif leaves
the conversations drawer and rests in this corner on every surface without a
prime spot. RFC-BPR-0031 records this as an owner amendment, and
[[BPR-1209]] builds it. The Home placement described here is unchanged.

**Group collapse (owner direction, 2026-09-15).** Collapsing an
organization, repository or account group is remembered only while its window
is open. Remembering it across a restart would mean the per-window workbench
settings record naming owners, repositories and accounts, which that file has
never held ([[BPR-0189]]); asked to choose, the owner answered "Do what's
easiest, which sounds like dont remember for now". The basis was ease, and
"for now", so this is revisitable: remembering it later means a bounded
durable record for those keys in a store whose purpose admits names, not a
weakening of the settings schema's guarantee. Everything else Home remembers
per window is unchanged.

### Language shared by both

A row is one line: label, an info glyph and the control. The consequence
sentence lives in the glyph's popover, shown on hover and on keyboard focus,
and is the glyph's accessible description (the owner's amendment of 2026-09-13,
recorded in RFC-BPR-0034); a row whose second text is a value rather than a
consequence keeps the value in the row. No colons, no "Label: value" pairs, no
capitalised eyebrows, no numbering. A fact appears once on a page. One filled
control per page, and it is the page's job. Empty is a sentence, never a box.
A refusal sits beside what was refused. Words are the reviewer's, in the first
person RFC-BPR-0028 fixes ("Needs me", "my last review"), in sentence case, with
verbs on buttons and the same verb through a flow (Sign in, Signing in, Signed
in). Rhythm is an 8-point grid: page inset 24, card inset 16, row height 44
with one hairline between rows, panel gap 14, one 8-point radius on cards,
panels and the bar and none on rows. Colour is the palette: Window, Base, Mid,
Text, PlaceholderText and Highlight by role, the accepted green for Connected,
and on Home three state colours (amber Needs me, blue Waiting on others, green
Following) used at low strength for panel headers and rails and at full strength
for the reason word; colour never stands alone as a label. Type is the interface font at the accepted
size; the code font is used for the device code only.

## Architecture

The core decides. The host performs. Qt presents. Nothing here moves.

- **Qt adapter.** A `server_card` widget composes the existing `account_card`
  facts, the `device_authorization_panel` state machine and the session panel's
  connection, capability, rate-limit and diagnostic projections into one card
  per configured host; the panels' dispatch closures are reused unchanged. The
  Accounts page becomes a column of server cards plus the add button. The
  `github_session_panel` locator group, public-preview strip, host picker row
  and status banner are no longer parented into Settings; whatever composed
  courses still select them is repointed to the bar, the card or the strip, and
  the dead widgets are deleted once no course names them. Home's landing loses
  its focus-only buttons and becomes the first-launch card; the bar becomes the
  address field; a `home_panel` widget (**new**) renders a collapsible panel
  with tinted sticky header and rail, used for states, groups and Recently
  viewed, nesting state bands inside a group; one `home_row_delegate` (**new**)
  paints identity, title, the why sentence, reason word, account chip, time and
  progress for both the queue and recent models; Group and State headers are
  strip actions whose state is window-owned; the six combos become one Filter
  `QMenu` over the same typed preferences minus Repository and Bucket; owner and
  account avatars come from the existing avatar cache; the status bar text on
  Home comes from the session projection.
- **Application hosts.** No new host. Connect on a non-active card dispatches
  the existing host-profile selection intent followed by the existing connect
  intent. Copy details reads the existing diagnostic text. Forget server uses the
  existing saved-server removal intent behind the existing confirmation path.
- **Core.** No change. The attention grouping, queue preferences, device flow
  and public-preview stage are consumed as they are.
- **Composition root.** Supplies the same closures to the same widgets; the
  Settings page adopts the card column instead of three panels.

Prohibited: a stylesheet, a hard-coded palette, a second sign-in path, a widget
that reaches a store or a provider, any change to what the bar accepts or how a
pull request loads, and any change to publication or credential authority.

## Non-goals

Personal discovery (RFC-BPR-0026's saved-repository search), the repositories
drawer, the other five Settings pages, the strip, the overview, the profile
chooser (RFC-BPR-0033), notifications, and any new preference. The list shows the
rows the queue projection supplies today; this RFC does not make it find more.
Grouping, headers and chips arrange those rows; they persist nothing new beyond
the window's view state.

## Acceptance criteria

- **A1 — One card per server.** Accounts & hosts shows the title, the sentence,
  one card per configured host with header, account row, Access, Rate limit and
  Details, and the Add enterprise server button; the host-profile picker and
  its buttons, the locator, the status banner and the servers combo editor are
  absent from the page and from its accessibility tree.
- **A2 — Sign-in is the account row.** Sign in begins the device flow inside the
  card's account row and shows the code with its copy glyph, the address, Open
  browser, the validity, one status sentence and Cancel; denied, expired, unavailable and
  cancelled states show in the same row with one retry; no disabled button and
  no truncated field is shown, and the manual-token path stays reachable.
- **A3 — Enterprise servers are cards.** Add enterprise server adds a card in
  its edit state using the existing saved-server intents; Remember this server
  is a switch with its consequence; Forget server confirms and names its scope;
  a token session is labelled session-only.
- **A4 — Nothing repeats.** On every state of the Accounts page each of host,
  login, transport and connection state appears once, checked by a text
  inventory over the rendered page.
- **A5 — Home leads with the bar.** Signed in, Home's first row under the
  strip is the address-field bar with its label, placeholder, ⌘L and Return,
  focused on arrival; the landing's focus-only buttons are gone; a load
  refusal is one sentence with Copy details under the bar; the pull-request
  stack's empty page is not part of Home; first launch and signed out show
  the strip with Home, account and settings only, the bar relabelled for
  public reading, the sign-in card with Sign in filled and Add enterprise
  server bordered, and the status bar line, and nothing else. A signed-out
  window that has public reading history keeps Recently viewed (owner direction,
  2026-09-16, attended candidate `bpr-0779-a5-nothing-else`); so "nothing
  else" bounds the landing chrome this clause lists rather than the window's
  reading history.
- **A6 — Rows in state order, panels by state.** Rows are always ordered Needs
  me, Waiting on others, Following from the core attention projection, with
  one row anatomy for the queue and Recently viewed; colour means state only,
  on the reason word and the row mark or panel rail; with State headers on
  each state is a bordered panel with a tinted sticky header, count, chevron
  and rail, collapsible by pointer and keyboard with a one-line summary when
  collapsed; empty states are absent and an all-empty list is one sentence
  linking to Reviews & queue; no empty box renders.
- **A7 — Group, account and Filter.** Group offers None, Organization,
  Repository and Account, each group a neutral panel headed by the owner's or
  account's avatar and name, with state bands nested inside when State
  headers is on and plain state-ordered rows when off; a host shows only where
  it differs; with more than one signed-in account each row carries the
  account chip at its right, a single-account group carries it in its header
  instead, and a single-account profile shows none; the strip holds Group,
  State headers, Filter and the refresh glyph before account and settings;
  Filter holds exactly My role, Added by, Quiet items and Sort within a group
  over the existing typed preferences with their store and reset unchanged;
  Group, State headers, the state panels' and Recently viewed's collapse, and
  Filter persist per window, while a group panel's collapse lasts only while
  its window is open (owner direction, 2026-09-15); Home over a
  review keeps its one return row; the status bar shows the session line and
  never that no pull request is open while Home is shown.
- **A8 — Rendered and reachable.** The Accounts page in its connected,
  signed-out and signing-in states and Home in its first-launch, default,
  long-list scrolled, nothing-needs-me, over-a-review and four grouping states
  render at 100, 125, 150 and 200 percent in Light, Dark, High-contrast Light
  and High-contrast Dark, every control reachable by keyboard in reading order
  with an accessible name and a collapsed panel's state announced, and the
  owner accepts the real macOS application through the attended-review
  workflow.

## Verification

| Claim | Direct surface | Check |
|---|---|---|
| A1, A4 | Composed Settings window through the shipped composition | Walk the Accounts page's widget and accessibility trees in each connection state; assert the card inventory and the absence of the retired objects; inventory visible text for repeated facts |
| A2 | Device authorization public-path course with the recording device port | One Sign in gesture reaches the code and browser controls inside the card; drive cancel, expiry, denial, unavailable and manual-token cases and assert the row's state and the absence of disabled controls |
| A3 | Saved-server contract and composed Settings | Add, remember, forget with confirmation; restart; assert the record and no credential enrollment |
| A5, A6, A7 | Composed Home in the shipped workbench with two-account queue and recent fixtures | Focus on arrival; landing and strip inventory per session state; refusal placement; row delegate geometry and state order; panel collapse by pointer and keyboard with summary text; sticky header geometry under scroll; each Group value with State headers on and off; avatar and chip placement per account count; Filter menu contents equal the four surviving preferences and reset; per-window persistence across two windows; status bar text per state |
| A8 | Deterministic full-window captures then attended review | The existing four-appearance, four-scale capture course extended to the Accounts states and the nine Home boards; keyboard walk including collapse; VoiceOver names and collapsed state on macOS; one attended candidate per surface |

Ordinary slices use focused checks and reuse the existing composed courses.
One integration pass at the pack boundary. No live GitHub or GHES mutation is
needed; recording ports and fixtures suffice, and the attended candidate uses
the owner's own account read-only.

## Documentation impact

The settings guide and the workbench specification describe the card page and
Home's column; `docs/ux/workbench-specification.md`'s "GitHub host and
pull-request presentation", "Public preview" and "Signing in with a device code"
sections are rewritten to the card, and the settings authority inventory gains
the row-to-authority mapping for the card. Historical cards and packets are not
edited.

## Adopted Home addition amendment — 2026-10-07

[RFC-BPR-0062](RFC-BPR-0062-saved-repository-onboarding.md), registered under four-drafts-r1, supersedes the Settings-only addition route: signed-in Home offers Add repository in its discovery row, including a no-saved-repository prompt. Settings remains management for saved repositories and rules and invokes the same addition dialog. Signed-out Home, Open-bar grammar/focus, queue/group/filter ownership and unrelated acceptance remain unchanged. The owner selected direct URL/owner-name entry. Historical Done cards and plan exits are preserved; they do not accept the new surfaces. New evidence and judgments belong to BPR-1306–1311.

## Adopted authored Home amendment — 2026-10-11

[RFC-BPR-0066](RFC-BPR-0066-home-authored-pull-requests.md) supersedes this RFC's Home panels, Group/account/Filter, A6–A7 and the implementation allocation where they use attention alone for Home placement. Accepted author-role or authored-discovery membership is scoped to each exact host/account/PR row. Authored rows whose current bucket is waiting-only remain in Waiting on others (the owner's settled Q1); all other authored rows belong to My pull requests and never appear or count in Needs me. The order is Needs me → My pull requests → Waiting on others → Following → Recently viewed, inside every named group and with headers hidden. Attention, unread facts, notification eligibility and repository-drawer counts remain unchanged.

My pull requests has a neutral header, existing state-colored row anatomy and a collapsed count/actionable/unread summary with unknown detail preserved. Its independent top-level collapse starts expanded on upgrade and persists per window; named group and band collapse remains session-only. Filters/counts, selection/opening authority, Recently viewed, the adopted onboarding correction and the later stable Home layout remain intact. The core owns the new deterministic section decision; the host persists its view state; Qt presents it. Historical Done cards and plan exits retain their exact evidence, and RFC0032 A8 human/native obligations are neither accepted nor waived by this amendment. Fresh changed-surface evidence belongs to BPR-1388.

## Adopted draft Home amendment — 2026-10-11

[RFC-BPR-0067](RFC-BPR-0067-home-draft-pull-requests.md) extends Home row anatomy with a compact neutral outlined Draft chip before identity/title and draft-first accessible wording. It carries last accepted exact-authority metadata through bounded discovery, retained reads and recent history; unknown remains unknown and displays no chip. Accepted ready changes remove the chip, including matching queue/recent rows. Attention colours, sections, counts, enrollment, row actions and native controls retain their contracts. No additional request or full PR hydration is introduced. Historical Done cards, plan exits and RFC0032 A8 human/native obligations are unchanged; fresh evidence belongs to BPR-1389.

## Home waiting successor (RFC-BPR-0070)

[RFC-BPR-0070](RFC-BPR-0070-preserve-home-waiting.md) corrects the observations
consumed by Home: exact enrolled authorship, preserved pending requests, unmet
server review requirements and concurrent check/merge blockers. Authored waiting
placement and unread precedence remain as settled in RFC0066. Known blockers stay
visible under Hide quiet; bounded names, separate people/team counts and last-known
freshness extend existing native row anatomy. This successor adds bounded provider
reads and numeric-account-bound metadata retention; historical section rearrangement
and its evidence are unchanged. Historical human/native obligations are not waived.

## Remembered CLI sign-in amendment — 2026-10-12

RFC-BPR-0071 preserves first-time CLI discovery, account choice and read trust. A saved successful connection instead follows targeted revalidation; shared windows do not repeat selection or trust. Existing connection status/recovery controls report validation failures and session-only persistence. This amendment introduces no account-page layout change and does not answer BPR-1373’s pending owner look.

## Owner amendment: ignored pull requests — 2026-10-20

The owner asked on 2026-10-20: "I want to be able to ignore a PR on the home
screen, which will put it in a new ignored category that sorts to the bottom
and starts collapsed, you can unignore something that's in ignored by
expanding the collapsed. If an ignored PR gets closed it stops being tracked
on home." [BPR-1683](../issues/60-done/BPR-1683-ignore-pull-requests-on-home.md)
builds it.

A queue row's menu offers **Ignore** (Delete, or Backspace as macOS lists take
it). The row moves to **Ignored**, a neutral panel after Recently viewed in
every grouping and with State headers on or off. Ignored starts collapsed to
its count in every window. Its collapse lasts while the window is open and is
not part of the remembered per-window view. Expanded, each row carries
**Unignore** on hover and focus, in its menu and on the same key, and returns
to its own section. An ignored row keeps its bucket and facts. It draws no
state colour, is counted in no other panel, and raises no notification.

The core owns the decision. `core::home_ignore_list` takes typed Ignore and
Unignore intents and returns semantic deltas. Its observation step prunes an
ignore once the queue holds that pull request as closed or merged, or as off
Home for any other observed reason, so a pull request that comes back arrives
as news. `home_section_for` places an ignored item in `ignored`, and
`queue_precedes` orders ignored items after every other item. The queue host
persists the deltas in the queue store's `home_ignore` table (schema 11),
keyed by host, account, repository and number. Qt only projects the result.
Nothing changes on GitHub.

## Owner amendment: my review on Home rows — 2026-10-21

The owner asked on 2026-10-21: "On the home page I would like to know if I
have approved a PR. Right now they go to following and I don't really know
that I took action." [[BPR-PENDING-X]] builds it.

A queue row whose pull request I have reviewed leads its second line with a
verdict chip: "Approved", "Changes requested", "Commented" or "Review
dismissed", with " · new commits" when commits were pushed after the verdict's
review. The chip uses the Draft chip's thin neutral outline. Its mark is a
check in the accepted ink, a plus over a minus in the defect ink or a speech
bubble in quiet ink, and goes quiet once new commits arrive. This is a second,
narrow exception to colour meaning state on Home, beside lines added and
removed. The word carries the meaning without colour. The chip's tooltip and
the row's accessible name say "You approved this on <date>." and its
counterparts. The [workbench specification](../ux/workbench-specification.md#my-review-on-home-rows)
holds the full wording.

The core owns the decision. `core::my_review_for` reads my submitted reviews
in GitHub's order. Approved, Changes requested and Dismissed replace one
another, and a comment-only review replaces none of them. A dismissed review
leaves no verdict, so it never shows as approved. Staleness is the verdict
review's commit against the current head. The host reads the verdict from the
retained read Home already makes (`ListHomePullRequests` adds `submittedAt`
to its review nodes, and the REST reviews carry `submitted_at`). It adds no
request. The verdict is kept with the row's stored facts. Section rules,
attention buckets, notifications, ordering and counts are unchanged: an
approved pull request with nothing new stays in Following, and one with new
commits stays in Needs me as Changed.
