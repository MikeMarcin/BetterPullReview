# Dif v002 offline source studies

Producer: `dif-takeover-20260914`, dispatch `d4`, mandate `r2`, bounded
BPR-0755 source preparation and BPR-0756 diagnosis. This is **partial artwork
preparation**, not a ten-clip production catalog, completed card, visual verdict,
or runtime selection. The shipped three-clip source tree remains untouched.
The core decides. The host performs. Qt presents.

## Retained candidates and exact provenance

The human accepted the neutral identity in session
`cbd48b91000b00f1e72e1ad62c7c384c`, decision
`6eabb190-287d-4e7c-a19f-bd14f4075701`, sequence 379, fingerprint
`1bcdc33cbd9175db6ee843f60f90e3a70e9800470a19562bd4814b18d600ad81`.
That decision covers the unchanged [v001 original](../dif-production-art-v001/dif-neutral-master-candidate.png),
SHA-256 `bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`.

The new [canonical master](source/dif-neutral-master.png) has SHA-256
`e2820eeff1f1ebf2584b1947a2b2062f98f29b541dd587cc8ce92196c71ab0cf`.
All 1024×1024 decoded RGBA pixels equal the accepted original. Only PNG encoding
changes: the original has twelve IDAT chunks; the compiler requires one.
This payload is not represented as the exact-byte reviewed candidate. Its new
payload digest belongs in the eventual complete-catalog review.

[prepare_sources.py](prepare_sources.py) verifies the accepted neutral and all
ten immutable input hashes before preparing their derivatives. It invokes the
existing compiler's PNG encoder and fixed-box reduction, not a replacement
shipping compiler. NumPy and Pillow are offline preparation dependencies only.

- `source/rest/frame-000.png`: 256-square exact box reduction, followed by zeroing
  RGB beneath zero alpha. Legacy `resize_box` leaves 492 nonzero RGB channel
  values when a small nonzero alpha sum rounds to alpha zero. The integrator added
  schema-2 `clear_rounded_transparency=True`; these cells equal that reduction
  exactly. Schema 1 remains unchanged. The cleanup changes no alpha or
  nonzero-alpha pixel. The source frame is canonical.
- `source/calm_idle/frame-000.png` through `frame-011.png`: 12 frames at 250 ms.
  The accepted 1024 master receives a smooth local vertical displacement confined
  to x=242..782, y=580..792. The peak is two runtime pixels; the head, eyes,
  hips and feet remain unchanged. The displacement sequence is
  `0, .5, 1, 1.5, 2, 1.5, 1, .5, 0, -.5, -1, -.5` runtime pixels.
  Fixed-box reduction and hidden-RGB cleanup follow. This is a newly authored
  local breath using accepted pixels, **not** a claim that raw idle cells were
  registered successfully. It avoids raw frame 7's blink and differing paw pose.
  Shoulder/chest texture motion still needs full-catalog human review.
- No `source/greet/` exists. The contact sheet's greeting is a rejected offline
  experiment: isolate the neutral right arm with the explicit polygon in the
  recipe, clone adjacent belly pixels under it, rotate around its shoulder, then
  reduce. This creates a visible joint discontinuity and fails the required
  cheek-height wave. It must not be promoted, aliased or counted as completed art.
  Only its recipe, contact and row previews remain, not eight rejected source files.

The 256-square cells do not claim fictitious native 512-pixel input quality.
The genuine 1024 master remains separately editable. No model call, prompt
replay, remote mutation, source-sheet overwrite or shared-skill change occurred.

## Native crop preparation and remaining retouch

[crop-inventory.json](crop-inventory.json) binds all 71 original row-major crop
rectangles to source and reproducible crop hashes. The recipe can materialize
these crops with `--write-native-crops`; they are omitted by default because
the immutable input sheets already retain all pixels. Crops preserve native
width/height, including 443×444 or 444×443 fractional-grid pieces; there is no
anisotropic resize. Only hidden RGB is cleared. Rectangles remain provisional:
contact-sheet inspection is not approval of every boundary or matte remnant.

[The extended contact](preview/extended-raw-contact.png) shows every raw extended
frame at a common maximum cell size. All seven contain genuine distinct actions,
but none is a normalized production sequence. Uniform scale/translation cannot
resolve mismatched anatomy or reconstruct occluded neutral parts.

