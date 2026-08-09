---
id: BPR-0233
title: Exercise the recorded UIforETW diff and intraline pixels through the selected public path
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0227]
---

# Exercise the recorded UIforETW diff and intraline pixels through the selected public path

The recorded `google/UIforETW#74` fixture is now served and its generator is
wire-exact, but the registered public-path test only opens the pull request and
saves a window grab. It never requests a selected diff. `RENDER_OK` means the
PNG save succeeded, not that it contains comparison or intraline content.

## Required final state

- Run the recorded fixture through BPR-0227's real file-selection path.
- Assert the selected comparison uses the recording's expected base/head blobs
  and contains the known near-identical replacement pairs.
- Assert nonempty intraline removed/added ranges and the exact semantic role
  applied to their pane layout.
- Capture the affected pixels in Light, Dark, High-contrast Light, and
  High-contrast Dark only after the deterministic range/role assertions pass.
- Bind an attended verdict to those exact captures before BPR-0214 or BPR-0210
  closes.

Fixture serving is complete in BPR-0187. It is not the remaining blocker.
