# Examples

Before: The cache, which is used in several places, is cleared when needed.

After: `renderer.cpp` clears the frame cache in `resize()`.
