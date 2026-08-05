---
id: BPR-0132
title: Share the hardened state-directory bootstrap between stores
status: Done
rfc: RFC-BPR-0007
depends_on: [BPR-0128]
---

# Share the hardened state-directory bootstrap between stores

The epoch catalog creates `state/v1` through a platform-specific bootstrap that
sets an explicit owner-only ACL on Windows, verifies ownership and mode on
POSIX, refuses symlinked or replaced directories, and cleans up orphaned
bootstrap artefacts.

The review ledger store added in BPR-0128 does not use it. It calls
`create_directories` and narrows the mode with `chmod`, because it must not
depend on the catalog having been composed first — an adapter that only works
when another adapter ran before it is a latent ordering bug.

The result is two directory-creation policies for one directory, and the weaker
one wins whenever the ledger is composed first. That is the same class of
problem the connection-policy extraction fixed, and it deserves the same
treatment.

## Required direct evidence

- One bootstrap creates `state/v1`, and both stores use it; no second
  `create_directories` for that directory remains.
- The hardening is asserted after the ledger composes it first, not only after
  the catalog does — the ordering that currently produces the weaker directory.
- A symlinked or wrong-mode directory is refused by whichever store opens
  first.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What was done

`detail::open_state_directory` is the one bootstrap. Both stores call it, and
no `create_directories` for `state/v1` remains anywhere in the adapter.

It is declared in `state_directory.hpp` and defined in `catalog_database.cpp`,
beside the primitives it composes, rather than moved into a translation unit of
its own. That was measured rather than assumed: every primitive the chain needs
— the handle types, the ownership and mode checks, the ancestor validation, the
security descriptor — is used more outside the bootstrap than inside it, by the
catalog's lock, files, and VFS. On Windows the counts run 2 to 3 times against
the move. Relocating them would have produced a component named for the state
directory that was really the adapter's platform layer.

The ordering worry in this card is about composition, not linkage. Calling a
function defined in another translation unit requires nothing to have run
first; that is what made the ledger's separate copy unnecessary rather than
merely redundant.

`state_directory` holds the handles rather than releasing them. A directory
validated and then let go can be replaced before it is used.

### A behaviour change worth stating

The ledger now refuses a storage root that is not owner-private, because the
catalog always did and there is one policy now. In production this is already
true: `prepare_application_data_root` sets `owner_all` before either store is
composed. A caller passing a public root used to get a working ledger and now
gets a refusal, which is the point.

The test harness had to be corrected for the same reason. Its scratch directory
came from `temp_directory_path`, which on macOS is reached through a symbolic
link, so neither store would accept it as a root. It now canonicalizes, as
`prepare_application_data_root` does.

### Evidence

Four tests, each run in both composition orders where order can matter:

| Test | Asserts |
|---|---|
| ledger composes first | `state/v1` is a real directory, mode 0700, owned by this user, and the catalog opens what the ledger made |
| symlinked `state/v1` | refused, ledger first and catalog first |
| group-readable `state/v1` | refused, ledger first and catalog first |
| public storage root | refused by both |

Four breaks, each one line, each landing on its own assertion:

| Break | First failure |
|---|---|
| `mkdirat` creates `0755` | `the ledger composes in a root no other store has touched` |
| `openat` drops `O_NOFOLLOW` | `the ledger refuses a symlinked state directory` |
| `ensure_private_child` drops the mode check | `the ledger refuses a group-readable state directory` |
| `open_private_directory` drops the root check | `the ledger refuses a root it cannot call private` |

### Windows

Compile only. `x86_64-w64-mingw32-g++ -fsyntax-only` accepts the rewritten
`catalog_database.cpp`, so the Windows branch of the shared bootstrap is
syntactically whole and its callers agree with it. Nothing here shows the ACL
it sets is the ACL Windows applies. That belongs to BPR-0053, and the mode
assertions above are guarded to POSIX rather than pretending to cover it.
