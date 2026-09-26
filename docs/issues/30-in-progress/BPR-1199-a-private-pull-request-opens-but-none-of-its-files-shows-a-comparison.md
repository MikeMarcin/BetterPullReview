---
schema: "implement-rfc.card.v1"
id: "BPR-1199"
title: "A private pull request opens, but none of its files shows a comparison"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"origin":"owner accept with note on attended candidate BPR-0641-private-pr-smoke-v001, session 06ad5734ed9f7c2fd6d0a62c8328fa0a, decision cbba6fac-e7dc-4b39-a7aa-1dbd0f7e1571, 2026-09-25","observed_revision":"3759b0d8caf5f1732c8bf807be427525f3f9a89a","observed_executable_sha256":"5b3984204f5a63ebf3bcc350d1deca1a1fb0aab6bb985343f95a3e738e0fe1ed","related_rfcs":["RFC-BPR-0037","RFC-BPR-0029","RFC-BPR-0015"],"related_cards":["BPR-0641","BPR-0922","BPR-0668","BPR-0642","BPR-0643"],"reservation":"docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json"}
---

# BPR-1199: A private pull request opens, but none of its files shows a comparison

## Observation

In [[BPR-0641]]'s read-only smoke (packet
[owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md)),
the owner signed in through GitHub CLI and opened the owner-named private
pull request `EpicGames/UnrealEngine#14366` (closed, 4 commits, 40 files) in a
tests-disabled release build of `3759b0d8`. The pull request loaded: Overview
showed its description, participants and all 40 files. No file showed a
comparison. The owner, verbatim: "However I wasn't able to view a file (this
may be already carded)." It was not carded.

What the owner's screenshots show, in the app's words (the screenshots show
private content and stay outside the repository):

- The readiness row: "File comparisons unavailable. Review history could not
  be updated. The exact comparison is unavailable. Refresh to try again."
- In Read with file 8 selected: both panes empty, reading "Select a file
  comparison"; the status line "Actionable file 8 of 40. Review history could
  not be updated. The exact comparison is unavailable."
- The Comments tray: nine review threads, each "Code anchor unavailable".

The readiness wording is `workbench_window.cpp`'s branch for a loaded pull
request whose epoch comparison is not available; the two middle sentences are
the epoch selectors' status text. So the pull request record and conversation
reads succeeded, and the update history / exact comparison did not.

## What is known and what is not

- [[BPR-0922]] fixed the same wording on a recorded GHES course through the
  CLI profile and recorded that the owner's live cause was never observed.
  [[BPR-0668]] made the failed comparison's workbench list the files and offer
  Refresh, which is what the owner saw.
- [[BPR-0642]] logs pull request load outcomes in diagnostics, so the owner's
  profile log may name the stage and reason code without a rerun.
- Not established: whether the refusal is private Git object acquisition under
  a CLI-connected profile, the repository's size and history (the retained
  stress test [[BPR-0643]] is the same repository), a history-catalogue limit,
  or something else. Do not assume one.

## Acceptance

- AC1 The failing boundary is established from the owner's diagnostics or an
  owner-attended read-only rerun: the stage, the reason code and the request
  identity, with observation kept separate from inference. No private
  repository content, path, title, author or body is committed.
- AC2 A supported private pull request opens a file comparison and renders
  base and head. Where a legitimate limit refuses, the surface names that
  limit and the recovery rather than "Review history could not be updated".
- AC3 A composed fixture course reproduces the failing class with synthetic or
  recorded public data, fails before the fix and passes after, and the
  affected epoch, workbench and public-path courses pass.
- AC4 The owner reopens #14366 read-only on the exact candidate and a file
  shows its diff: one attended question for this criterion alone.

## Boundaries

Read-only. No comment, review, label or publication on the private pull
request. The owner's diagnostics log is private: record reason codes and
stages only. This is a standalone follow-up; it changes no RFC's recorded
acceptance.
