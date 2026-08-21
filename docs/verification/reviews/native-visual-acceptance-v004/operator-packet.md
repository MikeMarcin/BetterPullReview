# BPR-0100 native visual-acceptance operator packet

Status: **pending operator judgment; not RFC closeout**

This packet presents BPR-0100's owner-attended judgment of the assembled
workbench's presentation quality on the real macOS composition: native Cocoa
and QMacStyle, a real display, real focus, and the recorded review rather than
synthetic content. Automation proves identity, wiring, and measured contrast;
it cannot decide whether the result looks right. This packet does not close
BPR-0114, BPR-0220, RFC-BPR-0005, or RFC-BPR-0006, and it grants no live or
package authority.

## Why v004 exists

The owner accepted v003 against executable
`sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93`.
That exact response and its validation record remain immutable historical
evidence. The later authorized BPR-0409/BPR-0410/BPR-0334 batch materially
relinked the executable, so the identity-bound v003 judgment cannot be widened
to the new bytes. This final-head successor asks for the same visual judgment
against the executable produced from committed source HEAD
`145f6d4c6345c65c42c6d5ea00dfc9b67fa514b0`.

The previous acceptance is not withdrawn or reinterpreted. v004 supersedes it
only as the live final-head question.

## Bound evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review` | `sha256:10899f24d0a2253eb707c345650522ce1ee9b91a7634dc65a7f1226338181b28` | 51,383,520 |

This is the same executable bound by the v008 appearance and v006 notification
packets, so one build serves all three attended gates. The deterministic
`application_attended_launch_contract` and
`application_uiforetw_selected_diff_contract` preflights passed at these exact
bytes: the attended launch composes and retains the real fixture window, and the
recorded review drives the real selected-file path with classified content.
Those checks do not supply this human judgment.

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
   - code, gutters, navigation, drawers, hunk headers, and state indicators
     retain readable contrast;
   - buttons, scrollbars, focus rings, and menus render correctly under
     Cocoa/QMacStyle;
   - the layout remains coherent at full screen, a mid-size window, and the
     smallest width at which you would actually review;
   - System, Light, Dark, and high-contrast appearance profiles remain readable
     and native-looking; and
   - nothing appears broken, misaligned, clipped, low-quality, or foreign to
     macOS.
4. Close the application normally when done.

Do not run offscreen, substitute screenshots for the judgment, or reuse the
v003 response: its statement names different executable bytes.

## Operator response

To accept, reply with this exact statement:

> I ran the BPR-0100 native visual-acceptance pass against executable
> sha256:10899f24d0a2253eb707c345650522ce1ee9b91a7634dc65a7f1226338181b28
> on a real macOS display under Cocoa/QMacStyle, inspected the recorded
> UIforETW review through the real selected-diff path at supported window
> sizes and appearance profiles, saw the C++ comparison rendered with syntax
> colouring in both panes, and I accept the assembled workbench's
> presentation quality.

To reject, name what looked wrong and where—profile, surface, and size—in your
own words. A rejection is recorded as evidence and opens the repair card; it
makes no wider claim. No partial response or screenshot substitutes for this
judgment.
