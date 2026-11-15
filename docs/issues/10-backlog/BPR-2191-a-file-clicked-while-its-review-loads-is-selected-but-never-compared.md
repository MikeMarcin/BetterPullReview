---
schema: "implement-rfc.card.v1"
id: "BPR-2191"
title: "A file clicked while its review loads is sometimes selected but never compared"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"application_pending_states_public_path flake reported after BPR-2180 closed, 2026-11-15","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-2180","BPR-1871","BPR-1842"],"reservation":"docs/issues/_reservations/BPR-2191--BPR-2191--b3c80c64710a.json"}
---

# BPR-2191: A file clicked while its review loads is sometimes selected but never compared

`application_pending_states_public_path` fails intermittently at "the clicked
file arrives", in the step [[BPR-2180]] added: under a read delay, Review's
Files drawer lists the reported files, the reviewer clicks one, the comparison
waits for it in the shape of code, and the file should open once the review
has loaded.

```text
comparison after 90 s: waiting 'review.cpp' held 'review.cpp' context 0 result 0 look 3
FAILED: the clicked file arrives
```

The comparison still holds the clicked file's wait 90 s later, and no
comparison of it was ever asked for (no context, no result).

## Measured

Each run is the course alone through the dev preset.

| Where | Passed | Failed |
|---|---|---|
| clean scratch worktree, 2af8e7665 (BPR-2180's last code commit) | 1 | 1 |
| clean scratch worktree, 9b0451225 (lane tip when reported) | 3 | 0 |
| lane tree, course with the result-based arrival check only | 0 | 1 |
| lane tree, plus an ask gated on an active workbench | 0 | 1 |
| lane tree, plus the ask deferred to its own event-loop turn | 0 | 1 |
| lane tree, plus the ask repeated until the cursor is on the file | 6 | 2 |

About 10 passed and 6 failed in all, roughly 60%, with the machine under
heavy shared build load throughout. None of the window-side changes clearly
helped, so none was committed. Failures cluster when other builds run.

There is no breaking commit to bisect to: the step first appeared in
1887d3281 and 2af8e7665 (BPR-2180 v003 and v004), and it fails at 2af8e7665
itself as well as on later trees.

## What the log shows

Temporary logging in the window, in a failing run (the third column is the
window's navigation cursor, the fourth whether it is on a file):

```text
remembered 'review.cpp'
publish refusal=-1 selection=1 commands=1 files=1 delta=0
ask? open='review.cpp' provisional=0 cursor=1 cursorfile=0 asks=0
found=1 same=0
publish refusal=-1 selection=1 commands=1 files=0 delta=0
ask? open='review.cpp' provisional=0 cursor=1 cursorfile=1 asks=1
found=1 same=1
```

The window's ask lands: the review's own rows replace the reported ones, the
window asks the workbench to select the file, and the workbench's cursor moves
onto it. The comparison is then never asked for it: the composition never
calls `diff_workbench::set_loading` for that selection (which would also
release the wait the window holds).

## Best hypothesis

The stall is in the application composition between the workbench's
selection and the selected diff starting
(`src/app/src/github_application/window_workbench.cpp`, the selected-diff
begin around [[BPR-1871]]'s `set_waiting` and `set_loading`, and
`present_opened_file`). A selection made while the review's comparison is
still being prepared (`epoch_status` loading, preparing or resolving) appears
not to be started again once preparation finishes. Under load preparation
takes longer, which matches failures clustering when other builds run. The
Overview's own waiting file goes through the same selection later (after the
review is accepted and its rows arrive) and has not been seen to fail.

## Acceptance

- AC01: The cause is shown with a deterministic course: a file selected
  while the comparison is still prepared is compared once preparation ends.
- AC02: `application_pending_states_public_path` and
  `application_progressive_open_public_path` pass repeatedly under load
  (`ctest --repeat until-fail:10`).
