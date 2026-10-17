## Summary

Moves the cache writer onto the serialized lane, so `refresh()` reads the snapshot outside the lock.

```cpp
template <typename T>
auto cache<T>::refresh() -> std::uint64_t
{
	auto next  = read_snapshot(); // outside the lock
	auto guard = std::lock_guard{ Mutex };
	Snapshot   = std::move(next);
	return ++Generation;
}
```

The fixture generator, in Python:

```py
def snapshot(rows: list[str], limit: int = 64) -> dict:
    """Keep the first `limit` rows."""
    return {"rows": rows[:limit], "count": len(rows)}  # trimmed
```

The settings it reads:

```json
{ "cache": { "lane": "serialized", "limit": 64, "enabled": true } }
```

```yaml
cache:
  lane: serialized   # was: shared
  limit: 64
```

```ts
export async function refresh(cache: Cache): Promise<number> {
  const next = await cache.read();
  return cache.swap(next);
}
```

```rust
fn refresh(cache: &Mutex<Cache>) -> u64 {
    let next = read_snapshot();
    let mut guard = cache.lock().unwrap();
    guard.swap(next)
}
```

```go
func (c *Cache) Refresh() uint64 {
	next := readSnapshot()
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.swap(next)
}
```

```sh
cmake --preset dev && ctest --preset dev -R "cache_" --output-on-failure
```

```cmake
add_executable(cache_tests cache_tests.cpp)
target_link_libraries(cache_tests PRIVATE BetterPullReview::Core)
```

## The change

```diff
@@ -7,8 +7,9 @@ namespace sample {
 	void cache::refresh()
 	{
-		auto guard = std::lock_guard{ Mutex };
-		Snapshot   = read_snapshot();
+		auto next  = read_snapshot();
+		auto guard = std::lock_guard{ Mutex };
+		Snapshot   = std::move(next);
 		Generation += 1;
 	}
```

An unknown language and a block without one stay plain:

```cobol-ish
MOVE NEXT TO SNAPSHOT.
```

```
plain text, no language
```

Indented code and `inline code` are never coloured:

    auto guard = std::lock_guard{ Mutex };
