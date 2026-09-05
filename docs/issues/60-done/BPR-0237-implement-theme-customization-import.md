---
schema: "implement-rfc.card.v1"
id: "BPR-0237"
title: "Implement semantic theme customization preview and safe import"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0236"]
---

# Implement semantic theme customization preview and safe import

RFC-BPR-0006 requires semantic colors to be edited, previewed, persisted, and
imported safely. The current application offers only the predefined appearance
profiles; there is no palette-customization surface and no path to
`core::import_theme`.

## Required final state

- Present semantic-role customization without exposing raw widget palettes as
  product authority.
- Preview changes against the selected comparison before committing them.
- Persist and restore customized roles through the appearance settings port.
- Import versioned theme data through `core::import_theme`, rejecting unknown,
  malformed, unsafe, and contrast-invalid input with typed explanations.
- Prove round-trip export/import, forward-version refusal, fresh-install System
  defaults, and no geometry/semantic-state reset for color-only changes.
