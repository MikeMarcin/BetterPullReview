---
id: BPR-0344
title: Resolve the queue composition first head differing by filesystem
status: Ready
rfc: RFC-BPR-0010
depends_on: []
---

# Resolve the queue composition first head differing by filesystem

`application_queue_composition_contract` fails on Linux with:

```
queue seed failed: queue-composition-failed:first-head-is-exact-and-quiet
```

The mechanism is not yet identified. What is established is that the outcome
depends on which filesystem the application data root is on, and on nothing
else that was varied.

## Reproduction

```sh
Q=build/dev/tests/end_to_end/better_pull_review_queue_composition_public_path
QT_QPA_PLATFORM=offscreen QT_STYLE_OVERRIDE=Windows "$Q" seed <root>
```

| root | result |
|---|---|
| `/tmp/...` (tmpfs) | `QUEUE_COMPOSITION_SEED_OK` |
| `/home/mmarcin/...` (ext4) | `first-head-is-exact-and-quiet` |
| the registered build-tree root (ext4) | `first-head-is-exact-and-quiet` |

Unlike BPR-0341 this does not depend on `TMPDIR`. A tmpfs root passes with
`TMPDIR` on ext4, and an ext4 root fails with `TMPDIR` on ext4, so the two
findings have different causes and should not be merged.

The test sandbox is not involved; the failures above are direct runs.

## What was ruled out

Timestamp granularity and small-directory iteration order were compared
between the two filesystems and matched: both report nanosecond `mtime`, and
both returned the same order for a three-entry directory. Neither is therefore
the explanation, though ext4's hashed ordering only diverges on larger
directories and has not been excluded at the sizes this contract creates.

## Required final state

- The mechanism is identified rather than worked around. A queue whose first
  head depends on the filesystem is either reading an order the filesystem is
  not required to provide, or depending on a metadata property that varies.
- The contract passes with its registered build-tree root on a stock Ubuntu
  26.04 host.
- Whatever ordering the queue requires is established by the core and asserted,
  not inherited from directory iteration.
