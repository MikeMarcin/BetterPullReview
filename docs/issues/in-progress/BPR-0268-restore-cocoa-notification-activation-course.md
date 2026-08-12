---
id: BPR-0268
title: Restore the Cocoa notification activation course
status: In Progress
rfc: RFC-BPR-0006
depends_on: [BPR-0251]
---

# Restore the Cocoa notification activation course

The real native-notification public path reaches its quiet-baseline, request,
payload, and display-boundary markers on macOS Cocoa, then waits for activation
until its budget expires. The same shipped application and fixture course pass
offscreen in under a second. The Cocoa failure is therefore distinct from the
runtime verifier's sandboxed `ps` failure and must be repaired without replacing
the native platform composition with the offscreen path.

## Required final state

- The test activation travels through the real `QSystemTrayIcon`
  `messageClicked` connection and notification coordinator before selecting
  the exact queue item; no direct notifier activation or window-selection
  bypass is introduced.
- Cocoa event delivery is bounded and deterministic for the positive,
  request-wire-cut, and activation-wire-cut courses.
- The positive course observes exactly one attributed activation and the exact
  focused landing state; both cut courses remain fail-closed at their intended
  boundaries.
- Timeout cleanup does not leave an application process or obscure the causal
  failure with a secondary process-inspection diagnostic.

## Non-goals

- Do not change platform notification permission policy or claim that an
  automated fixture is an attended human notification verdict.
- Do not switch the Apple contract to `QT_QPA_PLATFORM=offscreen`, widen the
  network sandbox, or use a credential/live target.
- Do not repair the independent runtime-surface child query.

## Verification

- Run the positive and two cut native-notification courses under Cocoa and the
  checked-in network-isolation wrapper, repeatedly.
- Run the notification delivery component/Qt tests and the application queue
  composition contract.
- Run architecture boundaries, repository format/tidy, documentation, and diff
  gates.
- Review the final platform-event and negative-control evidence independently.
