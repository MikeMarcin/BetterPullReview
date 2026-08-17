---
id: BPR-0324
title: Update the application icon source
status: Done
rfc: RFC-BPR-0014
depends_on: [BPR-0253]
---

# Update the application icon source

The product owner supplied `diff-icon-v1.svg` as the replacement source for
the existing cross-platform application icon family.

## Required final state

- Replace the canonical repository SVG byte-for-byte with the supplied source.
- Regenerate every tracked PNG, Windows ICO, and macOS ICNS derivative through
  the existing deterministic icon pipeline.
- Preserve the Qt runtime, Windows resource, macOS bundle, and Linux desktop
  bindings established by BPR-0253.
- Update direct asset evidence so the previous source cannot remain
  accidentally accepted.

## Completion record

- `packaging/icons/better_pull_review.svg` is byte-identical to the supplied
  `diff-icon-v1.svg` at SHA-256
  `3d84ff2090d1dd19ee4b79fd21afb4ced0d1889b26392777cce03b190bd6b88f`.
- The generator replaced all ten PNG sizes, the eight-size ICO, and the Retina
  ICNS. A second run reproduced every derivative digest exactly.
- The asset contract proves the new 1024-square view box, exact source digest,
  PNG dimensions, ICO frames, ICNS entries, and unchanged platform bindings.
- The real Qt application icon probe, system-tray icon contract, macOS
  tests-disabled bundle resource comparison, and production self-test pass.

## Non-goals

- Do not alter the supplied vector geometry, colors, effects, or framing.
- Do not claim target-native Windows or Linux visual acceptance.
- Do not change package signing, installation, or release readiness.
