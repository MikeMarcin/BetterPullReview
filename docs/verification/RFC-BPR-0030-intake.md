# RFC-BPR-0030 incoming asset intake

BPR-0753 audit performed 2026-09-14 against source revision
`d70c595eb15c76bba88dbd8721dd1400eb32ee3e` on `main`.
This mutable ledger records exact intake and corrective work, not production art
acceptance, runtime qualification, or completion of RFC A1/A2 in full.
The core decides. The host performs. Qt presents.

## Source identity and retention

The ten unchanged PNGs remain in the immutable maintainer archive
[assets/incoming](../rfcs/assets/bpr-dif-integration-v1/assets/incoming/).
No input was copied into runtime resources and no derivative was produced.
The [input manifest](../rfcs/assets/bpr-dif-integration-v1/assets/INPUT-MANIFEST.json)
retains original display filenames and every provisional rectangle. Its ten
clip IDs and ten payload hashes are unique: repeated tool delivery does not add
a clip. These are conversation-reported generation IDs, not independently
retrieved tool records. The historical September 12 dump identity remains in
[SOURCE-BASELINE](../rfcs/assets/bpr-dif-integration-v1/references/SOURCE-BASELINE.json);
it is not substituted for the live revision above.

| Clip | Verified SHA-256 | Reported generation ID | Actual pixels; columns×rows; frames |
|---|---|---|---|
| `calm_idle` | `52a2982e4d8b68b436249e96ddc17986fdf000c5782b6aa1e13c485dc97d5723` | `a470e5b2-d935-4d84-bbe4-7e365a6672f6` | 1448×1086; 4×3; 12 |
| `greet` | `1e740dad2a7f4a9f6465421a537d3ce3142525f8dfb2f9722560778c292c0b58` | `15dcfd74-015a-4ee4-a81b-0d7021f3ba6a` | 1774×887; 4×2; 8 |
| `rest` | `eacedea5ddfdc6317b007e2032093f620620777279c70f87250eadd79a4a90ae` | `48c9b65c-9d73-49fe-86c6-2e9cd8e036fd` | 1254×1254; 1×1; 1 |
| `blink` | `e67c8220ec8cd094d16272661b1358ee3f1d3b23ed0dcba019d12abf3424b6a0` | `ec988d23-a896-4a19-825f-f65843404d13` | 1536×1024; 3×2; 6 |
| `compare_glance` | `2838d50c8d6b3bec0f4300f03e439f07b3e8e812aaf0ac4bb2805d3f3e7bdff1` | `9cfffba0-11e1-4818-9151-0a1ff029088d` | 1774×887; 4×2; 8 |
| `consider` | `e7bcad4b7d8be15e7ff5a85663fbbce165a06161cdac883cb7202c2d06ade1d7` | `0fb28868-4740-433b-82cc-6911f19b3913` | 1774×887; 4×2; 8 |
| `comment_ready` | `212929b9b44bd66c587965a2cbf7f7ee4b7529224554dabcb656fa9212eb8865` | `0c5791e3-a37d-4afb-94d3-21deb7916759` | 1774×887; 4×2; 8 |
| `attention` | `4cff8d667eebcab8cf642f32481a39c377841f942a150ec07a76d0e16924e710` | `1c9fbb38-ab5e-436d-b299-c9f98ae5cdd8` | 1536×1024; 3×2; 6 |
| `acknowledge` | `0b426c9cd9633d22a76143181afa2f7c1f6278940d00cdf1fb451c843dca90ef` | `0442f260-81bb-4245-ae0c-f8ea3adb1001` | 1774×887; 4×2; 8 |
| `blocked` | `e0d6cba0d111af520d47f5437088882d8e386b9f1eb947ed9609e0f067b8d542` | `b1cd9de1-5ed5-4c3c-8d1a-cc03f20dfbf2` | 1536×1024; 3×2; 6 |

## Geometry and matte measurements

Decoded dimensions and divisibility were independently checked against the
manifest. `calm_idle` has 362-square integer cells; `rest` is one 1254-square
source; `blink`, `attention`, and `blocked` have 512-square integer cells.
The other five sheets have 443.5-square nominal cells. All 71 intended cells
are accounted for. The future 1024-square neutral master is a derivative
target, not the actual rest input size.

**Every crop is unapproved**, including the integer-grid proposals. For the
five fractional sheets, half-open x edges `[0,444,887,1331,1774]` and y edges
`[0,444,887]` yield widths/heights 443 or 444, including nonsquare pieces.
The manifest records all row-major rectangles. Review each boundary for limbs
and neighboring pixels, then pad rather than stretch; never advertise these
or idle's 362-pixel crops as native 512-pixel artwork.

All ten decode as RGBA and contain actual transparency. All include low-alpha
pixels; every sheet has at least one proposed crop with nonzero perimeter alpha.
The last column below counts affected proposed crops, not confirmed clipped
limbs. Near-opaque material does not by itself establish a matte defect.

