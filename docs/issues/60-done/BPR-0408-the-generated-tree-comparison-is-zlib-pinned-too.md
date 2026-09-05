---
schema: "implement-rfc.card.v1"
id: "BPR-0408"
title: "The generated-tree comparison is zlib-pinned too"
status: "Done"
rfcs: ["RFC-BPR-0019"]
depends_on: []
---

# The generated-tree comparison is zlib-pinned too

[[BPR-0378]] diagnosed one byte-equality term, `decode_png`'s, and its Parked
section called the remainder "a bounded implementation slice: replace the
byte-equality term in `decode_png`". The operator chose option 1 on that basis.

There is a second term, and the card never found it. `validate_assets`
(`scripts/dif_assets.py:1084-1095`) compares the committed `generated/` tree
against a fresh regeneration:

```python
expected = generated_products(root)
committed = _tree_bytes(root / "generated")
if committed != expected:
    raise AssetError(f"committed generated products are stale: {differing}")
```

and `generated_products` (`:1006-1035`) builds every product through
`encode_png(clip.strip)`, which is `zlib.compress(..., level=9)`. Same
dependence, different site.

## Measured, not inferred

Substituting a different DEFLATE for the one the assets were built with — the
exact shape of the zlib-ng difference — against the real committed tree, with
[[BPR-0378]]'s `decode_png` fix already in place:

```
== as-is, stock zlib 1.2.12 ==
validate_assets: OK

== with a different DEFLATE (level 6 substituted) ==
validate_assets: AssetError: committed generated products are stale:
  ['dif-calm-idle.png', 'dif-greet.png', 'dif-rest.png', 'dif-sprite-digests.json']
```

Independently, all 13 committed PNGs under `src/adapters/qt/assets/dif` change
their IDAT bytes under a different compression level — 13 of 13.

**So [[BPR-0378]]'s first required-final-state bullet is not met by its own fix.**
"The canon holds on an interpreter linked against either zlib or zlib-ng" is
still false at HEAD; the failure has moved from `decode_png` to `:1089`.

## The fourth entry is the hard part

Three of the four names in that error are PNGs, and the operator's chosen
principle answers them directly: compare decoded content rather than compressed
bytes, exactly as `decode_png` now does.

`dif-sprite-digests.json` is not a PNG. `generated_products:1021-1024` builds it
from `sha256_bytes(payload)` over each product's **compressed** payload, so the
committed digests file is itself a function of the local zlib. Applying the
principle to the PNGs does not fix it; the digests would still disagree.

## The decision this card carries

The PNG half is settled by the existing verdict. The digests file is not, and
the three answers give up different things:

1. **Hash decoded canonical content instead of compressed bytes.** Consistent
   with the principle already chosen, and portable thereafter. Costs a one-time
   rewrite of the committed `dif-sprite-digests.json`, and changes what that
   file's `output_sha256` *means* — from "these exact shipped bytes" to "this
   exact image". **Recommended**, because it is the same trade the operator
   already accepted one level down.
2. **Compare the digests file semantically**, normalizing its PNG digests before
   comparison and leaving the committed bytes alone. No asset rewrite, but it
   puts a second, subtler comparison rule in the validator and leaves the
   committed file environment-dependent — a later regeneration on a different
   host would still rewrite it.
3. **Leave `validate_assets` zlib-pinned and narrow the claim.** Say plainly
   that regeneration is reproducible only on the interpreter the assets were
   built with. Cheapest, and it means [[BPR-0378]]'s first bullet is withdrawn
   rather than met.

Option 1 is a change to what a provenance digest asserts, which is the same
class of decision [[BPR-0378]] correctly refused to make on its own authority.
It is recorded here rather than assumed.

## Required final state

- `validate_assets` succeeds on an interpreter whose DEFLATE differs from the
  one that produced the committed assets, or the claim that it should is
  explicitly withdrawn.
- Whichever answer is chosen, a regenerated product whose **pixels** differ is
  still rejected, and that is shown rather than assumed.
- No committed PNG's pixels change.
- [[BPR-0378]]'s first required-final-state bullet is either satisfied or
  rewritten to the narrower truth.

## Non-goals

- Do not touch anything under `src/`; see the dev-binary hold in the board
  README. Nothing here needs to.
- Do not regenerate the PNGs. Option 3 of [[BPR-0378]] was rejected precisely
  because it rewrites asset bytes, and nothing in this card requires it.
- Do not edit `docs/verification/reviews/**`. Those copies of
  `scripts/dif_assets.py` are frozen digest-bound review identity, not source.

