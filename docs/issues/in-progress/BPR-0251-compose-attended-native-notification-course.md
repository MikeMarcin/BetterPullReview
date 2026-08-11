---
id: BPR-0251
title: Compose the attended native notification course
status: In Progress
rfc: RFC-BPR-0010
depends_on: [BPR-0164, BPR-0193, BPR-0223]
---

# Compose the attended native notification course

BPR-0223 proves the real queue transition reaches the notification adapter and
that activation selects the exact local review, but its deterministic public
course uses a fixture sink. The production `QSystemTrayIcon` is shown without
an icon; the last attended fixture launch emitted
`QSystemTrayIcon::setVisible: No Icon set`. BPR-0168 must not ask the owner to
wait for a banner until the real native surface and the exact queue gesture can
be exercised together.

## Required final state

- Give the production tray surface a non-null BetterPullReview-owned or Qt
  standard fallback icon before it becomes visible. A real fixture launch must
  not emit the no-icon warning.
- Add one attended-only real-application course that starts from the quiet
  accepted head, enrolls through the shipped observer-rule gesture, drives the
  real Refresh transition to the later head, and submits through the production
  system notification sink. Do not call `deliver()` from a fixture seam.
- Keep the real workbench visible and alive for the owner to grant platform
  permission, inspect the banner, and click it. The banner must contain only
  review identity, fixed attention text, and bounded count metadata—no source
  path, source line, comment body, note, draft, or credential.
- Route the native message click through the production activation callback and
  prove the exact host/account/repository/pull-request row becomes selected and
  focused. A direct fixture selection helper cannot satisfy the course.
- Emit causal visible/log markers for quiet baseline, exact request submission,
  native activation, exact landing selection, refusal, and shutdown. Do not
  claim the operating system displayed a banner; BPR-0168 remains the human
  verdict.
- Add a deterministic registered contract that proves the course reaches the
  production sink/request wire without showing a real notification, plus
  independent request- and activation-wire cuts. Preserve BPR-0223's public
  queue contract.
- Write a digest-bound BPR-0168 operator packet naming the exact executable,
  command, expected banner content, click/selection check, and exact approval
  statement. Never automate Notification Center permission or the human click.

## Focused verification

```sh
ctest --preset dev --output-on-failure \
  -R '^(application_review_queue_notification|application_notification_delivery|qt_system_tray_notification_sink|application_queue_composition_contract|application_native_notification_attended_contract)$'
python3 scripts/verify_documentation.py --root .
git diff --check
```

## Non-goals

- Do not claim or query platform permission through
  `QSystemTrayIcon::supportsMessages()`; it reports capability, not an owner's
  Notification Center choice.
- Do not bypass the honest head-one to head-two queue transition with a direct
  notifier, sink, activation, or selection call.
- Do not send a live GitHub mutation, request provider credentials, or persist
  notification content.
- Do not mark BPR-0168 or RFC-BPR-0010 complete before the owner returns the
  packet's exact attended statement.
