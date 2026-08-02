---
id: BPR-0077
title: Implement shared comparison scroll controller
status: Backlog
rfc: RFC-BPR-0004
depends_on: [BPR-0074, BPR-0076]
---

# Implement shared comparison scroll controller

Own one retained subpixel comparison position, synchronization rail, and
shared vertical scrollbar. Route pixel/angle wheel and trackpad input over
either pane or the center divider, Page Up/Down, Home/End, scrollbar actions,
and programmatic reveal through that controller. Suppress recursive pane
feedback and keep horizontal scrolling pane-local.

The pane receiving input is not a leader. Never copy pane scrollbar values or
round-trip authority through integer scrollbar projection.

## Required direct evidence

Compare identical left/right input traces; prove right insertion and left
deletion pinning, 4:7/7:4 proportional motion, repeated forward/reverse without
drift, no handoff jump, smooth subpixel accumulation/inversion, no dead travel,
all navigation paths, feedback suppression, and horizontal isolation.
