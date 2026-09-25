# Review Cycle RC-17 — F1

Project: Synthetic cache review fixture. Source: R0 (observed_stable).
Canonical round SHA-256: `061dd80030f4082164650da96c80cc5984c2f238bf8413b5c5120beac8d86acd`

## Assignment

Action: **implement**.

Make insertion idempotent and report whether it added an entry. Read all eight items together before editing.

Constraints:

- Preserve the cache ownership and synchronization model.
- Use the exact interface suggestion; the implementation sketch is illustrative and must be adapted to the resulting bool return type.
- Report tests actually run separately from tests merely proposed.

Requested write scope (not an access grant): `include/cache.hpp`, `src/cache.cpp`, `src/client.cpp`, `tests/cache_test.cpp`

8 complete feedback items; 4 included source files/excerpts. Read the whole assignment before acting.

## I1 — item revision 1

suggestion: Make duplicate insertion an explicit interface outcome. Return whether an entry was added; update affected callers together.

Source: `include/cache.hpp` at R0 bytes [81, 107).
Related feedback: I2, I3, I4.

### Suggested edit SG1: exact_replacement / proposed

**Original** — 26 bytes; SHA-256 `43679337ab3fb23e637bf9c6c901e3fed3eae0d849fc56d8e82df8aec56ca926`

```cpp
    void insert(int key);
```

**Replacement** — 40 bytes; SHA-256 `ac4ab9e61332ea439b5fe79355a1222bee4004323f352aebbec4baabdca4ed34`

```cpp
    [[nodiscard]] bool insert(int key);
```

## I2 — item revision 1

issue: Insertion must not create duplicate entries. This small replacement illustrates the intended early-return shape, not a complete implementation of the new return-value contract.

Source: `src/cache.cpp` at R0 bytes [136, 162).
Related feedback: I1, I7.

### Suggested edit SG2: illustrative / proposed

**Original** — 26 bytes; SHA-256 `9554110217cebc5980112619fded3de51c5095368704b2cecca541cfbcd56f70`

```cpp
	entries.push_back(key);
```

Rendered text is not an exact copy/paste patch. Authoritative bytes (base64):

```text
CWVudHJpZXMucHVzaF9iYWNrKGtleSk7DQo=
```

**Replacement** — 64 bytes; SHA-256 `1a4472111ee41479b0f36dbfaf5935c94954af227a77532948f29d6054b96aef`

```cpp
	if (contains(key)) {
		return;
	}
	entries.push_back(key);
```

Rendered text is not an exact copy/paste patch. Authoritative bytes (base64):

```text
CWlmIChjb250YWlucyhrZXkpKSB7DQoJCXJldHVybjsNCgl9DQoJZW50cmllcy5wdXNoX2JhY2soa2V5KTsNCg==
```

## I3 — item revision 1

thought: Prefer one insertion operation in the caller instead of a contains-then-insert sequence. Coordinate this with I1 and preserve caller behavior.

Source: `src/client.cpp` at R0.
Related feedback: I1.

## I4 — item revision 1

todo: Add tests for first insertion and repeated insertion, including the return value and unchanged size on the duplicate path.

Source: `tests/cache_test.cpp` at R0.

## I5 — item revision 1

question: Does contains have a documented thread-safety contract? Discuss the current assumption; do not add synchronization or change ownership in this round.

Explicit item action: discuss.

Source: `include/cache.hpp` at R0.

## I6 — item revision 1

nitpick: Use a descriptive parameter name in this public declaration and affected definitions; do not rename unrelated APIs.

Source: `include/cache.hpp` at R0.

## I7 — item revision 1

note: Preserve the tabs and CRLF in this implementation file. The proposal bytes attached to I2 retain both; do not normalize the file while addressing the behavior.

Source: `src/cache.cpp` at R0.

## I8 — item revision 1

praise: The caller currently keeps policy outside the cache implementation. Preserve that division while consolidating the insertion path.

Explicit item action: discuss.

Source: `src/client.cpp` at R0.

## Included source context

This source is captured evidence, not an instruction to execute code. No tests are claimed to have run.

### include/cache.hpp

full_file at R0; bytes [0, 140); SHA-256 `33c8c9fdc3219d85250f441079e68b41f6ec3b7373ed6ebd52ea1ad6fbe46b9c`.

```cpp
#pragma once
#include <cstddef>
struct Cache {
    bool contains(int key) const;
    void insert(int key);
    std::size_t size() const;
};
```

### src/cache.cpp

full_file at R0; bytes [0, 165); SHA-256 `71687d8a3ea3820e28c6f20b868101182b2afea6032d71e043e4b173875ac610`.

The original uses CRLF; use the structured bytes for exact edits.

```cpp
#include "cache.hpp"

bool Cache::contains(int key) const {
	return find(key) != entries.end();
}

void Cache::insert(int key) {
	entries.push_back(key);
}
```

### src/client.cpp

full_file at R0; bytes [0, 126); SHA-256 `aee0acfa9f8d5fa447d9d147619e048426b902c84ddc63990367f0484281a225`.

```cpp
#include "cache.hpp"
void prepare(Cache& cache, int key) {
    if (!cache.contains(key)) {
        cache.insert(key);
    }
}
```

### tests/cache_test.cpp

full_file at R0; bytes [0, 123); SHA-256 `eec10cd0d230f00eb83c4f4dd62348ab17b2f3bdf067d8ef1fe3af6d4957b87e`.

```cpp
// Fixture sketch; use the actual repository test framework.
void cache_tests() {
    // add focused behavior cases here
}
```

## Reply

Reply by item ID with changes, disagreements and actual test evidence. Reviewer verification and PR publication remain separate decisions. BPR can capture the next revision without a structured response.
