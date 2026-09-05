---
schema: "implement-rfc.card.v1"
id: "BPR-0289"
title: "Create the original Dif source frame pack"
status: "Done"
rfcs: ["RFC-BPR-0019"]
depends_on: ["BPR-0287","BPR-0288"]
---

# Create the original Dif source frame pack

Create the locked neutral master and the smallest coherent `calm_idle`, `greet`,
and `rest` source sequences under the authority recorded by BPR-0287. Source
bytes become canonical; a model prompt is never the regeneration authority.

## Required implementation

- Preserve the purple-and-light panda identity, abstract diff-card prop,
  supportive expression, transparent background, and 64-pixel readability.
- Exclude Nib's television, antennae, magnifying glass, palette, geometry, and
  motion, plus Mel and AllTheThings identity or flattened composition.
- Lock a transparent neutral master of at least 512 x 512 pixels and declare
  the common center, baseline, body box, alpha encoding, and color space.
- Create no more frames than needed for one gentle closed idle, one restrained
  greeting that returns to idle, and one awake static rest pose.
- Record exact creation/edit tools, prompts or manual steps, input hashes,
  output hashes, creator, and notice disposition in provenance.

## Completion evidence

- Every source frame passes the BPR-0288 structural and geometry validator.
- Source images contain no opaque plate, baked text, private path, metadata,
  repository content, credential, GitHub mark, or review conclusion.
- The manifest declares every frame explicitly and admits no directory scan or
  filename-derived runtime behavior.

## Implemented source pack

The canonical source pack lives at `src/adapters/qt/assets/dif/`:

- one canonical 1024 x 1024 neutral master;
- three `calm_idle` frames using baseline-anchored 1018/1024/1020-pixel
  vertical scales for a closed breath without a duplicated terminal frame;
- five `greet` frames using a restrained baseline-anchored squash and exact
  recovery to the calm representative;
- one awake static `rest` frame;
- one exact three-clip manifest and a provenance inventory that hashes every
  source byte and reproduces the BPR-0287 creation-only authority boundary.

Every PNG is canonical non-interlaced 8-bit RGBA at 1024 x 1024, has actual
transparency and transparent corners, carries only `IHDR`/`IDAT`/`IEND`, and
stores zero RGB beneath zero alpha. The runtime resize reports a shared center
of 128, baseline 223 within one pixel, body bounds `[56,22,200,224]`, calm seam
mean delta 1, and greet handoff mean delta 0.

Creation inputs, exact prompt, tool versions, chroma-key source digest,
transformation method, authority, distribution scope, notice disposition, and
foreign-identity exclusions are recorded in the asset-local README and
`provenance.json`. The supplied opaque references and Nib archive remain
outside the repository.

## Verification evidence

- Two fresh empty output directories produced byte-identical generated trees
  with `python3 scripts/dif_assets.py generate --assets
  src/adapters/qt/assets/dif --out <empty-output-directory>`.
- `python3 tests/scripts/dif_asset_tests.py`: 18 tests passed.
- The source-only preview course generated every contact sheet, registration
  overlay, appearance context, 64/72/96-pixel declared-speed clip, calm seam,
  greet handoff, and reduced-motion comparison without changing source bytes.
- A scoped private-path/credential scan returned no match, documentation
  validation returned `DOCUMENTATION_OK`, and `git diff --check` passed.
- An independent read-only `rfc-scout` reproduced source-only generation and
  preview, inspected the source hygiene and visible nonsemantic direction, and
  found no BPR-0289 mechanical/source blocker.

These mechanical results close the source-creation slice; they do not decide
final source rights or visual acceptance. BPR-0291 retains both attended gates,
and BPR-0290 owns the committed generated products and byte-for-byte verifier.
