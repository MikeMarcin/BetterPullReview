---
id: BPR-0390
title: Cache-served comparisons render without syntax classification
status: In Progress
rfc: RFC-BPR-0006
depends_on: []
---

# Cache-served comparisons render without syntax classification

Found by the owner during BPR-0100's attended visual pass, minutes into
looking: `UIforETW/UIforETWDlg.cpp` — real C++ — renders monochrome in the
dark profile, no keyword, comment, or string coloring at all, while the
status line reports `Diff ready — 8 semantic changes (cached)` and every
composition marker stays green.

The mechanical facts, probed at the same binary:

- `SYNTAX_CLASSIFIER_COMPOSED_OK` — the classifier is composed.
- The KSyntaxHighlighting definitions are embedded in the executable.
- The automated `uiforetw-74` offscreen course reports
  `SYNTAX_CLASSIFICATION:1:2` — two submissions, one adopted — so even the
  green path adopts only half of what it submits.
- `adopt_classification` (`src/app/src/github_application.cpp:3993`) drops
  any publication whose blob no longer matches the current comparison's
  left/right key, deliberately and silently, and
  `adopt_token_spans` returning false is also uncounted.

The first hypothesis — cache-served comparisons never resubmit — is
contradicted by the live wiring and is withdrawn. `request_diff`
(`github_application.cpp:3938`) calls `classify_comparison` on every display
including diff-cache hits. The identified mechanism is a race with three
compounding parts:

- Span adoption and the diff result are two independent, unordered
  deliveries into the same pane: `diff_workbench::publish` never touches
  spans, while `clear_projection` (via `set_loading` on every new request)
  wipes them, so whichever arrives second decides whether code is colored.
- Classification identity rides a single latest-write-wins mutable context
  (`composition_blob_source::Context`, `github_application.cpp:1288`) read
  off-thread at execution time; there is no per-submission binding of blob
  to comparison beyond the cache key.
- `classification_in_flight` coalescing
  (`appearance_application_host.cpp:897`) starves resubmissions while an
  earlier comparison's classification is outstanding, and completions that
  land after supersession fail `adopt_classification`'s Current/Pending
  check and drop silently. The counters show it: the automated two-file
  course records `SYNTAX_CLASSIFICATION:1:2` with four blobs in play.

The instrumented run settled it, and both prior candidates were wrong. New
durable counters — cache hits, coalesced, dropped-at-adoption, failure
publications, terminals entering the decision lane — plus a
`SYNTAX_DISPLAYED:<left>:<right>` present-tense marker report
`SYNTAX_DELIVERY:0:0:0:1:2` with `SYNTAX_DISPLAYED:1:0` on the automated
course. Read together: nothing was coalesced, nothing was dropped at
adoption, and both submitted terminals eventually arrived — but one arrived
only when shutdown retired its record, meaning that classification stayed
outstanding for the entire session while its pane rendered monochrome. And
only two submissions exist where a two-comparison course owes four: one
comparison's `classify_comparison` never ran at all. Two independent
defects, then: **(A)** a display path that fails to submit for one of the
comparisons, and **(B)** a submitted classification whose effect never
completes — with no timeout, no retry, and no marker until shutdown refuses
it.

Defect B's blocking point is identified in code. The classification
effect's only blocking call is `composition_blob_source::blob_bytes` →
`epoch_application_host::materialize`, whose caller side waits on a
condition with **no timeout and no use of the caller's stop token**
(`epoch_application_host.cpp:3494`); the enqueued actor work is FIFO behind
whatever the repository actor is already running, and epoch maintenance can
hold that actor for most of a minute. Shutdown's `BlobIngressClosing` is
exactly what releases the wait — which is why the starved terminal arrived
only after `retire_all` had already refused its record. Once refused,
nothing retries: `classification_in_flight` keeps coalescing every later
display of the same key, so the pane stays monochrome for the session. The
repair therefore has three coupled parts: a bounded, stop-aware wait for
classification blob reads; in-flight bookkeeping that clears on failure so
a later display resubmits; and defect A's missing submission on the other
comparison, still to be traced. The markers
ask whether the classifier was composed and whether some classification was
ever adopted; nobody asks whether the comparison on screen is classified,
and no registered contract greps `SYNTAX_CLASSIFICATION` at all. This is
BPR-0216's shape wearing syntax colors.

## Required final state

- The dominant drop point is confirmed with one instrumented run:
  coalesced-before-submission versus completed-after-supersession.
- A displayed comparison's panes carry the classification for their exact
  blobs regardless of whether the comparison was computed or cache-served,
  with stale-result rejection preserved. The host shape follows the
  architecture: adopt synchronously from `AppearanceHost::cached` on
  display, bind classification identity to the request rather than shared
  mutable state, and keep the blob-identity drop policy as defense in
  depth, not the primary mechanism.
- A deterministic check fails when the selected comparison renders
  unclassified for a file whose grammar is embedded — asking about the
  screen, not the composition.
- The attended visual pass can be re-presented against repaired bytes.

## Verification

```sh
ctest --preset dev -R '^(application_uiforetw_selected_diff_contract|qt_diff_workbench_scale_100|application_workbench_composition_contract)$' --output-on-failure
```
