---
schema: "implement-rfc.card.v1"
id: "BPR-0800"
title: "Dif rests in the bottom-left corner on review surfaces too"
status: "Backlog"
rfcs: ["RFC-BPR-0031", "RFC-BPR-0032"]
depends_on: []
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"owner mandate r2, 2026-09-15; the remainder BPR-0775 bounded out and d8 left un-carded; allocated on main in d9","acceptance":["RFC-BPR-0031 B5","RFC-BPR-0031 B6"]}
---

# BPR-0800: Dif rests in the bottom-left corner on review surfaces too

## Outcome

The owner's direction of 2026-09-15, verbatim:

> fwiw regarding Dif I want him to act mostly like Nib from AllTheThings. He can
> show up on the home page for the first time login experience, but otherwise he
> should mostly sit in the very bottom left corner and stay out of the way.

**Half of that shipped.** [[BPR-0775]] reconciled it with RFC-BPR-0031 for
**Home**: signed out — the first-time experience — Dif keeps RFC-BPR-0031's
landing placement at 96 px beside the card's sentence, and on every other Home,
signed in and Home over a review, the same single widget rests at 40 px over
`workbenchDifCornerSlot`, the first widget of the status bar, with no pointer or
focus path, hidden while the status bar shows a transient message. The owner
accepted that on 2026-09-16 (`bpr-0779-dif-corner`, decision 123f144b).

**"Otherwise" is more than Home.** A reviewer spends their time on the review
surface, and there Dif is not in the corner: RFC-BPR-0031 places him in the AI
tools slot (B6) and hides him elsewhere. So the direction's own word — *mostly* —
is not met yet for the surface the application is mostly on. BPR-0775 bounded
this out deliberately, as window-level work rather than Home work, and recorded
it as a follow-up for main to allocate. d8 named it again and left it un-carded
as scope creep at a closeout boundary. This card is the home it never had.

## Acceptance

- On the review surface, the same single Dif widget rests at the corner slot at
  the leading edge of the status bar, at the size Home already uses, out of the
  way: no pointer path, no focus path, no space taken from a control, and hidden
  while the status bar carries a transient message.
- The one-sprite rule holds. There is never a Dif in the corner and a Dif in the
  AI tools slot at the same time, in the same window or across windows, and the
  existing single decoded retained catalog is still shared (RFC-BPR-0031 B10).
- **What happens to the AI tools slot is the open question and must be answered
  before any code is written.** RFC-BPR-0031 B6 requires one explicit visible AI
  tools slot that never overlays code or shared comments; B5's "all other
  surfaces hide it" courses assert the opposite of this card. Either the corner
  replaces the slot on review surfaces, or the slot keeps its cues and the corner
  is Dif's resting place between them. Whichever it is, say why, and carry the
  answer into RFC-BPR-0031 rather than leaving the courses to contradict it.
- RFC-BPR-0031 is `Auto-Verified`. Its accepted criteria are not rewritten to fit
  an implementation; an amendment naming the owner's direction is the honest
  path, exactly as RFC-BPR-0032's Home section already records the Home half.
- The rendered corner on a review surface gets its own attended verdict, one
  question for this criterion alone, as the Home corner did. Nothing about how it
  reads at 40 px beside live review text is inferred from an offscreen course.

## Evidence

Record the focused command or gesture, the direct surface, the result, the
producing revision and the remaining limitation. Evidence is **not run** at card
authoring: nothing here has been implemented or observed.

Where the existing evidence lives, for whoever takes this:

- The Home half and its reconciliation: [[BPR-0775]] "Decisions — d3, mandate
  r2", and RFC-BPR-0032's Home section, "Dif on Home (owner direction,
  2026-09-15)".
- The courses that would change: `qt_workbench_window_scale_100/125/200`
  `test_dif_moves_to_the_explicit_ai_area_without_owning_it`, and RFC-BPR-0031's
  B5 and B6 courses.
- The owner's acceptance of the Home corner: attended session
  e966c88f3320638208adf4fcf0651e31, candidate `bpr-0779-dif-corner`, decision
  123f144b, bound to executable sha256 `d500ef7a…` from source `ed2b3f14`.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference
is authorized by this card. The core decides, the host performs, Qt presents: a
mascot is presentation and reaches no store, provider or process. No new sprite
work: this is placement, not identity — sprite revision is RFC-BPR-0030's. The
Appearance page's Dif controls are [[BPR-0790]] and are not reopened here.
