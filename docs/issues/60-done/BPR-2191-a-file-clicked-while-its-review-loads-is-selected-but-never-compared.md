---
schema: "implement-rfc.card.v1"
id: "BPR-2191"
title: "A file clicked while its review loads is sometimes selected but never compared"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"application_pending_states_public_path flake reported after BPR-2180 closed, 2026-11-15","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-2180","BPR-1871","BPR-1842"],"reservation":"docs/issues/_reservations/BPR-2191--BPR-2191--b3c80c64710a.json"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"1eab5b868","evidence":{"kind":"observation","detail":"Amended by the coordinator on 2026-11-15: the stall was not reproduced on the current lane, so no course fails before the change. The criterion is met by the design fix the coordinator chose. The workbench host now holds the pending open, tied to the review that reported the file, and decides when it opens. application_workbench_host covers it directly: the file opens once that review's own rows are accepted; a newer review, an unnamed review, the rows of a review shown before, a new selection, a read that ends with no review, and a review that does not list the file each cancel it. qt_pending_veil shows that Qt asks the host and presents only what the host publishes."}},{"criterion":"AC02","outcome":"pass","revision":"1eab5b868","evidence":{"kind":"command","detail":"ctest --repeat until-fail:10 at 1eab5b868: application_progressive_open_public_path 10/10. application_pending_states_public_path 10/10 in a second batch (load 75 falling to 2). The first batch, under load 80, failed once at run 9 with a different signature: after 12.8 s the opened file's wait showed failed (look 8, nothing held). That is the same as the epoch failing with repair_required, seen before the change when 6 copies ran at once. It is not the stall this card names (no wait still held at 90 s). Before the change, at f65bfa1e2: 10/10 and 10/10."}}]
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
  Amended by the coordinator on 2026-11-15, because the stall was not
  reproduced: the workbench host owns the pending open, tied to the review
  that reported the file, and decides when it opens. A host-level course
  covers opening on accept, cancelling on a newer review and cancelling on a
  new selection.
- AC02: `application_pending_states_public_path` and
  `application_progressive_open_public_path` pass repeatedly under load
  (`ctest --repeat until-fail:10`).

## Investigation, 2026-11-15 (not reproduced)

No cause was found, and no product change was made. The course passes
every run on the current lane.

| Where | Load | Passed | Failed |
|---|---|---|---|
| f65bfa1e2, `ctest --repeat until-fail:10` (`application_pending_states_public_path`) | normal | 10 | 0 |
| f65bfa1e2, `ctest --repeat until-fail:10` (`application_progressive_open_public_path`) | normal | 10 | 0 |
| instrumented lane tree, the course alone or 3 to 4 copies at once | shared builds, load 40 to 47 | about 45 | 0 |
| instrumented lane tree, 6 copies at once | very heavy | 10 | 2 |

The two failures with 6 copies running were different from this card's. The
epoch failed in `preparing` with `repair_required`, the opened file's wait
showed it as failed (look 8, nothing held), and the course stopped at the
code veil's gap after about 12 s. It never reached the 90 s stall with
`held 'review.cpp'`. This failure seems to come from copies of the course
competing for resources. It is not the selection race.

With logging on the selection path (workbench host `submit_selected_diff`,
`observe_now` and file publication; the composition's `begin`, `publish`,
`request_diff`, epoch updates, preparation rounds, commit-filter re-observe and
handoff acceptance), every run took the same path. The click lands while the
epoch is `loading_catalog` or `preparing`. The submit is refused
(`selected_file_comparison_unavailable`). The ready transition, which carries an
available comparison, calls `refresh_selected_diff`. Then `begin`, `publish` and
`request_diff` run, and the diff host's `Pending` is the new key. No `begin`
or `publish` was dropped, no request was deferred, and nothing re-observed the
workbench between the click and the comparison.

Orderings forced on purpose all passed. These were: the clone held 3 s and
8 s (`GitFetchMilliseconds`), so preparation ends long after the selection;
the workbench observation held 4 s, so the epoch is ready before the review's
rows; a fast read (300/200 ms); the click 600, 1,000 or 1,500 ms after Review
opens; and a slow read (3,000/3,000 ms).

The measurements above this section need a correction. The 2af8e7665 and
9b0451225 rows ran the arrival check from before 4bb44f28f
(`current_context() && !busy()`). Under load the code veil can stay up as the
"Colouring syntax…" plate after the result arrives, so that check fails on
its own. 4bb44f28f replaced it with the result-based check. The remaining
failures were all on lane trees carrying uncommitted window changes made for
the measurement, and uncommitted BPR-2181 work that has since been committed
or changed.

