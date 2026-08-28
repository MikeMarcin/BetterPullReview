---
id: BPR-0444
title: Judge the nine vault findings the working tidy gate now shows
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0443]
completed: 2026-08-27
---

# Judge the nine vault findings the working tidy gate now shows

[[BPR-0443]] made `coding-tidy` run on the Linux-only sources for the first
time and resolved every identifier-naming finding. Nine non-naming findings
remain in `src/adapters/github/src/linux_secret_service.cpp`. They are older
than that card and were invisible while the gate could not execute.

| Count | Check | Where |
|---|---|---|
| 8 | `bugprone-suspicious-stringview-data-usage` | 394, 395, 618, 630, 713, 715, 820, 837 |
| 1 | `bugprone-branch-clone` | 1493 |

## Why they are not obviously nothing

Each `.data()` call hands a `std::string_view` to a libsecret C entry point
that expects a null-terminated string — `make_schema`, `secret_value_new`'s
content type, `secret_item_create_sync`'s label. On inspection the arguments
are `constexpr` views over string literals, which are null-terminated, so the
calls are very likely correct today. The check is warning about the shape
rather than a proven defect.

That is exactly why it needs a decision rather than a silent suppression. This
is the credential vault. A view that stops being a literal later would pass
review unchanged and read past its end into a secret-bearing call, and nothing
in the current code says the argument must stay a literal.

`bugprone-branch-clone` is the mildest: `SECRET_ERROR_ALREADY_EXISTS` and
`default` both return `failure::operation_failed`. Naming the case explicitly
next to a `default` that agrees is a legitimate style, and the honest outcome
here may be an accepted suppression with that reason recorded.

## Required final state

- Each of the nine is repaired, or suppressed with a written reason naming why
  the shape is safe and what would make it unsafe.
- No suppression is added without that reason.
- If any is a real defect, it gets its own card and a regression test rather
  than an inline fix folded into a hygiene pass.
- `coding-tidy` is clean on the Linux-only sources afterwards.

## Not this card

The identifier naming, which [[BPR-0443]] finished, and the native Secret
Service fixture failures recorded on [[BPR-0310]].

## Ready, and ready on Linux — 2026-08-23

[[BPR-0443]] is done, so nothing is queued ahead of this card. The lane is
Linux because the proof is: `linux_secret_service.cpp` appears only in a
Linux `dev` tree's compilation database — a macOS or Windows pass never
compiles it — so "coding-tidy is clean on the Linux-only sources afterwards"
can only be shown there. Judging the findings is portable reading, but the
split rule takes no card whose portable half is unverifiable here, so the
whole card moves to `23-ready-lin/`.

## Result — 2026-08-27

Eight repaired, one suppressed with its reason. Line numbers had moved from
the table above — BPR-0443's `NOLINTBEGIN` shifted them — but the counts and
the checks were exactly as recorded: 402, 403, 626, 638, 721, 723, 828, 845,
and 1561.

### The eight are one answer, not eight

The card asked why the shape is safe and what would make it unsafe. The
honest answer to the second half was: nothing in the file, which is why a
suppression would have been the wrong outcome. Every one of the eight passed a
`constexpr std::string_view` to a libsecret entry point that reads to a
terminator, and every one was correct only because the view happened to be
over a string literal.

`c_str` is now that requirement. It is `consteval`, so its argument is
evaluated as a constant expression, and it inspects the byte at `size()`; a
view that is not null-terminated fails to compile at the call. That is proven
in this file rather than in the abstract — making `canonical_service_bus_name`
a same-length `substr` fails both of its call sites, naming the reason:

```
error: call to consteval function 'c_str(canonical_service_bus_name)' is not
a constant expression
  throw "a string_view handed to a libsecret entry point must be null-terminated";
```

### One of the eight was different

Line 638 was `label_for(Scope).data()`. `Scope` is a member, not a template
parameter, and `label_for` was a `switch` with a `return { }` fall-through
whose `data()` is a null pointer. That pointer would have reached
`secret_item_create_sync` as the item label; only the caller's `valid(Scope)`
guard, several hundred lines earlier, stood between. `label_for` is now total
over the two scopes and returns a `const char*`, so no such value exists.

Not carded as a defect, because it was not reachable: the guard was real and
the enum has two enumerators. Recorded because the next reader deserves to
know the fall-through was looked at rather than missed.

### The ninth is a suppression, and says why

`bugprone-branch-clone` at what is now 1584 keeps the explicit
`SECRET_ERROR_ALREADY_EXISTS` case beside a `default` that agrees.
`linux_secret_service_contract_tests` asserts that exact mapping, so deleting
the case would lose the record that the code was classified rather than
defaulted while changing nothing. The `NOLINTNEXTLINE` carries that reason and
names what would make it wrong. Proven load-bearing: deleting the comment
reproduces the finding.

### The pins moved with the code

Four of the eight call sites are pinned by source text in
`cmake/VerifyArchitectureBoundaries.cmake`, which is why BPR-0443 declined to
rename in this file. Those pins now bind the same operations through `c_str`,
and the guard itself is pinned, so `consteval` cannot quietly become
`constexpr` — proven by making that exact substitution and watching
`architecture_boundaries` fail. `architecture_boundary_contract` carries the
fixture and three new removal cases for the guard, so the pins are exercised
rather than merely present.

| Claim | Command | Result |
|---|---|---|
| the nine are gone | `tidy.py --build-dir build/dev --warnings-as-errors --paths src/adapters/github/src/linux_secret_service.cpp` | clean, was 9 |
| the guard rejects a non-literal view | `canonical_service_bus_name` as a same-length `substr`, then `cmake --build --preset dev --target better_pull_review_adapter_github` | fails at both call sites with the written reason |
| the guard cannot be downgraded | `consteval` -> `constexpr`, then `ctest --preset dev -R '^architecture_boundaries$'` | Failed, as required |
| the branch-clone suppression is needed | delete the `NOLINTNEXTLINE`, rerun tidy | finding returns at 1594 |
| the pinned lifecycle still holds | `ctest --preset dev -R '^architecture_boundaries$'` | Passed |
| the pins are exercised, not just present | `ctest --preset dev -R '^architecture_boundary_contract$'` | Passed, 127 cases |
| behaviour preserved | `ctest --preset dev -R '^linux_secret_service_contract$'` | Passed |
| nothing else in the adapter's reach regressed | `ctest --preset dev -R 'secret\|github\|architecture'` | 32 of 35; the 3 are below |
| format | `format_check.py --clang-format /usr/bin/clang-format` | passed, 425 of 425 |

### The three that still fail, and why they are not this card

`linux_secret_service_native_setup`, `_native`, and `_native_cleanup` fail on
this host. BPR-0443 recorded them on [[BPR-0310]] and proved them
pre-existing; that proof was taken again at this head, against the
pre-BPR-0444 adapter restored from `64e00620` and rebuilt. They fail
identically, with the same `linux-secret-service-setup:fail`. The host's
keyring collection is locked and the fixture refuses prompts by construction.

### What closing this card cost

`coding-tidy` on the Linux-only sources needed a full `dev` build, and the
build did not complete on this platform. That is [[BPR-0469]], now done.
Pointing the working gate one directory over found that the two Linux-only
*test* sources had never been read by it either — 310 identifier-naming
findings, the same gap BPR-0443 closed for the adapter. That is [[BPR-0470]],
and because its RFC is RFC-BPR-0002, it gates that closeout the way this card
did.
