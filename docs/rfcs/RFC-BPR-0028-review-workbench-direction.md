---
id: "RFC-BPR-0028"
title: "Review Workbench Direction: Overview, Epoch Line, Modalities, and Conversation"
status: "Proposed"
area: "BPR"
milestone: "Private alpha"
depends_on: ["RFC-BPR-0004","RFC-BPR-0005","RFC-BPR-0007","RFC-BPR-0008"]
pack_version: "post-v2"
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
  with no scroll area, at every window size the workbench supports.
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
- AC-06 Home shows the grouped action list from the queue and ledger
  projections.
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
