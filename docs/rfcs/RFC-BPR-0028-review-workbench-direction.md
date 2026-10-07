---
id: "RFC-BPR-0028"
title: "Review Workbench Direction: Overview, Epoch Line, Modalities, and Conversation"
status: "Proposed"
area: "BPR"
milestone: "Private alpha"
depends_on: ["RFC-BPR-0004","RFC-BPR-0005","RFC-BPR-0007","RFC-BPR-0008"]
pack_version: "post-v2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# RFC-BPR-0028: Review Workbench Direction: Overview, Epoch Line, Modalities, and Conversation

## Current state, 2026-09-24

This RFC is not ready to close; its remaining acceptance and explicit dependencies govern closeout.
[[BPR-1021]] is its closeout, in Backlog under the milestone `close-rfc-0028`
of plan `ux-redesign-pack-0028` (see the
[closeout planning record](plans/ux-redesign-pack-0028/rfc-0028-closeout-planning.md)).
The owner's verdicts of 2026-09-24 (recorded below) settled two of the three
things that held it open:

- **AC-01 is amended.** It now admits the review surface's entry rows between
  the strip and the epoch line: RFC-BPR-0037's composition entry row (Compose
  review, Draft comment or reply; `cc038748`, [[BPR-0929]]) and, while a pull
  request loads, [[BPR-0795]]'s readiness line. The chrome paragraph says the
  same. [[BPR-0940]]'s window course already asserts that arrangement. Where
  the entries should finally sit is a UX design follow-up, [[BPR-1106]], which
  does not hold this RFC. It is Done. The owner's decision moves the
  scaffolding to RFC-BPR-0052, which supersedes this RFC's chrome where they
  conflict, so AC-01 keeps its 2026-09-24 wording and this RFC closes on what
  it built.
- **[[BPR-0746]] is Done** by an owner amendment that withdraws its
  unobtainable AC1 and AC2. The amendment is recorded as an amendment, not as
  a pass.
- **[[BPR-0653]]**, the Windows title row, is still parked on human-review.
  On 2026-09-25 the owner accepted the station's Windows capture at 150%
  (decision `1aba8610`). The capture at 100% display scale is still owed, and
  so is one Windows station pass for its criterion 3; [[BPR-1021]] lists what
  blocks the closeout. The owner accepted Qt's private Windows native
  interface, and its criterion 1 passes under the amended AC-01.

## Current attended pointer, 2026-09-12 (d30)

The owner accepted the sole epoch-handle candidate in the protected
[v011 attended packet](../verification/reviews/rfc-bpr-0028-attended-v011/operator-packet.md)
at session sequence 373 on its signed, digest-bound bundle. [[BPR-0646]] is
Done, as is the separately accepted [[BPR-0729]] one-pane card; their v009
and v010 packet links retain historical lane paths and unchanged bytes. The
initial [v006 packet](../verification/reviews/rfc-bpr-0028-attended-v006/operator-packet.md)
prepared all nine authored milestone cards on one bundle and offline page.
This RFC remains open for its other criteria and native platform work; the
owner's handle verdict alone does not close it.

## Status and scope

The owner locked this direction on 2026-09-06 after five mockup rounds and
shipped it the same day. The accepted assets are
[the workbench states](../design/RFC-BPR-0028-workbench-direction-v5.html),
[the full review surface](../design/RFC-BPR-0028-full-review-surface-v1.html),
which is authoritative for the strip,
[the same surface in light](../design/RFC-BPR-0028-full-review-surface-light-v1.html),
and [Settings and the drawers](../design/RFC-BPR-0028-settings-and-drawers-v1.html).
It is the owner-directed replacement for the workbench presentation and is
the frontier exception the deferred UX RFCs name. It supersedes the layout and
chrome sections of RFC-BPR-0025 and the Home presentation of RFC-BPR-0026 where
they conflict; RFC-BPR-0025's browser-style file tabs are not part of this
direction and stay registered but unscheduled (the owner brought them back on
2026-09-24; RFC-BPR-0052 schedules them). The drawers and the first three
Settings pages are fixed below; RFC-BPR-0018 keeps the remaining pages.

The architecture rule is unchanged: the core decides, the host performs, Qt
presents. Everything here is presentation over projections that already exist
(review epochs, the link map, the thread map, the private ledger, the appearance
themes). The diff text colour themes and the DiffLinkMap ribbons are finished
work and are rendered as they ship.

## Problem and outcome

The current header is a 220-pixel scroll area whose contents (session panel,
device sign-in, queue landing, pull-request stack, body editor, appearance
settings) hide sign-in and settings below the fold and steal height from the
comparison. Reviewing a pull request the way Code Collaborator supports it, an
overview first and then a linear read with next-change and next-file, is not
possible. The outcome is a window whose chrome is two thin rows, whose default
reading state is nearly all code and comments, and whose starting page answers
what changed and who has signed off.

## Accepted design

**Chrome.** One 30-pixel strip in full ink, bordered buttons, three regions:
left, the two drawer toggles (repositories, files), the repository and pull
request identity, then the navigation groups (previous/next change with its
position, previous/next file with its position, jump to the next new comment
with the count); centre, alone, the modality control as a filled segmented
control with its shortcuts; right, refresh, the comments toggle, a divider,
account, settings. Below it the 22-pixel epoch line: one row as wide as the
window's client area, "Updates" at its left, the line, and the "Comparing …"
menu at its right, above the drawers and their headings and never inside the
centre pane, so its labels hold their place whatever the drawers' states and
widths and the modality ([[BPR-0737]], owner direction 2026-09-11). Between
the strip and the epoch line sit the review surface's entry rows and nothing
else: the composition entry row (Compose review, Draft comment or reply,
RFC-BPR-0037) and, while a pull request loads, the readiness line
([[BPR-0795]]) (owner amendment, 2026-09-24). [[BPR-1106]]'s design round
decided the successor on 2026-09-24: RFC-BPR-0052's review bar, folding
Updates line and document tabs. No toolbar with labels, no menu bar rows.
Nothing in the strip may read as disabled.

The strip is the title row. The owner's references (Fork and Visual Studio
Code on both platforms, Windows Terminal, Ghostty, the Claude app) share one
pattern: the window controls sit at the edges of the first row and the
application's own navigation fills it, with the document's identity standing
in for a title. A separate title bar that names only the application is
wasted height. On macOS the traffic lights occupy the strip's left inset and
the title text is hidden; the identity is the title, exactly as Fork shows the
repository and branch there. On Windows, where Visual Studio Code is the
closest reference, the strip carries a compact menu at its left, the same
regions, and the minimise, maximise and close buttons at its right, with the
system's snap layouts and drag behaviour preserved. Empty strip space drags
the window and double-clicking it zooms, as the platform title bar would.

