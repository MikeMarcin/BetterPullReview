---
schema: "implement-rfc.card.v1"
id: "BPR-1558"
title: "Both comparison panes take their classification"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"Owner report, 2026-10-16","governing_rfc":"RFC-BPR-0006","latent_since":"ae6da0b54","related_cards":["BPR-0185","BPR-0368","BPR-0390","BPR-1382"],"reservation":"docs/issues/_reservations/BPR-1558--BPR-1558--c6c048645db2.json","acceptance":["BPR-1558:identical-sides","BPR-1558:departure","BPR-1558:gates"]}
acceptance: [{"criterion": "BPR-1558:identical-sides", "outcome": "pass", "revision": "6d93bc68e", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_syntax_delivery$'", "detail": "source-pr-search-v1 through the shipped composition: with the classifier's blob reads held until src/renamed.cpp's diff is shown (LeftBlob == RightBlob asserted), release ends with both panes classified and sharing the one span list (1 submission, 1 coalesced, 2 adoptions). With only the previous adopt_classification restored it fails at \"identical-sides: the right pane carries its classification\" (left adopted, right null)."}}, {"criterion": "BPR-1558:departure", "outcome": "pass", "revision": "6d93bc68e", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(application_syntax_delivery|application_appearance_host)$'", "detail": "bulk/000.cpp left while its left read is held, bulk/001.cpp shown, bulk/000.cpp returned to: both panes carry 31 spans and visible lines carry syntax formats. test_classification_cancellation_resubmits_and_spares_other_work: after cancel_classifications a request for the same key is a second submission (none coalesced), the retired terminal is refused as cancelled, the fresh one publishes and is cached, and an export posted before the cancel still writes and is acknowledged. The departure course also passes without the cancel (the public file switch orders the stopped read's terminal first); the cancel covers publish()'s same-turn clear-and-request, which no fixture reaches."}}, {"criterion": "BPR-1558:gates", "outcome": "pass", "revision": "82f34e520", "evidence": {"kind": "command", "command": "full dev ctest in disk-bounded batches (scratch runner: build each batch's executables, ctest --preset dev -j6 -R batch, delete executables past their last use); focused reruns; verify_documentation.py; format.py; tidy.py", "detail": "Pre-merge tree 0da3f6309's tests over 740 rows: 723 passed in the batched run. Batching artifacts: rfc0016/0017_test_inventory and test_network_isolation_manifest (gh_cli_git_smart_transport's executable absent when listed; the manifest also caught this course's missing WINDOWS_UNCONTAINED_TESTS pin, fixed in 0da3f6309) pass on rerun; live_mutation_isolation needs every binary at once, so verify_live_mutation_isolation.py ran its source half and 15 present binaries including this course's and the positive control: LIVE_MUTATION_ISOLATION_OK. Load-sensitive rows rerun serially pass: application_diff_host, qt_markdown_body_1_25/2_0, release_update_native_https, native_https_transport_fixture, github_provider, application_large_review_responsiveness/pathological, application_epoch_portable_occurrence_two_catalogues, application_private_review_retry (twice each for the last two). application_large_review_launch fails its Mac responsiveness budget on this Debug -O0 tree and identically on the merge base bd04bf3df's sources built in the same tree; application_large_review_cache_bytes skips as registered. Merged tree 82f34e520 (main 1e5c35b7f): the 13 syntax, composition and inventory rows pass, including the resealed round-trip contract, and application_composition_contract passes (618.7 s). clang-format 22.1.8 clean; clang-tidy clean on the four changed translation units; DOCUMENTATION_OK; documentation verifier 67 tests OK."}}]
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

## Limits

Offscreen fixture evidence on macOS arm64, Qt 6.11.2, Debug. It reproduces
the reported symptom on one path (one blob on both sides, classification
answering after the diff); the owner's file was not inspected, so whether it
was a rename or move, or unchanged between the compared updates, is not
known. Other ways a side can stay plain were traced and not found reachable:
both sides are classified symmetrically on one serial worker; adoption is by
blob identity against the surface's exact context; a departed comparison's
failed terminal reaches the decision lane before a later selection's request
on every public path except `publish`'s same-turn clear and request, which
the retirement now covers. A classification that fails for a transient read
reason on a kept selection is still not retried. The installed 0.2.x keeps the
defect until a release carries the fix.
