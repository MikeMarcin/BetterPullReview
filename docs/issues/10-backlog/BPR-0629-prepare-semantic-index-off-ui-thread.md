---
schema: "implement-rfc.card.v1"
id: "BPR-0629"
title: "Prepare the selected semantic index outside the UI publication turn"
status: "Backlog"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0627"]
---

# Prepare the selected semantic index outside the UI publication turn

BPR-0627's temporary phase instrumentation on source `d698b338` measured about
111.14 ms in `build_diff_semantic_index` during real 20,000-line publication.
Installing both native source documents separately took 62.12 ms. BPR-0628 owns
native pane measurement; neither step should be hidden by a larger UI budget.

## Bounded acceptance

- Inspect the host/Qt publication ownership and immutable-index lifetime before
  choosing the smallest design. Keep deterministic validation/index semantics
  in core and effect scheduling/cancellation in the host; no worker may touch Qt.
- Reuse or prepare the exact immutable semantic index off the UI thread, bind it
  to the accepted result and full current request identity, and prevent stale or
  mismatched index/result adoption. Do not skip validation to improve a timing.
- Preserve one semantic index per accepted immutable diff and bounded cache/job
  retention. Verify cache hits, cancellation, replacement and rejected late
  completion without extra whole-result work on routine interaction.
- Run focused core/host/Qt checks and the real stored large-review course. Record
  publication and event-loop measurements on the same Mac and verify exact
  source, navigation, scrolling and final-selection precedence.
- Confirm remaining indivisible native text-install cost separately. Define a
  measured local publication target without claiming universal timing or native
  Windows/Linux evidence.

Expected scope is the existing diff host/publication port and Qt workbench;
final exclusive ownership must be recorded after inspection. RFC base
`d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617. Use local synthetic
fixtures only; no Actions, push, paid CI, credential use or production app launch.
