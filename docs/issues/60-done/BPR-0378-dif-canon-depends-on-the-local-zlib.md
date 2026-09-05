---
schema: "implement-rfc.card.v1"
id: "BPR-0378"
title: "The Dif asset canon depends on which zlib the interpreter was built with"
status: "Done"
rfcs: ["RFC-BPR-0019"]
depends_on: ["BPR-0408"]
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

## Corrected — 2026-08-19

An independent review broke the first version of this analysis, and it was right.
The correction matters more than the measurement it replaces, so it comes first.

### `decode_png`'s byte equality does not defend against a hand-edited sprite

The card's own framing says the point of `verify_assets` is "that nobody
hand-edited a sprite". That is true of the tree as a whole and **false of the term
under discussion**. Measured directly: change one channel of the master, re-encode
it through the tool's own `encode_png`, and

| Check | Verdict on the edited sprite |
|---|---|
| structural check | accepts |
| **today's byte-equality term** | **accepts** |
| a scanline comparison | accepts |

`data == encode_png(decode_png_bytes(data))` asks whether a file is in the tool's
canonical *serialization*, not whether its pixels are the committed ones. It cannot
ask the latter: `decode_png` runs on the source master and every source frame,
where the only input is the file's own bytes and no expected image exists.

The first measurement reported a rejection because it compared the altered file's
scanlines against scanlines derived from the *committed* asset held in memory. That
is a comparison the canon site cannot make, and it is the provenance mechanism
wearing the canon's name.

### What does defend against it

`provenance.json` records a sha256 per source file and `_validate_provenance`
(`scripts/dif_assets.py:612-620`) raises `provenance source digest mismatch` when
the file on disk disagrees. The master is `3a52a8b1…`; the edited file is
`af2357e0…`. **No option in this decision touches that check.** The card's second
required bullet is therefore satisfied by a mechanism that is not changing, rather
than by anything the replacement must prove.

### Byte equality's marginal coverage is three things, not one

The withdrawn analysis said the only thing byte equality adds is "your DEFLATE
matches mine". Measured, it adds three, and two are not about DEFLATE at all:

| Variant | Structural check | Same scanlines | Only byte equality rejects it |
|---|---|---|---|
| level-6 recompression | accepts | yes | yes — the zlib-ng case |
| non-empty IEND payload | accepts | yes | yes |
| IDAT split in two | accepts | yes | yes |

`decode_png_bytes` permits a non-empty IEND payload and any number of consecutive
IDATs, so those two properties were only ever held by the byte comparison.

**Consequence for Option 1:** a replacement that merely stops comparing compressed
bytes silently loses them. It has to assert *exactly one IDAT* and *an empty IEND
payload* explicitly. And it must not be written as a scanline comparison, which
would be vacuous — `_filtered_rgba_rows` emits filter byte 0 for every row, so it
can only ever agree.

### The decision, restated

Option 1 still looks right, for a corrected reason: it trades an exact-serialization
pin, which is environment-dependent and was never the pixel-provenance guarantee it
was taken for, against portability — while the property everyone actually cares
about stays with the provenance digests either way.

The packet at
`docs/verification/reviews/dif-canon-portability-v001/offline-package/index.html`
presents the three options as one choose-exactly-one question. Batch identity is
`bpr:/attended-review/betterpullreview/568e2cf7d36599fa83d41f43`. Each subject binds
`scripts/dif_assets.py`, the measurement script, and its recorded output —
deliberately not this card, since binding a card to a verdict recorded on it would
force a rebuild on every edit. Option 3's mechanical audit is `fail`: it fixes this
host, breaks the next, and is the only option that rewrites asset bytes, which also
means re-recording every provenance digest.

The measurement script resolves the repository root from its own location rather
than hard-coding a path, so it can be re-run on another host.

## Parked

Every dependency is done and the remaining work is one decision nobody has made.
Selecting Option 1 makes the rest a bounded implementation slice: replace the
byte-equality term in `decode_png` with the existing structural check plus an
exactly-one-IDAT and empty-IEND requirement, and cover all three variants above as
regression cases.

