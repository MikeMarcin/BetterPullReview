---
id: BPR-0377
title: The Dif tool's scratch root is unusable when contained
status: Done
rfc: RFC-BPR-0019
depends_on: []
---

# The Dif tool's scratch root is unusable when contained

`scripts/dif_assets.py` used `tempfile.TemporaryDirectory`, which calls
`os.mkdir(path, 0o700)`, which modern CPython turns into a protected Windows
descriptor an AppContainer's lowbox token cannot read back. The tool was denied
the directory it had just created.

[[BPR-0350]] fixed this everywhere else with a shared helper, converted this
tool along with the rest, and then **reverted** that one conversion:
`verify_dif_asset_boundaries.py` holds `ALLOWED_TOOL_IMPORTS` to a closed stdlib
set and rejected the import with `Dif asset tool imports unapproved capability:
scripts`. That entry is a per-module capability boundary; widening it to the
package would let this tool import anything in the repository, so the boundary
was kept and the conversion backed out. [[BPR-0350]] recorded that the tool
needed its own answer.

This is that answer, and it needed no capability at all.

## Measured

Inside the launcher:

```
Path.mkdir() default mode : USABLE ['f.txt']
Path.mkdir(mode=0o700)    : DENIED
```

`pathlib` and `tempfile` are both already on the allowed list. A directory made
with the default mode inherits the parent's entries, which include the
container's own grant, so the tool can create and use a scratch root without
gaining a single new import.

`_make_scratch_root` uses `tempfile.gettempdir()` and `Path.mkdir()` with a
counter that retries on `FileExistsError`. `_remove_tree` walks the tree with
`pathlib` alone, because `shutil` is not on the list either. The call site moved
from a `with` block to `try`/`finally` for the same reason — `contextlib` is not
allowed.

The boundary is unchanged. `ALLOWED_TOOL_IMPORTS` has exactly the same members
it had before.

## Result

`Dif asset tool imports unapproved capability` is gone, and so is the
`PermissionError` underneath it. The tool now reaches its actual work.

Two failures remain in `dif_assets_contract` and neither is this:

- `source/dif-neutral-master.png is not a canonical product PNG` — the asset
  canon re-encodes and compares bytes, and this host's Python bundles zlib-ng.
  [[BPR-0378]] carries it.
- `a C++ compiler is required for the catalog consumer test` — that case needs a
  compiler on `PATH`, which it has under CTest inside the developer environment
  and not in a bare shell.

## Verification

```sh
py -3 tests/scripts/dif_asset_tests.py
ctest --preset core-only --output-on-failure -R '^dif_assets_contract$'
```
