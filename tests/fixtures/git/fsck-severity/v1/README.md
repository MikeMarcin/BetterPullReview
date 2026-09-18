# Deterministic fsck-severity packs

Three tiny secret-free SHA-1 packs that pin what the Git cache import does with
Git's own fsck severities ([[BPR-0880]]).

| Pack | Object | Git's default severity | The import must |
|---|---|---|---|
| `warning-class.pack` | a tree whose file mode is `0100644` | `zeroPaddedFilemode` (WARN) | accept it |
| `information-class.pack` | a tree with a symlinked `.gitignore` | `gitignoreSymlink` (INFO) | accept it |
| `error-class.pack` | a tree naming `fixture.txt` twice | `duplicateEntries` (ERROR) | refuse it, by name |

The information class fails differently and is here for that reason: it never
made `index-pack` exit nonzero, it made it write a `warning:` line, and an
accepted exit that wrote to standard error is malformed output to this adapter.
The repository was unopenable either way.

All three packs hold the same three objects — one blob `fixture\n`, one tree, one
parentless commit — so each is self-contained and connected and exercises the
strict and the sparse import path equally. Author and committer are
`BetterPullReview Fixture <fixture@betterpullreview.invalid>` at `1704067200
+0000`.

## Why these are committed rather than built on the fly

`git mktree` normalises a zero-padded file mode away and `git hash-object`
will not write a tree with a duplicate entry, so those objects cannot be built
with ordinary Git commands. The generator writes the object and pack bytes
directly, which is also why the recipe does not depend on the Git or zlib
version of whoever runs it.

## Rebuilding

```sh
python3 tests/scripts/fsck_severity_fixture.py \
    --write tests/fixtures/git/fsck-severity/v1
```

That rewrites `warning-class.pack`, `error-class.pack` and `manifest.json`.
Regeneration is byte-exact for a given zlib; the object identities in the
manifest are not, and the verifier checks those separately so a different
zlib shows up as a changed pack file rather than as a silently different
fixture.

## Checking

```sh
python3 tests/scripts/fsck_severity_fixture.py \
    --fixture tests/fixtures/git/fsck-severity/v1 \
    --git "$(command -v git)" \
    --source src/adapters/git/src/installed_git_process.cpp
```

The `fsck_severity_fixture_contract` test runs exactly this. It rebuilds the
recipe and compares the object identities, checks the frozen bytes against the
recorded size, SHA-256 and pack checksum, and then runs the installed Git to
prove three things:

- a bare `--strict` import still names each pack's finding, and fails in the way
  that pack's severity is supposed to fail, so the fixture is not vacuous;
- the closed ignore list the adapter ships accepts the warning- and
  information-class packs on `--strict`, and on `--fsck-objects` where the
  installed Git is new enough to take a severity there, writing nothing to
  standard error either way;
- the error-class pack is still refused on both, naming `duplicateEntries`;
- every message identifier in that list is one the installed Git knows.

The last point is the cross-version guard. Git answers an identifier it does
not recognise with `fatal: Unhandled message id`, which would fail every
import, so a Git too old for a shipped identifier fails this row on that
station instead of failing the product in front of a reviewer. It does not
show the converse: an identifier the adapter withholds from an older Git may
well be one that Git knows, and lowering its recorded version needs a
measurement taken here.
