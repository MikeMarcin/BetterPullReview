---
id: BPR-0372
title: The GitHub fixture contract fails on Windows for two separate reasons
status: Done
rfc: RFC-BPR-0002
depends_on: []
---

# The GitHub fixture contract fails on Windows for two separate reasons

`github_contract_fixture_security` failed with four cases down and two
unrelated causes underneath them. They are recorded separately because they are
separate, and because only one of them is a defect in this repository's own
code.

## One — the fixture tree is compared with the wrong separator

```
Items in the first set but not the second:
  'github-com\\supported.json'
  'ghes-3.18\\manifest.json'
Items in the second set but not the first:
  'github-com/supported.json'
  'ghes-3.18/manifest.json'
```

The expected sets are literals written with `/`. The actual sets were built
with `str(path.relative_to(root))`, which yields the native separator, so on
Windows every entry differed from its counterpart and the sets shared nothing.

`Path.as_posix()` at the three sites that build those strings. The expected
literals are unchanged, and the comparison now means the same thing on every
platform.

## Two — the fixtures' bytes are pinned, and a CRLF checkout changes them

```
AssertionError: '4ae279ce6582cc8b405af39c88eec4a716c5d01c25f02c2096d0f98849c3e435'
             != '07bb01a60bf0c640d1b98bce84a7f405503421fafaa2b50fd753e63b2dedfa01'
```

`hashlib.sha256((host_root / "supported.json").read_bytes())` against a pinned
constant. 44 files under `tests/fixtures/github/` differ between blob and
working tree — 42 JSON and 2 Markdown. The one binary in that tree, the
`objects.pack`, does not: git already classifies it `-text`, and it is
deliberately not listed.

Pinned `text eol=lf`, exactly as [[BPR-0359]] and [[BPR-0365]] did for their
files, and for the same reason: normalizing inside the checker would redefine a
pin from "these exact bytes" to "these bytes modulo a transformation".

## Recurrence

This is the **fourth** distinct place the same checkout defect has surfaced:

| card | what was pinned |
|---|---|
| [[BPR-0359]] | the designated-live canary surface and the app icon |
| [[BPR-0365]] | the Dif asset canon |
| [[BPR-0371]] | fixtures the architecture-boundary tests write at runtime |
| this card | the GitHub contract and capability fixtures |

Four times is a pattern rather than a coincidence, and the per-file pinning used
so far only fixes the instances someone has already tripped over. The candidate
policy fix is `* text=auto eol=lf` in `.gitattributes`, which would make the
working tree LF on every platform and retire the class — with `*.bat` and
`*.cmd` kept as CRLF, because the Windows command interpreter requires it.

That is deliberately **not** done here. It rewrites the working tree of every
file in the repository on Windows, and it is a repository-wide policy rather
than a repair, so it belongs to the owner rather than to this card. The evidence
for it is above.

## Result

```
py -3 tests/scripts/github_contract_fixture_security_tests.py
Ran 16 tests in 0.266s
OK
```

No file in `tests/fixtures/github/` differs from its blob any more, and the
`objects.pack` still reports `i/-text w/-text`.
