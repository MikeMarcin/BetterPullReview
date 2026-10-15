---
schema: "implement-rfc.card.v1"
id: "BPR-1528"
title: "Give Home's My pull requests section its own header and rail colour"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-15","owner_note":"Can we add a header and rail color for my reviews on the home","governing_rfc":"RFC-BPR-0066","related_cards":["BPR-1388","BPR-0776","BPR-0783"],"reservation":"docs/issues/_reservations/BPR-1528--BPR-1528--cdb07d8484c2.json","requested_skill":"frontend-design"}
---

# BPR-1528: Give Home's My pull requests section its own header and rail colour

The owner asked on 2026-10-15: "Can we add a header and rail color for my
reviews on the home". "My reviews" is Home's **My pull requests** section: the
owner's own words when RFC-BPR-0066 created it were "the reviews where i am
the author". It is the only state section on Home with a neutral header and
no rail. Needs me (amber), Waiting on others (blue) and Following (green)
each draw a tinted header and a rail in their state's colour.

Governing design: [RFC-BPR-0066](../../rfcs/RFC-BPR-0066-home-authored-pull-requests.md),
which chose the neutral header "because its rows can have different states".
This card is the owner's amendment of that one choice, recorded on the RFC and
in the [Home specification](../../ux/workbench-specification.md#home). Row
anatomy, row state colours, section membership, counts, collapse and order are
unchanged.

Home's section colours are built-in per appearance variant and are deliberately
neither stored nor customisable (`builtin_attention_color`, [[BPR-0776]]); the
Appearance page exposes none of them. The new colour follows the same pattern:
a fourth built-in Home tone per variant, with no new setting or storage.

## Acceptance

- AC1: With State headers on, the My pull requests panel draws a tinted
  header and a rail in its own colour, at the same strengths as the other
  state panels; inside an Organization, Repository or Account group its band
  carries the same rail. Name, count, collapsed summary and keyboard behaviour
  are unchanged.
- AC2: The colour is a fourth built-in Home tone for each of Light, Dark,
  High Contrast Light and High Contrast Dark. It differs from the other three
  Home tones and reads as small text at 4.5:1 or better on its variant's base
  and surface, like them. It follows the resolved appearance variant exactly
  as the other tones do.
- AC3: Row reason words and marks keep their existing state colours; Group
  None with State headers off, the other sections and Recently viewed are
  unchanged.
- AC4 (human): The owner accepts the exact frozen candidate's Home, shown
  before and after in offscreen Light and Dark renders.

## Implementation

Load frontend-design:frontend-design before choosing the colour. Qt presents:
the change is the Qt adapter's built-in tone table and the panel's tone for
the My pull requests group; no core, host, provider or storage change. Never
drive or capture the owner's desktop; renders are offscreen. Visual judgement
goes through a fresh short attended-review session and a frozen build.
