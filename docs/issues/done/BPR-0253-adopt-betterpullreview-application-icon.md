---
id: BPR-0253
title: Adopt the BetterPullReview application icon
status: Done
rfc: RFC-BPR-0014
depends_on: []
---

# Adopt the BetterPullReview application icon

The application had no product icon of its own. Its notification surface could
only inherit a process icon or use the bounded Qt fallback, and no canonical
Windows, macOS, or Linux product-identity assets existed for packaging.

## Required final state

- Retain the supplied pure-vector SVG unchanged as the canonical source.
- Generate and track fixed PNG sizes, a multi-image Windows ICO, and a Retina
  macOS ICNS without requiring an SVG runtime plugin.
- Apply the PNG family to the real Qt application before composition creates
  any window or notification tray.
- Compile the ICO into Windows executables and place the ICNS in tests-disabled
  macOS application bundles with an exact `CFBundleIconFile` binding.
- Retain a Linux desktop entry and hicolor-ready vector/fixed-size family for
  RFC-BPR-0014 packaging without claiming that Linux installation rules exist.
- Prove the canonical source digest, every image dimension/container, platform
  metadata, Qt resource binding, and non-null running application icon.

## Completion record

- `packaging/icons/better_pull_review.svg` is byte-identical to the supplied
  SVG at SHA-256
  `51392db0f0b959f9c1906e801506b084e3d6203be7254cf5a49ef2ed140199bc`.
- The tracked family contains 16, 24, 32, 40, 48, 64, 128, 256, 512, and
  1024-pixel PNGs; the ICO contains the eight Windows sizes through 256; and
  the ICNS contains standard and Retina entries through 1024.
- The real application resource supplies nine fixed sizes through
  `QApplication::setWindowIcon`. The existing production tray therefore
  inherits the BetterPullReview icon instead of reaching its fallback.
- The tests-disabled macOS Release target built as
  `better_pull_review.app`, copied the exact tracked ICNS, and emitted
  `CFBundleIconFile=better_pull_review.icns` plus bundle identifier
  `io.github.MikeMarcin.BetterPullReview`. Its production self-test passed.
- The Windows resource compiled successfully to an x86-64 COFF resource during
  available-host cross-tool inspection. Target-native Explorer/taskbar
  inspection remains package verification rather than being inferred here.
- The focused asset, running-icon, and tray tests passed 3/3. The SVG generator
  was rerun and reproduced every PNG, ICO, and ICNS digest exactly.

## Non-goals

- Do not claim signed, notarized, installable, or self-contained packages.
- Do not add Linux installation paths before RFC-BPR-0014 owns the complete
  relocatable install/package graph.
- Do not change theme, window chrome, notification policy, or application
  architecture.