The failure does not reproduce on this host, whose interpreter links stock zlib
1.2.12, and no asset changed.

## Operator verdict — option 1 — 2026-08-19

**Selected: option 1, compare decoded content, not compressed bytes.**

Given as direct operator direction in session, in response to the three options
presented as a choose-exactly-one question. Recorded verbatim:

> 1. Compare decoded content

### What this verdict is, and what it is not

It is a decision on the option set. It is **not** a validated batch export:
`docs/verification/reviews/dif-canon-portability-v001/offline-package/` still has
no `human-verdict.json` and no `verdict-validation.json`, and batch
`bpr:/attended-review/betterpullreview/568e2cf7d36599fa83d41f43` remains formally
unanswered.

That gap is recorded rather than papered over, and the reason it is acceptable
here is worth stating rather than assuming. The batch bound
`scripts/dif_assets.py`, the measurement script, and its recorded output because
the *measurement* needed exact identity — the question was which bytes this
host's zlib produces. The decision the operator just made is not about those
bytes. It is a choice about what the canon should assert, it is reversible, and
its consequence is a bounded change to a check that this card already specifies
in full. Nothing about it depends on the digests the batch pinned.

If a formal export is wanted for the record, it can be produced against the
existing page without redoing any measurement. That is a separate action and
this card does not require it.

### What the verdict settles

Per this card's own Parked section, option 1 makes the rest a bounded slice:
replace the byte-equality term in `decode_png` with the existing structural check
plus an exactly-one-IDAT and empty-IEND requirement, and cover all three recorded
variants as regression cases.

It also carries this card's own constraint at `:72-73`, which the verdict does
not relax: a hand-edited sprite must still fail, and that must be **shown** on a
deliberately altered pixel rather than assumed.

The card returns to `20-ready/`: the decision has arrived, and what remains is
implementation.

## The canon term is replaced — 2026-08-20

`decode_png` no longer compares the file against a fresh `encode_png` of its own
pixels. `decode_png_bytes` gained `require_canonical_form`, which `decode_png`
passes and nothing else does, so the general decoder still accepts all five legal
filters for the round-trip test while a source file must be in the form the
generator writes.

### Byte equality was holding four properties, not three

The card's corrected analysis listed three. Measured directly, there is a fourth,
and it is the one most easily lost in silence:

| variant, same pixels | general decoder | old byte-equality | new canonical check |
|---|---|---|---|
| canonical (control) | accepts | accepts | accepts |
| level-6 recompression | accepts | REJECTS | **accepts** |
| IDAT split in two | accepts | REJECTS | REJECTS |
| non-empty IEND payload | accepts | REJECTS | REJECTS |
| **row 0 filtered with Sub** | accepts | REJECTS | REJECTS |

The **general decoder** column is the negative control the card asked for: it is
what the canon would have become had the byte comparison simply been deleted, and
it accepts every one of them. So all three retained checks are load-bearing rather
than decorative.

The fourth property is filter canonicality. `decode_png_bytes` accepts filter
types 0 through 4, `_filtered_rgba_rows` only ever writes 0, so a file that
filtered one row with Sub decoded to identical pixels and was rejected only by the
byte comparison. It is now rejected explicitly. That the committed tree actually
holds this property was checked rather than assumed — all 13 PNGs under
`src/adapters/qt/assets/dif` use filter 0 on every row, have exactly one IDAT, and
have an empty IEND payload.

The `level-6 recompression` row flipping to `accepts` is the trade the operator
chose, stated plainly rather than buried.

### A hand-edited sprite still fails, and this records which check does it

The card's second required bullet is met, by the mechanism its own corrected
section identified rather than by anything that changed here.
`test_an_edited_pixel_is_caught_by_provenance_not_by_the_canon` edits a pixel,
re-encodes through `encode_png`, and asserts both halves: the canon **accepts**
the edited file, because it is perfectly canonical, and `_validate_provenance`
rejects it on the per-file sha256. Writing it as one test keeps the two from
being confused again, which is the confusion the card's first analysis made.

