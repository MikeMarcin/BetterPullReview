# RFC-BPR-0030 compiler contract evidence

BPR-0754 implements the offline structural portions of RFC-BPR-0030 A4–A6.
The candidate is `d56a8a565735c2ba000623edb6f37a50147d95ca` plus the owned
compiler/test changes. No production art, source authority, runtime selection,
or visual acceptance changes in this slice.

## Schema selection and limits

The existing [compiler](../../scripts/dif_assets.py) selects schema 1 or 2 from
the canonical production manifest. Schema 1 retains its three-clip descriptors
and shipped assets. Schema 2 admits the ordered ten IDs and exact initial
counts, timings, representative indices, kinds, priorities and return targets
in the immutable [design contract](../rfcs/assets/bpr-dif-integration-v1/specs/CLIP-CONTRACT.json).
Changing those values requires an explicit compiler/contract revision; unused
resource headroom does not authorize silent count or timing changes.

The provenance document independently retains schema 1: its authority, rights,
references and exact source-digest inventory have not changed shape. A schema-2
manifest does not require relabeling that existing provenance schema. Fixture
provenance is synthetic, and confers no authority on any new production art.

There are 71 content cells (18,612,224 bytes), plus twenty transparent gutter
cells: 23,855,104 complete decoded bytes. The hard ceilings remain 76 content
frames, 24 frames/clip, 12 fps, 19 MiB content and 24 MiB complete decoded strips.
Python integers cannot overflow; bounded counts precede pixel arithmetic and
C++ descriptor generation. The 76-frame boundary is exactly 24 MiB including
the gutters. Source PNGs retain the 2048 dimension and 16 MiB file ceilings;
source files are checked before digest reads. Generated PNG decoding has a
separate 6656 dimension ceiling, 24 MiB file/decoded ceilings and then exact
regenerated geometry/pixel comparison. This admits wide strips without
relaxing source dimensions. The neutral master retains the RFC's at-least-512
rule, with the synthetic fixture using the 1024-square target.

## Registration fields and evidence

Schema 2 adds these fields to the existing manifest shape:

| Field | Enforced meaning |
|---|---|
| `registration.anchor_regions` | Four runtime-pixel rectangles named `head`, `body`, `left_foot`, `right_foot`, ordered vertically with separate feet. |
| Per-clip `content_bounds` | Exact union of visible alpha bounds across that clip, within the 16-pixel outer clearance. |
| Per-clip `head_travel_px` | Bounded x/y deviations of the measured head-region alpha edges from the normalized master; at most four pixels. Idle, blink and rest declare zero. |
| Existing `body_bounds` | Common neutral enclosing box, equal to `neutral_body_bounds`; distinct from the moving-limb envelope. |
| Existing `allowed_travel_px` | Zero for schema-2 body anchors. Foot tolerance is separately one pixel. |

Anchor regions are measured against normalized master pixels. Each must contain
substantial visible content and horizontal transparent margins, so a tiny
annotation or a solid interior patch cannot stand in for body registration.
The body region is a locked pelvis/torso band below the animated chest; its
RGBA pixels must remain exact. Each foot must match the master within one
pixel and remain within one pixel of the declared baseline. Neutral body
center must agree with x=128. Head alpha bounds and visible occupancy are
checked separately; idle head pixels remain exact, and blink retains the head
silhouette and every pixel below its head region. Thus a moving paw may expand
the content union without moving a whole-alpha-box center rule.

Every one-shot has exact normalized master entry/return pixels and cannot be
a static-neutral alias. Blink's open-eye tail is exact. Idle starts at exact
neutral and has a distinct terminal frame. Its seam guard compares occupied
pixels rather than transparent padding, with a maximum declared tolerance of
four mean byte units. These are structural guards, not proof of artistic
identity, semantic body landmarks, eyelid-only motion, or smooth playback.
Reviewed region placement and real-size adjacent-frame review remain required
for final artwork. Local changes outside the locked regions can still be
artistically wrong even when the compiler passes.

## Products and portability

Each strip contains one transparent leading cell, every content frame at
`(i + 1) * 256`, and one transparent trailing cell. Schema-2 C++ descriptors
include frame count/timing/kind/priority/return, resource name, cell dimensions,
content origin, strip dimensions, decoded byte count, and a SHA-256 of the
complete row-major RGBA strip including gutters. The manifest digest binds the
metadata. The generated digest inventory separately hashes actual compressed
payloads, including the catalog.

Both `validate` and `verify` compare canonical PNG pixels/metadata and exact
text, then verify the recorded hashes against actual committed payload bytes.
`verify` also compares two clean builds on the current compressor byte-for-byte;
it no longer compares those compressed streams directly with committed PNGs.
Different DEFLATE streams are permitted only with unchanged canonical pixels
and correctly refreshed actual-byte digests. Canonical chunk/filter rules and
exact inventories remain mandatory.

## Focused verification

The [asset tests](../../tests/scripts/dif_asset_tests.py) build disposable
synthetic schema-2 sources using the immutable design JSON as an independent
count/timing oracle. They assert all 71 content frames, every gutter, 22.75 MiB
complete arithmetic, wide-strip descriptors/checksums and C++ compilation.
Negative controls cover displaced body/head/each foot under unchanged
annotations, false solid-interior annotations, empty content, broad envelopes,
non-neutral returns, static aliases, blink body changes, bad/reordered/missing
IDs, timing/count/return errors, wrong gutter metadata/pixels, source limits,
overflow-sized counts, excess decoded budget and stale actual-byte digests.
Compressor variation exercises the full `verify` path.

Commands use `PYTHONDONTWRITEBYTECODE=1`:

- `python3 tests/scripts/dif_asset_tests.py -v` — passed all 55 tests in 41.728 seconds.
- `python3 tests/scripts/dif_asset_tests.py SchemaTwoTests.test_rejects_displaced_body_and_each_foot_with_unchanged_annotations -v` — passed in 7.049 seconds after adding vertical-foot cases and named-anchor assertions; compiler unchanged since the full pass.
- `python3 scripts/dif_assets.py validate --assets src/adapters/qt/assets/dif` — passed.
- `python3 scripts/dif_assets.py verify --assets src/adapters/qt/assets/dif` — passed.
- `python3 scripts/verify_documentation.py --root .` — passed.
- `git diff --check` — passed.

The host used Python 3.14.7 on macOS. The passing source identities are:

- Compiler SHA-256: `81a9164cb8b350ba482a9e881354c0afb32abdfd7e6c838ab6b9927f635b6c70`.
- Final test SHA-256: `849369a748818fadbb0fc205510f8a12364511784fba714e2bbc4c355e91da5f`.

An earlier focused schema-2 iteration passed 10 tests in 105.693 seconds.
The final suite adds budget, envelope and alias controls; direct copying of
already-unfiltered PNG rows avoids redundant per-byte decoding work, with all
five filter modes still covered by the existing round-trip test. No
human verdict is required for this synthetic compiler slice. New master,
rights, normalization and full art/motion acceptance belong to the later
assigned artwork cards and remain open.
