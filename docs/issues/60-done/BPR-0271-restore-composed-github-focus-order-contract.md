---
id: BPR-0271
title: Restore the composed GitHub focus-order contract
status: Done
rfc: RFC-BPR-0010
depends_on: [BPR-0239]
---

# Restore the composed GitHub focus-order contract

After BPR-0270 restores the first masked assertion, all three composed GitHub
scale courses reach a later stale tab-order expectation. The test assumes the
technical-details disclosure immediately follows Refresh. The shipped
workbench's declared order now intentionally inserts the review queue and its
filter, sort, and enrollment controls between Refresh and pull-request
technical details, as accepted by BPR-0231 and BPR-0239.

## Required final state

- The composed course proves every available shell-owned stop in the declared
  workbench order appears in the live Qt tab chain in that same order, from
  Refresh into the review-queue surface and onward to the technical-details
  disclosure.
- Hidden, disabled, or `Qt::NoFocus` controls are skipped exactly as a keyboard
  user experiences them; no stale pre-queue adjacency is restored.
- Profile, Connect, connection-details, queue, filter, sorting, rule, technical
  disclosure, and remaining workbench stops retain their accepted ordering,
  names, and descriptions.
- The 100, 125, and 200 percent composed courses pass without product focus,
  widget, core queue-policy, or projection changes.

## Non-goals

- Do not remove or reorder the BPR-0231/BPR-0239 queue controls merely to make
  an older assertion pass.
- Do not weaken focus evidence to presence-only or bypass the actual Qt focus
  chain with a hand-written expected list. Do not claim this shell declaration
  enumerates internal stops owned by its composed child surfaces.
- Do not execute a live target, use a credential, or claim Windows/Linux
  runtime evidence.

## Verification

- Rebuild the composed GitHub and workbench-window Qt targets and run their
  100, 125, and 200 percent selectors.
- Run queue presentation, accessibility, filter/sort, and conversation-panel
  component tests.
- Run repository format/tidy, documentation, diff, and the final macOS `dev`
  continuation.
- Review the repaired focus sequence and hostile skipped/removed queue-stop
  counterexamples independently.

## Closeout

The composed course now enables the real ready queue presentation, filters the
declared workbench order to available keyboard stops, and walks the live Qt
focus chain to require those shell-owned stops in the same order. A separate
exact subsequence retains Profile, Connect, connection details, Refresh, queue,
all filter/sort/rule inputs, and technical details. Internal stops owned by
composed child surfaces remain outside this shell declaration.

The composed GitHub, workbench-window, conversation, and queue matrix passed at
100, 125, and 200 percent. Scoped format/tidy and diff gates passed. Independent
review returned `ACCEPT`; an LLDB hostile tab-order mutation between declared
queue controls failed the repaired assertion, and removing an enumerated queue
control fails the exact subsequence.
