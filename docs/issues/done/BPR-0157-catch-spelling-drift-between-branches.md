---
id: BPR-0157
title: Catch spelling drift between conditional branches
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0155]
---

# Catch spelling drift between conditional branches

BPR-0155 repaired one shape of half-finished rename: a parameter `aXxx`
referred to as `xxx` in a branch no local compiler reads. It said plainly that
it proved *one class* of mistake absent. It was right to.

The Linux build was still broken. Scoping BPR-0132 turned up
`catalog_database.cpp:4040`:

```cpp
	#if defined(__linux__)
		return catalog_database{ database, final_path, ... };   // declared as finalPath
	#else
		return catalog_database{ database, finalPath };
	#endif
```

`final_path` is declared nowhere. The BPR-0155 gate could not see it: the
parameter rule only knows about `aXxx`, and this is a local.

## Two more shapes

**A name under a different spelling.** The branch says `expected_bytes` where
the declaration reads `aExpectedBytes`, or `final_path` where the local is
`finalPath`. Dropping the `a` prefix is one way a rename stops halfway;
changing the case is another, and the first rule only knew the first.

**A name declared by some branches of a chain and used after the `#endif`.**

```cpp
	#if defined(__linux__)
		const auto* const requested_vfs = (*retained_vfs)->name();
	#else
		constexpr const char* requestedVfs = nullptr;
	#endif
		...
		auto created = create_bootstrap(*version, aStop, requestedVfs);
```

This one is worse than an undefined name, because nothing looks undefined.
Both spellings exist, each in its own branch, and the code compiles wherever
the `#else` is taken. Only a compiler on the other platform ever finds out.

## Required direct evidence

- Every site the two new rules find is repaired, and the MinGW cross-compile
  still passes where it can reach.
- Each new rule is shown to fail when one site is reverted, separately.
- The report names the spelling to change to, not merely the one that is
  wrong — a suggestion that sends the reader to the wrong edit is worse than
  no suggestion.
- What no rule here can reach is named.

## What was done

Eleven more sites, in five files:

| File | Was | Is |
|---|---|---|
| `installed_git_process.cpp` | `expected_bytes` ×2, `process_id`, `system_root` ×2 | `aExpectedBytes`, `aProcessId`, `systemRoot` |
| `installed_git_cache.cpp` | `link_like` | `linkLike` |
| `catalog_database.cpp` | `requested_vfs`, `final_path` | `requestedVfs`, `finalPath` |
| `github_cli_process_runner_tests.cpp` | `unknown_name`, `exact_argument_count`, `invalid_child_name` | `aUnknownName`, `exactArgumentCount`, `invalidChildName` |
| `platform_test_support.hpp` | `path`, `suffix` | `aPath`, `aSuffix` |

The last two were found only after the report was corrected to name the
parameter rather than its bare form. The first rule had skipped them because
the branch appeared to declare a local of that name; the spelling rule does not
make that mistake.

Each rule was reverted at one site in turn:

| Reverted | Report |
|---|---|
| `aExpectedBytes` → `expected_bytes`, `installed_git_process.cpp:317` | ``` `expected_bytes` is used where the declared name is `aExpectedBytes` ``` |
| `requestedVfs` → `requested_vfs`, `catalog_database.cpp:3951` | ``` `requestedVfs` is used where the declared name is `requested_vfs` ``` at line 3978, the use after the `#endif` |

The second is the one no cross-compiler here reports: `catalog_database.cpp`
compiles clean for Windows either way, because the `#else` that declares
`requestedVfs` is the branch Windows takes.

## What this still cannot reach

A rename that changes a name rather than its spelling. If `finalPath` had
become `catalogPath`, nothing here would relate the two, and only a compiler
for that platform would object. The rules cover drift, not renaming.

Nor does any of this make a Linux build. It removes reasons one would fail;
BPR-0156 still carries actually running it.
