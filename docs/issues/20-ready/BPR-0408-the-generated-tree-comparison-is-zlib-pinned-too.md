---
id: BPR-0408
title: The generated-tree comparison is zlib-pinned too
status: Ready
rfc: RFC-BPR-0019
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
