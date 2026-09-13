# The ten actual image inputs

`assets/INPUT-MANIFEST.json` binds every exact PNG byte, original display filename,
generation ID available in the conversation, dimensions, frame count, and provisional
row-major extraction rectangles. Duplicate tool deliveries have been deduplicated by
source identity; each clip occurs once. No source PNG was altered for this pack.

| Clip | Actual sheet | Layout | Native nominal cell |
|---|---|---|---|
| calm_idle | 1448×1086 | 4×3 | 362×362 |
| greet | 1774×887 | 4×2 | 443.5×443.5 |
| rest | 1254×1254 | 1×1 | 1254×1254 |
| blink | 1536×1024 | 3×2 | 512×512 |
| compare_glance | 1774×887 | 4×2 | 443.5×443.5 |
| consider | 1774×887 | 4×2 | 443.5×443.5 |
| comment_ready | 1774×887 | 4×2 | 443.5×443.5 |
| attention | 1536×1024 | 3×2 | 512×512 |
| acknowledge | 1774×887 | 4×2 | 443.5×443.5 |
| blocked | 1536×1024 | 3×2 | 512×512 |

## Measurements versus impressions

All files contain actual alpha. The three 1536×1024 images also retain RGB values
under zero alpha, which can make an alpha-ignoring preview appear to have a purple
background. Do not classify them as opaque from such a preview. The generated body
pixels are mostly near alpha 253/254; semitransparency alone is not necessarily a
visible defect. Inspect actual compositing before any matte adjustment.

There are low-alpha remnants around some cutouts/boundaries. The generic audit reports
pixel facts, not an approved matte. Remove isolated background fragments without
silently eroding ears, paws or antialiased edges. Canonical production PNGs must zero
RGB under zero alpha and meet the existing PNG structure restrictions.

## Required visual reconciliation

The neutral `rest` source is the **default candidate identity anchor**, because it is
prop-free, closed-mouth, and high resolution. It is not already an approved new master.
Confirm it once against the supplied Dif references and user direction. Then keep one
master across all rows; do not average the ten designs.

Observed review concerns: idle/compare use different resting paws from rest; greet,
blink and compare use open mouths; blink adds a purple chest band not present in the
neutral; proportions/foot placement differ between sheets. Expressive mouth changes
can be intentional, but their entry and return must be animated rather than popping
at a splice. The band is character drift, not an eyelid motion. `calm_idle` includes a
closed-eye frame although the design deliberately schedules blinking separately.

Fix only the affected art and transitions. Use existing pixels for unchanged body
regions when the registration supports it. Preserve the character and gesture; no
whole-sheet redesign by default. The generation output is not binding evidence that
camera/scale/lighting were consistent merely because the prompt asked for them.

## Extraction and normalization

For 1774×887, rounded rational edges are x=[0,444,887,1331,1774],
y=[0,444,887]. These are half-open crop proposals. They produce 443/444-pixel
pieces, not equal integer cells. Inspect each boundary before approving the extraction;
character placement in generated art need not follow the mathematical grid exactly.
Never drop the final column, floor every step, or distort a rectangle into a square.

Extract reviewed rectangles at native resolution, record actual cuts, clean mattes,
and pad into fixed intermediate canvases. Establish one body registration from the
master. Use stable hips/feet/head landmarks; do not center the full alpha box every
frame, since a waving paw changes it. A common clip-scale/placement transform is the
default; exceptional per-frame registration correction must be explicit and inspected.
Reject genuine scale/pose drift that transforms cannot repair without changing meaning.

Retain incoming sheets, cleaned native crops and edit recipes as maintainer/reference
inputs. Canonical shipping source frames may be 256-square for this batch, or a larger
multiple of 256 where genuine source resolution supports it. Retain the 1024-square
neutral master separately. Do not upscale 362/443-pixel cells merely to claim 512-pixel
quality. A 256-square canonical cell is an explicit normalized production source,
not a replacement for the native editing material.

Copy the accepted normalized neutral pixels to `rest`, `calm_idle` representative
frame 0, and all one-shot neutral endpoints. Check the adjacent transitions too:
copying an endpoint is not a repair for an incompatible penultimate frame. The idle
last frame is deliberately distinct but close to frame 0; do not duplicate it to
pass a seam check. Remove the unintended blink from idle so the separate blink policy
is the only recurring eye-close cue.

Any required creative retouch is recorded before source freeze. Once derivatives
are accepted, deterministic regeneration starts from their frozen pixels, not from
model prompt replay. No art-cleanup algorithm runs in Qt paint/decode.

## Skill compatibility

Read the absolute installed `sprite-sheet-animation-updater/SKILL.md` and BPR's
consumer context. Its bundled generic body detector is blue-character-specific;
`--no-body-check` only removes that check, it does not prove Dif's geometry.

Screen **normalized content-only rows** with the generic validator and explicit
layout flags. Do not pass raw fractional grids as valid fixed cells. Do not pass a
BPR strip with a leading blank cell as if the first cell were content. BPR's compiler
adds one fully transparent 256-square cell at each end; BPR-specific tests validate
those final strips, body anchors, seams and handoffs.

The expanded catalog still uses the existing strict source inventory. Do not put
incoming sheets, review images, crop manifests, prompts, or native intermediate crops
into that inventory unless the revised compiler explicitly inventories that separate
source category. Preferred: keep them under a maintainer/art intake path, outside
`src/adapters/qt/assets/dif/`'s production products.
