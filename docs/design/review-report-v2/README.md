# Review report, round 2: the app's own hand (BPR-2219)

The owner, 2026-11-16, handing back look review-report-v001:

> The more I look at it the more this review theme just doesnt really work in
> the app. I know I asked for this but let's see if we can redesign the
> report styling and formatting to feel more native but also still special
> compared with a normal file tab.

`frontend-design:frontend-design` was loaded first. The structure and the
layout accepted in round 1 stay ([round 1](../review-report-v1/README.md), A:
the verdict over the summary, one keep action at each finding's foot). This
round changes only how the page is drawn.

`png/` holds renders made by the report view itself, in the platform's own
style and palette, light and dark, with nothing shown on screen (Cocoa,
`BPR_NATIVE_LOOK=1`; `qt_agent_report_view`'s render mode). The content is
the real agent's review of MikeMarcin/BPR-Test#185.

## What changed for all three

- **Type.** The application's font throughout, at the app's sizes: 14 for
  reading, 15 semibold for a finding's heading, 13 for labels and rows. The
  title is the same font, larger and semibold. The serif is gone.
- **Colour.** The page stands on the pane's own ground in its own ink, so it
  follows the theme as every other pane does. The three verdict colours stay
  the theme's report roles. Code and the action's face sit on a wash a
  little off the ground.
- **Controls.** The keep action is the app's bordered button face.

## The three looks

| | | Light | Dark |
|---|---|---|---|
| Cards | The opening on a raised band, each finding a bordered box | [png](png/cards-light.png) | [png](png/cards-dark.png) |
| Band | The opening on a raised band, findings beside their rules | [png](png/band-light.png) | [png](png/band-dark.png) |
| Plain | Rules and hairlines only | [png](png/plain-light.png) | [png](png/plain-dark.png) |

- **Cards.** The opening (verdict, summary, Keep summary) is one bordered
  band with the verdict's rule at its left and the verdict as a filled mark.
  Each finding is a bordered box with its verdict's rule, as a conversation
  card is: label and place at the top, the reasons, the action at the foot.
  What sets the page apart from a file tab is that it is made of the things
  the reviewer already acts on in this app.
- **Band.** Only the opening is boxed. Findings read as a document with
  rules at the left. Quieter, and closer to a plain page.
- **Plain.** No boxes at all. The closest to a rendered Markdown file, and
  the least special.

## Chosen: Cards

A finding is something to act on, and in this app things to act on are
cards. The box also gives each finding's action an edge to belong to, which
the plain page lacks when findings are long.
