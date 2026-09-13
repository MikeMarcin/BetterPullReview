# BetterPullReview sprite animation consumer

BPR owns the prop-free purple-and-white panda **Dif**. Use the installed shared
sprite-sheet-animation-updater skill; this file is consumer configuration, not a fork.
Resolve all paths against the BPR checkout; resolve bundled skill scripts against the
absolute installed skill directory. Do not import another product's dimensions/colors.

## Read first

Live AGENTS and nearest Qt adapter guide; RFC-BPR-0030/0031 (or their recorded import
mapping); `src/adapters/qt/assets/dif/README.md`; `dif-sprite-manifest.json`;
`scripts/dif_assets.py`; the new revision's intake/provenance and visual decisions.
The imported pack's source/audit/spec files explain known raw-sheet defects.

## Consumer contract

The final runtime is RGBA8 sRGB, 256×256 cells with straight-alpha sources, one leading
and one trailing fully transparent cell on each one-row strip. Frame i is content
column i+1. The ten IDs and timings are in the current BPR manifest; that manifest and
generated compile-time descriptor, not a directory scan, are authority.

Incoming generated sheets are maintainer inputs, not ready strips. The eight-frame
sheets have half-pixel nominal grids; review exact crop rectangles. Pin one prop-free
neutral master and normalize all identities/handoffs. Retain native editable inputs;
normalized production sources may be 256 or a larger genuine square multiple of 256.
Do not upscale merely to claim source quality. No changed props or body color drift.

Body registration uses feet/hips/head facts separately from moving limb envelopes;
full-alpha-box recentering is not a body-lock algorithm. Keep untouched body pixels
for blinks. Calm idle has no embedded blink; Home schedules the separate blink clip.
No whole-raster squash substitutes for a real nod/wave/breath.

## Tools and review

For generic structural screening, use normalized content-only rows, explicit dimensions
and `--no-body-check`; the shared blue-body detector does not validate Dif. BPR's own
checks must then validate body anchors, actual baselines, envelopes, final blank end
cells, seams and one-shot handoffs. A disabled body check is never acceptance evidence.

Regenerate with the existing commands (resolve absolute paths in attended instructions):

```sh
python3 scripts/dif_assets.py generate --assets src/adapters/qt/assets/dif --out <empty-output-directory>
python3 scripts/dif_assets.py validate --assets src/adapters/qt/assets/dif
python3 scripts/dif_assets.py verify --assets src/adapters/qt/assets/dif
```

Preserve compressor-independent canonical PNG pixel comparison and exact actual-file
output hashes. Do not restore DEFLATE byte equality. Ordinary app builds do not run
these maintainer commands or creative image tools.

Review actual-speed 64/72/96-pixel clips, alpha on real themes, shared neutral handoffs,
loop seam and static rest. Freeze exact derivative/provenance hashes and record only
actual verdicts. Runtime resources contain no incoming sheets, prompts, inspection
plates, scripts, UI text, repository data or other mascots. The normal app uses retained
Qt images/source rects, not QMovie, GIF/APNG, QML or downloaded assets.

Motion/visibility comes from the profile's typed Appearance settings. Stop on reduced,
hidden, inactive, modal-obstructed or teardown; no semantic cue or history playback from
mascot pose alone. Refer to RFC-0031 for exact Home/explicit-AI-slot mappings.
