# Offline Attended Review Packages

The builder converts a JSON input manifest into a self-contained, offline
`index.html` plus `attended-review-manifest.json`. The page works over
`file://`, stores progress in the browser keyed by batch, subject, and
content digest, and exports one complete verdict once every subject is
resolved. Gates, scopes, claims, and media groups are caller-declared.

## Input Manifest — `bpr.attended_review_batch_input` v1

```json
{
  "schema_name": "bpr.attended_review_batch_input",
  "schema_version": 1,
  "project_id": "betterpullreview",
  "title": "Diff line-mapping visual acceptance (RFC-BPR-0004)",
  "review_gate": "diff_rendering_acceptance",
  "review_scope": "human_ui_criterion_isolation_only",
  "downstream_claims": {
    "rfc_closeout_claimed": false,
    "release_gate_satisfied": false
  },
  "reference_disclaimer": "Reference images are illustrations, not pixel specifications. RFC text and acceptance criteria outrank reference pixels.",
  "subjects": [
    {
      "subject_id": "criterion:/betterpullreview/rfc-bpr-0004/line-mapping",
      "subject_key": "line-mapping-selected-rows",
      "display_name": "Selected-line mapping stays exact across rename hunks",
      "description": "Falsifiable question the reviewer answers.",
      "acceptance_criteria": ["Selected rows map to identical file lines on both sides"],
      "known_concerns": ["Intra-line change casing near hunk boundaries"],
      "ab_group": null,
      "choice_group": null,
      "audit_verdict": {"status": "pass", "note": "Offscreen public-path course passed at <commit>."},
      "media_groups": [
        {"group_id": "state-captures", "title": "State captures", "required": true,
         "layout": "grid", "note": "Light and dark at two scales.",
         "items": [{"label": "Dark 100% DPR2", "path": "captures/dark-100.png"}]}
      ],
      "detail_sections": [
        {"heading": "Provenance", "kind": "definitions",
         "items": [{"term": "Command", "definition": "the capture invocation recorded with the evidence"}]}
      ],
      "identity_files": [{"label": "Capture manifest", "path": "captures/manifest.json"}]
    }
  ]
}
```

Field rules:

- `ab_group`, `choice_group`, `selection_group`: at most one of the three.
  `ab_group` groups subjects for presentation and leaves each an independent
  pass or fail. `choice_group` declares them **alternatives to one question**,
  and changes what a verdict must say: exactly one member accepted, and every
  other member listed in `not_chosen_subjects` rather than
  `rejected_subjects`. `selection_group` is the general form — a declared
  batch-level group with `min_accept`..`max_accept` cardinality covering A/B
  comparisons, multiple choice, and select-N; see Selection Groups below.
  `choice_group` remains the choose-exactly-one shorthand.

  Use `choice_group` whenever the subjects are shades of one decision. Without
  it, an n-way choice has to be recorded as one acceptance and n-1 rejections,
  which puts candidates on the record as defective when they merely lost — and
  if the winner is later withdrawn, nothing distinguishes a runner-up from a
  real failure. A product owner said as much: *"it also feels a lot more like
  multiple choice rather than pass/fail"*.

  `not_chosen_subjects` is optional and must be omitted when no subject
  declares a `choice_group`. Verdicts recorded before choice groups existed do
  not carry it and still validate.
- `review_gate`, `review_scope`: free tokens matching
  `^[a-z0-9][a-z0-9_.-]{0,63}$`. Declare what this batch is and is not.
- `downstream_claims`: non-empty; keys match `^[a-z0-9][a-z0-9_]{0,63}$`;
  every value must be `false`. The claim set is the anti-escalation fence:
  it is echoed into the manifest, the page, and the verdict, and the
  validator refuses any change. Name the claims a careless reader might
  otherwise infer from an accept.
- `reference_disclaimer`: optional; rendered prominently; part of batch
  identity, so changing the framing changes the batch.
- `media_groups`: caller-named and ordered. `required: true` (default) with
  no items is a build error; `required: false` with no items drops the
  group from the manifest and digest. Every subject must retain at least
  one non-empty group. `layout: compare` renders a two-up grid and needs
  at least two items. Previewable kinds: image (.gif .jpeg .jpg .png
  .webp), video (.mp4 .webm), audio (.flac .m4a .mp3 .ogg .opus .wav).
  Anything else goes in `identity_files`, which binds bytes without
  previewing them.
