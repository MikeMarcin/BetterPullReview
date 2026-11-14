# What this change could break

The move itself is safe: every include resolves and the tests pass. One thing is likely to break where no test looks: the packaging script still collects shaders from the old folder. Two more are possible.

## Likely

### A packaged build starts with no shaders

`tools/build/scripts/package.py:41`

The script collects `src/shaders/*.frag`. After the move that matches nothing, and nothing fails: the archive is built without them.

```python
SHADERS = glob("src/shaders/*.frag")
```

**Check:** Run the packaging script and list the archive's `shaders/` folder.

## Possible

### A minimised window asserts

`src/engine/renderer.cpp:13`

The width is clamped to one pixel but the height is not, and the swap chain asserts on a zero extent.

**Check:** Add a zero-height resize beside the zero-width one in `tests/engine/renderer_tests.cpp:6`.

### Code outside this repository that includes the old paths

`src/engine/renderer.h:1`

Anything that includes `renderer.h` by its old path stops compiling.

**Check:** Search the dependants for `#include "renderer.h"`.

## Unlikely

### Layers drawn in a different order

`src/engine/renderer.cpp:25`

The loop over the layers is as it was; only the call inside it gained an argument.

**Check:** None needed.

## Not checked

- The Windows build. No CI ran on this push.
- `docs/api/v2/endpoints.md` against the server it describes.
