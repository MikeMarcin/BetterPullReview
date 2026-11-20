---
schema: "implement-rfc.card.v1"
id: "BPR-2316"
title: "The return reread asks GitHub whether anything changed before reading the pull request again"
status: "Backlog"
rfcs: []
depends_on: ["BPR-2315"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-2315--BPR-2316--9d803824eace.json","origin":"owner, 2026-11-20: \"if something did change after the cheap anything new then im fine with dimming\"","related_cards":["BPR-1867","BPR-1850","BPR-2315"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2316: The return reread asks GitHub whether anything changed before reading the pull request again

## Asked

The owner, 2026-11-20, of the reread a window makes when it comes back to a
pull request whose update is five minutes old ([[BPR-1867]]): "after 5
minutes usually nothing has changed so its just disruptive for no value for it
to dim like that", then: "if something did change after the cheap anything new
then im fine with dimming".

[[BPR-2315]] made that reread quiet: it no longer dims, rewords or disables
anything, and when it finds nothing nothing on screen changes. It is still a
whole read of the pull request (metadata, files, commits, reviews, threads,
comments, checks and update history, each paged), so it still spends the
account's GraphQL budget and the window's read lane every time the window is
activated more than five minutes after its last update. This card makes it
cheap.

## Design to settle first

- **The probe.** One small read whose answer changes whenever anything the
  review shows changes. The candidates:
  - REST `GET /repos/{owner}/{repo}/pulls/{number}` with `If-None-Match` (a
    304 does not count against the rate limit, as [[BPR-1850]]'s notification
    poll uses). Its ETag moves with the head, the title and body, the state,
    `updated_at` and the comment and review-comment counts. It does **not**
    move when a check run finishes, a thread is resolved or a reaction is
    added, which the Overview and Discuss show.
  - A one-query GraphQL stamp: `updatedAt`, `headRefOid`, the head commit's
    `statusCheckRollup { state }` and the review-thread and comment total
    counts. One point, no conditional request, covers checks.
  Either needs a baseline taken by the full read itself, so the first return
  after an open compares against what was shown, not against a previous
  probe.
- **The contract.** A new provider read goes through the GitHub read
  contract, the wire request validation and operation policy, and the fixture
  provider (`contracts/v1` packs), like every other read; signed-out public
  preview needs its own answer or none.
- **The rule.** Unchanged: nothing happens and the update's age is reset, so
  the next return is five minutes on. Changed or unknown (the probe failed):
  the full reread runs as today. A rate limit's wait still holds the probe.

## Acceptance

- AC1. A return to a pull request whose probe answers unchanged sends no full
  read and changes nothing on screen; its diagnostic line names the probe and
  its answer.
- AC2. A return whose probe answers changed (a push, a comment, a finished
  check, a resolved thread, whichever the chosen probe covers, and the gaps
  named) runs the full reread.
- AC3. A probe that fails or is refused falls back to the full reread.
- AC4. Fixture and contract courses for both answers; a live course on
  BPR-Test measures the probe's cost against the full read's.
