# Recording Evidence And Card Lifecycle

## Review Directory

Each reviewed gate gets one directory under
`docs/verification/reviews/<gate-slug>-vNNN/`, containing:

- `offline-review-input.json` — the exact input manifest that built the
  package. Keep it path-local: reference media relative to the input file,
  never by absolute path.
- `offline-package/` — the built package: `index.html`,
  `attended-review-manifest.json`, and copied media.
- `human-verdict.json` — the user's pasted export, byte-preserved.
- `verdict-validation.json` — the validation evidence record.

## Validation Record

The validation record binds artifacts with the artifact triple
`{path, sha256, byte_length}` (repo-relative POSIX paths) for the
manifest, the verdict, and the validator itself:

```json
"official_validator": {
  "path": ".agents/skills/bpr-attended-review/scripts/build_attended_review.py",
  "sha256": "sha256:<64hex>",
  "byte_length": <n>
}
```

Record `recorded_at` as RFC 3339 with a UTC offset, the exact
`validate-verdict` invocation, and a claims object in which exactly the
decided fact is true and every adjacent claim is `false`. Write records
immutably: never rewrite a recorded verdict or validation file. A changed
subject digest is a new batch and a new record, with the old one preserved
as history. If a later schema pins this validator's path and digest,
historical records keep their recorded digests; reproduce them with the
recorded tool version rather than editing the record.

## Card Lifecycle (betterpullreview-implement-rfc)

1. When a card's remaining evidence is attended-only, build the packet or
   page, reference it from the card, and park the card with an
   `awaiting-user` label in the board's blocked-equivalent lane — with the
   current `backlog`/`ready`/`in-progress`/`done` board, `backlog` holds
   work blocked on the human dependency. Continue with work that does not
   depend on it; an attended element never stops the run.
2. At the run boundary, present every pending checkpoint together: for
   each, what to open or launch, the bounded decision, and where the
   verdict lands.
3. On the user's response: validate (`validate-verdict` for pages, the
   exact-statement rule for packets), write the review directory, then
   update the tracker in the same slice — tick the card's attended
   acceptance checkbox, append the verification-matrix row on the closeout
   card (`claim`, the validate invocation as `command or interaction`,
   result, the validation record as `evidence`, the commit), update the
   RFC's verification document, and move the card out of the parked lane.
4. A rejection is evidence too: record it, open the follow-up card for the
   fix, and leave the gate pending. Never downgrade a rejection into a
   softer claim.
