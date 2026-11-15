# Was this addressed?

Of six threads, two are addressed, two partly, one is not, and one cannot be told from the code. What is left: the slider's maximum, the zero-height resize and a test for it.

## Not addressed

### Should this clamp to the maximum?

`src/ui/widgets/slider.cpp:18`

> Should this clamp to the maximum? A value of 1.2 draws past the track.

The lines are as they were when you commented. tbutler replied that it belongs in a follow-up; no follow-up is linked.

```cpp
Value = aValue;
if (Value > Maximum) { notify_overflow(); }
```

## Partly

### Clamp the size to at least one pixel

`src/engine/renderer.cpp:12`

> Clamp the size to at least one pixel. A zero extent asserts.

The width is clamped. The height still goes to `Target.resize` as it arrives.

```cpp
const auto width = std::max(aSize.Width, 1);
Target.resize(extent{ width, aSize.Height });
```

### Cover the zero-size resize with a test

`tests/engine/renderer_tests.cpp:6`

A zero width is tested. A zero height is not.

## Cannot tell

### Is the bloom threshold still right after the move?

`src/engine/shaders/post/bloom.frag:5`

The file moved, so its lines cannot be compared with the ones you commented on, and nothing in the push says.

## Addressed

### Name the magic number

`src/ui/widgets/button.cpp:7`

`7` is now `corner_radius`.

### Say who owns the target

`src/engine/renderer.h:14`

A comment on the member says the renderer does.

```bpr-claims
PRRT_a1: Not addressed, the lines are unchanged
PRRT_a2: Partly, the width only
PRRT_a3: Partly, zero width only
PRRT_a4: Cannot tell, the file moved
PRRT_a5: Addressed, named corner_radius
PRRT_a6: Addressed, a comment says so
```