The design has one weakness, but it was not shown to cause the failure. Qt
(`workbench_window::apply_file_rows`) treats any non-empty file publication as
"the review's own rows" and opens the remembered file then
(`open_provisional_file`), so the pending open is Qt-local state. Moving it
into the workbench host, keyed to the review that reported it, would remove
that, but no course fails without the move.

AC01 is still open: no deterministic course fails on the current tree.
AC02 holds on f65bfa1e2.

## Closeout, 2026-11-15

The card closes with two records.

(a) Not reproduced. The investigation above holds. On the current lane
neither public path stalls. Most of the failures measured earlier ran the
arrival check that 4bb44f28f replaced, which fails by itself while the syntax
plate stays up; the rest came from lane trees carrying uncommitted changes.

(b) The design fix, 1eab5b868. Before it, Qt held the pending open
(`ProvisionalOpen`) and opened the path on any non-empty file publication,
whichever review the publication belonged to. That broke the rule that the
core and host decide while Qt only presents, whether or not it caused the
stall. Now:

- **Qt.** A click on a reported file, or a file double-clicked on the
  Overview while the review loads, is sent to the composition by path
  (`workbench_window::set_pending_open_dispatch`).
- **Composition.** It names the review the read asks for
  (`workbench_review`, from `github_state::PendingPullRequest`) and asks the
  workbench host to open the file once that review lists it
  (`workbench_application_host::open_when_listed`).
- **Host, opening.** The handoff names the same review when the review is
  accepted (`observe(authority, review)`). Owner and repository names are
  compared case-insensitively. When that review's own rows are observed,
  the host selects the file as the reviewer's selection would and asks for
  its comparison.
- **Host, cancelling.** Any of these cancels the pending open:
  - a newer review, or an observation that names no review;
  - another selection;
  - a failed observation;
  - a review that does not list the file;
  - a read that ends with no review (`cancel_pending_open`).
- **Publication.** The host publishes the pending open
  (`workbench_projection::PendingOpen`: the path, or none and whether it
  opened). Qt holds the comparison's code wait while the file is pending,
  keeps it as the opened file's wait, and takes it back on a cancel.

Courses at 1eab5b868:

- the new tests in `application_workbench_host` and the updated
  `qt_pending_veil`;
- `application_pending_states_public_path` and
  `application_progressive_open_public_path`, 10/10 each (see AC02 for the
  one failure under load 80, which had a different signature);
- `qt_composed_github_scale_100`, `qt_workbench_window_scale_100`,
  `application_workbench_composition_contract`,
  `application_setup_destination_public_path`,
  `application_file_open_while_loading_public_path` and
  `architecture_boundaries`;
- clang-format 22, and clang-tidy on the changed translation units.

The Overview's own pending open (BPR-1842, `PendingReviewPath`) is still
held in Qt. It is gated on the accepted review's ready status and is outside
this card.

## Correction, 2026-11-15: a click after acceptance was dropped (a9e612a3a)

1eab5b868 broke `application_agent_chat_custom_agent_contract`,
`application_agent_chat_custom_agent_refused_contract` and
`application_agent_chat_setup_list_contract`. Each failed at `selected-diff
comparison-shown=false`.

The composition asked the host to open a reported file only while the read
was still pending. After the review is accepted, its reported files can stay
the drawer's rows until the review's own arrive. A click in that interval was
dropped, so nothing opened. That click is now tied to the accepted review, and
the host opens it when that review's rows are observed, or at once if they
already are.

Bisected in a scratch worktree, building the contracts' executable at each
commit:

| Commit | Result |
|---|---|
| 32c0850e0 (parent of 1eab5b868) | all three pass |
| 1eab5b868 | all three passed in the one run made |
| 12aa86eb6 (same code as 1eab5b868; the commits between change only docs) | all three fail |
| 2d9705edb | all three fail |
| 12aa86eb6 with a9e612a3a's change applied | 5 of 5 each |

Whether the contracts fail depends on whether the course's click lands
before or after the review is accepted.

In the lane at a9e612a3a: the three contracts pass 5 of 5 each, and
`application_workbench_host`, `application_pending_states_public_path` and
`application_progressive_open_public_path` pass.
