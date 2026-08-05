---
id: BPR-0168
title: Confirm a native notification is delivered and its link opens the review
status: Backlog
rfc: RFC-BPR-0010
depends_on: [BPR-0164]
---

# Confirm a native notification is delivered and its link opens the review

BPR-0164 proves what BetterPullReview asks the operating system to show and
where the link points. It cannot prove the operating system showed it.

## What is missing

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
that is not.