**Epoch line.** A single line with one node per review epoch, that is each
update (push) that changed the pull request, never a commit or a file's
history; the label reads "Updates" and the dates sit in their own row beneath
the nodes. The two compared updates are drawn as bracket handles with a blue
bar between them, and an amber tick marks "my last review" with only a
tooltip. Handles drag. Click a node to compare from it, ⌘-click to compare to
it, right-click for a menu (compare from here, compare to here, show this
update alone, copy head commit). Hovering a node shows the update's files and
head commit. Because dense histories make precise mouse selection hard (the owner's
Perforce experience), each handle is also moved by keyboard one upload at a
time when the line has focus, and the named comparisons (whole PR, since last
upload, since my last review) remain in a dropdown at the line's right.

**Modalities.** Overview, Read, Review and Discuss are presets for which
drawers are open, chosen from the strip or ⌘1 to ⌘4, and they stay until
changed. Read collapses the files and repositories drawers ([[BPR-0724]],
owner direction 2026-09-11: the code lacked horizontal room) and keeps
comments beside the code. Read presents the comparison in one of two ways: two
whole-document panes over the link map, or one document in reading order with
the other side's lines kept in place and struck through on their theme field.
The link map is the same map in both: with one column it has no second pane to
cross to, so it joins the two halves of each change — the struck rows and the
added rows — at a spine in the gutter at the left, and a change with an empty
side draws one ribbon rather than two. The presentation is a View item beside
Line numbers, remembered per window and seeded for a new window from the
profile default, and it changes nothing about which lines the comparison
contains ([[BPR-0729]], owner direction 2026-09-11 and 2026-09-12).
Review opens the files drawer too; Discuss widens the conversation over the
code. Overview keeps every drawer collapsed: a drawer opened by hand while
Overview is the modality is refused by the core with a stated reason, the
strip toggle stays enabled and says so, and choosing Read, Review or Discuss
is the way to open one ([[BPR-0727]], owner direction 2026-09-11). Nothing
collapses on its own. Automatic transitions (for example, opening
a pull request shows Overview) are off by default and are configured as rules
in Settings. A collapsed drawer's presentation follows the drawer-controls
setting ([[BPR-0721]], owner direction 2026-09-10): with the controls shown it
is an 18-pixel rail with a vertical label and an unread count; with them
hidden, the default, it leaves no column at all and the strip's toggle is the
way back. Amended 2026-10-06 by the owner's direction on [[BPR-1295]]: Read
merges into Review, leaving three modalities, Overview, Review and Discuss, on
⌘1 to ⌘3 (the keys are proposed and confirmed at the built UI). Every path that
entered Read enters Review, which keeps comments beside the code and offers
both presentations. Review remembers the drawer arrangement it last had in each
window, hidden drawers included, and applies its preset (files and comments
open) only on a window's first entry and when ⌘2 is pressed while already in
Review. One command, ⌘B (Ctrl+B elsewhere) and an icon beside the comments
toggle, hides every open drawer, and a second press restores exactly the
drawers that were open at their saved widths; Overview refuses it and Discuss
disables it. A focused composer keeps ⌘B to itself. Amended 2026-10-06 by the
owner's decision and answers on [[BPR-1293]]: Conversations is added beside
Discuss, so the modalities are Overview ⌘1, Review ⌘2, Conversations ⌘3 and
Discuss ⌘4. Conversations puts a finder over every conversation on a file in
the main pane, grouped by file, for navigation only; picking one enters Review
on its file with the thread open in the comments drawer at the picked post.
⌘J goes straight to it. The drawers are hidden while it shows, without
changing their states, and ⌘B is disabled there. Discuss is kept as it is,
even after [[BPR-1294]]. Amended 2026-10-06 by the owner ("can we swap
discuss to 3 and conversations to 4"): Discuss is ⌘3 and Conversations ⌘4.
[[BPR-1295]] builds Overview ⌘1, Review ⌘2 and Discuss ⌘3 and leaves ⌘4 for
[[BPR-1293]].

**Overview.** A pull request opens on an overview: title, author and branches,
description, a files table grouped by folder with operation, added and removed
line counts, changed-since-my-last-review, my read state, unread and total
comment badges and my note; participants with their sign-off state; a
"since my last review" summary (uploads, files changed, new comments, replies to
me, my unresolved threads, checks); my review progress; and one action,
Continue review (Start review on first visit), which enters Read at the first
unseen change. Amended 2026-10-06 ([[BPR-1295]]): both enter Review.

**Read** (Review since 2026-10-06, [[BPR-1295]]). Side-by-side diff with the product theme and the DiffLinkMap between
the panes. Gutter marks: amber for a line with a thread, blue for a new one.
The comments drawer lists this file's threads aligned to their lines as cards
on an avatar rail with a thread line; a whole-PR filter lists every thread in
file order. Unread posts carry a blue dot and full-ink names; read posts step
back. My unpublished notes appear as dashed drafts in the same column with
Publish, Edit and Delete. Amended 2026-10-06 by the owner's verdict on
[[BPR-1290]]'s inline drawer prototype ("I like it"): the drawer is inline
only. It shows this file's threads as notes beside their lines, each opening in
place to the whole conversation with a reply; the whole-PR filter leaves the
drawer for [[BPR-1293]], and the thread map with it.

**Discuss.** The conversation widens across the code. Left: a thread list
(file and line roots, replies under a guide line, unread dots, counts) with
Whole PR, This file, Unread and Unresolved filters, and below it the accepted
thread map for the selected thread. Right: a feed with one card per thread,
its code excerpt on the theme, the root post and replies on a continuous rail,
each post headed by author, role, "replying to", unread dot and time, and
Reply, Resolve, Mark read and Open in code actions. Amended 2026-10-06 by the
owner's verdict on [[BPR-1290]]'s inline drawer prototype ("I like it"): the
thread map and the separate feed leave Discuss as they leave the drawer.
Discuss keeps its Whole PR, This file, Unread and Unresolved filters and lists
every thread as the drawer's notes in file order, each opening in place.

**Drawers.** Left to right: repositories, files, comparison, comments. The
repositories drawer is a tree: each repository the profile's accounts have
saved is a bold row with a chevron and "needs you of total"; its pull requests
of interest hang under it on a guide line, one per row with number, title, the
unread dot and a one-word reason at the right (changed, requested, replied,
waiting, approved, following), amber when it needs me; the account is named
once in the drawer header and a host shows only where it differs. Selecting a
row opens that pull request on its Overview. The files drawer shows the
selected pull request's tree: chevrons, guide lines, nesting-only folders
compacted into one row, file counts on folders, the read-state dots and line
counts of the overview, tree/flat/changed-first orders and a filter that keeps
parents visible. Each drawer has its own strip toggle and shortcut and, while
the drawer controls are shown, a Hide button and an 18-pixel rail with a label
and count; with the controls hidden (the default since [[BPR-0721]]) a
collapsed drawer is absent and an expanded one has no Hide button. Collapsing
one never moves the other; a modality sets both, a manual toggle overrides
until the next modality change. Amended 2026-10-06 ([[BPR-1295]]): Review
restores the arrangement it last had rather than setting the drawers, and ⌘B
hides every open drawer and brings back the same set; opening a drawer by hand
while they are hidden drops that set.

**Settings.** RFC-BPR-0018's window and sidebar, with these pages fixed by the
asset: Accounts & hosts (one card per server, device-grant sessions shown as
remembered and self-renewing, enterprise tokens labelled session-only with a
"remember this server" switch that keeps hostname and account), Appearance
(theme as four previews from the real palette with a half-and-half System
thumbnail, interface size, code font defaulting to JetBrains Mono NL as
installed, code size and tab stops, ligatures, per-role colour customisation
with preview, theme import and export, the tuned diff colours untouched), and
Reviews & queue (modality rules, none on by default; reading behaviour;
drawer controls, off by default; observer rules; saved repositories). The other four pages remain as
RFC-BPR-0018 describes them. Amended 2026-10-06 ([[BPR-1295]]): the modality
rules offer Overview, Review or the last used modality for opening a pull
request, and Continue review always enters Review.

**Home.** An action list: grouped rows (needs you, waiting on others,
following) with pull request, title, why the row is here, my progress, author,
role and updated time. Open by URL or owner/repo#number in the bar. Rules and
saved repositories live in Settings. Signed out ([[BPR-0732]], owner direction
2026-09-11): Home is the sign-in card, the bar and the status bar; the bar
opens a public pull request without an account in one gesture (the core's
`public_preview` stage, serviced by the host), and the queue, its filters and
the pull-request stack's empty page are not shown until a session with an
account exists. A public-preview session has none. Recently viewed
([[BPR-0733]], owner direction 2026-09-11): the app no longer opens the
last pull request by itself at startup; Home stays until a gesture. Between
the bar and the queue, a "Recently viewed" list (per profile, ten entries,
most recent first, persisted by the restoration store) opens a row on
Overview or resumes its saved reading; signed out it lists the public
entries only, and an account's entries appear once that account's session
exists. Empty, the section is not shown.

**Vocabulary.** First person and event names: "my last review", never "you".
Column headings are one word; actions are two.

## Non-goals

Browser-style file tabs (scheduled again by RFC-BPR-0052 on the owner's
2026-09-24 decision, outside this RFC); the lane-rail conversation drawer
rejected earlier;
changes to the diff themes or the link map, except that the link map may be
extended to the one-pane Read presentation, which is the same map at its limit
with one column instead of two (owner direction 2026-09-11, rejecting
[[BPR-0734]] in the v008 attended session: "Relax the non-goals restriction to
allow extending the link-map in this limit direction"). That extension is this
and nothing wider: with one column the map has no second pane to cross to, so
it joins the two halves of each change — the struck rows and the added rows —
at a spine in the gutter at the left, and a change with an empty side draws one
ribbon rather than two. The two-pane ribbons, the diff themes and the thread map
stay settled work, and the one-pane gutter carries no whole-file overview,
because the two-pane state has none ([[BPR-0745]], the state redrawn in
[direction v7](../design/RFC-BPR-0028-workbench-direction-v7.html#read-one-pane)).
Also out of scope: automatic drawer collapsing without a configured rule; the
Settings pages, the explorers and the file tree, which follow in their own
design rounds.

## Acceptance criteria

- AC-01 The chrome above the comparison is the strip, the review surface's
  entry rows and the epoch line, in that order, with no scroll area and no
  separate title bar, at every window size the workbench supports; the strip is
  the row the window controls sit in. The entry rows are the composition entry
  row (Compose review, Draft comment or reply) and, while a pull request loads,
  the readiness line; nothing else sits between the strip and the epoch line
  (owner amendment, 2026-09-24).
- AC-02 The epoch line selects any two uploads by drag, click, ⌘-click, menu and
  keyboard stepping, shows my last review, and drives the comparison the
  existing epoch host already resolves.
- AC-03 The four modalities set the drawers as described, persist per window,
  never change on their own unless a configured transition fires; in Overview
  a drawer gesture is refused with a stated reason and every drawer stays
  collapsed until another modality is chosen ([[BPR-0727]]); a collapsed
  drawer is its rail while the drawer controls are shown and no column while
  they are hidden ([[BPR-0721]]). Amended 2026-10-06 ([[BPR-1295]]): there are
  three modalities, Overview, Review and Discuss; Review restores the
  arrangement it last had in the window, hidden drawers included, and its
  preset only on the window's first entry or ⌘2 in Review; ⌘B and its icon hide
  every open drawer and restore exactly the same set, refused in Overview,
  disabled in Discuss, and yielded to a focused composer. Amended 2026-10-06
  ([[BPR-1293]], with the owner's answers): there are four modalities,
  Overview, Review, Conversations and Discuss on ⌘1 to ⌘4; ⌘J goes straight
  to Conversations; while it shows the drawers are hidden without changing
  their states, and ⌘B is disabled there.
  Amended 2026-10-06 by the owner ("can we swap discuss to 3 and
  conversations to 4"): Discuss is ⌘3 and Conversations ⌘4.
- AC-04 The overview shows every file with the listed columns from projections
  the host already publishes, plus participants and the since-my-last-review
  summary, and Continue review lands on the first unseen change.
- AC-05 Review (Read until [[BPR-1295]] merged it into Review, 2026-10-06)
  shows this file's threads aligned to their lines with unread state
  and drafts, in both presentations; in one pane a thread names a side and a
  line and sits against the row carrying that identity, so a thread on a
  removed line sits against its struck row and a thread on an added line
  against the added row, and threads that would collide keep document order and
  are pushed down with their leader still on the anchored row; Discuss shows the
  list, map and feed with the listed filters. Amended 2026-10-06 by the owner's
  verdict on [[BPR-1290]]'s inline drawer prototype ("I like it"): the open
  thread is pinned at its anchored row and the threads around it make room
  above and below it, with the same document order; Discuss shows the list
  with the listed filters, each thread opening in place, and no map or
  separate feed.
- AC-06 Home is the whole centre while nothing is open, with none of the
  review chrome, and the strip then shows only Home, account and settings;
  a Home button (⌘0) at the strip's left returns to Home from any review
  (owner decision, 2026-09-07: Home is a view with its button, not a fifth
  modality); it shows the grouped action list from the queue and ledger
  projections; with no account it funnels to sign-in, and with no pull
  request open it funnels to the queue or the locator, rather than showing
  empty panes.
- AC-07 Sign-in remains reachable in every state without scrolling.
- AC-09 The repositories and files drawers render the trees above from the
  queue, ledger and snapshot projections and collapse independently.
- AC-10 The three fixed Settings pages exist with live controls, and every
  other page names where its settings will live.
- AC-08 A recorded conversation with more than twenty posts and three branches
  reads correctly in Review and Discuss (the owner's stated stress case; Read
  until [[BPR-1295]] merged it into Review, 2026-10-06).
  Amended 2026-10-06 by the owner's verdict on [[BPR-1290]]'s inline drawer
  prototype ("I like it"): it reads in place, every post in posting order with
  its complete body, and a reply that answers anything but the first post
  names whom it answers. Navigating its structure is [[BPR-1293]]'s.

## Implementation entry

[[BPR-0646]] chrome, epoch line, modalities and rails; [[BPR-0647]] overview;
[[BPR-0648]] aligned comment cards; [[BPR-0649]] Discuss; [[BPR-0650]] Home
action list; [[BPR-0651]] the repositories and files drawers; [[BPR-0652]] the
three fixed Settings pages. Closeout follows the ordinary structure gate.

The pack grew as the direction was carried in: [[BPR-0653]] the strip as the
Windows title row (Windows only, not eligible on this Mac); [[BPR-0654]] the
files changed since my last review; [[BPR-0655]] and [[BPR-0664]], two course
defects the slices exposed; [[BPR-0656]] read state through conversation
receipts; [[BPR-0657]] threaded replies and resolve through the mutation port;
[[BPR-0658]] the files tree; [[BPR-0659]] the repository projection and saved
repositories; [[BPR-0660]] enterprise servers and the session's grant facts;
[[BPR-0661]] the review behaviour preferences; [[BPR-0662]] drawer widths and
[[BPR-0663]] the public-preview review history, two defects the signed bundle
found; and [[BPR-0665]], an understated automated-run bound left for later.

Status, 2026-09-07. Fifteen cards are Done with direct passing evidence.
BPR-0646, BPR-0662 and BPR-0663 are complete except for what a person has to
look at, and are parked on one attended session prepared at this head
([the operator packet](../verification/reviews/rfc-bpr-0028-attended-v001/operator-packet.md)):
the workbench chrome with a pull request open, the drawer widths in the signed
bundle, and the review history over Public preview. The RFC stays open until
those three verdicts land; BPR-0653 waits for the Windows station and BPR-0665
is an ordinary follow-up.

Reconciled 2026-09-08: the three attended candidates have no recorded verdict.
Their former candidate inputs were released for the owner-directed continued
development and their live attended-session phase is now `waiting`, with no
active protection. The historical packet above is not a current prepared build;
prepare affected material again before requesting those verdicts. Required
Windows evidence and the nonterminal RFC status remain unchanged. The activated
0018 → 0024 → 0025 → 0026 workstream continues under the canonical index, retaining
this RFC's superseding Home/layout decisions and unscheduled browser-style tabs.

Prepared again, 2026-09-10, under the adopted pack plan
[ux-redesign-pack-0028](plans/ux-redesign-pack-0028/milestone-plan.json),
milestone `attended-preparation-pack-head`. The tests-disabled bundle at the
pack head (executable sha256 `484b2072…`) and one attended session carry all
nine human-pending cards of this RFC and RFC-BPR-0018: BPR-0646, BPR-0662,
BPR-0663, BPR-0667, BPR-0674 (two candidates), BPR-0675, BPR-0676, BPR-0677
and BPR-0683. The packet is
[rfc-bpr-0028-attended-v002](../verification/reviews/rfc-bpr-0028-attended-v002/operator-packet.md);
its identity file binds the executable digest, the page digest and each
candidate fingerprint. The integration pass at that head first found four
red-at-HEAD test surfaces left by BPR-0689, BPR-0695, BPR-0710 and BPR-0714
(a stale Home-bar course, two `make` callers, two variant counts and three
application digest pins) and repaired them in the same pass; no product
source changed. No verdict is recorded yet; every card stays parked on
human-review. Next: consume the owner's verdicts and finish each card from
its own acceptance, then `close-rfc-0018` (BPR-0593), then BPR-0595, which
also waits on the canonical RFC-BPR-0008 closeout BPR-0242 outside this pack.

Rejected, 2026-09-10. The owner ran the v002 session and rejected
[[BPR-0646]] (decision `bdd9244a`): the strip is accepted, but no file
could be opened and no epoch line appeared, and opening a pull request
without an account should be possible directly from Home. Nine candidates
stay undecided. The blocker is not the chrome: the signed-out open no longer
reaches a comparison, bisected to `826fb871` (BPR-0705, RFC-BPR-0024) and
carded as [[BPR-0715]] under RFC-BPR-0024; the Home direction is registered
as [[BPR-0716]] under RFC-BPR-0026 as an input to its inventory. Candidate
protection for session `cbd48b91…` is still active and no product source
changed. Next: fix BPR-0715, run the two red selectors in the pack's
integration pass, rebuild the tests-disabled bundle and prepare the attended
session again at that head for all ten candidates.

Prepared again, 2026-09-10 (v003). [[BPR-0715]] is diagnosed and fixed:
the bundle's failing event was the workbench refusing the signed-out
authority (no account, no viewer) before listing any file, and the course's
red was the application host no longer seeing the primary window's accepted
read after the RFC-BPR-0024 window composition; the fixture now serves the
signed-out smart-Git lease so the course reaches an exact comparison. The
integration pass at that head is the composed workbench and Settings Qt
selectors, `qt_anonymous_public_preview_scale_{100,125,200}`, both
application composition contracts, the public-path courses including
`application_anonymous_public_preview_public_path`, the touched host suites,
`check_boundaries`, strict tidy, `clang-format --Werror` and the
documentation gate; `application_workbench_composition_contract` is red at
the same head before and after the fix and is carded as [[BPR-0718]], and
the stale reflow assertion is repaired as [[BPR-0717]]. The tests-disabled
bundle (executable sha256 `9334b13a…`) carries all ten candidates again in
[rfc-bpr-0028-attended-v003](../verification/reviews/rfc-bpr-0028-attended-v003/operator-packet.md),
session `cbd48b91…` at revision `app-release-9334b13a-2026-09-10.2`; the
v002 rejection stays in the session history. BPR-0715 is parked on
human-review for the one criterion only the loaded bundle can show. Next:
the owner's run, then consume the verdicts and finish each card from its
own acceptance.

Attended verdict, 2026-09-10 (v003). Session `cbd48b91…` was handed back
at sequence 141 with one decision: [[BPR-0646]] rejected (decision
`65f3f881`, three screenshots; the identical `b1348a9e` was withdrawn and
is superseded). The files, strip, rails and epoch line all reached the
owner this time, so [[BPR-0715]] no longer masks the pack. The rejection is
four bounded follow-ups, none of them BPR-0646's criterion as written:
[[BPR-0719]] (the core refuses an 8,000-line file with a 291-line insertion
at its LCS matrix bound, owner direction that no file may be refused for
size; RFC-BPR-0004/0013), [[BPR-0720]] (comments green and includes grey on
the dark palette; owner direction under RFC-BPR-0006, unchanged since
BPR-0199 and held for the owner's confirmation of extent), [[BPR-0721]] (a
setting that hides the rails and Hide buttons leaving the strip toggles,
plus the drawer heading row left visible above each rail; AC-03's rail
wording is amended when it lands) and [[BPR-0722]] (the strip hides the
pull request identity below 1,400 pixels). The nine other candidates are
undecided and stay protected; the epoch line's "No updates loaded." in the
screenshots is recorded on [[BPR-0663]] as an observation. The owner's
"Even in read mode viewing experience is not great" is an open question to
the owner, not a card.

Attended preparation, 2026-09-10 (v004). [[BPR-0719]], [[BPR-0721]] and
[[BPR-0722]] landed; [[BPR-0720]] waits on the owner's answer. The same ten
candidates plus one for each of the three are prepared again in
[rfc-bpr-0028-attended-v004](../verification/reviews/rfc-bpr-0028-attended-v004/operator-packet.md)
on the bundle built at 3c9a3f1c (executable sha256 `9261509a…`), candidate
revision `app-release-9261509a-2026-09-10.3`; the v003 packet is
historical. The drawer controls are off by default in this bundle, so the
rail criteria are judged with the setting on. No host is running until the
owner asks for the session.

Owner's answers, 2026-09-11. The three questions the v003 verdict left
open were answered. Read mode "was difficult to read the code because of
a lack of horizontal area for the text display": [[BPR-0724]] lands the
bounded part (Read collapses the repositories drawer; the modality
paragraph above is amended) and asks the owner, as an options candidate in
the v005 packet, whether Read should also close the conversations drawer
(which would amend AC-05), let it go narrower, get a single-pane layout, or
stay as it is. The dark palette: "the whole palette, yes reset", landed on
[[BPR-0720]] under RFC-BPR-0006. The strip below about 960 pixels: "leave
it be for now", recorded on [[BPR-0722]] as a decision to keep the
900-pixel floor and the whole-identity yield; nothing else on that card is
settled by it.

Attended preparation, 2026-09-11 (v005). [[BPR-0720]] and [[BPR-0724]]
landed. The thirteen v004 candidates plus one for each of the two are
prepared again in
[rfc-bpr-0028-attended-v005](../verification/reviews/rfc-bpr-0028-attended-v005/operator-packet.md)
on the bundle built from the source at 25e1525d (executable sha256
`d238842d…`), candidate revision `app-release-d238842d-2026-09-11.1`; the
v004 packet is historical. No host is running until the owner asks for
the session.

Attended preparation, 2026-09-11 (v006). [[BPR-0718]] and [[BPR-0723]]
landed (the workbench composition and UIforETW selected-diff contracts are
green again, the latter's rail block amended for BPR-0721's default). The
fifteen v005 candidates are prepared again, unchanged in substance, in
[rfc-bpr-0028-attended-v006](../verification/reviews/rfc-bpr-0028-attended-v006/operator-packet.md)
on the bundle built at d1ba791f (executable sha256 `cc2afa7f…`). The
v005 packet is historical. No host is running until the owner asks for
the session.

Attended verdict, 2026-09-11 (v006). Session `cbd48b91…` was handed back
at sequence 278 with all fifteen candidates decided on the d1ba791f
bundle. Accepted and Done: [[BPR-0662]], [[BPR-0667]], [[BPR-0674]]
(headings; and the owner chose that Overview refuses a hand-opened
drawer, [[BPR-0727]]), [[BPR-0675]], [[BPR-0677]], [[BPR-0719]],
[[BPR-0721]] (both drawer-control states), [[BPR-0722]] and
[[BPR-0724]] (the owner chose a narrower conversations drawer in Read,
[[BPR-0728]], and a single-pane Read layout in the manner of p4merge as
a new slice, [[BPR-0729]], which needs a design round on the accepted
asset and amendments to the Modalities paragraph and AC-05 before it
starts); RFC-BPR-0018's [[BPR-0683]] was accepted for macOS. Rejected
and parked: [[BPR-0646]] (the strip is accepted again; the epoch line
could not be judged; the rail finding was retracted on BPR-0675; sign-in
is "technically yes", with the Home flow on [[BPR-0716]]), [[BPR-0663]]
and [[BPR-0676]], all three because the epoch line reads "No updates
loaded." on a restored review: a cache hit prepares the revision with
no mutation and nothing publishes the catalogue's updates to the line,
[[BPR-0725]], which is AC-02's defect to fix; and [[BPR-0720]] under
RFC-BPR-0006, because the C++ grammar emits no function, user-type or
macro role, [[BPR-0726]]. AC-03 gains the Overview rule when BPR-0727
lands. The session's candidates were released and the host stopped.
Next: BPR-0725 and BPR-0726 on main, a rebuilt bundle, and a v007 packet
re-asking the four parked candidates.

Epoch line on a cache hit, 2026-09-11. [[BPR-0725]] landed: the core
requests the newest history page as soon as the catalogue revision is
settled (a no-mutation prepare or a committed mutation), so the line
carries every update the catalogue holds on a restored or cached open;
AC-02's defect from the v006 rejections. Parked for the v007 verdict.
The owner's Home answer became [[BPR-0732]] and [[BPR-0733]]; the
single-pane design round is [[BPR-0734]], after the v007 verdicts.

Attended preparation, 2026-09-11 (v007). The bundle was rebuilt at
2cdc0e57 (executable sha256 `64d9e46a…`, tests disabled, signed) after
[[BPR-0733]] was parked, so no active card can rebuild it. Ten candidates
are prepared in
[rfc-bpr-0028-attended-v007](../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md):
the four v006 rejections asked again ([[BPR-0646]] narrowed to the epoch
line, [[BPR-0663]], [[BPR-0676]], [[BPR-0720]]) and the six landings since
([[BPR-0725]], [[BPR-0726]], [[BPR-0727]], [[BPR-0728]], [[BPR-0732]],
[[BPR-0733]]). The v006 packet is historical; its fifteen verdicts stand.
No host is running until the owner asks for the session.

Attended verdict, 2026-09-11 (v007). Session `cbd48b91…` was handed back
at sequence 322 with all ten candidates decided on the 2cdc0e57 bundle.
Accepted and Done: [[BPR-0663]], [[BPR-0676]], [[BPR-0725]] (the line
carries the catalogue's updates on a cache hit; AC-02's v006 defect is
closed), [[BPR-0727]] (including the judgement that Overview is chosen,
never inferred), [[BPR-0728]] (with a new defect: the resize cursor does
not stay over the handle, [[BPR-0739]]), [[BPR-0732]] and [[BPR-0733]]
(including the judgement that a refused startup authority is silent);
[[BPR-0716]] finished on BPR-0732's verdict; RFC-BPR-0006's [[BPR-0726]]
was accepted. Rejected and parked: [[BPR-0646]], because the epoch line
sits in the centre pane and its labels move with the drawers in every
modality; the owner wants the line and the "Comparing …" menu on one
full-width row under the strip, above the drawer headings, [[BPR-0737]],
which amends the chrome paragraph when it lands; the handle criterion
itself is still unjudged because the packet's pull request has one
update, so the next ask names one with several. [[BPR-0720]] under
RFC-BPR-0006, because the macro and function roles paint the wrong
colours against the Visual Assist reference, [[BPR-0738]]. Of this RFC's
cards, BPR-0646 (one criterion) is the only one parked; BPR-0653 waits for
the Windows station; BPR-0729 waits for the design round [[BPR-0734]];
BPR-0736 is in Inbox; BPR-0737 and BPR-0739 are Ready. The RFC stays
open. Next: BPR-0734, then BPR-0737 and BPR-0739 on main, a rebuilt
bundle and a v008 packet re-asking BPR-0646 and BPR-0720. The owner
named NVIDIA/stdexec#1846 (2026-09-11, mandate r9) as the multi-update
pull request for the BPR-0646 and BPR-0737 asks.

Attended preparation, 2026-09-11 (v008). [[BPR-0737]], [[BPR-0738]] and
[[BPR-0739]] landed and the design round [[BPR-0734]] was drafted; the
bundle was rebuilt at 35438244 (executable sha256 `e4fdb0ea…`, tests
disabled, signed) after [[BPR-0740]] finished. Six candidates are prepared
in [rfc-bpr-0028-attended-v008](../verification/reviews/rfc-bpr-0028-attended-v008/operator-packet.md):
the two v007 rejections asked again ([[BPR-0646]], [[BPR-0720]]) on
NVIDIA/stdexec#1846, which the owner named as a pull request with more than
one update, and the four since ([[BPR-0734]], [[BPR-0737]], [[BPR-0738]],
[[BPR-0739]]). The v007 packet is historical; its ten verdicts stand.

Attended verdict, 2026-09-11 (v008). Session `cbd48b91…` was handed back at
sequence 348 with all six candidates decided on the 35438244 bundle; every
decision matches its candidate's fingerprint in the packet's identity file.
Accepted and Done: [[BPR-0737]], the epoch line and the "Comparing …" menu as
one full-width row under the strip, which closes the v007 finding on
[[BPR-0646]]; the chrome paragraph above already carries it.

Rejected and parked, four of them with new work:

- [[BPR-0646]], whose one remaining criterion — a handle moves the comparison
  — is unjudged for the third round. Not for want of a busier pull request:
  a review epoch is this install's own observation of base and head, nothing
  enumerates a pull request's earlier updates, and a first open therefore
  yields one epoch for any pull request. The local catalogue held exactly one
  epoch for NVIDIA/stdexec#1846, minted five minutes before the verdict.
  [[BPR-0743]] holds the owner's decision: import a pull request's earlier
  updates under RFC-BPR-0003, or keep the line a record of what this install
  has seen and reword AC-02's attended criterion to match. The criterion is
  not asked again on another pull request until that answer lands.
- [[BPR-0734]], because the one-pane presentation should keep the link map
  rather than gain a change map of its own. That answers the round's third
  question by rejecting its premise and relaxes this RFC's non-goal above;
  block order and where the presentation choice lives are still unanswered
  and are asked again. [[BPR-0745]] redraws state 2b over the link map and
  rewords the Modalities amendment; [[BPR-0729]] still waits behind
  BPR-0734's acceptance.
- [[BPR-0739]], because the resize cursor over the seam is still the arrow.
  The seam itself is accepted ("at least it draws the seam now"). The
  highlight is painted only while Qt believes the pointer is on the handle,
  so the handle had the pointer and the cursor was still wrong: the d17
  cause (a hot zone too narrow to hit) is refuted and the defect is cursor
  delivery on macOS after Qt's enter. [[BPR-0746]] carries it, with the
  logging check that discriminates the hypotheses on the real application
  rather than another offscreen pass. Whether the hover highlight should go
  is still unanswered.
- RFC-BPR-0006's [[BPR-0720]] and [[BPR-0738]], one requirement in two
  notes: a preprocessor line paints grey end to end where the reference greys
  only the directive. [[BPR-0744]] carries it.

Of this RFC's cards, BPR-0646, BPR-0734 and BPR-0739 are parked on
human-review; BPR-0653 waits for the Windows station; BPR-0729 waits for the
design round; BPR-0736 is in Inbox. The RFC stays open, and BPR-0646's
criterion can no longer be closed by preparing another bundle. The owner's
answers of 2026-09-12 are recorded below.

## Owner direction, 2026-09-12 (the answers to the v008 asks)

The four questions the v008 round left open, and the one the round raised,
were put to the owner directly and answered. They are recorded on their cards;
what they do to this RFC is here.

**An epoch is an update to the pull request, with an identity stable across
machines.** Verbatim: "An Epoch is an update to the PR, epochs identity should
be stable and repeatable if I look at the PR on different machines. A purely
observed epoch is not useful and terribly confusing." This refuses the option
of rewording AC-02's attended criterion to match a product that mints one
epoch per observation, so **AC-02 stands as written**. It is also larger than
importing history: `core::review_epoch_id` is `{Authority, Incarnation,
Ordinal}` and both variable parts are machine-local — a random per-catalogue
incarnation and a locally minted ordinal — so no producer behind the existing
model satisfies the answer. [[BPR-0743]] carries the work, now Ready and
specified, against RFC-BPR-0003. [[BPR-0646]] is not asked again on any bundle
until the epoch line carries the pull request's own updates.

**One pane: unified-diff order, and the choice lives in the View menu.**
Verbatim: "don't care, unifed diff order is fine" and "Should be in the view
menu like line numbers". The order [[BPR-0734]] drew stands. The strip toggle
does not: the choice is a checkable View item beside Line numbers, which in
this codebase means one scalar with no modality of its own, persisted per
window and seeded for a new window from the profile default — so checking it
gives Review one pane too. [[BPR-0745]] draws that in
[direction v7](../design/RFC-BPR-0028-workbench-direction-v7.html#read-one-pane),
rewords the Modalities amendment against the redrawn state, leaves AC-05's
proposed text unchanged, and offers the flip with the ask. The non-goal above
is worded against that state.

**The seam's hover highlight: not settled.** Verbatim: "I'm not sure what this
is, but leaning towards keep it." The first clause is the operative one — the
owner does not know which element was meant — so nothing is decided and the
highlight is left exactly as it is, which is what the leaning would have asked
for anyway. [[BPR-0739]] records how it is asked again: by showing the element
in its two states rather than naming it.

**RFC-BPR-0006's two palette questions are closed**, both answered: the
standard-library aliases' italic does not matter ("no"), and `CE8AD4` is
acceptable as the macro purple in place of the reference's `BD63C5` ("yes it's
acceptable"), so the 4.5:1 floor is not asked to yield. Recorded on that RFC
and on [[BPR-0738]].

## Attended session, 2026-09-12 (v009)

The session lives in
[rfc-bpr-0028-attended-v009](../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md),
prepared and protected at sequence 357 on the tests-disabled signed bundle built
from source `aff9fc64` (executable sha256 `c2381f32…`, `codesign --verify --deep
--strict` clean). Four candidates: `BPR-0744-directive-ranges` (carrying the one
criterion RFC-BPR-0006's [[BPR-0720]] and [[BPR-0738]] both hold, as one ask
rather than two), `BPR-0734-read-one-pane` on
[direction v7](../design/RFC-BPR-0028-workbench-direction-v7.html#read-one-pane),
`BPR-0739-seam-hover`, and `BPR-0746-cursor-probe`, which is a logging run
rather than a judgement.

The owner decided all four and handed back at `2026-09-12T16:13:55Z`, sequence
362, nothing pending. Every decision is `actor: human` and carries a fingerprint
equal to its prepared candidate's in
[session-identity.json](../verification/reviews/rfc-bpr-0028-attended-v009/session-identity.json),
so none is stale and each is bound to exactly what was shown.

- `BPR-0734-read-one-pane`, revision `design-v7-2026-09-12`, decision
  `31462e78`, 16:03:03Z: **accept**, note empty. **The one-pane state is
  accepted**, and with it the reworded Modalities amendment and the AC-05
  amendment, which are landed in this document's Modalities paragraph and AC-05
  above. An empty-note accept answers no question, so it accepts the
  candidate's four criteria as written, and the two consequences those criteria
  offered for refusal stand unrefused: **Review shows one pane too while One
  pane is checked**, and the one-pane gutter carries **no whole-file overview**.
  Both were accepted in words and neither is drawn on v7, which has no Review
  mock at all and draws no change with an empty side; that limit is recorded on
  [[BPR-0729]] as work rather than as a settled picture.
- `BPR-0739-seam-hover`, decision `9298ad8d`, 16:05:46Z: **select**, "Drop it:
  draw the rule at one constant weight, as the asset does", note "We don't need
  it anymore now that the cursor is fixed, it wasn't drawn in the right
  location anyways." The central splitter handle now paints its rule in `Mid`
  whatever the pointer does. The question [[BPR-0739]] left open is closed;
  **AC-03's seam criterion is not**, and the note's claim about the cursor is
  reconciled on [[BPR-0746]] rather than read as a verdict on a criterion the
  packet told him was not being asked.
- `BPR-0744-directive-ranges`, decision `4f1d1354`, 16:06:15Z: **accept**, note
  empty, against the bundle he had open from 16:03:30Z. The criterion
  [[BPR-0720]] and [[BPR-0738]] both carried since v005 is satisfied. The
  costed question that rode the candidate — whether every identifier inside
  `#if`, `#ifdef`, `#ifndef`, `#elif`, `#undef` and `defined()` should take the
  macro purple rather than only ALL_CAPS ones — is **not** answered by an
  accept of the implemented behaviour and is carried to [[BPR-0748]].
- `BPR-0746-cursor-probe`, decision `3e06fb9d`, 16:13:53Z: **accept**, note "The
  pointer stuff works correctly. Dunno about the test you had me run." The
  candidate's criterion required `build/logs/bpr-0746-cursor-probe.log` to
  exist. **It does not**, and the accept therefore does not close the card; see
  [[BPR-0746]] for why the log was lost and how the note is reconciled.

[[BPR-0646]] is not in it. AC-02's attended criterion stands as written and
returns when [[BPR-0743]] makes the epoch line carry the pull request's own
updates; re-asking it on a bundle buildable today would fail for the same reason
it failed in v006, v007 and v008. AC-03's seam criterion on [[BPR-0739]] is
likewise not re-asked: no product change touched cursor delivery this round.

## The epoch work has its own RFC, 2026-09-12

The owner chose an RFC of its own for the epoch work, verbatim: "It's own rfc".
[[BPR-0743]] now belongs to
[RFC-BPR-0029](RFC-BPR-0029-machine-independent-epoch-identity-update-history.md),
not to RFC-BPR-0003 as the direction above says. RFC-BPR-0029 is Proposed and
not accepted, and nothing in it is built until the owner answers its three asks.

AC-02's attended handle criterion on [[BPR-0646]] still stands as written. It
waits on BPR-0743, which waits on those answers, so this RFC cannot close before
that work lands. BPR-0646 now records the dependency and sits in Backlog, not
parked on human-review, because nothing about it is waiting on the owner.

## Later accepted epoch plan and one-pane packet, 2026-09-12 (v010)

The preceding paragraph records the state when RFC-BPR-0029 was Proposed.
The owner subsequently accepted its revised form at `5d5df7cd`; [[BPR-0743]]
is Ready, and [[BPR-0750]] holds its separate closeout. BPR-0646's epoch
handle criterion still waits for product work, so this RFC remains open.

[[BPR-0729]]'s one-pane implementation is committed at `3dcbf237` and its
single signed-bundle verdict is prepared in
[attended v010](../verification/reviews/rfc-bpr-0028-attended-v010/operator-packet.md).
The tests-disabled bundle executable sha256 is
`a6dc1565908cfacc5eb7d573d8d05983c51aa10780db2052e1c138682870eaf2`;
candidate `BPR-0729-one-pane-layout` is ready and protected at sequence 366,
with no human verdict. Review in one pane and the empty-side ribbons are
shown in the packet's same-source renders; acceptance is not inferred from
those renders.

## Owner decisions, 2026-09-24 (attended session cbd48b91)

Session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
handed back at sequence 589. Each decision is `actor: human` and carries a
fingerprint equal to its prepared candidate's in the packet's
[session-identity.json](../verification/reviews/owner-decisions-20260924-v001/session-identity.json).
Three of them bear on this RFC.

- `BPR-0653-entry-row-v001` (revision `38cc26d7`, ask restated at `919e2154`),
  decision `007a6eb5`, 18:54:09Z: **select** "Amend RFC-BPR-0028 AC-01 and the
  chrome paragraph to admit the entry rows between the strip and the epoch
  line". Note, verbatim: "We should do a UX design iteration with the
  /frontend-design skill to figure out where best to put these. For now you can
  ammend the card but open a follow-up for the ux design." AC-01 and the chrome
  paragraph above are amended accordingly, and [[BPR-1106]] is the follow-up.
  This supersedes, for those rows only, the reading of [[BPR-0737]]'s direction
  that the epoch line is the first row under the strip.
- `BPR-0653-private-qt-api-v001` (revision `53d229d7`, Qt 6.11.2 recheck at
  `4380f310`), decision `8dde5e4d`, 18:54:24Z: **accept**, note empty.
  `QNativeInterface::Private::QWindowsWindow::setCustomMargins` may ship in the
  Windows target.
- `BPR-0746-disposition-v001` (board at `86cd8227`), decision `14327a18`,
  18:54:49Z: **select** "Amend: withdraw BPR-0746 AC1 and AC2 as unobtainable
  (recorded as an amendment, not a pass) so it can be Done and RFC-0028 can
  close", note empty. [[BPR-0746]] is Done on that amendment.

`BPR-0653-windows-capture-v001` was not asked: it waits on a tests-disabled
Windows build at or after `e08a0409`. The owner also answered
`RFC-0039-0051-start-v001` with "Not yet: finish RFC-0028 to RFC-0038 first",
recorded in the [RFC index](README.md).

## Owner direction, 2026-10-06: Read merges into Review

Placed here, ahead of the 2026-09-24 scaffolding decision, only so it does not
collide with [[BPR-1290]]'s in-flight amendment at this document's end.

The owner, in chat on 2026-10-06, verbatim: "Okay what if we merge read and
review modalities into just review, and we have a hotkey/icon to quickly hide
and restore open drawers. That should basically give read but be a little less
awkward I think." Asked three questions on [[BPR-1295]], the owner answered,
verbatim: "Every drawer (Recommended)"; "Yes, remember (Recommended)"; and,
about the proposed ⌘\ and a title-row button beside the modality, "Those
hotkeys and that button location don't sound very good. I want something for
this toggle I can hit with my left hand easily. A quick google shows maybe
cmd+b is a reasonable option on mac for hiding clutter."

The Modalities, Overview, Read, Drawers and Settings paragraphs and AC-03
above carry dated amendments that cite this; their original wording is kept
beside each. **AC-05 and AC-08 said Read; from 2026-10-06 that means Review.**
[[BPR-1290]]'s amendment to the same criteria landed first, and
[[BPR-1295]] then worded both with Review in place.
Elsewhere in this document, too, where Read is named, read Review. The amendment
changes the modalities and the drawer commands, not this RFC's closeout
dependencies. [[BPR-1295]] builds it after [[BPR-1290]] lands, and the owner
confirms the icon's place and the ⌘1-⌘3 keys at the built UI.

## Owner decision, 2026-10-06: Conversations beside Discuss

Placed here for the same reason as the section above: [[BPR-1290]]'s in-flight
amendment edits this document's Read and Discuss paragraphs, AC-05, AC-08 and
its end.

[[BPR-1293]] prototyped a finder for every conversation in the pull request
([prototype v2](../design/BPR-1290-inline-drawer-v2/index.html)), with two
placements to try: a tab in the main pane and a view summoned like Home. The
owner said in chat, verbatim: "I like that a lot." Asked "Which finder
placement should we build? Like Home / Tab / Both", the owner answered,
verbatim: "What if we were to replace discuss with conversations."

The first reading was that **Conversations replaces Discuss**. The owner's
answers to BPR-1293's two questions the same day, in chat, verbatim, amend
it. On the drawers while Conversations shows: "Hide drawers". On the gap for
pull request wide comments if Discuss is removed: "just keep discuss for now
even after 1294".

So **Conversations is added and Discuss is kept**, even after [[BPR-1294]].
The modalities are Overview ⌘1, Review ⌘2, Conversations ⌘3 and Discuss ⌘4
(Read merged into Review above; the owner swapped the last two the same day,
below). Conversations is the
finder in the main pane, as prototype v2 draws it: one row per conversation
on a file, grouped by file, branching only where a post has two or more
replies that have replies, with search and the Only new, Hide resolved and
label filters. It is for navigation only. Picking a conversation enters Review
on its file with the thread open in the inline comments drawer at its line and
the picked post shown, as the prototype's Tab placement does. ⌘J goes straight
to Conversations. The drawers are hidden while it shows, without changing
their states, and ⌘B is disabled there. Pull request comments and review
summaries that are not on a file are not in it; Discuss still reads them, and
[[BPR-1294]] looks for a better home.

Discuss keeps its behaviour. Amended 2026-10-06 by the owner, verbatim:
"can we swap discuss to 3 and conversations to 4". Discuss is ⌘3 and
Conversations ⌘4, so Discuss's key does not move at all. A
`#discussion_r` link, the reveal after an inline comment is published, and
defect navigation to a thread the drawer does not hold open the thread in
place in Review rather than in Discuss; a `#pullrequestreview-` link and an
older-pair tab's Show them still open Discuss.

The Modalities paragraph and AC-03 above carry dated amendments that cite
this. The Discuss paragraph and AC-05's Discuss clause stand as written. AC-08
reads its "Read" as Review (above) and is otherwise unchanged; a long
conversation is also found in Conversations with its branches as rows.

BPR-1293 builds it after [[BPR-1290]] and [[BPR-1295]], and the owner accepts
it at the built UI. The amendment adds a modality, not a closeout dependency
of this RFC; [[BPR-1021]]'s rows follow the criteria's wording.

Built 2026-10-06 ([[BPR-1293]]). The strip reads in its keys' order, Overview
⌘1, Review ⌘2, Discuss ⌘3 and Conversations ⌘4, and ⌘J and the Comments
drawer's finder button go to Conversations too. The core has a fourth
modality that changes no drawer on the way in or out and refuses ⌘B there
with its own reason. A conversation's label is its root's, read from
[[BPR-1087]]'s counts, so the finder's label filter agrees with the
Overview's chips. The drawer toggle is named the Comments drawer. The kept
`github_thread_map` was not reused, since the finder draws branches as rows,
and is removed. A long conversation (AC-08) is found in Conversations with its
branches as rows and read whole in Review's drawer, the picked post washed
amber. The workbench specification's Modalities paragraph says the rest.

## Owner decision, 2026-09-24: the scaffolding moves to RFC-BPR-0052

[[BPR-1106]]'s design round ran in the separate session that decision
`ece7a6f3` asked for. The owner widened the question there from where the
composition entries sit to the workbench's outer scaffolding. The decision,
verbatim on BPR-1106, has five parts:

- the modality at the title row's right;
- the Updates line kept full width, without its permanent caption, and
  folding, its timeline opening just below a review bar (the owner's second
  same-day revision; round v6 had it above);
- the review bar reading, from its left, Updates and the comparison, then
  the active tab's controls, with Compose review and Comment as icons with
  tooltips on its right (the owner's same-day revision);
- private progress as an icon;
- RFC-BPR-0025's document tabs brought back, each with its own comparison.

Asked where that work should live, the owner chose "Its own RFC". It is
[RFC-BPR-0052](RFC-BPR-0052-review-workbench-scaffolding.md), which follows
RFC-BPR-0038 in the index and supersedes this RFC's chrome where they
conflict. It records the owner's answers on the default fold (folded) and the
keys (as proposed).

This RFC's criteria are unchanged. AC-01 keeps the owner's 2026-09-24 wording,
and this RFC closes on what it built.

## Owner decision, 2026-10-06: the drawer is inline only

[[BPR-1290]] redesigned the Conversations drawer. After three design rounds
and a round of whole-window concepts, the owner asked for the drawer to show
only this file's line-anchored threads, each opening in place, with finding a
conversation across the pull request moved to [[BPR-1293]] and pull request
comments that are not on a file to [[BPR-1294]]. Shown the clickable
[inline drawer prototype](../design/BPR-1290-inline-drawer-v1/index.html), the
owner said in chat, verbatim: "I like it".

The Read and Discuss paragraphs, AC-05 and AC-08 above are amended to that
choice and cite it; their original wording is kept beside each amendment. The
amendment changes where the map and the feed live, not this RFC's closeout
dependencies.
