# BPR-0100 native visual-acceptance operator packet

Status: **answered — rejected 2026-08-18 — and superseded by
[`native-visual-acceptance-v003`](../native-visual-acceptance-v003/operator-packet.md);
do not run or approve.**

This packet is preserved as the superseded v001 procedure, and unlike the other
superseded packets in this directory it was run. The owner reached the real C++
comparison through the Files drawer and rejected it on a defect no automated
marker had caught, asking in their own words: *"How come there's no syntax
highlighting?"* That rejection is the pass working — it is recorded and
root-caused as [[BPR-0390]], and it produced the contract check that was
missing.

The version is also unrunnable now for two independent reasons: its bound digest
is stale, and the path it names, `build/dev/src/app/better_pull_review`, stopped
being a file when [[BPR-0392]] made the tests-enabled macOS build an application
bundle. No acceptance is inferred from the aspects the owner did not reach, and
no response to this version can satisfy [[BPR-0100]].

This packet presents the owner-attended pass BPR-0100 inherited from the
RFC-BPR-0003 and RFC-BPR-0004 ledgers: judge the assembled workbench's
presentation quality on the real macOS composition — native Cocoa and
QMacStyle, a real display, real focus — against the recorded review rather
than synthetic content. Automation proves identity, wiring, and measured
contrast; it cannot judge whether the result looks right. This packet does
not close BPR-0114, BPR-0220, RFC-BPR-0005, or RFC-BPR-0006, and it grants
no live or package authority.

## Bound evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/dev/src/app/better_pull_review` | `sha256:7fc2a16ade93ec35c322b927eb8cb4811a29e2a102c5a64d51eb3bb30a6a0ade` | 51132976 |

This is the same executable the pending appearance-scheme-foreground-v004
and native-notification-delivery-v002 packets bind. The deterministic
preflights at these exact bytes are `application_attended_launch_contract`
(the attended launch composes the fixture application, shows the window, and
stays open) and `application_uiforetw_selected_diff_contract` (the recorded
review drives the real selected-file path); both pass. Rebuilding the binary
reopens all three gates together.

Before starting, verify the bound input:

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
shasum -a 256 build/dev/src/app/better_pull_review
```

## Operator procedure

1. Launch the attended fixture application on your real display:

   ```sh
   cd /Users/jaeger/Documents/GitHub/BetterPullReview
   ./build/dev/src/app/better_pull_review --attended-launch --github-fixture uiforetw-74
   ```

2. Open the recorded pull request the fixture serves (`google/UIforETW#74`)
   and select the changed file through the Files drawer so the real two-pane
   comparison with its intraline marks is on screen.
3. Judge the assembled workbench deliberately, not at a glance:
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
> sha256:7fc2a16ade93ec35c322b927eb8cb4811a29e2a102c5a64d51eb3bb30a6a0ade
> on a real macOS display under Cocoa/QMacStyle, inspected the recorded
> UIforETW review through the real selected-diff path at supported window
> sizes and appearance profiles, and I accept the assembled workbench's
> presentation quality.

To reject, name what looked wrong and where — profile, surface, size — in
your own words. A rejection is recorded as evidence and opens the repair
card; it makes no wider claim. No partial response or screenshot substitutes
for this judgment.
