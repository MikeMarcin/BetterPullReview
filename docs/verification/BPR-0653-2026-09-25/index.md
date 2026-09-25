# BPR-0653 Windows capture, 2026-09-25

The station's own capture of the strip as the title row, for the owner's
remote review of [BPR-0653](../../issues/40-parked/BPR-0653-windows-caption-in-the-strip.md)'s
criteria 2 and 4. Taken by
[`windows_caption_capture.ps1`](../../../tests/scripts/windows_caption_capture.ps1)
in one launch of [BPR-0974](../../issues/30-in-progress/BPR-0974-dispose-windows-defender-finding-and-complete-native-settings.md)'s
tests-disabled candidate. [`capture.json`](capture.json) holds every state's
rects, caption-button rects, pointer, and every gesture with its time.

- Image: `installed\better_pull_review.exe`, SHA-256 `6aa27c8de85604d38a04c8728c610d2afb1fbcbd541e489bdedcc3891d798ac4`;
  bundle `ca3bc2b191085ef61a5225369565376e163b4efb80920b2b52e72e68bf048f91`; source `85e522d86f29230454c72b4d965fa2e06986e394`.
- Launched with `--render-evidence` (a private temporary profile, signed out;
  standard output `AUTOMATED_PROFILE_PRIVATE_OK`). PID 36952, exit 0 after `WM_CLOSE`.
- Display: Microsoft Windows NT 10.0.26200.0, system DPI 144 (150%), the scale
  Windows is set to, not changed; no Qt scale override. Rects are physical pixels.
- 2026-09-25T04:29:54.7403221Z to 2026-09-25T04:30:23.4761876Z. Pointer saved at
  (1895, 699) and restored to (1895, 699).
- Every move and press was guarded: the window in front, the window under the
  point, and the point's own `WM_NCHITTEST` answer. A held button was always
  released on empty strip space, where the title row host does nothing.

Each state has three images: the window's visible frame (downscaled to at
most 1920 pixels wide; `factor` below), and the strip and the caption-button
area at full resolution. No image contains the desktop or the taskbar.

**Redacted.** Other applications' content, and nothing of the app, was filled
with the surrounding colour in four window images. The originals are station
records and are not committed. `capture.json` lists each rectangle.

- `hover-maximize`, `snap-flyout` and `pressed-maximize`: the snap flyout's
  recent snap groups showed two other applications' icons (Windows Terminal
  and an OpenAI app). Three icon rectangles were filled; the flyout's tiles
  and layout are unchanged.
- `maximized`: a notification toast from another application sat over the
  bottom-right corner. Its rectangle was filled with the window's background.

| State | Window | Strip | Caption buttons | Frame | DPI | Time (UTC) |
| --- | --- | --- | --- | --- | --- | --- |
| **rest**: The three caption buttons at rest; the pointer parked outside them | [window](rest-window.png) `9d45714ca5df` | [strip](rest-strip.png) `132d7ac7ffb1` | [caption](rest-caption-buttons.png) `c9df76311a9c` | 958,384 1924x1232, factor 0.998 | 144 (150%) | 04:30:01.391 |
| **hover-minimize**: Minimize hovered | [window](hover-minimize-window.png) `3076bdfca511` | [strip](hover-minimize-strip.png) `a6eda02004a1` | [caption](hover-minimize-caption-buttons.png) `7a295882ef25` | 958,384 1924x1232, factor 0.998 | 144 (150%) | 04:30:02.728 |
| **pressed-minimize**: Minimize pressed and held (released on empty strip space, so nothing fired) | [window](pressed-minimize-window.png) `c466491a67dd` | [strip](pressed-minimize-strip.png) `e212a72ea27a` | [caption](pressed-minimize-caption-buttons.png) `865dc6ce936a` | 958,384 1924x1232, factor 0.998 | 144 (150%) | 04:30:03.565 |
| **hover-maximize**: Maximize hovered; the snap layouts flyout already shown | [window](hover-maximize-window.png) (redacted) `94e4af3a6d46` | [strip](hover-maximize-strip.png) `e141e2fa996d` | [caption](hover-maximize-caption-buttons.png) `1533f3c370ee` | 958,384 1924x1232, factor 0.998 | 144 (150%) | 04:30:05.836 |
| **snap-flyout**: The snap layouts flyout, 4.6 s into the hover; byte-identical to hover-maximize, as the flyout was already up | [window](snap-flyout-window.png) (redacted) `94e4af3a6d46` | [strip](snap-flyout-strip.png) `e141e2fa996d` | [caption](snap-flyout-caption-buttons.png) `1533f3c370ee` | 958,384 1924x1232, factor 0.998 | 144 (150%) | 04:30:10.403 |
| **pressed-maximize**: Maximize pressed and held, flyout still up (released on empty strip space) | [window](pressed-maximize-window.png) (redacted) `8063fdff3146` | [strip](pressed-maximize-strip.png) `0e2361daaf95` | [caption](pressed-maximize-caption-buttons.png) `f449315a62e0` | 958,384 1924x1232, factor 0.998 | 144 (150%) | 04:30:11.250 |
| **hover-close**: Close hovered | [window](hover-close-window.png) `d377312c1175` | [strip](hover-close-strip.png) `beb5ea82df07` | [caption](hover-close-caption-buttons.png) `4fdfdfe49c71` | 958,384 1924x1232, factor 0.998 | 144 (150%) | 04:30:13.512 |
| **pressed-close**: Close pressed and held (released on empty strip space, so the window did not close) | [window](pressed-close-window.png) `56d06fd8082d` | [strip](pressed-close-strip.png) `466055cc7837` | [caption](pressed-close-caption-buttons.png) `a994f912768d` | 958,384 1924x1232, factor 0.998 | 144 (150%) | 04:30:14.328 |
| **drag-before**: Before a drag of empty strip space | [window](drag-before-window.png) `9d45714ca5df` | [strip](drag-before-strip.png) `132d7ac7ffb1` | [caption](drag-before-caption-buttons.png) `c9df76311a9c` | 958,384 1924x1232, factor 0.998 | 144 (150%) | 04:30:16.195 |
| **drag-mid**: Mid-drag, the left button held on empty strip space (SendInput) | [window](drag-mid-window.png) `d15dede853b3` | [strip](drag-mid-strip.png) `002053016e84` | [caption](drag-mid-caption-buttons.png) `1520d790f16d` | 1038,444 1924x1232, factor 0.998 | 144 (150%) | 04:30:16.989 |
| **drag-after**: After the drag: the frame moved by exactly (+160, +120) | [window](drag-after-window.png) `281b7190d51b` | [strip](drag-after-strip.png) `581d41fd6d19` | [caption](drag-after-caption-buttons.png) `c9df76311a9c` | 1118,504 1924x1232, factor 0.998 | 144 (150%) | 04:30:18.515 |
| **maximized**: Maximized by a click on Maximize | [window](maximized-window.png) (redacted) `b5bca421fe29` | [strip](maximized-strip.png) `52c1c87a5f93` | [caption](maximized-caption-buttons.png) `e44a00496cd1` | 0,0 3840x2088 maximized, factor 0.500 | 144 (150%) | 04:30:21.013 |
| **restored**: Restored by a double-click on empty strip space | [window](restored-window.png) `ffb5ec25b186` | [strip](restored-strip.png) `581d41fd6d19` | [caption](restored-caption-buttons.png) `c9df76311a9c` | 1118,504 1924x1232, factor 0.998 | 144 (150%) | 04:30:23.356 |

The full SHA-256 of every file is in `capture.json` (`sha256`, and
`captured_sha256` for the bytes as captured, before any redaction).
