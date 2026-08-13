# Dif source asset pack

This directory contains the canonical, owned Dif source bytes for
RFC-BPR-0019. The checked-in PNGs, not a prompt replay, are the source of truth.
`generated/` is the committed deterministic runtime product of these sources,
produced by `scripts/dif_assets.py` under BPR-0290.

## Authority and boundaries

Creation was authorized by the accepted and manifest-validated BPR-0287
attended verdict under
`docs/verification/reviews/dif-provenance-v001/`. That decision authorizes
original reference-informed Dif creation and the stated repository and
distribution uses. It does not provide final source-rights validation, visual
acceptance, runtime composition, package release, or RFC closeout; BPR-0291
retains the final rights and visual gates.

The two supplied Dif images were used only as identity and brand-family
guidance. Neither opaque reference image is cropped, embedded, or shipped. The
Nib archive supplied only process guidance: lock a master, declare explicit
clips, validate geometry, and review real-size motion. No Nib or Mel artwork,
code, names, silhouette, prop, palette, dimensions, timing, or branding enters
this pack.

## Creation record

The initial master was generated with OpenAI's built-in image-generation tool
from the authorized Dif references. The generated chroma-key source had SHA-256
`e50ad520b26e31b7ad42db250ef235d0eddd52bfed97841f3bba779b43582b22`.
The exact prompt was:

```text
Use case: stylized-concept
Asset type: canonical high-resolution 2D sprite master for the BetterPullReview desktop application's mascot
Primary request: Create one original full-body neutral master of Dif, the calm supportive panda code-review companion, suitable as the identity anchor for a small animated sprite pack.
Input images: Image 1 is the authorized Dif identity and expression reference. Image 2 is authorized only to show Dif's relationship to the BetterPullReview brand family; use only the Dif panda from the right side as identity guidance and do not copy or adapt the Nib or Mel mascots.
Scene/backdrop: a perfectly flat solid #00ff00 chroma-key background for later background removal; one uniform color edge to edge, with no floor plane, gradient, shadow, texture, glow, reflection, atmosphere, or lighting variation.
Subject: a single compact panda with rounded proportions, purple ears, purple limbs, purple eye patches and accents, a pale neutral lavender-white face and body, open attentive eyes, and a small calm warm smile. Dif holds a small rounded rectangular diff card centered at mid-torso; the card contains only abstract plus, minus, and short neutral-line shapes in deep purple and muted coral, with no green and no semantic approval/error state.
Style/medium: polished soft 3D-style illustration rendered as a clean 2D sprite master; subtly plush and dimensional but with a crisp stable silhouette and no wispy fur.
Composition/framing: straight-on neutral pose, complete body and both feet visible, feet level on one shared baseline, visual center fixed on the panda body rather than the prop, generous transparent-removal padding on all sides, centered in a square canvas. Keep ears, feet, hands, and card comfortably away from every edge.
Lighting/mood: neutral soft frontal studio lighting confined to the character; calm, focused, friendly, and supportive.
Color palette: rich BPR purples, pale lavender-white body, charcoal-black eyes, tiny muted coral accent; absolutely no #00ff00 or similar green anywhere in the subject.
Constraints: original Dif artwork only; readable facial expression and silhouette at 64, 72, 96, and 256 pixels; no text, letters, numbers, usernames, code, patch lines, badges, conclusions, logos, watermarks, UI, or extra objects. No cast shadow, contact shadow, ground glow, halo, or reflection. Keep the subject fully separated from the key background with crisp antialiased edges.
Avoid: Nib's blue television body, antennae, magnifying glass, geometry, palette, or identity; Mel's lotus silhouette, petals, glow, palette, or identity; the flattened group composition; green markings on the prop; exaggerated waving or semantic approval/error expressions.
```

The installed ImageGen chroma-key helper removed the uniform background with
border sampling, soft matte, despill, transparent threshold 12, and opaque
threshold 220. ImageMagick 7.1.2-29 then resized the master to 1024 x 1024 and
derived baseline-anchored source poses. The checked-in `scripts/dif_assets.py`
PNG codec rewrote every source as canonical non-interlaced 8-bit RGBA with
zero RGB beneath zero alpha.

Motion is deliberately minimal and deterministic:

- `calm_idle`: three baseline-anchored vertical scales (1018, 1024, and 1020
  source pixels high) produce one gentle closed breath without a duplicated
  terminal frame;
- `greet`: neutral, restrained 1032 x 1008 and 1040 x 1000 squash poses,
  recovery, and the exact calm representative produce one nonsemantic
  acknowledgement and byte-exact handoff;
- `rest`: the awake neutral master is a static reduced-motion fallback.

## Maintainer commands

Generate a proposed product tree into a new empty directory:

```sh
python3 scripts/dif_assets.py generate \
  --assets src/adapters/qt/assets/dif \
  --out <empty-output-directory>
```

Validate the complete source/generated inventory and reproduce the committed
tree twice in isolated temporary directories with:

```sh
python3 scripts/dif_assets.py validate --assets src/adapters/qt/assets/dif
python3 scripts/dif_assets.py verify --assets src/adapters/qt/assets/dif
ctest --preset dev -R '^dif_assets_contract$' --output-on-failure
ctest --preset core-only -R '^dif_assets_contract$' --output-on-failure
```

Do not hand-edit generated strips or the descriptor. Change checked-in source
bytes and the manifest, regenerate into an empty directory, rerun the asset
contract, and reopen the digest-bound BPR-0291 review.
