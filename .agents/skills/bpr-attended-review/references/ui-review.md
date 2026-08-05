# UI And Criterion Reviews

For interaction and rendering acceptance the unit of decision is the
acceptance criterion, not the screenshot. Model each criterion as one
subject whose media groups hold the state captures that prove or falsify
it. No new decision machinery is needed: accept means "this criterion holds
in these exact captures", reject means it does not, and the notes field
carries the basis.

## Shaping The Batch

- One subject per falsifiable criterion. Phrase `display_name` and
  `description` as the question with a wrong answer that would otherwise
  pass unnoticed — for example: "Do selected rows map to identical file
  lines on both diff sides across rename hunks? A mapping error highlights
  a different line than the one the reviewer acted on."
- The required `state-captures` group holds the criterion's evidence
  matrix: themes, UI scales, window sizes, and interaction states
  (drawers open and collapsed, focus order stops, loading, error, and
  empty states), each capture labeled with its state and device pixel
  ratio.
- An optional `compare`-layout group pairs a reference image with a
  capture. Reference images are illustrations, not pixel specifications;
  set `reference_disclaimer` so the page says so: RFC text and acceptance
  criteria outrank reference pixels.
- Put the capture manifest (per-capture sha256, device pixel ratio,
  logical and pixel dimensions, producing commit) in `identity_files`, and
  surface the load-bearing values in a `definitions` detail section along
  with the exact producing command.

## Capture Provenance

Captures come from the real BetterPullReview application — driven by its
deterministic UI automation where it exists, or by a documented manual
capture procedure — never from mockups or component harnesses alone.
Record with each capture set: the producing command, commit, build preset
and configuration, platform and Qt version, and device pixel ratio.
Formats outside the previewable set are bound as `identity_files` with a
PNG derivative in the media group.

Set `audit_verdict` from the deterministic automated course that covers
the same surface (the offscreen public-path and contract courses): a
criterion whose mechanical course failed renders accept-disabled, and the
validator refuses an accepting verdict for it. Diff-rendering criteria
should use representative fixtures from the checked-in fixture catalogs
so displayed line identity is verified against known expected data, not
only that rendering completed.

## Recording The Verdict

Convert the validated verdict into per-criterion entries in the RFC's
verification document under `docs/verification/` — one entry per subject
with the dimension, the verdict, and the basis taken from the subject's
note — alongside the build identity (commit, preset, configuration),
platform record, explicit limits for platform-only claims, and any
rejected attempts. Then complete the card lifecycle per
`references/integration.md`.
