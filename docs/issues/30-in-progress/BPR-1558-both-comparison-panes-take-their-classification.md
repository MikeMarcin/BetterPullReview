---
schema: "implement-rfc.card.v1"
id: "BPR-1558"
title: "Both comparison panes take their classification"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"Owner report, 2026-10-16","governing_rfc":"RFC-BPR-0006","latent_since":"ae6da0b54","related_cards":["BPR-0185","BPR-0368","BPR-0390","BPR-1382"],"reservation":"docs/issues/_reservations/BPR-1558--BPR-1558--c6c048645db2.json","acceptance":["BPR-1558:identical-sides","BPR-1558:departure","BPR-1558:gates"]}
---

# BPR-1558: Both comparison panes take their classification

## Owner notes

Owner report, 2026-10-16, on the installed 0.2.x app (possibly GHES), verbatim:
"the right side of a diff didn't syntax highlight until I switched off and
back to the file." Intermittent; switching away and back fixed it.

## What exists

- `classify_comparison` asks the appearance host to classify each side's blob
  under one definition. The host answers from its cache, coalesces a request
  for a blob already in flight, or submits one classification.
- Two routes put spans on the panes. A classification's own publication goes
  through `adopt_classification`, which decides the side by blob identity
  against the surface's current context. The diff's publication goes through
  `adopt_published_classification`, which reads the host's cache for both
  sides ([[BPR-0390]]).
- `adopt_classification` picked the first side whose blob matched. When both
  sides are the same blob (a pure rename or move, or a file two compared
  updates share), `classify_comparison` asks twice, the second request is
  coalesced, and the one publication coloured only the left pane. If it
  arrived before the diff, the display-time cache read coloured both; if it
  arrived after, which a slow (selected-object, network) read makes likely,
  the right pane stayed plain. Coming back to the file reads the now-cached
  spans onto both panes, which is the owner's recovery. Latent since
  BPR-0185's `ae6da0b54` (2026-08-07); not a recent regression.
- A departure from the comparison (`clear_diff_selection`) drops the
  classifier's blob bindings and stops their reads, but left the host's
  classifications in flight. A request for the same blob made in the same
  decision turn (`publish` presenting a new diff under a kept request clears
  and requests at once) would be coalesced onto a classification whose read
  was just stopped, and that pane would never be classified.

## Scope

- **Every matching side.** `adopt_classification` hands one publication to
  each side whose blob it names.
- **Retire with the reads.** `appearance_application_host` gains
  `cancel_classifications()`, retiring only classifications (settings, import
  and export work continue); `clear_diff_selection` calls it beside
  `clear_comparison`, so a later request is submitted afresh.
- **Course.** `application_syntax_delivery` holds the classifier's blob reads
  (a tests-on hold in the composition's blob source, released by the test or
  ended by a departure, as a real read is) so classification answers after the
  diff is on screen.

## Acceptance

- `BPR-1558:identical-sides`: through the shipped composition on
  `source-pr-search-v1`, the pure rename `src/renamed.cpp` (one blob on both
  sides) classified after its diff is shown ends with both panes carrying the
  one immutable classification. Fails against the previous
  `adopt_classification` at "the right pane carries its classification".
- `BPR-1558:departure`: leaving `bulk/000.cpp` while its classification reads,
  visiting `bulk/001.cpp` and returning ends with both panes classified and
  coloured; `application_appearance_host` proves a classification-only cancel
  makes the next request for the same key a fresh submission, refuses the
  retired terminal and leaves an export in flight writing.
- `BPR-1558:gates`: focused syntax and composition checks, a full dev
  `ctest` on the merged tree, clang-format 22, tidy on the changed units, the
  composition reseal and the documentation gate.

## Boundaries

The core decides, the host performs, Qt presents. No core, classifier,
palette, Qt painting or provider change. Size and grammar limits are per
blob and unchanged: a side over the classifier's 8 MiB bound or its span
bound stays plain while the other side may be coloured; making a skip
symmetric is not in this card.
