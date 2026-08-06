---
id: BPR-0100
title: Run the inherited native visual-acceptance pass
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0097, BPR-0186, BPR-0187]
---

# Run the inherited native visual-acceptance pass

The RFC-BPR-0003 and RFC-BPR-0004 ledgers both defer the integrated-workbench presentation-polish and native visual-acceptance pass to RFC-BPR-0005. This card owns it.

On the real macOS composition with native Cocoa and QMacStyle, inspect the assembled workbench for presentation quality at supported window sizes and scales, with readable contrast and correct native control rendering.

DEFERRED CLASS: owner-attended. It needs a person at the keyboard to authorise and observe a live appearance context and to judge visual acceptance, which automation cannot assert. Batch it with the other attended checks. It does not block any other RFC-BPR-0005 card.

## Two dependencies were added after this card was written

Attendance is necessary and was not sufficient. BPR-0186 records that no launch
of this application can be looked at by a person against fixture data — a
fixture launch schedules an automated run and exits, and a launch with no
fixture takes the real credentialed path. There was no window to inspect.

BPR-0187 records the second half. The existing fixture is hand-written payloads,
which is sound for driving a deterministic run and weak ground for judging
presentation, because the content was written to satisfy the code that reads
it. The product owner designated `microsoft/DirectXMath#336` for this, and it
reads unauthenticated.

Judge the assembled workbench against the recorded review, not against
synthetic content.
