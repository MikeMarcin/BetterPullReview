# BPR-0100 native visual-acceptance operator packet

Status: **pending operator judgment; not RFC closeout**

This packet presents BPR-0100's owner-attended judgment of the assembled
workbench's presentation quality on the real macOS composition: native Cocoa
and QMacStyle, a real display, real focus, and the recorded review rather than
synthetic content. Automation proves identity, wiring, and measured contrast;
it cannot decide whether the result looks right. This packet is the terminal
identity replay RFC-BPR-0005's closeout requires; an acceptance feeds that
closeout but grants no live or package authority and does not close
BPR-0114 or RFC-BPR-0006.

## Why v005 exists

The owner accepted v004 against executable `sha256:10899f24…181b28`. That
exact response and its validation record remain immutable historical
evidence. Thirty-five source commits have landed since v004's head, six of
which change what a person sees, so the identity-bound v004 judgment cannot
be widened to the closeout bytes. This successor asks for the same visual
judgment against the executable produced from committed source HEAD
`fc46b5d5`, at the RFC-BPR-0005 closeout replay whose automated gates —
full dev matrix, core-only matrix, whole-tree strict tidy, format,
documentation — are all green at these bytes.

New since your v004 sitting, worth deliberate attention:

- per-pane **line-number gutters** (BPR-0455) and the **View menu** with the
  Line numbers toggle (BPR-0457) — the menu merges into the macOS system
  bar, which no automated check could see;
- the conversation drawer's **flat reply feed** grouped by root (BPR-0456)
  and the **thread-map view** (BPR-0418);
- the conversation **anchor marker** beside the code (BPR-0414);
- the pane **viewport fill** repair (BPR-0475 — Windows-motivated, inert on
  macOS by measurement, but the pane background is worth a glance).

## Bound evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review` | `sha256:462896aa2c48a2a97215bc8595a2fca9c5d1a7f635d1ab0759d8733592652347` | 55,045,344 |

The deterministic preflights passed at these exact bytes in the closeout
replay's full matrix: the attended launch composes and retains the real
fixture window, and the recorded review drives the real selected-file path
with classified content. Those checks do not supply this human judgment.

Before starting, verify the bound input:

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
shasum -a 256 build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
```

Do not proceed unless the digest exactly matches the table.

## Operator procedure

1. Launch the attended fixture application on your real display:

   ```sh
   cd /Users/jaeger/Documents/GitHub/BetterPullReview
   ./build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
     --attended-launch --github-fixture uiforetw-74
   ```

2. Open the recorded pull request the fixture serves (`google/UIforETW#74`)
   and select the changed file through the Files drawer so the real two-pane
   comparison with its intraline marks is on screen.
3. Judge the assembled workbench deliberately:
   - `UIforETW/UIforETWDlg.cpp` carries keyword, comment, and string syntax
     colouring in both panes and every profile rather than sitting monochrome;
   - the line-number gutters read correctly beside the code, and the View
     menu's Line numbers toggle hides and restores them cleanly;
   - the conversation drawer's flat feed and thread map both read well, and
     the anchor marker sits beside the right line;
   - code, gutters, navigation, drawers, hunk headers, and state indicators
     retain readable contrast;
   - buttons, scrollbars, focus rings, and menus render correctly under
     Cocoa/QMacStyle;
   - the layout remains coherent at full screen, a mid-size window, and the
     smallest width at which you would actually review;
   - System, Light, Dark, and high-contrast appearance profiles remain
     readable and native-looking; and
   - nothing appears broken, misaligned, clipped, low-quality, or foreign to
     macOS.
4. Close the application normally when done.

Do not run offscreen, substitute screenshots for the judgment, or reuse the
v003/v004 responses: their statements name different executable bytes.

## Operator response

To accept, reply with this exact statement:

> I ran the BPR-0100 native visual-acceptance pass against executable
> sha256:462896aa2c48a2a97215bc8595a2fca9c5d1a7f635d1ab0759d8733592652347
> on a real macOS display under Cocoa/QMacStyle, inspected the recorded
> UIforETW review through the real selected-diff path at supported window
> sizes and appearance profiles, saw the C++ comparison rendered with syntax
> colouring in both panes, and I accept the assembled workbench's
> presentation quality.

To reject, name what looked wrong and where—profile, surface, and size—in your
own words. A rejection is recorded as evidence and opens the repair card; it
makes no wider claim. No partial response or screenshot substitutes for this
judgment.
