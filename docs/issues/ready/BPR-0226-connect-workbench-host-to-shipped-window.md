---
id: BPR-0226
title: Connect the workbench host to the shipped window and accepted review
status: Ready
rfc: RFC-BPR-0005
depends_on: []
---

# Connect the workbench host to the shipped window and accepted review

The shipped application constructs `workbench_window` and
`workbench_application_host`, but they are not connected.

At the reviewed head the window receives empty workbench intent, change, and
drawer-width callbacks. The host publishes into
`composition_workbench_projections`, a non-rendering sink whose own comment
says BPR-0191 should have replaced it. Accepted pull-request handoffs reach the
epoch host but never call `workbench_application_host::observe()`.

## Required final state

- The window's workbench intents, change selections, and observed drawer
  widths dispatch to the composed host.
- The host publishes success and failure to the real window, not the retained
  intermediate sink.
- An accepted pull request supplies the exact authority and bounded file
  observation to the host; invalidation clears or supersedes it by identity.
- Failure to open the layout database still composes a functional host with
  in-memory/default layout behavior. The existing comment promising defaults
  must become true.
- A public fixture-backed window trace invokes a primary action and proves the
  host publishes the resulting command/cursor/drawer projection to the window.
- Cutting each wire separately makes a distinct assertion fail.

No selected-file diff request belongs here; BPR-0227 owns that next slice.