| Clip | Maximum alpha | Zero-alpha pixels | Alpha 1–8 pixels | Hidden nonzero RGB pixels | Crops with nonzero perimeter |
|---|---:|---:|---:|---:|---:|
| `calm_idle` | 255 | 694872 | 71888 | 0 | 12/12 |
| `greet` | 255 | 760778 | 41288 | 0 | 8/8 |
| `rest` | 255 | 1003581 | 12379 | 0 | 1/1 |
| `blink` | 254 | 660945 | 40195 | 660945 | 5/6 |
| `compare_glance` | 255 | 760181 | 96142 | 0 | 8/8 |
| `consider` | 255 | 800624 | 33885 | 0 | 7/8 |
| `comment_ready` | 255 | 814303 | 38499 | 0 | 8/8 |
| `attention` | 254 | 759940 | 39161 | 759940 | 6/6 |
| `acknowledge` | 255 | 805689 | 32554 | 0 | 8/8 |
| `blocked` | 254 | 762208 | 44899 | 762208 | 6/6 |

For the three 1536×1024 sheets, every zero-alpha pixel retains nonzero RGB.
An alpha-ignoring purple preview therefore does not prove an opaque background.
Canonical derivatives must zero hidden RGB, retain antialiasing, and remove
reviewed detached remnants/boundary debris without indiscriminate erosion.
The exact per-cell bounds and PNG chunks are in the unchanged
[raw audit](../rfcs/assets/bpr-dif-integration-v1/verification/raw-asset-audit.json),
reproduced exactly by this run.

## Candidate and bounded corrective work

Select `rest` (`eacedea5…a90ae` above) as the default high-resolution,
prop-free, awake, closed-mouth **candidate**. It is not an accepted master.
Agent inspection of the archived
[first-frame light/dark comparison](../rfcs/assets/bpr-dif-integration-v1/review/raw-first-frames-light-dark.png)
and raw idle sheet supports the following diagnosis; it is no human verdict
or real-speed motion evidence.

| Scope | Corrective task and completion evidence |
|---|---|
| Master | Confirm rest against the authorized Dif identity references before retouch; retain the 1254-square original and hash the separate 1024-square master derivative. Do not average differing row identities. |
| `calm_idle` | Remove the closed eyes at zero-based frame 7 (row 2, column 4); preserve awake head/hips/feet and local chest/shoulder breath. Match resting paws to the accepted master. Review the distinct final pose and actual-speed loop seam; do not duplicate the terminal. |
| `blink` | Remove the purple chest-band drift and reconcile the open mouth and paw pose with neutral. Freeze unchanged body pixels and limit motion to eyelids; verify masked-body equality and planted feet. |
| `greet`, `compare_glance` | Reconcile open mouths and resting paw differences with the master; retain the intended wave/glance, with reviewed adjacent entry/return frames rather than pasted neutral endpoints that pop. |
| All ten | Register body/feet/head independently of moving-paw alpha envelopes; address observed proportion and foot-placement differences. Record actual landmarks, enclosing geometry and exceptions; validate normalized x=128 / baseline y=223 target against visible pixels. |
| All native crops | Inspect the explicit unapproved rectangles and matte debris; preserve native crops and exact edit recipes separately from production inventory. Validate actual RGBA composites on light/dark/high-contrast backgrounds after repair. |
| All normalized clips | Hash each derivative and bind timing, endpoints, 64/72/96-pixel playback, theme edges and handoff evidence to that revision. No raw sheet or old card-bearing art is a shipping fallback. |

These are bounded downstream normalization/acceptance tasks, not new issue IDs
or permission to generate art. The intake slice adds no motion implementation.

## Provenance and authority limitations

Reuse the recorded BPR-0287 creator/rightsholder authority and its future Dif
creation/distribution scope in the existing
[provenance record](../../src/adapters/qt/assets/dif/provenance.json).
That record binds the reference hashes and excludes foreign identities. BPR-0291
accepted its exact older derivative subjects; it does not approve these ten new
bytes or future edits by filename. Existing authority is context for a focused
new derivative disposition, not a fabricated new license or operator decision.

The archived manifest is the supplied source/history record. Exact generation
model/version, seed, complete prompts/edit history, and the reference inputs
used for each of these ten generations are not independently established here.
No unknown value is inferred from captions. No derivative or distribution
acceptance has been recorded for this new catalog. The master and final catalog
reviews remain downstream checkpoints, with consolidated completion owned by
BPR-0769; this audit does not close them.

## Reproduction and results

Run from the repository root with Python 3.10+ and Pillow:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 docs/rfcs/assets/bpr-dif-integration-v1/scripts/audit_incoming.py
(cd docs/rfcs/assets/bpr-dif-integration-v1 && shasum -a 256 -c SHA256SUMS)
PYTHONDONTWRITEBYTECODE=1 python3 "$(realpath .agents/skills/sprite-sheet-animation-updater/scripts/validate_sprite_sheet.py)" --sheet docs/rfcs/assets/bpr-dif-integration-v1/assets/incoming/dif-rest.png --cell-width 1254 --cell-height 1254 --columns 1 --rows 1 --animations rest:1 --no-body-check --json
python3 scripts/verify_documentation.py --root .
```

The audit passed all ten hashes. An independent `hashlib.sha256` pass verified
all 55 entries of `SHA256SUMS`; decoded-size/grid arithmetic and unique IDs/hashes
passed. Calling `audit(root)` and comparing its result with the archived JSON
returned exact equality. The installed generic validator returned `ok: true`
for the rest source with explicit 1254-square layout and body detection disabled.
This is a structural screen only: its blue-character detector cannot establish
Dif identity, and raw fractional sheets are not fixed-cell runtime strips.
Documentation gate result is recorded on BPR-0753 after this ledger is written.

Semantic documentation disposition: no impact to public APIs, runtime UI,
authentication, persistence, packaging or architecture behavior; only intake
facts and downstream corrective evidence are added.
