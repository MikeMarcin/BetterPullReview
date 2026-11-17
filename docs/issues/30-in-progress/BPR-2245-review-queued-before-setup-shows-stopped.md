---
schema: "implement-rfc.card.v1"
id: "BPR-2245"
title: "A review asked before the agent's setup finished answers, but shows as stopped and leaves Stop all up"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner report, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2245--BPR-2245--6c262208afb6.json","related":["BPR-2243","BPR-2181","BPR-1876"],"acceptance":["BPR-2245:answered","BPR-2245:once","BPR-2245:stop-all","BPR-2245:cancelled"]}
---

# BPR-2245: A review asked before the agent's setup finished answers, but shows as stopped and leaves Stop all up

## Report

The owner, 2026-11-17, on the 0.5.0 prerelease (main at about 0e01b3e9a):
"Interesting when the agent isn't fully set up, it was on the check step,
and I had clicked review this pull request, it seems like after I clicked
check it did set up the workspace and the agent and it looks like it did
actually run the review and gave me a report back that made sense but it
shows up as red and says canceled even though I'm pretty sure the whole
thing completed successfully."

And: "Also stop all is showing up in the status bar after that even though
no agent stuff is running."

## Scope

- Review this pull request pressed while the agent setup waits on its
  Check step is queued; once Check passes the review runs once and its turn
  says it answered, in the ordinary (not error) look.
- Once that run retires and nothing waits, the agent footer's Stop all is
  hidden.
- A turn genuinely stopped is still shown stopped; its ending never paints
  a later turn that answered.

## Acceptance

- `BPR-2245:answered`: through the window, a review queued before setup
  and released by Check ends answered, not stopped, and not drawn as an
  error.
- `BPR-2245:once`: the agent runs the review once.
- `BPR-2245:stop-all`: afterwards no queued entry remains and the footer's
  Stop all is hidden.
- `BPR-2245:cancelled`: a stopped attempt still reads stopped.

## Evidence

Recorded at the end of the slice.
