Each view now clears and draws its own target between `begin_frame()` and `present()`, so two views no longer share one frame's scene.

- `renderer::clear()` takes the view it clears.
- `renderer::draw()` draws only that view's scene.
- **Resize** keeps the size it was given.

The order in `src/engine/renderer.cpp` stays the same:

```cpp
begin_frame();
clear(view);
draw(view);
present();
```

1. Read `clear()` first.
2. Then check that `draw()` never clears.
