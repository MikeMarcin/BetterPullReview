---
schema: "implement-rfc.card.v1"
id: "BPR-2217"
title: "Seen and Approve again move from Home into the review"
status: "In Progress"
rfcs: ["RFC-BPR-0098", "RFC-BPR-0116"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2217--BPR-2217--3558698e5e90.json","origin":"owner, 2026-11-16, about BPR-2049","supersedes_surface_of":"BPR-2049"}
---

# Seen and Approve again move from Home into the review

## Origin

The owner, 2026-11-16, about [[BPR-2049]] (Seen / Approve again for an
approval with new commits, until now only a hover action on Home rows):
"Oh well its dumb to put that on home, I'm going to look at the new commits
from within the review."

## Scope and acceptance

- **AC-a.** In the review of a pull request I approved that has new commits
  since, where my approval still stands, the review offers Seen beside
  Approve. Seen marks the new commits seen without submitting anything to
  GitHub, and the review then reads as up to date.
- **AC-b.** Where GitHub dismissed my approval, the review's primary action
  reads Approve again.
- **AC-c.** The Home row's hover Seen / Approve again is removed. Home keeps
  its verdict chip.
- **AC-d.** The states and their wording are decided in the core; Qt only
  presents them.

Load `frontend-design:frontend-design` before the Qt work and say so in the
report. The owner's look on a frozen build (Fixture agent only, a fresh short
session, one Accept/Reject per visible criterion) is filed, not waited on.

## Intended verification

Core tests for the review's follow-up states and words. A composed course on
the fixture GitHub server: the review of an approved pull request with new
commits offers Seen beside Approve, Seen asks GitHub for nothing and the
review then reads up to date; a dismissed approval's review reads Approve
again; Home rows offer no Seen or Approve again.

## Checkpoint 1 (2026-11-16)

Frontend-design was loaded before the Qt work.

Lane commits: `200ada033` (core words, review editor, Home removal, courses),
`e3f7f16ee` (drops a BPR-2216 line that rode in with it), `dc2bda139` (the
public-path fixture's opened pull request carries its Home row's earlier
verdict, so the open agrees with the row).

- Core: `core::review_followup_words_for` (`review_queue_attention.hpp`)
  decides the standing (`new_commits_since_approval`, `approval_dismissed`,
  `up_to_date`) and every word the review shows; Qt
  (`review_submission_editor::set_followup`) presents them: a note above the
  verdicts, Seen beside Approve, Approve's name.
- Home: the hover Seen / Approve again action, its `followup` role and
  `home_list::set_followup_dispatch` are gone; the verdict chip stays. The
  Pull Request menu's Mark New Commits Seen stays.

Verification, on a private worktree of the lane's BPR-2217 commits without
the other lanes' uncommitted work (`c073674b4`, same tree for these paths),
`ctest --preset dev -R …`:

- `core_review_queue_attention` (test_review_followup_words) — pass (AC-a, AC-b, AC-d).
- `application_review_seen_public_path` — pass (AC-a): #17's review offers
  Seen beside Approve, Seen asks GitHub for nothing, no review submitted, the
  review then reads "Up to date…", Home has it in Following.
- `application_home_approve_again_public_path` — pass (AC-b): the dismissed
  approval's review reads Approve again and says GitHub dismissed it.
- `application_home_oversized_public_path` — pass (AC-c): #18/#19 keep their
  verdict chips in Needs me with no row action and no hover Seen / Approve again.
- `qt_review_landing_model`, `qt_home_list_scale_100`,
  `qt_review_submission_editor_scale_100` — pass.

Pending: the owner's look (filed, not waited on).

## Checkpoint 2 (2026-11-16): owner look filed, not waited on

Lane `764cfbee0`: the review lightbox fits its content again whenever the
review presents (a draft read after it opened, or the line about my approval,
had left the verdict row below its foot); the course renders natively.
Re-run on the private verification checkout (`2aee77350`, same tree for these
paths): `application_review_seen_public_path`,
`application_home_approve_again_public_path`,
`application_home_oversized_public_path`,
`qt_review_submission_editor_scale_100` — pass.

Frozen build `/Users/jaeger/bpr2217-review-seen-v002.c04pcimv` from
`2aee77350` (fixture `github-com`, `--attended-home-look
--attended-home-approvals`: #17 approved before new commits, #4004 dismissed;
Fixture agent only; launcher `--check` reached `ATTENDED_HOME_SHOWN:rows=5`).
Native Cocoa renders in `/Users/jaeger/bpr2217-look-renders`.

Attended session `90d237427a59a8fc8e95d016e4d3573f`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261116-bpr2217-review-seen-v002/state`,
served on the tailnet, cursor 9, three Accept/Reject candidates ready and
protected: `BPR-2217-review-seen` (AC-a, build), `BPR-2217-approve-again`
(AC-b, render: the frozen fixture opens only #17), `BPR-2217-home-row` (AC-c,
build). The URL stays in the session state.

Next: read `status --after 9`; Accept on all three closes the card.
