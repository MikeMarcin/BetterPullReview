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

The owner locked this direction on 2026-09-06 after five mockup rounds; the
accepted mockup is [docs/design/RFC-BPR-0028-workbench-direction-v5.html](../design/RFC-BPR-0028-workbench-direction-v5.html).
It is the owner-directed replacement for the workbench presentation and is
the frontier exception the deferred UX RFCs name. It supersedes the layout and
chrome sections of RFC-BPR-0025 and the Home presentation of RFC-BPR-0026 where
they conflict; RFC-BPR-0025's browser-style file tabs are not part of this
direction and stay registered but unscheduled. RFC-BPR-0018 (Settings), the
repository/pull-request explorer and the file tree inside a pull request are
the next design items and are not fixed by this RFC.

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

**Chrome.** One 28-pixel strip: files-rail toggle; repository and pull request
identity; previous/next change with its position; previous/next file with its
position; jump to the next new comment with the count; the modality control;
refresh; comments-rail toggle; account; settings. Below it a 22-pixel epoch
line. No toolbar with labels, no menu bar rows.

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
- AC-08 A recorded conversation with more than twenty posts and three branches
  reads correctly in Read and Discuss (the owner's stated stress case).

## Implementation entry

[[BPR-0646]] chrome, epoch line, modalities and rails; [[BPR-0647]] overview;
[[BPR-0648]] aligned comment cards; [[BPR-0649]] Discuss; [[BPR-0650]] Home
action list. Settings, explorers and the file tree get cards after their design
rounds. Closeout follows the ordinary structure gate.