## Verification

```sh
python3 tests/scripts/dif_asset_tests.py
ctest --test-dir build/core-only --output-on-failure -R '^dif_assets_contract$'
python3 scripts/verify_documentation.py --root .
git diff --check
```

plus a direct replay of the DEFLATE-substitution probe above, which must reach
`validate_assets: OK` for whichever answer is chosen — or be recorded as
deliberately still failing, under option 3.

## Result — 2026-08-20

Owner direction was "do whatever". The recommendation this card carried was
option 1, hashing decoded content into `dif-sprite-digests.json`. **It was wrong,
and checking before implementing is what caught it.**

### Why the recommendation was withdrawn

`docs/verification/reviews/dif-final-v001/review-identity.json:12` binds
`generated/dif-sprite-digests.json` to
`7e066db7b5415995e9b7e9cf72f985f3208b5fd9c39b34a4567410d01d2b4bc4`, and that is an
**accepted** attended batch — it has both `human-verdict.json` and
`verdict-validation.json`. The committed file still hashes to exactly that value.

So option 1 would not have been a one-time rewrite of a generated file. It would
have been this session breaking an accepted human verdict's recorded identity,
which currently still holds. Nothing enforces that binding mechanically, which
makes it easier to break and no more acceptable to break.

### What was done instead

The goal was never "rewrite the digests file". It was "stop asserting which
compressor built the tree". Those come apart, and separating them costs nothing:

| product | compared as | why |
|---|---|---|
| `*.png` | decoded image, in canonical form | the pixels are the claim; the compressed stream is not |
| `dif-sprite-catalog.hpp` | bytes | deterministic text |
| `dif-sprite-digests.json`, every field but `output_sha256` | bytes | schema version, manifest digest, source digests — all environment-independent |
| `dif-sprite-digests.json`, `output_sha256` | against the **committed** payloads on disk | this is what the field actually claims, and hashing files that already exist gives the same answer everywhere |

The last row is the whole trick. Comparing the inventory against a freshly built
one reintroduced the exact dependence being removed, because a fresh inventory
hashes fresh compressed payloads. Asking instead whether the recorded digests
describe the products actually committed is portable, is a real check, and needs
no byte to move.

### Verification matrix

| claim | surface | command | proves it | result | evidence |
|---|---|---|---|---|---|
| the comparison is portable across DEFLATE implementations | `validate_assets` on the real committed tree | substitute level 6, then level 1, for the generator's level 9 | a different compressor is exactly what zlib-ng is | **OK at 9, 6, and 1** — was `stale: [3 PNGs, dif-sprite-digests.json]` | this commit |
| a wrong image is still rejected | `test_rejects_a_generated_product_whose_pixels_differ` | flip one channel, re-encode canonically, **re-stamp the digest** | re-stamping leaves the decoded-content comparison as the only check that can fire | rejected | this commit |
| a non-canonical product is still rejected | `test_rejects_a_generated_product_that_is_not_in_canonical_form` | same pixels, two IDATs, digest re-stamped | only the canonical-form requirement can fire | rejected | this commit |
| a recompression that changes no pixel is accepted | `test_accepts_a_generated_product_recompressed_at_another_level` | rewrite one product at level 1 | this is the property deliberately traded away | accepted | this commit |
| a tampered inventory is still rejected | `test_rejects_stale_digest_inventory` | zero one recorded digest | the pre-existing case, unweakened | rejected | this commit |
| the suite grew rather than shifted | `tests/scripts/dif_asset_tests.py` | `python3 tests/scripts/dif_asset_tests.py` | 42 cases | **42, OK**, was 39 | this commit |
| the registered contract passes | `dif_assets_contract` | `ctest --test-dir build/core-only -R '^dif_assets_contract$'` | it runs the live boundary check over the real tree | pass, 34.4 s | this commit |
| **no committed byte moved** | Git | `git status --short` | only the script and its tests are modified | 2 files | this commit |
| **the accepted review identity still holds** | the bound file | `shasum -a 256 .../dif-sprite-digests.json` | it is what `dif-final-v001` recorded as reviewed | `7e066db7…`, unchanged | this commit |

The two rejection tests re-stamp the digest deliberately. Without that step the
inventory check would fire first and the test would pass while proving nothing
about the comparison it names — the kind of pass that looks like coverage and is
not.

### The two options not taken

Option 2, comparing the inventory semantically while leaving `output_sha256`
unverified, would have left a field that looks like provenance and is checked
against nothing. Option 3, narrowing the claim, is now unnecessary: the claim is
true.
