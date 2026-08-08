# google/UIforETW#74, recorded

A real public pull request, recorded once and replayed. BPR-0214.

| | |
|---|---|
| source | `https://api.github.com/repos/google/UIforETW/pulls/74` |
| retrieved | 2026-08-08 |
| head | `9b01c587b3e46637ff69c3801841993bd054f106` |
| base | `7c519e97eb906bdc6b83c8f576c91c063d6ceee9` |
| shape | 5 files, +46 −6, 1 commit, closed |

## Why a second recording

`directxmath-336` is a **pure addition**: +221 −0. A comparison with no removed
line has no line to pair an added line against, so it cannot produce intraline
emphasis at any zoom, under any palette, on any host. Every capture taken from
it was therefore incapable of showing the word-level highlighting RFC-BPR-0004
specifies, and no reviewer has ever been shown it. That is BPR-0214, and it is
why this fixture exists rather than a wider crop of the old one.

The product owner designated this pull request for it.

## What makes it the right shape

Three modification pairs where the removed and added lines are nearly the same
text:

| file | shared | differs |
|---|---|---|
| `UIforETW/Resource.h` | 98% | 1 character |
| `UIforETW/UIforETW.rc` | 99% | 1 character |
| `UIforETW/UIforETW.rc` | 97% | 2 characters |

```
-#define _APS_NEXT_RESOURCE_VALUE        136
+#define _APS_NEXT_RESOURCE_VALUE        137
```

A one-character delta on an otherwise identical line is the clearest case
intraline emphasis has: whole-line background alone says "this line changed",
and only the word-level mark says *what* changed. If the emphasis is missing or
mis-registered, a capture of these rows shows it immediately.

It also keeps a plain addition-only file (`UIforETWDlg.cpp`, +22 −0) and a
mixed one, so a capture can hold added, removed and modified rows together
rather than needing three fixtures to compare them.

## Recorded, not read live

Fetched once with three unauthenticated `GET`s — the pull request, its files,
its commits — then trimmed to what a review surface reads. A test that reaches
the network fails when the network does, and a fixture that changes when
somebody pushes to a branch is not a fixture.

Everything the API returns that a review surface has no use for was dropped:
avatars, node identifiers, reaction counts, and the URL fields on every object.

Grepping the recording finds three URLs rather than one. Two are inside the
diff text — `msdn.microsoft.com` links in comments the pull request itself adds
— so they are reviewed content, not API metadata. Outside the patches the
provenance line is the only URL, and the validator holds that distinction rather
than counting URLs in the file as a whole.

**No credential is involved.** These are unauthenticated reads of public data,
and nothing here is written back.