### Verification matrix

| claim | surface | command | proves it | result | evidence |
|---|---|---|---|---|---|
| the suite passes, with four new cases | `tests/scripts/dif_asset_tests.py` | `python3 tests/scripts/dif_asset_tests.py` | it exercises the canon through `compile_assets` | **39 tests, OK**, was 35 | this commit |
| the registered contract passes | `dif_assets_contract` | `ctest --test-dir build/core-only -R '^dif_assets_contract$'` | it runs the same suite plus the live boundary check on the real tree | 1/1, 25.74 s | this commit |
| each retained check is load-bearing | `decode_png_bytes` with and without the flag | the table above | the general decoder accepts all three variants; only the flag rejects them | pass | this commit |
| a recompression that changes no pixel is now accepted | same | same table | this is the property being traded away | accepts | this commit |
| the committed tree really is canonical | all 13 PNGs | chunk walk over each file | one IDAT, empty IEND, filter 0 on every row | 13/13 | this commit |
| no committed asset byte changed | Git | `git status --short` | only the script and its tests are modified | 2 files | this commit |
| the dev application is untouched | the bound binary | `shasum -a 256 build/dev/.../better_pull_review` | a relink would invalidate three pending operator packets | `7b9d6be9…a78fcd93`, unchanged | this commit |

### The first required-final-state bullet is NOT met, and this card stays open

> The canon holds on an interpreter linked against either zlib or zlib-ng

It does not, and the fix above is not why. There is a **second** byte-equality
term the card never found: `validate_assets:1089` compares the committed
`generated/` tree against a fresh `generated_products(root)`, which re-encodes
every product through the same `zlib.compress(level=9)`.

Measured against the real tree with this fix already in place, substituting a
different DEFLATE:

```
committed generated products are stale:
  ['dif-calm-idle.png', 'dif-greet.png', 'dif-rest.png', 'dif-sprite-digests.json']
```

Three are PNGs the chosen principle answers directly. The fourth is not:
`dif-sprite-digests.json` hashes the **compressed** payloads, so it is itself a
function of the local zlib and needs an answer the operator has not been asked
for.

[[BPR-0408]] carries it, with that measurement and three options. This card now
depends on it and moves to `10-backlog/` rather than closing, because closing it
would claim a bullet that is demonstrably false — which is the exact failure this
card was opened to correct one level down.

## Closed — 2026-08-20

[[BPR-0408]] is done, and with it this card's first required-final-state bullet
is now true rather than aspirational. Re-checked against the live tree:

| bullet | status |
|---|---|
| the canon holds under either zlib, and says which property it is asserting | **met.** `validate_assets` succeeds with the generator's level 9 substituted by level 6 and by level 1 — it previously failed both. `decode_png_bytes`' docstring states what canonical form now means and why the byte comparison was dropped |
| a hand-edited sprite still fails it | **met**, by `_validate_provenance`'s per-file sha256, which no part of this work touched. `test_an_edited_pixel_is_caught_by_provenance_not_by_the_canon` asserts both halves so the two checks cannot be confused again |
| `dif_assets_contract` passes, and is not made to pass by weakening the check | **met.** 42 tests, up from 35 across the two slices. Every check that replaced byte equality has a negative control that isolates it: the bare decoder accepts all four variants at the source site, and the two generated-tree rejection tests re-stamp the digest first so only the comparison under test can fire |

No committed asset byte changed in either slice, and
`docs/verification/reviews/dif-final-v001/review-identity.json`'s binding of
`generated/dif-sprite-digests.json` still matches the file on disk.

### What this card is worth remembering for

Not the zlib difference. The card's own first analysis said byte equality
defended against a hand-edited sprite; an independent review falsified that, and
the corrected version listed three properties the comparison was really holding.
Implementing it found a fourth — filter canonicality — and then a second call
site the card had never looked at, where the same fault lived unnoticed.

Each of those was found by measuring rather than by reading the previous
sentence, and each one would have shipped as a silent loss.
