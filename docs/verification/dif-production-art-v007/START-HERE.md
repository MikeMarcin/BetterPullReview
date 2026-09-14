# Dif external-generator handoff — d9

This archive is a portable **candidate-making** packet for the `production-art`
milestone in RFC-BPR-0030 (run `dif-takeover-20260914`, dispatch `d9`, producer
`rfc-integrator-d9`, mandate revision 6). It does not contain accepted new
production gestures. The existing three-clip runtime art is a separate older
revision and must not be used as the prop-free identity anchor.

## Use the references in this order

1. **Identity/edit target:**
   `docs/verification/dif-production-art-v001/dif-neutral-master-candidate.png`
   is the exact prop-free neutral accepted for identity at attended session
   `cbd48b91000b00f1e72e1ad62c7c384c`, event 379. Its SHA-256 is
   `bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`.
   `docs/verification/dif-production-art-v002/source/dif-neutral-master.png`
   has pixel-identical 1024×1024 RGBA, compiler-canonical PNG bytes and SHA-256
   `e2820eeff1f1ebf2584b1947a2b2062f98f29b541dd587cc8ce92196c71ab0cf`.
   The first is the accepted exact-byte review subject; the second is its
   canonical source encoding. Do not average identities or substitute a raw
   frame, previous card-bearing mascot, or generated full-body result.
2. **Action guidance only:** ten immutable `docs/rfcs/assets/bpr-dif-integration-v1/assets/incoming/dif-*.png`
   sheets show gesture intent. Their identities and provisional grids are in
   `assets/INPUT-MANIFEST.json`; exact provisional 71-cell rectangles and
   hashes are in `docs/verification/dif-production-art-v002/crop-inventory.json`.
   `preview/contact.png` and `preview/extended-raw-contact.png` index their
   poses. Raw face, proportions, chest band, feet, matte and half-pixel crop
   boundaries are defects, not source authority.
3. **Prompts and timing:** `docs/verification/dif-production-art-v005/PROMPTS.md`
   contains copyable G1/G2/B1/E1–E6 pose edits and common locks. Read
   `docs/verification/dif-production-art-v007/PROMPTS-D9.md` for the later d8
   shoulder failure and current corrections. The ten-clip frame-by-frame
   target, duration and neutral endpoints are in
   `docs/rfcs/assets/bpr-dif-integration-v1/specs/CLIP-CONTRACT.json`.
4. **Diagnostics, not approved poses:** the v004 open-paw and blink studies,
   v005 greeting/E3/E6 donors, and v006 greeting/blink/extended studies are
   labeled by their paths and the archive manifest. The d8 greeting
   `shoulder-root-comparison.png` shows the near-vertical inner arm cut and
   disappearing root wedge; do not reproduce it. The extended comparisons
   show why whole donor bodies cannot replace the accepted neutral.

## Output to make

Make **one pose per transparent PNG**, ideally at the neutral's genuine
1024×1024 source canvas. Keep the same subject scale, visual center and camera;
do not stretch the body or claim native detail by upscaling. The final 256×256
runtime cell targets visual center x=128, alpha-bottom-exclusive baseline
y=223, planted feet within one runtime pixel, at least 16 pixels of outer
clearance, and zero RGB under zero alpha. Use RGBA8 sRGB straight alpha with
clean antialiased edges and no baked background, contact/cast shadow, halo,
prop, text, badge, code or other mascot. Keep neutral head/chin, opposite
limbs, torso, hips and feet exact outside the small authored moving region;
external tools may drift, so preservation is checked locally rather than
assumed from a prompt.

The contract asks for 12-frame local-chest `calm_idle` (250 ms, no blink),
8-frame viewer-right-paw `greet` (125 ms), 1-frame exact `rest`, 6-frame
eye-only `blink` (100 ms), and `compare_glance` (8×150 ms), `consider`
(8×150 ms), `comment_ready` (8×125 ms), `attention` (6×150 ms),
`acknowledge` (8×125 ms), and `blocked` (6×150 ms). `calm_idle` loops with a
distinct final pose and clean seam; the other moving clips play once and
return to neutral. Copy the exact neutral into specified endpoints during
local assembly. Do not generate a replacement `rest` or count a single peak
as a complete clip. Source poses should contain no runtime gutters: the
offline compiler adds one fully blank 256×256 cell at both ends of each
one-row strip.

Submit a ZIP or folder with `poses/<clip-id>/frame-NNN.png` for each proposed
unique frame, using the contract's zero-based indices. Include native editable
inputs/layers when available, a `generation-log.json` naming tool/provider,
model and seed **only if known**, exact prompt per pose, input image roles and
SHA-256, output dimensions/hash, edits, and any external-service terms or
third-party material obligations. Mark gaps and alternatives explicitly.
The project will reassemble, validate, inspect actual 64/72/96-pixel motion
on light `#F6F7F9`, dark `#1C2026` and high-contrast `#000000`, and seek a
focused exact-byte art/derivative decision. Raw and generated candidates are
**not accepted production** merely because a tool returns them.

`MANIFEST.json` lists every archived file with its archive-relative path,
SHA-256, byte count and role. Existing Dif authority covers the earlier
reviewed source; the exact new derivatives and any external generator terms
still need recorded rights/distribution disposition. No model, seed, license
or approval is inferred from the incoming files.
