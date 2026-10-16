---
schema: "implement-rfc.card.v1"
id: "BPR-1544"
title: "Markdown reader GitHub parity: details, sanitised HTML and emoji shortcodes"
status: "Backlog"
rfcs: ["RFC-BPR-0036"]
depends_on: ["BPR-1543"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner decision 2026-10-16: close the GitHub-parity gaps in PR description and comment Markdown, as two sequential cards (this is card B, after BPR-1543)","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 1 (2026-10-16)","criteria":["AM1-05","AM1-06","AM1-07","AM1-08"],"related_cards":["BPR-1543","BPR-1545","BPR-1548"],"reservation":"docs/issues/_reservations/BPR-1543--BPR-1544--05e6fe38f3d4.json","design_skill":"frontend-design:frontend-design"}
---

# BPR-1544: Markdown reader GitHub parity: details, sanitised HTML and emoji shortcodes

Card B of [RFC-BPR-0036 owner amendment 1](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md#owner-amendment-1--2026-10-16-github-parity),
after [[BPR-1543]].

## Scope

- `<details>`/`<summary>`: a collapsible section, closed by default and
  keyboard accessible. If a true collapsible block is infeasible in
  `QTextDocument`, the reader's own block model carries it; the choice is
  decided and documented here.
- GitHub's sanitised HTML subset: `br`, `sub`, `sup`, `kbd`, `b`, `strong`,
  `i`, `em`, `u`, `s`/`del`, `code`, `pre`, `p`/`div` with `align`, `a href`
  (only through the existing core admission), `hr`, `blockquote`,
  `ul`/`ol`/`li`, `h1`–`h6` and table tags. Everything else stays literal or is
  dropped per a documented policy. Never a resource load, script, style, event
  attribute, form or frame. `<img>` and `<picture>` belong to
  [RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md): leave its
  hooks, do not implement them.
- Emoji shortcodes (`:+1:` and the rest of GitHub's common set) from a
  generated static data file; no runtime fetch.

## Acceptance

- **AC1 (AM1-05):** details render closed, open and close by keyboard and
  pointer, and are announced as collapsible.
- **AC2 (AM1-06):** the subset renders with its meaning; other HTML is literal
  or dropped as documented; hostile-HTML security tests prove RFC-BPR-0036
  AC-03 still holds.
- **AC3 (AM1-07):** shortcodes render from the generated table; unknown and
  coded ones stay literal.
- **AC4:** gates and the preserved reader rows, as on [[BPR-1543]].
- **AC5 (AM1-08), owner:** one Accept/Reject per visible construct on a
  frozen build beside GitHub's rendering in Light and Dark.
