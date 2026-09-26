---
id: "RFC-BPR-0019"
title: "Dif Visual Identity, Sprite Assets, and Animation Contract"
status: "Auto-Verified"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0006","RFC-BPR-0013"]
pack_version: "post-v2"
---

# RFC-BPR-0019: Dif Visual Identity, Sprite Assets, and Animation Contract

## Status and scope

**Auto-Verified on 2026-09-26 through [[BPR-0295]].** The
[final ledger](../verification/RFC-BPR-0019.md#final-reconciliation--2026-09-26)
binds the unchanged v008 asset set, the exact AC1 amendment below, supplied
independent review dispositions, fresh selected graph/build evidence and final
documentation checks. RFC-BPR-0006/0013 and all earlier canonical RFCs are
Auto-Verified. RFC-0030/0031 govern their accepted clip, resource, regeneration
and runtime deltas; the original stage scope and criteria below are preserved.
Recorded creator/tool unknowns remain disclosed. This closes no other RFC and
supplies no installed-package or new native visual acceptance.

This is the first of two ordered deferred post-pack RFCs for the BetterPullReview
mascot. It establishes an original, reviewable, deterministic asset system for
**Dif**, a panda code-review companion. RFC-BPR-0020 consumes that contract in
the Qt presentation adapter and real application.

This RFC creates no runtime widget, timer, application composition, or product
status behavior. Its terminal result is a BPR-owned, provenance-complete source
pack and reproducible sprite catalog that later runtime work can consume
without creative inference.

The work may begin after RFC-BPR-0014 closes or under an explicit post-pack
implementation-frontier exception. Auto-Verified closeout still requires every
front-matter dependency to be Auto-Verified and every attended and provenance
gate in this RFC to be resolved.

**Owner amendment, 2026-09-25 (attended session 06ad5734): AC1's provenance
for the v008 asset set.** Asked on [[BPR-0295]] whether to accept the recorded
provenance unknowns or supply them (candidate `BPR-0295-ac1-provenance-v001`,
decision `d7af6bd8`, 04:32:59Z), the owner selected, verbatim, "Amend AC1 for
this exact v008 asset set: accept the recorded unknowns as disclosed gaps",
and noted "It was ChatGPT 5.6 Sol that generated the images, but I also don't
care." For the v008 asset set as it stands at `3759b0d8`, and for no other
bytes (`provenance.json` `f77354d6…6f1c`, sprite manifest `c9f3f38e…f40d`,
v008 verdict `57a2a6d8…e116`, and the 72 source files that match the accepted
intake), AC1's complete ownership/license and tool-provenance record is met
with these gaps disclosed: the creator's identity; the generator's provider,
model, seed and version; the full prompt and input-role history; and the
generator service's terms. The rights basis, distribution scope, notices and
the absence of foreign or private material are unchanged and already evidenced.
AC1's wording below is unchanged; this changes acceptance, not observations.
The generator name in the note is the owner's statement and is not recorded in
`provenance.json`, whose bytes the amendment binds. Terminal reconciliation of
all seven rows is complete on [[BPR-0295]]; the amendment remains scoped to
the exact unchanged v008 bytes.

## Source material and adaptation boundary

The following product-owner-supplied material informed this RFC. It is planning
input, not checked-in product content or acceptance evidence:

| Source | SHA-256 | What it contributes | What it does not authorize |
|---|---|---|---|
| `dif_ref_sheet.png`, 1448 x 1086 opaque PNG | `cf35d96198e20575069b3dd43edd352a04393e457970219738a9e888458729c0` | Dif's panda identity, calm purple-and-white direction, code-review prop vocabulary, expression study, scale study, and product tone | Direct use as a sprite, proof of transparent source layers, or a license/assignment |
| `mascot_group.png`, 1536 x 1024 opaque PNG | `5859655aa4c39e50a7e3c522e48f8b34c48bd5749553089be95860e7f2d0f39e` | Dif's place in the BetterPullReview brand family | Use of Nib, Mel, AllTheThings identity, or the flattened composition in BPR |
| `nib-animation-bootstrap.zip` | `e924c69865ff2998fed3bc5981a405c46960a9ef561947ce64b99fa52e54aeef` | Process guidance: locked masters, deterministic overlays, explicit clips, geometry checks, reduced-motion review, and real-speed previews | Import of Nib code, Swift/AppKit architecture, artwork, names, dimensions, palettes, clip metadata, or singleton design |

The archive contains no runtime image assets and no license or provenance
record. Its `160 x 96` Nib cell and AppKit implementation are
AllTheThings-specific. BetterPullReview adopts none of them.

The repository must record the final Dif creator, ownership or license grant,
permitted derivative and distribution scope, generation or editing tools,
source hashes, and third-party notice disposition before any mascot raster is
shipped. Product-owner possession of a reference image is not substituted for
that record. A structural validator can prove that required fields and bound
hashes exist; it cannot prove that the asserted rights are true. Rights
resolution therefore also requires a dated operator decision or underlying
license/assignment evidence bound to the exact reference and production-source
hashes, permitted uses, derivative scope, distribution scope, and notice
obligations.

## Problem

The concept sheet expresses a coherent character but cannot be used as a
runtime asset. It is flattened onto an opaque background, combines unrelated
layouts and captions, contains no registered frame sequence, and supplies no
loop, timing, transparency, geometry, provenance, or resource-budget contract.

Starting Qt work directly from that image would couple creative decisions to
runtime code, encourage hand-edited strips, and make frame drift, accidental
foreign identity, excessive decoded memory, and unverifiable regeneration
likely. A visually plausible screenshot would not prove that the shipped
frames are original, correctly registered, bounded, reproducible, or safe to
package.

## Goals

- Establish Dif as an original BetterPullReview panda mascot with a locked
  canonical neutral master and an approved silhouette, face, palette, prop,
  and tone.
- Produce transparent, consistently registered source frames and deterministic
  generated sprite strips for a deliberately small first clip set.
- Define clip identity, timing, loop/one-shot behavior, priority, representative
  frames, return targets, motion envelopes, and accessible meaning in one
  versioned manifest.
- Make ordinary application builds consume committed generated outputs without
  invoking image generation, network access, or nondeterministic tooling.
- Validate dimensions, alpha, gutters, registration, body bounds, loop seams,
  budgets, manifest integrity, and output hashes mechanically.
- Require attended art-direction and motion approval against the actual assets
  at their intended size and speed.
- Preserve enough high-resolution source material to revise Dif without
  resampling an already packed sprite strip.

## Non-goals

- Implementing a Qt mascot widget, frame timer, reduced-motion observer,
  application setting, or public application path.
- Replacing the BetterPullReview application icon, wordmark, or platform
  package identity.
- Copying or adapting Nib, Mel, AllTheThings code, art, poses, clip names,
  dimensions, timing, palette, or branding.
- Presenting Dif as an AI agent, reviewer, GitHub actor, approval authority, or
  source of review findings.
- Shipping reviewing, thinking, issue-found, approving, or commenting states
  merely because the concept sheet illustrates them. A future semantic cue
  requires its own truthful product mapping.
- Generating art during CMake configure, compilation, application startup, or
  packaging.
- Claiming signed, notarized, installable, or self-contained packages.

## Dif identity contract

Dif is a calm, focused, supportive panda companion for code review. The first
approved master must retain these distinguishing traits:

- a compact panda silhouette with purple ears, limbs, eye patches, and accents;
- a light neutral face and body that remain legible without relying on the
  background shown in the concept sheet;
- a small diff card using plus, minus, and neutral-line shapes as a review prop;
- a friendly, attentive expression without implying that Dif performed a
  review, approved a change, or discovered a defect;
- a readable outline and facial expression at 64 logical pixels, with the
  canonical composition optimized for the 72-96 logical-pixel landing use;
- no visual element derived from Nib's blue television body, antennae, or
  magnifying glass, and no element derived from Mel's lotus silhouette.

The application inherits native appearance. Dif may have a stable brand
palette, but every frame must remain distinguishable on BPR's System, Light,
Dark, and high-contrast surfaces. A hard-coded background plate, glow needed
for separation, forced application palette, stylesheet, or text baked into the
sprite is prohibited.

## Canonical asset pack

Implementation should place the owned source and generated material under a
single Qt-adapter asset root, expected to be
`src/adapters/qt/assets/dif/`, with the following logical contents:

```text
src/adapters/qt/assets/dif/
  README.md
  provenance.json
  dif-sprite-manifest.json
  source/
    dif-neutral-master.png
    calm-idle/frame-*.png
    greet/frame-*.png
    rest/frame-000.png
  generated/
    dif-calm-idle.png
    dif-greet.png
    dif-rest.png
    dif-sprite-catalog.hpp
```

Exact generated filenames may change during implementation, but the ownership
and separation may not: editable high-resolution sources remain distinct from
committed deterministic runtime products. The neutral master is transparent
RGBA at no less than 512 x 512 pixels. Runtime frames use transparent RGBA
cells of 256 x 256 pixels. The master and source frames may be larger; the
generator performs the only shipping resize.

Ordinary builds do not require the art-generation or editing toolchain. A
documented maintainer command regenerates all committed products from the
source frames in an empty output directory, and a verification command proves
that the result is byte-for-byte identical to the checked-in generated pack.

## Initial clip contract

The first catalog is intentionally small:

| Clip | Kind | Required behavior | Runtime meaning available to RFC-BPR-0020 |
|---|---|---|---|
| `calm_idle` | persistent loop | Gentle breathing/blink motion with a closed loop and no translation that makes the body appear to walk or float | Decorative presence only |
| `greet` | one-shot | One restrained acknowledgement, then an explicit return to `calm_idle` | Landing surface became visible; not a semantic application result |
| `rest` | one static frame | Calm, awake representative pose with no procedural motion or fade | Reduced-motion and suspended fallback |

Each manifest clip records a stable ID, strip resource, cell size, frame count,
integer frame duration in milliseconds, loop/one-shot kind, representative
frame, priority, return target, baseline, body bounds, allowed per-frame travel,
and accessible description. The initial catalog contains no weighted random
idle variants. Later fidgets require deterministic selection, a declared
maximum frequency, and a separate reviewed asset change.

The catalog admits at most 48 content frames in total. With 256 x 256 RGBA
cells, content pixels are bounded to 12 MiB. The two fixed transparent gutter
cells on each of the three strips add at most 1.5 MiB, so complete decoded strip
pixels are bounded to 13.5 MiB before Qt/container overhead. No clip exceeds 24
content frames or 12 frames per second. A generated strip has one row, a
transparent one-cell gutter at both ends, and no unreferenced frames.

These are maximums, not targets. Motion should use the fewest frames that looks
intentional at shipping size and speed.

## Registration and motion rules

- All frames share one declared ground baseline, visual center, and neutral
  body box. A frame cannot silently recenter Dif around a moving prop.
- The feet or seated base remain within a two-runtime-pixel tolerance after the
  generator scales to the 256-pixel runtime cell, except for an explicitly
  declared intentional-travel interval.
- The calm loop's first and final displayed poses match within the manifest's
  pixel-difference and body-bound tolerances; a duplicated terminal frame must
  not create a visible dwell.
- Transparent gutters contain no nonzero alpha. No frame touches a cell edge.
- Alpha is straight or premultiplied according to one declared generator
  contract; mixed encodings are rejected.
- Frame content contains no text, repository data, usernames, credentials,
  GitHub marks, review conclusions, notification badges, or network-derived
  material.
- The diff-card prop remains readable as abstract plus/minus structure but
  carries no literal patch, line number, score, or approval state.

## Deterministic generation and manifest integrity

The asset generator is a checked-in, offline, noninteractive tool. It validates
source dimensions and alpha before resizing, uses a pinned and documented
resampling/color-space contract, composes each strip in manifest order, emits
the compile-time catalog, and writes a digest inventory. It neither calls a
model nor contacts a network service.

Creative image generation or manual painting may be used before a frame enters
`source/`. That step is deliberately outside deterministic regeneration and is
recorded in `provenance.json`. Once approved, the source bytes are the
authority. Re-running a generative prompt is never the way to reproduce a
released asset.

The manifest and generated catalog share one schema version and digest. The
catalog contains no filesystem path and no runtime JSON parser requirement.
RFC-BPR-0020 compiles the strips as Qt resources and consumes the fixed
descriptor. A stale descriptor, missing resource, changed frame, unknown clip,
invalid return target, cyclic one-shot return, or exceeded bound fails the
asset gate.

## Visual review contract

The implementation run uses the attended-review workflow after all mechanical
asset checks pass. The review batch is digest-bound to the source frames,
manifest, generated strips, and preview renderer and contains:

- the canonical neutral master on Light, Dark, and checkerboard backgrounds;
- every source frame and every generated cell at 1x pixels;
- full clip previews at declared speed, at 64, 72, and 96 logical pixels;
- calm-loop seam and greet-to-idle handoff previews;
- System/Light/Dark/high-contrast context plates at 1.0x, 1.25x, and 2.0x;
- the `rest` frame beside the representative frame of every animated clip;
- body-bound, center, baseline, and intentional-travel overlays;
- a direct identity check that Dif is coherent with the reference direction
  without reproducing Nib, Mel, or AllTheThings identity.

The batch asks one exact choose-one/approve decision per subject; it does not
turn competing variants into independent pass/fail questions. Any frame edit,
timing change, resize algorithm change, or manifest change invalidates the
affected verdict digest.

## Security, privacy, and distribution

- Source and generated assets contain no metadata that exposes prompts,
  credentials, usernames, private paths, repository content, or transient
  review data. Required provenance is stored in the explicit record.
- No SVG script, remote reference, embedded profile fetched at runtime, font,
  executable payload, or active content enters the raster pack.
- The validator rejects decompression-bomb dimensions, manifest overflow, and
  unexpected ancillary chunks rather than trusting filenames.
- The provenance record states whether a license notice or SBOM entry is
  required. RFC-BPR-0014's package rules remain authoritative for final
  distribution metadata.
- Product art is not copied into test logs or transient screenshots committed
  as proof. Checked-in product assets and digest-bound attended evidence have
  different purposes.

## Performance and resource limits

- Source and runtime dimensions, frame count, strip width, file bytes, decoded
  bytes, clip count, and frame rate have hard validator limits.
- The generated catalog admits exactly one initial asset family and three
  clips. A provider, plugin, repository, theme, or remote response cannot add
  assets or frames.
- Runtime strips are suitable for one decode at application composition; no
  per-frame filesystem read, image slicing, scaling, or color conversion is
  required.
- Generated previews and contact sheets are maintainer artifacts, not linked
  into the application target.

## Prohibited designs

- Cropping frames directly from either supplied opaque concept image.
- Importing the Nib Swift type, operation enum, singleton controller, timer,
  source generator, or `160 x 96` cell contract.
- Hand-editing generated strips or their descriptor without changing source
  frames and regenerating.
- Using one giant atlas with undeclared frames, variable cell sizes, or runtime
  packing.
- Treating file order, filename parsing, or a mutable directory scan as the
  clip catalog.
- Downloading mascot assets, models, fonts, or metadata at build or runtime.
- Encoding review truth, error severity, approval, or agent activity only in a
  pose or animation.
- Approving a contact sheet while omitting real-speed, real-size, loop, and
  appearance-context review.

## Acceptance criteria

1. The checked-in neutral master and all source frames are original Dif assets
   with a complete ownership/license and tool-provenance record plus a
   digest-bound rights decision or underlying license/assignment evidence; no
   Nib, Mel, AllTheThings, private-repository, or credential material is
   present.
2. A clean offline regeneration reproduces every committed strip, generated
   catalog, and digest byte for byte from the checked-in source frames.
3. The versioned manifest defines exactly `calm_idle`, `greet`, and `rest`,
   with valid timing, representative frames, priority, return, geometry,
   accessibility, and bounded-resource fields.
4. Every runtime cell is transparent 256 x 256 RGBA, all gutters and body-bound
   rules pass, and the admitted catalog stays within 48 content frames, 12
   frames per second, 12 MiB content pixels, and 13.5 MiB complete decoded
   strips including the six fixed gutters.
5. Mechanical loop and handoff checks pass, and the digest-bound attended
   review approves Dif's identity, motion, registration, appearance contexts,
   shipping scales, calm-loop seam, greet-to-idle handoff, and reduced-motion
   `rest` frame.
6. The ordinary developer, core-only, and tests-disabled application graphs do
   not run art generation or require its tool dependencies; core-only remains
   free of mascot assets and Qt.
7. Documentation names the canonical source, generated products, regeneration
   command, review procedure, limits, provenance, and RFC-BPR-0020 consumption
   boundary without claiming runtime composition.

## Verification matrix

| Claim | Direct surface | Command or interaction | What proves the claim |
|---|---|---|---|
| Source pack is owned and original | `provenance.json`, source tree, license/notice ledger, digest-bound rights packet or source agreement | Validate required provenance fields/hashes and foreign-identity exclusions, then validate the exact owner/licensor decision against those bytes and permitted scopes | A filled JSON assertion alone cannot pass; missing authority, scope, tools/inputs, notices, or foreign material fails |
| Generation is reproducible | Maintainer generator in an empty temporary output | Regenerate twice and compare complete tree hashes with committed products | Any nondeterminism, hand edit, stale descriptor, or environment-dependent output disagrees |
| Clip contract is exact and bounded | Manifest, generated descriptor, strips | Run schema/resource validator and compile the descriptor consumer | Unknown/missing clips, invalid returns, excessive frames/rate/bytes/dimensions, or digest drift fails |
| Geometry and alpha are stable | Every generated frame | Inspect alpha gutters, baseline, center, body bounds, edge clearance, travel declarations, and loop seam | A drifting, clipped, opaque, or discontinuous frame fails with exact coordinates |
| Art direction is accepted | Digest-bound attended review batch | Review all frames, shipping-speed clips, scale plates, appearance contexts, overlays, and handoff | Exact per-subject verdicts bind approval to the reviewed bytes |
| Runtime dependencies do not leak inward | `core-only`, tests-disabled graph, dependency scan | Configure/build both graphs and inspect selected sources/resources/dependencies | Qt assets or art tools in core-only, runtime downloads, or generator execution fails |

Every closeout row must be expanded with the exact command, result, evidence
location, and proving commit. A static contact sheet alone cannot satisfy any
motion claim.

## Eligibility gates

Implementation planning may begin only when:

- the product owner supplies or confirms the ownership/license disposition for
  the Dif references and approves creation of derivative production assets;
- the implementation run records its immutable RFC base and creates live
  issue cards, a verification matrix, a documentation ledger, an attended
  review card, a simplification card, and a closeout card;
- RFC-BPR-0014 is closed or the product owner records an explicit post-pack
  implementation-frontier exception; and
- no concurrent card has reserved the relevant issue IDs or asset paths.

## Suggested implementation decomposition

1. Plan the RFC, settle asset paths/schema/limits, record the immutable base,
   and build the direct verification matrix.
2. Resolve ownership/license provenance and create the locked neutral master.
3. Produce the three bounded source-frame sequences and compile the versioned
   clip manifest.
4. Implement deterministic strip/catalog generation and mechanical validators.
5. Generate the digest-bound attended review batch and record the verdict.
6. Verify build-graph isolation, documentation, notices, and exact regeneration.
7. Simplify the asset tooling and run an independent ordinary RFC closeout
   review before status change.

## Documentation impact

Required documentation covers Dif identity, asset ownership, source/generated
separation, manifest schema, clip meanings, regeneration, bounds, visual review,
security/privacy, license/SBOM disposition, and the strict no-runtime claim.
The documentation ledger must explicitly review Qt resource ownership, core-only
isolation, appearance/high-contrast expectations, build/package inputs, and
agent instructions for future mascot edits.
