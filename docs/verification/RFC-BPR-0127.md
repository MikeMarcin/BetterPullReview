# RFC-BPR-0127 verification

Mandate `v0-5-0-prerelease-targets-r1`, plan `comments-absorb-notes-v1`
([[BPR-2157]], [[BPR-2158]], [[BPR-2159]], [[BPR-2160]], [[BPR-2161]],
closeout [[BPR-2162]]). Frontend-design was loaded before every Qt surface
round. Owner amendments in force: "If you need it you need it go build it.";
"main should be the source of truth for the tracker"; "Continue working
unattended, you can file reviews but dont block on their results". Debug
timings are nongating; nothing below is a timing claim.

Engineering evidence for each card is on the card:
[BPR-2157](../issues/30-in-progress/BPR-2157-comments-absorb-notes-model-store-migration.md),
[BPR-2158](../issues/10-backlog/BPR-2158-comments-drawer-holds-local-comments.md).

## Owner verdicts, first look (attended session 12534e154303dd1913382442a83b8c8f, handed back 2026-11-13T17:44:00Z)

Frozen build from `beede2fc33b5` (`--attended-local-comments-look` on
`conventional-comments-v1`). Verified in the session's `session.json`, each
decision by the human actor with an empty note:

| Candidate | Outcome | Saved | Note |
|---|---|---|---|
| `comment-states-on-a-line` | **Accept** | 17:41:50Z | none |
| `composer-state-and-computer-icon` (including the line-drawn computer glyph in place of `QStyle::SP_ComputerIcon`) | **Accept** | 17:42:15Z | none |
| `notes-tab-retired` | **Accept** | 17:43:58Z | none |

These count toward [[BPR-2158]]'s owner look (the milestone's "owner's look
on a frozen build with all three states on one line"). The line-drawn computer
glyph is accepted as the "OS-matched computer glyph" the RFC allows. Host
68507 (its `tailscale serve` child 68517) was stopped with the attended-review
`stop`; nothing ran from the root, and
`/Users/jaeger/comments-absorb-notes-v001.1a7vqce8` was removed with
`freeze_attended_build.py remove`.
