---
id: BPR-0378
title: The Dif asset canon depends on which zlib the interpreter was built with
status: Parked
rfc: RFC-BPR-0019
parked_on: awaiting-product-decision
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

## The trade-off is measured, and the decision is ready — 2026-08-19

This host's interpreter links stock zlib 1.2.12, so the canon passes here and the
failure does not reproduce. What could be done without an owner is establish what
each option actually costs, which the card had left as reasoning. It is now
measurement, recorded at
`docs/verification/reviews/dif-canon-portability-v001/canon-measurement.txt` from
the script beside it.

Three facts, all against the committed `dif-neutral-master.png`:

| Question | Answer |
|---|---|
| Does a deliberately altered pixel still fail Option 1? | **Yes.** One channel changed by 17, re-encoded into a structurally valid canonical PNG that passes every existing structural check: the scanline comparison rejects it. This is the card's second required bullet, measured rather than assumed. |
| What does byte equality catch that Option 1 does not? | Exactly one thing: a re-compression that changes no pixel and no filter choice. The identical filtered scanlines at level 6 are 746,919 bytes against the committed 745,543 — byte equality rejects it, Option 1 accepts it. |
| What does the structural check already cover? | `decode_png_bytes` already rejects any chunk that is not IHDR/IDAT/IEND, any bad CRC, non-consecutive IDATs, anything not non-interlaced 8-bit RGBA, and any decompressed size disagreeing with the header. None of that is at stake. |

So byte equality's entire marginal coverage over Option 1 is "your DEFLATE
implementation matches mine" — the property the card already identified as one
nobody wants. The replacement is not weaker in any other respect, and on the
filter-choice question it is not weaker at all: comparing the decompressed
scanlines against `_filtered_rgba_rows` asserts the generator's filter decisions
directly.

The committed tree carries one IDAT and scanlines equal to the generator's own
filtering, so Option 1 holds today without touching an asset.

### The packet

`docs/verification/reviews/dif-canon-portability-v001/offline-package/index.html`
presents the three options as one choose-exactly-one question rather than three
pass/fail checks, because accepting one makes the others pointless. Every subject
binds the same three identities: `scripts/dif_assets.py`, the measurement script,
and its output. Deliberately not this card: binding a card to a verdict recorded
on the card forces a rebuild on every edit to it, and the card is context rather
than governed bytes. Batch identity is
`bpr:/attended-review/betterpullreview/737479c73240621e0f2d4a91`. Option 3's
mechanical audit is `fail`, so it can be rejected but not accepted — the card already records that it fixes this host and
breaks the next, and it is the only option that rewrites committed asset bytes.

Nothing in the packet changes an asset, and no claim of closure is attached to it.

## Parked

Every dependency is done and the remaining work is one decision nobody has made.
Selecting Option 1 makes the rest a bounded implementation slice: replace
`data != encode_png(image)` in `decode_png` with a scanline comparison plus an
exactly-one-IDAT requirement, and keep the altered-pixel case as its regression
test.
