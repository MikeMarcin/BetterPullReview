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
direction and stay registered but unscheduled. The drawers and the first three
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
account, settings. Below it the 22-pixel epoch line. No toolbar with labels,
no menu bar rows. Nothing in the strip may read as disabled.

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
changed. Read collapses the files drawer to an 18-pixel rail and keeps comments
beside the code; Review opens the files drawer too; Discuss widens the
conversation over the code. Nothing collapses on its own. Automatic transitions
(for example, opening a pull request shows Overview) are off by default and are
configured as rules in Settings. Rails carry a vertical label and an unread
count.

**Overview.** A pull request opens on an overview: title, author and branches,
description, a files table grouped by folder with operation, added and removed
line counts, changed-since-my-last-review, my read state, unread and total
comment badges and my note; participants with their sign-off state; a
"since my last review" summary (uploads, files changed, new comments, replies to
me, my unresolved threads, checks); my review progress; and one action,
Continue review (Start review on first visit), which enters Read at the first
unseen change.

**Read.** Side-by-side diff with the product theme and the DiffLinkMap between
the panes. Gutter marks: amber for a line with a thread, blue for a new one.
The comments drawer lists this file's threads aligned to their lines as cards
on an avatar rail with a thread line; a whole-PR filter lists every thread in
file order. Unread posts carry a blue dot and full-ink names; read posts step
back. My unpublished notes appear as dashed drafts in the same column with
Publish, Edit and Delete.

**Discuss.** The conversation widens across the code. Left: a thread list
(file and line roots, replies under a guide line, unread dots, counts) with
Whole PR, This file, Unread and Unresolved filters, and below it the accepted
thread map for the selected thread. Right: a feed with one card per thread,
its code excerpt on the theme, the root post and replies on a continuous rail,
each post headed by author, role, "replying to", unread dot and time, and
Reply, Resolve, Mark read and Open in code actions.

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
parents visible. Each drawer has its own strip toggle, shortcut and 18-pixel
rail with a label and count; collapsing one never moves the other; a modality
sets both, a manual toggle overrides until the next modality change.

**Settings.** RFC-BPR-0018's window and sidebar, with these pages fixed by the
asset: Accounts & hosts (one card per server, device-grant sessions shown as
remembered and self-renewing, enterprise tokens labelled session-only with a
"remember this server" switch that keeps hostname and account), Appearance
(theme as four previews from the real palette with a half-and-half System
thumbnail, interface size, code font defaulting to JetBrains Mono NL as
installed, code size and tab stops, ligatures, per-role colour customisation
with preview, theme import and export, the tuned diff colours untouched), and
Reviews & queue (modality rules, none on by default; reading behaviour;
observer rules; saved repositories). The other four pages remain as
RFC-BPR-0018 describes them.

**Home.** An action list: grouped rows (needs you, waiting on others,
following) with pull request, title, why the row is here, my progress, author,
role and updated time. Open by URL or owner/repo#number in the bar. Rules and
saved repositories live in Settings.

**Vocabulary.** First person and event names: "my last review", never "you".
Column headings are one word; actions are two.

## Non-goals

Browser-style file tabs; the lane-rail conversation drawer rejected earlier;
changes to the diff themes or the link map; automatic drawer collapsing without
a configured rule; the Settings pages, the explorers and the file tree, which
follow in their own design rounds.

## Acceptance criteria

- AC-01 The chrome above the comparison is the strip and the epoch line only,
  with no scroll area and no separate title bar, at every window size the
  workbench supports; the strip is the row the window controls sit in.
- AC-02 The epoch line selects any two uploads by drag, click, ⌘-click, menu and
  keyboard stepping, shows my last review, and drives the comparison the
  existing epoch host already resolves.
- AC-03 The four modalities set the drawers as described, persist per window,
  never change on their own unless a configured transition fires.
- AC-04 The overview shows every file with the listed columns from projections
  the host already publishes, plus participants and the since-my-last-review
  summary, and Continue review lands on the first unseen change.
- AC-05 Read shows this file's threads aligned to their lines with unread state
  and drafts; Discuss shows the list, map and feed with the listed filters.
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
  reads correctly in Read and Discuss (the owner's stated stress case).

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
