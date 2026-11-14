# What changed since your review

Two commits and one merge of main since you reviewed at `6f2a91c`. The resize is now clamped to one pixel, as you asked, and a tonemap pass is new. Nothing else you read has changed.

## Changed

### Rendering

- `src/engine/renderer.cpp:12` The width is clamped to at least one pixel before the resize. The height is not.
- `src/engine/renderer.h:31` The comment on `resize()` now states the one-pixel floor.

```cpp
const auto width = std::max(aSize.Width, 1);
Target.resize(extent{ width, aSize.Height });
```

### Shaders

- `src/engine/shaders/post/tonemap.frag:1-5` A new pass that samples `uScene` and writes it straight through.

### Tests

- `tests/engine/renderer_tests.cpp:6` A zero-width resize is covered. A zero height is not.

## Merged main

- Merged main (9 commits). None touched this pull request's files.

## Your conversations

- **Lines changed** `src/engine/renderer.cpp:12` Clamp the size to at least one pixel.
- **Replied** `src/ui/widgets/slider.cpp:18` Should this clamp to the maximum? tbutler replied; the lines are as they were.
- **Unchanged** `src/ui/widgets/button.cpp:7` nitpick: name the magic number.
