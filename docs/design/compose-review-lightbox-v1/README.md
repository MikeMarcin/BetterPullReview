# Compose review as the lightbox

Mockups for [BPR-2175](../../issues/30-in-progress/BPR-2175-compose-review-hides-when-navigating.md),
2026-11-14. Not accepted; nothing is built from them until the owner accepts
them.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right; `png/` holds headless renders of its three frames, light and dark,
not screen captures. The window, its tokens and the lightbox (`.lbx`, the
scrim from under the title bar down) are the [Ask v1](../ask-v1/README.md)
page's, where the lightbox was drawn for long agent answers and not chosen.
Content is invented (#42, three pending comments).

| Frame | What it shows |
|---|---|
| `cr-wide` | Finish your review in the lightbox over a 1280 by 800 window: summary and verdict on the left, what the review carries on the right, Discard and Submit at its foot. |
| `cr-narrow` | The same at 900 by 640, in one column. |
| `cr-chrome` | The window's controls while it is open: the title bar is not dimmed; drag, resize, minimise, zoom and full screen keep the lightbox open and re-fit it. |

## The owner's words, verbatim (look v007 of RFC-BPR-0127)

> I feel like clicking off compose review into the diff or drawers or review
> bar or using the mode tabs should hide it. It feels more in the way than
> helpful staying open when i'm trying to navigate the app.

> hmm maybe compose review should use the light box we mocked up but decided
> not to use for big agent responses instead

> Yeah just make sure we can still do basic window controls while lightbox
> stays open like drag window or resize it

## Regenerating

The page was generated from the Ask v1 page's `lbx-agent` frame by a scratch
script that is not kept; edit `index.html` directly. Renders:
`"Google Chrome" --headless=new --user-data-dir=<a fresh scratch directory>
--hide-scrollbars --force-device-scale-factor=2 --window-size=1360,880
--screenshot=png/cr-wide-light.png "index.html?theme=light&frame=cr-wide"`
(980 by 720 for `cr-narrow`).
