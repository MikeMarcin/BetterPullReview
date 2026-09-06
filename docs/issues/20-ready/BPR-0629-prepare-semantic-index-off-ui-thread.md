---
schema: "implement-rfc.card.v1"
id: "BPR-0629"
title: "Prepare the selected semantic index outside the UI publication turn"
status: "Ready"
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


## Selected implementation boundary

Inspection after BPR-0628 confirms the existing diff effect job already produces
an immutable result. Its worker validates the full result, the UI decision-lane
`finish` validates it again, and Qt independently validates/builds its semantic
index. Cache lookup also repeats full validation. Core's `diff_semantic_index`
has a private constructor, factory validation, and shared ownership of its exact
result, making that existing type the prepared proof; no second proof subsystem
is needed.

Own `src/core/{include/bpr/core,src}/diff_semantics.*` for exact retained index
allocation accounting, `src/application/include/bpr/application/diff_runtime_ports.hpp`
and `src/application/src/diff_application_host.cpp` for prepared publication,
worker construction, identity checks, bounded cache retention and cancellation,
and Qt `diff_workbench.*` for adoption. Update direct fixture publishers and
focused core/host/Qt/application tests as required by the stronger publication
contract. No Qt API runs on the worker. Full current request context and exact
index/result pointer binding remain mandatory on acceptance; typed index
construction preserves full validation while eliminating repeated UI scans.
Account for index allocation within existing cache byte limits, including cache
hits; do not raise limits or keep a second index per result.

BPR-0628's final same-Mac course measures initial-selection maximum 297.119 ms
while native measurement input gaps are maximum/p95 5.356/4.488 ms. Its final
source is `49aefc1c`; phase/publication work remains the next latency boundary.
Keep initial native text installation measured separately when establishing a
local target. Required checks include focused semantic-index, diff host,
workbench, architecture-boundary, and composed large/unavailable/recovery lanes.
