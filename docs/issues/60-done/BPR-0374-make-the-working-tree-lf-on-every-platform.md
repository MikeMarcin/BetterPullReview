---
schema: "implement-rfc.card.v1"
id: "BPR-0374"
title: "Make the working tree LF on every platform"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# Make the working tree LF on every platform

`.gitattributes` said `* text=auto`, so git stored LF and handed Windows CRLF.
This repository pins the exact bytes of checked-in files in a lot of places, and
every one of those pins was wrong on Windows and right everywhere else.

[[BPR-0372]] recorded the pattern and deliberately declined to fix it
repository-wide, because rewriting the working tree of every file is a policy
rather than a repair. Two things changed that.

## Six instances, and the sixth binds operator approvals

| card | what a CRLF checkout broke |
|---|---|
| [[BPR-0359]] | the designated-live canary surface digest and the app icon |
| [[BPR-0365]] | the Dif asset canon |
| [[BPR-0371]] | fixtures the architecture-boundary tests stage at runtime |
| [[BPR-0372]] | the GitHub contract and capability fixtures |
| [[BPR-0372]] | the recorded-contract generator, latent rather than failing |
| [[BPR-0375]] | the attended-review page, whose digest binds an operator verdict |

The last one is what settled it. `docs/verification/reviews/` holds
digest-bound attended evidence: 339 files there and in the attended-review
assets differed between blob and working tree. A verdict recorded against bytes
that only one platform produces is not a verdict bound to anything.

## Nothing here needs CRLF

Measured before changing anything:

```
tracked files                     1894
tracked text files, w/crlf        1318
binaries (w/-text)                 470
.bat, .cmd, .sln, .vcxproj, .reg     0
```

The last line is why this is safe. The usual reason to keep CRLF in a tree is
the Windows command interpreter, and there is nothing here it would run. The
470 binaries are detected as binary and are untouched by a text attribute.

Per-file pinning was no longer proportionate either: it had covered 15 files
across four cards, and the exposure was 1318.

## What changed

`* text=auto eol=lf`. That is the whole change.

It rewrites no stored content — git already holds LF for every one of these
files. It makes what is on disk agree with what is already committed, on every
platform, and it is one line to revert.

The per-file `eol=lf` rules from [[BPR-0359]], [[BPR-0365]], and [[BPR-0372]]
are kept even though the global rule now subsumes them. They are not redundant
documentation: each says *why* that particular file's bytes matter, and losing
that would make a future reader think the pins were arbitrary.

## Result

```
working tree files differing from their blob:   0 of 300 sampled  (was ~1318)
py -3 scripts/verify_documentation.py --root .  DOCUMENTATION_OK
ctest --preset core-only                        74 of 85
```

Directly fixed by this alone: the attended review page stopped disagreeing with
its own rebuild for a reason that was never about its content.
