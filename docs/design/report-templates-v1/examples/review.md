# Review of #21

The move is clean: fourteen of the seventeen files moved without a change in behaviour. Two things need fixing before this merges: a zero height still asserts, and `package.py` still ships the old shader paths. The new tonemap pass raises a question.

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

### issue (blocking): package.py ships the old shader paths

`tools/build/scripts/package.py:41`

The script still collects `src/shaders/*.frag`. After the move that matches nothing, so a packaged build starts with no shaders.

```suggestion
SHADERS = glob("src/engine/shaders/**/*.frag", recursive=True)
```

## Worth a look

### question: Is the tonemap pass meant to ignore exposure?

`src/engine/shaders/post/tonemap.frag:5`

It writes the scene straight through. Either it is a placeholder, or `uExposure` was lost in the move; `src/engine/shaders/post/bloom.frag:3` still declares it.

```glsl
void main() { vec4 c = texture(uScene, vUv); fragColour = c; }
```

### suggestion (non-blocking): Clamp the slider's value before storing it

`src/ui/widgets/slider.cpp:18`

The value is stored and then compared with `Maximum`. Your conversation there is still open.

## Good

### praise: The move keeps history

`src/engine/renderer.cpp:1`

Every file moved with `git mv`, so blame still reaches the original commits.

## Questions for the author

- `src/engine/shaders/post/tonemap.frag:5` Is this pass a placeholder for now?
- `docs/guide.md:9` Should the guide name the new shader folder?

## Not reviewed

- `docs/api/v2/endpoints.md` and `docs/api/reference.md`. Prose only.
- Fourteen files that moved with only their include paths changed.
