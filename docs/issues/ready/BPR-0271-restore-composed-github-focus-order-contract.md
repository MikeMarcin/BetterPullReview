---
id: BPR-0271
title: Restore the composed GitHub focus-order contract
status: Ready
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

- The composed course proves the live Qt tab chain follows the declared
  workbench order from Refresh into the review-queue surface and onward to the
  technical-details disclosure.
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
  chain with a hand-written expected list.
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
