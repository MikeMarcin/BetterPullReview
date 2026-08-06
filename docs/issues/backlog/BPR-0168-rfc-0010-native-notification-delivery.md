---
id: BPR-0168
title: Confirm a native notification is delivered and its link opens the review
status: Backlog
rfc: RFC-BPR-0010
depends_on: [BPR-0164, BPR-0193]
---

# Confirm a native notification is delivered and its link opens the review

BPR-0164 proves what BetterPullReview asks the operating system to show and
where the link points. It cannot prove the operating system showed it.

## This card was misfiled, and the correction matters

It was parked as owner-attended on the assumption that the behaviour existed
and only a person was missing. That was wrong, and it was found the same way
BPR-0180 and BPR-0188 found theirs — by asking the linker.

| Measurement | Result |
|---|---|
| notification symbols in the shipped binary | `0` |
| references to `QSystemTrayIcon`, `UNUserNotification`, `NSUserNotification`, or `UNMutableNotificationContent` in `src/` | `0` |
| files naming `review_queue_notification` outside its own translation unit and tests | none |

**Nothing in this product asks the operating system to show anything.**
`application::review_queue_notification` decides *whether* a queue item
warrants a notification and *what* such a request would carry. No adapter
consumes it. The decision is complete, correct, well tested — and connected to
nothing, so the request it produces is never made.

Sitting a person in front of this today would have them wait for a banner that
no code path can cause. Attendance was never what was missing.

BPR-0193 supplies the delivery adapter. This card follows it.

## What is missing, once BPR-0193 lands

A person at the keyboard, once:

1. Grant Notification Center permission to the application. macOS asks the
   first time and never again, and an unattended run has nobody to answer it.
2. Trigger a queue item that requests a notification.
3. See the banner and read it — it must name the review and carry no source
   line and no comment text.
4. Click it, and confirm the correct local review is selected.

## Required direct evidence

- The banner is described as seen, including what it says, not inferred from
  the request.
- The link is followed from the banner rather than invoked directly, because
  the failure being ruled out is the operating system's handling of it.
- If any content beyond identity and counts appears, the run stops and the card
  reopens against BPR-0164 rather than being noted as cosmetic.

## What must not be done instead

Granting permission from a script, or asserting the request and calling the
delivery proven. The request is already proven; this card exists for the part
that is not — and for most of this card's life, "the part that is not" included
the entire delivery path, not merely the operating system's handling of it.
