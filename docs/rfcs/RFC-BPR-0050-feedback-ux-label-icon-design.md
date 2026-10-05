---
id: "RFC-BPR-0050"
title: "Feedback UX and Label Icon Design and Delivery"
status: "Proposed"
area: "BPR"
milestone: "Post-tour feedback conventions"
depends_on: ["RFC-BPR-0047", "RFC-BPR-0006", "RFC-BPR-0036", "RFC-BPR-0037"]
pack_version: "conventional-comments-v1"
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "proposed_id_requires_live_reconciliation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# RFC-BPR-0050: Feedback UX and Label Icon Design and Delivery

## Design mandate

Design and deliver the actual optional feedback controls, compact label visuals,
overview counters and resolution presentation. The user deliberately did not
select a widget layout, a set of twelve glyphs or a particular resolved/unresolved
control in the discussion. This RFC assigns that work; this pack contains no
approved mockups, icon artwork or preselected metaphors.

The semantic requirements in RFC-BPR-0047–0049 are fixed product constraints.
Actual spatial layout, control shape, icon metaphors, sizing, tooltips, overflow,
compact-versus-expanded treatment and visual treatment of resolved counts must be
selected from concrete design candidates during this workstream. Do not treat
example language in other RFCs as an approved UI specification.

## Surfaces and obligations

| Surface | What the design must make clear |
| --- | --- |
| Local comment/reply editor | Plain versus labelled feedback; all twelve choices; decorations; exact outgoing body |
| Review submission | Native Comment / Approve / Request changes; own published and pending blocking feedback without automatic verdict |
| PR Overview file table | Optional dedicated labels; catchall as remainder; counting unit, numeric state and coverage |
| Settings | Column choice/order and authoring/default-count preferences, without a new settings subsystem |
| Compact thread/list/card/header | One consistent label icon vocabulary; root versus reply annotations; resolution distinct from label |
| Full selected thread | Readable Markdown/source, native suggestion content, public versus local classification |

Design against the current imported tour-enabled application. Do not reintroduce
a clipped sidebar composer, duplicate full thread bodies, excessive nested
scrolling, the old small Overview description reader, or a new navigation mode
solely to expose a label selector. Preserve the existing dedicated composition,
Read/Discuss relationships and native Markdown component from RFC-BPR-0036/0037.

## Decision work before implementation

Prepare a small useful set of alternatives for the actual surfaces, rather than
a large speculative design system. Use realistic synthetic mixed-label reviews.
Demonstrate at least no dedicated columns, a typical small subset, and all twelve;
zero, one and multi-digit counts; unresolved-only and all-state views; partial
coverage; plain comments; long source bodies; a labelled reply under a differently
labelled root; native suggestions; and tour comments.

Compare compact icon headers with a discoverable textual identification route,
resolved/unresolved presentation choices and how column activation drills into the
correct conversation population. Record tradeoffs and one selected design with
its decision authority in the live RFC-linked design record. If project policy or
an explicit owner criterion requires a human verdict, use the existing shared
attended-review path, retain partial decisions and park only that design-dependent
work. Do not invent a new mandatory approval ritual for every small control.

Freeze only the decisions needed by the corresponding implementation cards. Core,
fixtures, host contracts and independent work can proceed before visual selection.
A chosen mockup is not runtime completion; the chosen design must later be built
and exercised in the real application.

## Twelve reusable icon roles

Create one distinguishable compact role for each catalog label, reused across
overview headers and actual review threads. `feedback.label.<id>` in the catalog
is a semantic role, not an instruction to use a particular picture. No supplied
symbol, color, animal or metaphor is approved by this pack.

Choose geometry and stroke treatment that remain legible at the actual smallest
shipping size and supported scale factors. Validate the full set together, with
special attention to related labels such as suggestion/polish and
nitpick/quibble. Do not silently collapse them to one indistinguishable semantic
choice. An icon can share family traits, but each role must remain identifiable
with its supported accessible/textual presentation.

Use existing appearance tokens and the actual Qt asset pipeline. Prefer compact
native vector/path resources where appropriate, with recorded rights/provenance
and deterministic packaging; select the concrete resource form in the design
record. Do not bundle fonts, require an icon-font install, fetch images at runtime,
or introduce WebEngine. No image generation occurs in ordinary builds or the app.
This task is unrelated to Dif mascot art and must not replace or alter its assets.

Label and resolution must not share an ambiguous visual encoding. Color is
supplementary, never the only distinction. A resolved Issue remains visibly an
Issue when included. A non-blocking Note is not painted as resolved merely because
it is informational. Public intent and private defect severity need distinct
ownership cues; the same red icon must not imply both without explanation.

## Accessibility and body fidelity

Icon-only headers require meaningful accessible names and keyboard-discoverable
help. A tooltip alone is not sufficient for keyboard/screen-reader identification.
Textual fallback and source remain available. Count accessible descriptions carry
label, unit and state, including uncertainty when applicable. Do not depend on
hover or color to explain catchall membership or excluded unknown-state items.

Use the same label meaning in overview, thread cards and the full detail. A compact
badge may visually substitute for a recognized prefix only when the label remains
unambiguous and View/Copy source retains the original body. Never remove the prefix
from published GitHub Markdown or replace it with a BPR-only icon/HTML marker.
A native suggestion's actionable meaning and code whitespace must survive the
compact and expanded presentations.

Exercise tab order, focus restoration, activation, sort and filter feedback,
small windows, large text, keyboard-only operation and the inherited editor's
selection/undo behavior. Existing editing actions must remain reachable. Avoid
layout shifts that move a caret or selected thread during refresh.

## Delivery and evidence

The design record includes candidates, selected treatment and rationale, a
complete twelve-role inventory, source/resource provenance, and the actual
application surfaces that consume each role. Record unresolved decisions as open,
not as implementation discretion disguised as an accepted design.

The implementation composes through the existing Overview model/delegate,
conversation cards/detail, review editors and Settings host. Capture and inspect
actual rendered application states at representative 100%, 125% and 200% scales,
light/dark/high-contrast themes and narrow/wide windows. Reuse unchanged target
coverage and the project's supported native lanes; do not call a mockup, source
scan or offscreen geometry test a native visual verdict.

Retain measured and observed evidence for readability and interaction. Where a
required target or human check is unavailable, keep that exact criterion open.
Continue unaffected semantic and host work rather than blocking the whole pack.
No screenshots or artwork are generated as part of authoring this RFC archive.

## Design record

The live design record is the
[Conventional Comments delivery contract](../ux/conventional-comments-feedback.md)
([[BPR-1081]], 2026-10-05). It binds the owner's selections from
[BPR-1079](../design/BPR-1079-feedback-ux-selected.html) and
[BPR-1080](../design/BPR-1080-label-icons-selected.html) to the surfaces and
the Qt adapter unit that build them. It also marks each rule as owner-selected,
existing or derived, and lists what is still open. That record satisfies R30
for the design-dependent cards. R31 to R33 are earned by the cards that build
the surfaces.

## Acceptance criteria

- R30: A dedicated, recorded UX/art decision precedes design-dependent implementation; no chat sketch or pack placeholder is treated as approved art.
- R31: All twelve consistent icon roles have compact, accessible, theme-aware presentation and textual identification without color-only semantics.
- R32: Compact visuals preserve public Markdown/source, native suggestion meaning and distinct public/local ownership.
- R33: The chosen surfaces work in the real application across representative sizes/scales/themes and input modes without reviving prior composition problems.

The detailed visual specification becomes a live implementation deliverable of
this RFC. The pack deliberately locks behavior, not the pixels.
