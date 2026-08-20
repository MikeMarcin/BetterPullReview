---
id: BPR-0168
title: Confirm a native notification is delivered and its link opens the review
status: Parked
rfc: RFC-BPR-0010
depends_on: [BPR-0164, BPR-0193, BPR-0223, BPR-0251]
parked_on: awaiting-owner-verdict
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

## Misfiled a second time, 2026-08-09 — and the symbol check is what hid it

This card was prepared for an attended Session 1 and pulled back before the
owner was asked. The same measurement that corrected it the first time
corrects it again, one level up.

The first correction asked the linker and found zero notification symbols.
That is now fixed and it passes:

| Measurement | Result |
|---|---|
| `nm` symbols matching the notification path | 120 |
| `verify_composed_hosts.py` | `COMPOSED_HOSTS_OK` |
| runtime marker | `NOTIFICATION_DELIVERY_COMPOSED_OK` |

**All three are true and all three are insufficient.** They prove the delivery
path is composed. They do not prove any path can ask it to deliver.

```
$ grep -rn "deliver(" src/app/ | grep -v notification_delivery.hpp
(nothing)
```

`review_queue_notifier::deliver()` is the outbound entry point and **nothing in
`src/app/` ever calls it**. The only call into the notifier is
`Notifications->activate(aTarget)` from the sink's activation sink, which is the
inbound direction — a banner being clicked, not a banner being shown.

The composition root says so itself, at `github_application.cpp:977-981`:

> *"what still does not exist is a composed review queue to produce requests,
> which RFC-BPR-0010's own composition owns. Every delivery in this build
> therefore comes from the fixture probe."*

And the activation callback, at `:988-992`:

> *"The notifier selects nothing; this is where a chosen review would be
> opened. RFC-BPR-0010's queue surface owns that, so the target is recorded and
> the separation is kept rather than short-circuited."*

`ActivatedNotification` is written there and read nowhere — the member appears
exactly twice in the file, its declaration and that assignment.

### What that means for this card's four steps

| Step | State |
|---|---|
| 1. Grant Notification Center permission | possible |
| 2. Trigger a queue item that requests a notification | **no code path can do this** |
| 3. See and read the banner | unreachable, because step 2 is |
| 4. Click it, correct local review selected | unreachable twice — no banner, and activation selects nothing by design |

This card's own warning applies again, verbatim: *"Sitting a person in front of
this today would have them wait for a banner that no code path can cause.
Attendance was never what was missing."*

Two thirds of the gap closed since that was written. BPR-0164 supplies the
decision, BPR-0193 supplies the adapter, and both are composed. What is missing
is the wiring at each end: something that asks for a delivery, and something
that consumes an activation.

That is [[BPR-0223]], and this card depends on it.

## Final attended-course repair

BPR-0223 completed the product request and activation wires, but the production
tray still has no icon and the deterministic public course deliberately uses a
fixture sink. [[BPR-0251]] owns the non-null production icon, the real
head-one-to-head-two attended launch course, and this card's digest-bound owner
packet. This card remains parked until that course is green and the owner sees
and clicks the actual platform banner.

### What must not be done instead

Driving `deliver()` from a test seam to make a banner appear for the owner. It
would produce a real banner from a path no user can reach, and step 4 would
still have nothing to select. The banner is not the claim; the queue item
becoming a banner becoming a selected review is.

## Attempt one falsified the packet's premise — 2026-08-18

The owner ran the v002 course. It armed correctly and printed the exact
payload, then waited at `NATIVE_NOTIFICATION_AWAITING_PLATFORM_CLICK` for a
banner that structurally cannot exist: the tests-enabled executable is a
bare Mach-O with no bundle identity, macOS registers Notification Center
applications by bundle, and the owner confirmed System Settings →
Notifications carries no BetterPullReview entry at all. The attempt log is
preserved beside the packet as `notification-run-attempt-1.log`. This
card's refrain applies a third time: attendance was never what was missing.
[[BPR-0392]] owns the bundling repair and the preflight check that was
missing; this card stays parked until a regenerated packet binds bytes that
can actually produce the banner.

## Packet regenerated, 2026-08-18

The live procedure at the time was
`docs/verification/reviews/native-notification-delivery-v002/operator-packet.md`.
The v001 packet was never run and bound a dev executable that no longer exists
at its recorded digest; v002 rebinds the same course to the executable built at
commit `0e34519` and the unchanged BPR-0251 implementation record at its
current `60-done/` path. Its deterministic preflight,
`application_native_notification_attended_contract`, passes against those
exact bytes.

## The live packet is v005 — corrected 2026-08-19

The paragraph above stopped being true three regenerations ago and was still
being read as an instruction. The live procedure is
[`native-notification-delivery-v005`](../../verification/reviews/native-notification-delivery-v005/operator-packet.md).

v002 is not merely stale. The owner ran it, no banner appeared, and System
Settings carried no BetterPullReview entry — [[BPR-0392]] established that an
unbundled Mach-O has no Notification Center identity at all, so v002 asked for a
click no process could have produced. v003 moved the course onto a real
application bundle; v004 and v005 rebound it as later repairs rebuilt the
executable.

The cause was structural. This packet family serves both this card and
[[BPR-0392]], and each regeneration updated only the card that ordered it.
[[BPR-0398]] carries the verifier check that now fails when a card names a
superseded packet.

## The attended course ran and passed — 2026-08-20

Permission was granted by the owner at the macOS prompt, not by a script. The
banner was clicked on screen, not invoked directly. Exit 0, all eight required
markers present, log
`sha256:307ec1f34a307a8d24e07d6165015ee1d40c5e8c3b70e8059301c5c911fbe9d4`.

Against this card's four steps:

| step | evidence |
|---|---|
| 1. grant Notification Center permission | granted at the macOS prompt by the owner; the entry reads "BetterPullReview (tests)" under bundle identity `io.github.MikeMarcin.BetterPullReview.tests` |
| 2. trigger a queue item that requests a notification | `NATIVE_NOTIFICATION_QUIET_BASELINE_OK` then `REQUEST_SUBMITTED_OK` |
| 3. see the banner and read it | the request carried `octo-org/review-repository #17` and `The code moved after you reviewed it.` — **see the outstanding bullet below** |
| 4. click it, and confirm the correct review is selected | `ACTIVATION_OK` followed by `EXACT_LANDING_SELECTION_OK`, then `SHUTDOWN_OK` and `ATTENDED_RUN_OK` |

### One required bullet is still outstanding

This card's "Required direct evidence" is deliberately stricter than the log:

> The banner is described as seen, including what it says, not inferred from
> the request.

`NATIVE_NOTIFICATION_PAYLOAD` is the request, which is the thing this card
exists *not* to trust. The owner clicked a banner that macOS actually drew, so
delivery and activation are proven — but what that banner read is not yet on the
record in the owner's own words, and neither is the negative half:

> If any content beyond identity and counts appears, the run stops and the card
> reopens against BPR-0164 rather than being noted as cosmetic.

So the card stays parked on one short description rather than on a run. Nothing
else in it is outstanding.