- Every previewable image is also a full-resolution trigger. The grid
  sizes media to the layout, which is not always enough to judge the
  claim: a one-device-pixel rendering detail in a full-window capture
  cannot be seen at grid size, and a rejection produced that way carries
  no information about the product. Clicking an image opens it at native
  size, unscaled and unfitted; Escape or a click outside closes it. The
  viewer only reads the DOM, so decision state, the pending count, and
  the export lock are unaffected. Arrow keys or the on-screen chevrons step
  through every visible full-size image in page order, with a position
  counter, so walking a capture matrix or comparing alternatives is one
  keypress per image; navigation never touches decision state.
- `audit_verdict`: the mechanical precondition. `fail` renders the accept
  button disabled and makes any accepting verdict invalid; the subject can
  still be explicitly rejected.
- `detail_sections`: `list`, `definitions`, or `code` blocks for
  provenance, state coverage, or excerpts. Code lines are stored with
  trailing whitespace stripped.

## Selection Groups

Declare a batch-level selection group and reference it from each member
when subjects are alternatives to one question with declared bounds:

```json
"selection_groups": [
  {"group_id": "hero", "title": "Pick the hero capture",
   "min_accept": 1, "max_accept": 1},
  {"group_id": "supports", "title": "Keep up to two supporting shots",
   "min_accept": 1, "max_accept": 2}
]
```

- An A/B comparison is a two-member choose-exactly-one group; multiple
  choice is the same with more members; select-N declares its bounds,
  including `min_accept: 0` for "keep up to N".
- The authoring tell: if accepting one subject would make its siblings
  pointless to accept, they are one question — declare a group instead of
  emitting independent pass/fail subjects.
- Members must be contiguous in subject order, at least two per group, and
  a member may not also declare `choice_group` or `ab_group`. The group
  definitions and memberships are part of batch identity.
- On the page, declared groups render under a header with a cardinality
  line and a live status; choose-exactly-one groups (including
  `choice_group` shorthand members) behave like radios, and wider groups
  refuse choices beyond `max_accept` until one is cleared. Unchosen
  members of a satisfied group are recorded as not chosen; Reject still
  means genuinely defective.
- Zero accepted members is valid only when every member is explicitly
  rejected — "none of these are acceptable" is a recordable answer, and
  the only alternative to meeting the declared range. (`choice_group`
  keeps its strict exactly-one contract for compatibility; use a
  `selection_group` when the no-acceptable-answer outcome must be
  expressible.)

## Identity And Digests

`subject_digest` hashes the subject's metadata plus each bound file's
digest — never its path — so the package is location-independent while any
changed byte changes the digest. `batch_id`
(`bpr:/attended-review/<project>/<24hex>`) hashes the gate, scope, claims,
disclaimer, and every subject identity. Browser storage keys include the
digest, so a revised subject returns to pending; stale decisions cannot
carry across content changes.

## Review Procedure

1. Build with the skill's `build` subcommand into an empty directory.
2. Present `index.html` to the user and stop.
3. The user resolves every subject; the export button unlocks only at zero
   pending, and any later edit clears the exported text.
4. Save the pasted verdict to a temporary file and run `validate-verdict`.
5. Apply accepted identity/digest pairs only; record rejected identities as
   evidence.

Never assemble a verdict yourself from screenshots, partial page state, or
"approve the rest". When transcribing a user's decisions into a hand-built
verdict for a packet-adjacent flow, list subjects in manifest order — the
validator enforces it.

## The Validator Checks

1. The manifest is a `bpr.attended_review_batch` v1.
2. `schema_name`, `schema_version`, `batch_id`, `review_gate`, and
   `review_scope` equal the manifest's values exactly.
3. Every entry in every subject list is exactly `{subject_id, subject_digest}`.
4. Accepted, rejected, and not-chosen form an exact, duplicate-free
   partition of all manifest subjects (missing and extra identities are
   reported; `not_chosen_subjects` absent means empty).
5. Note keys are `subject_id@subject_digest` with string values.
6. `downstream_claims` equals the manifest's declared false-only map.
7. The verdict has exactly the expected top-level fields;
   `not_chosen_subjects` is required when the manifest has any choice or
   selection group.
8. Every list preserves manifest subject order.
9. No accepted subject failed its mechanical audit.
10. `not_chosen_subjects` contains only members of a choice or selection
    group.
11. Each `choice_group` has exactly one accepted member with the remainder
    not chosen; each `selection_group`'s accepted count is within its
    declared `min_accept`..`max_accept` range, or zero with every member
    explicitly rejected.