| Clip | Observed content and next offline operation |
|---|---|
| greet | Raw frames 2–5 contain the real raised/waving paw and pads. The open mouth and smaller resting hands differ from neutral. Isolate the genuine limb, retouch its shoulder/belly occlusion at the master scale, then construct smooth adjacent entry/return frames. The rejected rotation study is insufficient. |
| blink | Raw eyelids close, but the open mouth, raised paws and purple chest band differ. Extract/retouch eyelids into the master's unchanged eye patches; keep all remaining master pixels fixed. Raw frames 2 and 3 are both closed, while the contract calls for frame 3 reopening. |
| compare_glance | The raw head turns noticeably with open mouth and different paws; the contract requests at most about two degrees. Use restrained eye motion and a local head edit; whole raw-head replacement would change identity. |
| consider | The left paw reaches under chin and the head tilts, with different resting hands and proportions. Isolate hand/forearm and head motion, reconstruct exposed torso, and preserve neutral feet. |
| comment_ready | Both paws present outward. Raw body/paws are smaller than the master. Retouch both shoulder joints and newly exposed belly; independent body registration is required. |
| attention | A real viewer-right raised paw is present. Isolate the limb and reconcile material/size and the shoulder seam; preserve master head, opposite arm and feet. |
| acknowledge | The raw nod includes a closed-eye smile and conspicuous head/body compression. Retain a shallow local head articulation with open neutral expression and planted unchanged body, rather than normalizing a whole-character squash. |
| blocked | The raw head tilts while an upturned paw extends. Separate head/paw edits and reconstruct shoulder contact with the master; do not translate the whole silhouette to center the extended paw. |

These operations are feasible with careful offline illustration/layer retouch;
a new model run is not logically required or authorized. This slice did not
produce trustworthy masks/reconstructed pixels for those operations. The next
worker needs ownership of this staging tree and focused illustration work,
not relaxed geometry checks or an idle alias. Full BPR-0755/0756 acceptance
remains open.

## Verification and review boundary

[checks.json](checks.json) records the canonical/accepted master hashes, exact
master pixel equality, exact box-plus-hidden-RGB cleanup, frame bounds, pixel
change regions and adjacent-frame changed-pixel counts. Every idle cell has
alpha bounds `[47,22,210,223)`, fully transparent outer clearance, zero hidden
RGB, exact head/eye rows 0..144 and exact hip/foot rows 198..255. Frame 0 equals
rest; frame 11 differs. These are objective facts, not proof of appealing motion.
The shared validator also passed 12 explicit 256-square cells with
`--no-body-check`; the sample-character detector is not used as identity evidence.
No final padded strips, catalog manifest, generated descriptors or runtime
resource selection were created, so final generator/integration gates remain due.

[The offline preview](preview/index.html) includes an APNG with exactly twelve
250-ms frames and a static-rest toggle. It shows 64/72/96-pixel sprites over the
actual built-in application surfaces `#F6F7F9`, `#1C2026`, and `#000000` from
`appearance_profile.cpp`. Agent inspection of the actual-size static theme plate
found readable faces and planted silhouettes; low-alpha edge appearance is
still a final art decision. Browser policy rejected the `file://` preview and
prohibited workaround access. Browser-observed playback is therefore **not
verified**; the animation's decoded frame count/timings and static plates were
checked locally. Native Qt presentation, DPR behavior and human motion acceptance
are not established by these media.

At integrator direction no new partial attended session was created. The next
human checkpoint remains the complete ten-clip digest-bound review required by
RFC-0030; the accepted neutral decision is preserved. The installed attended-review
skill was inspected after invocation-tool discovery found no callable skill API.

Reproduce from any working directory (Python 3.10+, Pillow, NumPy):

```sh
PYTHONDONTWRITEBYTECODE=1 python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v002/prepare_sources.py
PYTHONDONTWRITEBYTECODE=1 python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v002/verify_and_preview.py
```

`python3 scripts/verify_documentation.py --root .` passed with
`DOCUMENTATION_OK` on this preparation revision.

The optional native-crop materialization uses the first command with
`--write-native-crops`. Hashes in [SHA256SUMS](SHA256SUMS) bind retained artifacts.
Semantic documentation disposition: no change to public APIs, architecture,
UI behavior, persistence, authentication, ordinary builds or packaging.
