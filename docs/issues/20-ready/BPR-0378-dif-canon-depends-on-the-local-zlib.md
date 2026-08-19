---
id: BPR-0378
title: The Dif asset canon depends on which zlib the interpreter was built with
status: Ready
rfc: RFC-BPR-0019
depends_on: []
---

# The Dif asset canon depends on which zlib the interpreter was built with

```
AssetError: source/dif-neutral-master.png is not a canonical product PNG
```

Nothing is wrong with that file. It does not differ from its blob — it is
binary, git leaves it alone, and its digest is unchanged. The canon simply
cannot reproduce it here.

`decode_png` asserts `data == encode_png(decode_png_bytes(data))`, and
`encode_png` builds the IDAT with `zlib.compress(..., level=9)`. DEFLATE at a
given level is not specified to produce identical output across
implementations, and this host's interpreter does not use the one the assets
were built with:

```
zlib version: 1.3.1.zlib-ng      runtime: 1.3.1.zlib-ng
original bytes: 745543   re-encoded: 742315   equal: False
first difference at byte 35
```

Byte 35 is inside the first IDAT. zlib-ng compresses the same pixels 3,228
bytes smaller than the stock zlib that produced the committed asset.

## This is not a Windows defect

It is an *interpreter* defect, and it will reproduce on any platform whose
Python is linked against zlib-ng — which is increasingly the default, including
recent CPython Windows builds and several Linux distributions. A macOS or Linux
machine that upgrades Python could hit it tomorrow. It was found during the
Windows program because that is where a fresh interpreter was first used.

## What the canon is actually for

The point of `verify_assets` is that the committed products are exactly what the
generator produces from the committed sources — that nobody hand-edited a
sprite. Byte equality of the *compressed* stream is one way to say that, and it
happens to also assert "and your zlib is the same as mine", which is not a
property anyone wants and not one the assets depend on.

## The decision this card carries

1. **Compare decoded content, not compressed bytes.** Assert that the committed
   PNG decodes to exactly the pixels the generator produces, and that its
   structure is canonical — one IDAT, no ancillary chunks, the filter choices
   the generator makes. This is environment-independent and still catches a
   hand-edited sprite, which is the actual threat. It stops detecting a
   re-compression that changes no pixel, which nobody was defending against.
2. **Pin the compressor.** Vendor a deterministic DEFLATE, or store the expected
   IDAT alongside and compare against that. Keeps byte equality; adds a
   dependency or a second copy of every asset's compressed stream.
3. **Regenerate the assets with the current zlib.** Fixes this host and breaks
   the next one. Not a resolution.

Option 1 looks right, and it is a change to what a provenance canon asserts, so
it is the product owner's call rather than an implementation detail.

## Required final state

- The canon holds on an interpreter linked against either zlib or zlib-ng, and
  says which property it is asserting.
- A hand-edited sprite still fails it. Whatever replaces byte equality must be
  shown to fail on a deliberately altered pixel, not assumed to.
- `dif_assets_contract` passes, and is not made to pass by weakening the check
  to something that would accept an edited asset.

## Verification

```sh
py -3 -c "import zlib; print(zlib.ZLIB_RUNTIME_VERSION)"
py -3 tests/scripts/dif_asset_tests.py
ctest --preset core-only --output-on-failure -R '^dif_assets_contract$'
```

## One hypothesis checked and rejected — 2026-08-18

The failing path names a *source* file while the message says "canonical
**product** PNG", which reads like the check being applied to something it was
not meant for. It is not.

`decode_png` is called on the source master at `dif_assets.py:932` and on every
source frame at `:942`, deliberately: the sources have to be in the tool's own
canonical form so that regeneration from them is deterministic. Applying the
canon to sources is the design, not a slip, and narrowing it to `generated/`
would drop a real property rather than fix anything.

So the decision above stands as written. It is recorded here because two other
cards in this program — [[BPR-0376]] and [[BPR-0364]] — turned out to be asking
for decisions the codebase had already made, and this one was checked the same
way before being left open.

Under zlib-ng no PNG in the tree is canonical, not just the one the error names;
that file is simply the first one read.
