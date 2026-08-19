# BPR-0100 native visual-acceptance operator packet

Status: **pending operator run and judgment; not RFC closeout**

This packet presents the owner-attended pass BPR-0100 inherited from the
RFC-BPR-0003 and RFC-BPR-0004 ledgers: judge the assembled workbench's
presentation quality on the real macOS composition — native Cocoa and
QMacStyle, a real display, real focus — against the recorded review rather
than synthetic content. Automation proves identity, wiring, and measured
contrast; it cannot judge whether the result looks right. This packet does
not close BPR-0114, BPR-0220, RFC-BPR-0005, or RFC-BPR-0006, and it grants
no live or package authority.

## Why this version exists, and why the bytes are settled now

The previous packet was never run: [[BPR-0397]] bounded three unbounded waits in
the epoch host and rebuilt the executable before the owner reached it. A changed
byte reopens the gate, so this is a new batch and the previous packet is
preserved as history.

That is the third rebuild in a row to invalidate a pending packet, so the honest
question is why this one should be different. The board answers it: every card
that could change these bytes is now in `60-done/`, and no card in any active
lane is executable without a Windows or Linux host, a product decision, or the
very verdict this packet asks for. There is no queued unattended work left to
move the executable. **The bytes are settled because the board is.**

The preflights are also stronger than any earlier version's. Rather than a
focused selector set, the complete 193-test `dev` matrix passed at exactly these
bytes, including `application_attended_launch_contract`,
`application_uiforetw_selected_diff_contract`,
`application_native_appearance_probe_contract`, and
`application_native_notification_attended_contract`.

**Nothing about what you do has changed.** Same procedure, same markers, same
acceptance statement; new digest and new artifact paths.

## Why v002 exists: the v001 rejection was repaired, and the path changed

You ran v001 on 2026-08-18, reached the real C++ comparison through the
Files drawer, and rejected it:

> How come there's no syntax highlighting?

That rejection was correct and is recorded as evidence. [[BPR-0390]] traced
it and repaired it, and this packet binds the repaired bytes. Two things
therefore differ from v001, and neither changes what you are asked to judge:

- **The executable path moved.** v001 named `build/dev/src/app/better_pull_review`,
  which no longer exists as a file — [[BPR-0392]] made the tests-enabled macOS
  build an application bundle, so the binary is now nested inside
  `better_pull_review.app`. The launch command below is the corrected one.
- **Syntax classification now reaches the pane.** BPR-0390 found a delivery
  race, not a missing classifier: spans and the diff result arrived as two
  unordered deliveries into the same pane, and whichever landed second decided
  whether code was coloured. Adoption now happens at display time against the
  comparison the surface is actually showing. On the recorded course the left
  pane went from zero token spans to 1,361, and
  `application_uiforetw_selected_diff_contract` now fails if the selected
  comparison renders unclassified.

One honest limit, stated before you start rather than discovered mid-run.
BPR-0390 measured one blob whose classification arrives late — the right-hand
side of the final `UIforETW.rc` comparison. Its classification succeeds; on the
automated course it simply loses a race with that course's own shutdown. A
person at the keyboard waits, so you should see it colour. **If a pane sits
uncoloured for more than a second or two after you select a file, that is a
finding worth rejecting on** — it would mean the repair is incomplete in a way
no automated course can currently observe.

## Bound evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review` | `sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93` | 51,085,872 |

This is the same executable the pending `appearance-scheme-foreground-v006`
and `native-notification-delivery-v004` packets bind, so one build serves all
three attended gates. The deterministic preflights at these exact bytes are
`application_attended_launch_contract` (the attended launch composes the
fixture application, shows the window, and stays open) and
`application_uiforetw_selected_diff_contract` (the recorded review drives the
real selected-file path and its left pane carries a nonzero token-span count);
both pass, together with `application_diff_workbench_public_path`,
`application_workbench_composition_contract`, and `qt_diff_workbench_scale_100`
— 5/5. Rebuilding the binary reopens all three gates together.

Before starting, verify the bound input:

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
shasum -a 256 build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
```

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
3. Judge the assembled workbench deliberately, not at a glance:
   - **syntax presentation on the real C++ file** — `UIforETW/UIforETWDlg.cpp`
     should carry keyword, comment, and string colouring in both panes and in
     every profile, and should not sit monochrome;
   - readable contrast throughout — code, gutters, navigation row, drawers,
     hunk headers, state indicators;
   - correct native control rendering under Cocoa/QMacStyle — buttons,
     scrollbars, focus rings, menus;
   - coherent layout across supported window sizes: full screen, a mid-size
     window, and the smallest width you would actually review at;
   - the four appearance profiles via the shipped appearance selection —
     System, Light, Dark, and the high-contrast profiles — each remaining
     readable and native-looking;
   - anything that reads as broken, misaligned, clipped, low-quality, or
     foreign to macOS.
4. Close the application normally when done.

## Operator response

To accept, reply with this exact statement:

> I ran the BPR-0100 native visual-acceptance pass against executable
> sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93
> on a real macOS display under Cocoa/QMacStyle, inspected the recorded
> UIforETW review through the real selected-diff path at supported window
> sizes and appearance profiles, saw the C++ comparison rendered with syntax
> colouring in both panes, and I accept the assembled workbench's
> presentation quality.

To reject, name what looked wrong and where — profile, surface, size — in
your own words. A rejection is recorded as evidence and opens the repair
card; it makes no wider claim. No partial response or screenshot substitutes
for this judgment.
