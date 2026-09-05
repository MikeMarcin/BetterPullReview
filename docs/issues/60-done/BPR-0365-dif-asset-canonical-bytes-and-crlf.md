---
schema: "implement-rfc.card.v1"
id: "BPR-0365"
title: "The Dif asset canon compares bytes a CRLF checkout has already changed"
status: "Done"
rfcs: ["RFC-BPR-0019"]
depends_on: []
---

# The Dif asset canon compares bytes a CRLF checkout has already changed

[[BPR-0359]] fixed five files whose exact SHA-256 is pinned by a constant, and
claimed the sweep was complete:

> A repo-wide sweep for `file(SHA256`, `file(MD5`, `hashlib.sha256`, and
> `hashlib.md5` found no sixth case.

That claim was too narrow and is corrected here. It swept for *digest against a
constant*. It did not sweep for *byte equality against regenerated content*,
which is the same defect with no digest anywhere in it.

`scripts/dif_assets.py:121` does exactly that:

```python
raw = path.read_bytes()
...
if raw != canonical_json_bytes(value):
    raise AssetError(f"{path.name} is not canonical UTF-8 JSON")
```

`canonical_json_bytes` ends every document with a literal `"\n"`. A CRLF working
tree can never equal it, so the canon rejects its own checked-in assets on
Windows:

```
verify_dif_asset_boundaries.BoundaryError: Dif asset validation failed:
dif-sprite-manifest.json is not canonical UTF-8 JSON
```

## Measured

Five text files under `src/adapters/qt/assets/dif/` differ between the blob and
the working tree. The PNGs do not, because git already detects them as binary:

| file | `git ls-files --eol` | drifts |
|---|---|---|
| `dif-sprite-manifest.json` | `i/lf w/crlf` | yes |
| `provenance.json` | `i/lf w/crlf` | yes |
| `generated/dif-sprite-digests.json` | `i/lf w/crlf` | yes |
| `generated/dif-sprite-catalog.hpp` | `i/lf w/crlf` | yes |
| `README.md` | `i/lf w/crlf` | yes |
| `generated/*.png`, `source/**/*.png` | `i/-text w/-text` | no |

## Resolution

The same one [[BPR-0359]] chose, for the same reason: pin the affected paths
`text eol=lf` so the working tree matches the blob on every platform, rather
than teaching the checker to normalize. Normalizing would redefine the canon
from "these exact bytes" to "these bytes modulo a transformation", and a canon
whose whole job is byte-exact regeneration cannot afford that.

The PNGs are deliberately not listed. `text eol=lf` on a binary would corrupt
it, and git already classifies them correctly.

## Required final state

- The five text files match their blobs on Windows.
- `dif_assets.py`'s canonical check passes against the checked-in tree on
  Windows, macOS, and Linux.
- [[BPR-0359]]'s sweep claim is corrected rather than left standing, and the
  corrected sweep covers byte-equality checks as well as digest-versus-constant.
- No PNG gains a text attribute.

## Not in scope

`dif_assets_contract` will still fail after this, on the temporary-directory
defect [[BPR-0350]] recorded and could not fix inside the tool's
`ALLOWED_TOOL_IMPORTS` boundary. This card fixes the canon, not that.

## Verification

```sh
git ls-files --eol src/adapters/qt/assets/dif/
py -3 scripts/dif_assets.py verify --root src/adapters/qt/assets/dif
py -3 scripts/verify_dif_asset_boundaries.py --root .
```

## Result

The five paths are pinned `text eol=lf`, re-checked out, and now match their
blobs exactly. `git ls-files --eol` reports `i/lf w/lf attr/text eol=lf` for all
five, and `i/-text w/-text` for every PNG, which is what should not change.

`load_canonical_json` accepts all three documents:

```
CANONICAL  dif-sprite-manifest.json
CANONICAL  provenance.json
CANONICAL  generated/dif-sprite-digests.json
```

`dif_assets.py validate` no longer reports `dif-sprite-manifest.json is not
canonical UTF-8 JSON`. It now stops later, at
`source/dif-neutral-master.png is not a canonical product PNG`.

That next failure is **not** this card's and not caused by it. Verified by
stashing the change and re-running: the same PNG error appears without it. It is
recorded in [[BPR-0347]] as a separate open Windows cause. A plausible line of
enquiry, not measured here: the canon recompresses with `zlib`, and zlib output
is not guaranteed identical across versions, so a "canonical PNG" defined by
re-compression may be platform-dependent in the same way the JSON canon turned
out to be checkout-dependent.

[[BPR-0359]]'s sweep claim is corrected on that card.

`dif_assets_contract` still fails, on the temporary-directory defect
[[BPR-0350]] recorded and could not fix inside the tool's
`ALLOWED_TOOL_IMPORTS` boundary, and now on the PNG canon above.
