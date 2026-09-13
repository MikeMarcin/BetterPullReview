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
RFC text. The design reference is
`docs/rfcs/assets/bpr-shell-surfaces-v1/design/shell-surfaces-v1.html` with its
notes; the reference illustrates palette roles and is not runtime evidence.

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

Home is one left-aligned column of at most 720 logical points, centred in the
window, on the window ground. Signed in with nothing open it holds, in order:

- **The bar**, first and the page's one bold object: a bordered field with the
  heading "Open a pull request" inside it, the placeholder "Paste a link, or
  write owner/name#123", and Open as the filled button. It has focus when Home
  appears. The `workbench_landing` card's "Choose from the queue" and "Paste a
  link" buttons, which only moved focus, are removed; the landing's
  choose-pull-request state has no card.
- **A refusal under the bar** when the last open failed: one sentence from the
  existing diagnostic ("This sign-in cannot read EpicGames/UnrealEngine#1.")
  with Copy details beside it. The pull-request stack's empty page is not shown
  on Home; it remains the review surface's own empty state.
- **Recently viewed**, when it has entries: a section heading and rows. A row is
  the identity in the row weight, the title in ink, the time quietly at the
  right, and Resume as a row action shown on hover, on focus and in the row's
  menu. Return opens the row; Shift-Return resumes, as today. The heading has no
  button beside it.
- **The action list** as the accepted grouped rows: Needs me, Waiting on others,
  Following. A row is identity, title, my progress or role quietly, and the
  one-word reason at the right (changed, requested, replied, waiting, approved,
  following), amber only when the row needs me. Groups come from the existing
  core attention projection; Qt invents no urgency. An empty group is not shown;
  when every group is empty the page shows one sentence: "Nothing needs me. Pull
  requests from saved repositories and observer rules appear here." with the
  link opening Settings › Reviews & queue. No empty box.
- **Filter**, one flat menu button on the first group heading, holding the same
  six typed queue preferences (role, observer reason, bucket, repository, quiet
  or current, sort). A non-default choice shows as a removable chip under the
  heading. The menu is absent while there is nothing to filter. The preferences
  keep their store, schema and reset.
- **The status bar** carries the session line ("Signed in as MikeMarcin on
  github.com; 4,807 requests remaining." or "Not signed in. Public pull requests
  open read-only.") and never "No pull request is open in the workbench." while
  Home is shown.

Signed out, Home is the sign-in card (title "Sign in to GitHub", one sentence,
one filled Sign in button that opens Accounts & hosts with the device flow
begun), the same bar headed "Or read a public pull request" with a bordered
Open, and the status bar. The card's second button ("Accounts & hosts
settings") is removed. Home over an open review keeps its return: one row at the
top of the column, "Reviewing NVIDIA/stdexec#1846" at the left and Back to the
review at the right, above the bar; nothing else about the review is shown.

### Language shared by both

A row is label, consequence, control: no colons, no "Label: value" pairs, no
capitalised eyebrows, no numbering. A fact appears once on a page. One filled
control per page, and it is the page's job. Empty is a sentence, never a box.
A refusal sits beside what was refused. Words are the reviewer's, in the first
person RFC-BPR-0028 fixes ("Needs me", "my last review"), in sentence case, with
verbs on buttons and the same verb through a flow (Sign in, Signing in, Signed
in). Rhythm is an 8-point grid: page inset 24, card inset 16, row height 44
with one hairline between rows, section gap 24, one 8-point radius on cards and
the bar and none on rows. Colour is the palette: Window, Base, Mid, Text,
PlaceholderText and Highlight by role, the accepted green for Connected and
amber for a reason that needs me. Type is the interface font at the accepted
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
  its two buttons in the choose state and its second button in the sign-in
  state; the bar grows its heading; Recently viewed and the queue landing become
  one row style with a shared delegate; the six combos become a `QMenu` on one
  tool button with chips; the status bar text on Home comes from the session
  projection.
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
chooser (RFC-BPR-0033), notifications, and any new preference. The action list
shows the rows the queue projection supplies today; this RFC does not make it
find more.

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
- **A5 — Home leads with the bar.** Signed in with nothing open, Home's first
  object is the bar with its heading, placeholder and filled Open, focused on
  arrival; the landing's focus-only buttons are gone; a load refusal is one
  sentence with Copy details under the bar; the pull-request stack's empty page
  is not part of Home.
- **A6 — Lists are rows.** Recently viewed and the grouped action list share
  one row anatomy; Resume is a row action; an empty group is absent and an
  empty list is one sentence with a link to Reviews & queue; the six queue
  preferences are one Filter menu with chips for non-default choices, absent
  while there is nothing to filter, with their store and reset unchanged.
- **A7 — Signed out is three things.** Signed-out Home is the sign-in card with
  one filled button, the bar headed for public reading, and the status bar with
  the session line; the status bar never says no pull request is open while
  Home is shown; Home over a review keeps its one return row.
- **A8 — Rendered and reachable.** The Accounts page in its connected, signed-out
  and signing-in states and Home in its signed-out, empty, listed and over-a-review
  states render at 100, 125, 150 and 200 percent in Light, Dark, High-contrast
  Light and High-contrast Dark with the column at most 720 points, every control
  reachable by keyboard in reading order with an accessible name, and the owner
  accepts the real macOS application through the attended-review workflow.

## Verification

| Claim | Direct surface | Check |
|---|---|---|
| A1, A4 | Composed Settings window through the shipped composition | Walk the Accounts page's widget and accessibility trees in each connection state; assert the card inventory and the absence of the retired objects; inventory visible text for repeated facts |
| A2 | Device authorization public-path course with the recording device port | One Sign in gesture reaches the code and browser controls inside the card; drive cancel, expiry, denial, unavailable and manual-token cases and assert the row's state and the absence of disabled controls |
| A3 | Saved-server contract and composed Settings | Add, remember, forget with confirmation; restart; assert the record and no credential enrollment |
| A5, A6, A7 | Composed Home in the shipped workbench with queue and recent fixtures | Focus on arrival; landing button inventory; refusal placement; row delegate geometry; empty-sentence rendering; Filter menu contents equal the six preferences and chips reflect the store; status bar text per state |
| A8 | Deterministic full-window captures then attended review | The existing four-appearance, four-scale capture course extended to these states; keyboard walk; VoiceOver names on macOS; one attended candidate per surface |

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
