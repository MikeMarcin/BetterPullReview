---
id: BPR-0100
title: Run the inherited native visual-acceptance pass
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0097]
---

# Run the inherited native visual-acceptance pass

The RFC-BPR-0003 and RFC-BPR-0004 ledgers both defer the integrated-workbench presentation-polish and native visual-acceptance pass to RFC-BPR-0005. This card owns it.

On the real macOS composition with native Cocoa and QMacStyle, inspect the assembled workbench for presentation quality at supported window sizes and scales, with readable contrast and correct native control rendering.

DEFERRED CLASS: owner-attended. It needs a person at the keyboard to authorise and observe a live appearance context and to judge visual acceptance, which automation cannot assert. Batch it with the other attended checks. It does not block any other RFC-BPR-0005 card.
