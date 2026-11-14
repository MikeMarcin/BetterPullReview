# What renderer.cpp does

`src/engine/renderer.cpp` draws one frame: it sizes the swap chain, then paints each layer of the scene between `begin_frame()` and `present()`. It keeps no state beyond the target and the shader set. This pull request moves it into `src/engine/` and adds a one-pixel clamp to the resize.

## How it works

### resize()

`src/engine/renderer.cpp:9-14`

Clamps the width to one pixel and resizes the target. It runs whenever the window's extent changes, including when the window is minimised.

```cpp
const auto width = std::max(aSize.Width, 1);
Target.resize(extent{ width, aSize.Height });
```

### draw()

`src/engine/renderer.cpp:16-21`

Begins the frame, draws the scene and presents it. Nothing is drawn outside that bracket.

### draw_scene()

`src/engine/renderer.cpp:23-28`

Paints each layer with the post shaders, in the scene's own order. Nothing here sorts.

## What this pull request changes

- `src/engine/renderer.cpp:1-3` The include paths follow the move to `src/engine/`.
- `src/engine/renderer.cpp:11-13` The width clamp is new.
- `src/engine/renderer.cpp:26` `paint()` now takes the post shaders.

## How it fits

- **Declared in** `src/engine/renderer.h:12`
- **Called by** `src/main.cpp:41` Once a frame.
- **Calls** `src/engine/swap_chain.h:20` `resize`, `begin_frame` and `present`.
- **Tested by** `tests/engine/renderer_tests.cpp:6`

## Worth knowing

- `src/engine/renderer.cpp:13` A zero height reaches `Target.resize` unclamped.
- `src/engine/renderer.cpp:19` `draw_scene()` assumes `begin_frame()` has run. Nothing checks.
