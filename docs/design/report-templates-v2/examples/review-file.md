# Review of renderer.cpp

The file does what the pull request says: it moves under `src/engine/` and clamps the resize. One thing must be fixed before it merges: the clamp covers the width and not the height. Two smaller things are worth a look.

## Must fix

### issue (blocking): A zero height still asserts

`src/engine/renderer.cpp:12-13`

The width is clamped, but the height goes to `Target.resize` as it arrives. A minimised window reports a height of zero, and the swap chain asserts on it.

```cpp
const auto width = std::max(aSize.Width, 1);
Target.resize(extent{ width, aSize.Height });
```

```suggestion
    const auto height = std::max(aSize.Height, 1);
    Target.resize(extent{ width, height });
```

## Worth a look

### question: Should draw_scene() check that the frame has begun?

`src/engine/renderer.cpp:23`

It paints straight away and trusts `draw()` to have called `begin_frame()`. Nothing stops another caller from getting that wrong.

### nitpick: The comment says what, not why

`src/engine/renderer.cpp:11`

"A minimised window reports zero" would be more use as "the swap chain asserts on a zero extent".

## Good

### praise: The frame bracket is easy to read

`src/engine/renderer.cpp:16-21`

Begin, draw, present, and nothing else. It is hard to get the order wrong.

## Not reviewed

- `src/engine/renderer.h`. Only its include path changed.
