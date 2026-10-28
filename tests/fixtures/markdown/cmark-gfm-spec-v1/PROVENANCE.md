# cmark-gfm test suites (test-only fixture)

Source: https://github.com/github/cmark-gfm at commit
`27d942c8b0a62d192f616e5bf3578f4b6a89e180` (2026-09-28), files `test/spec.txt`,
`test/extensions.txt` and `test/regression.txt`, and the repository's
`COPYING`. All four are copied unmodified.

| File | SHA256 | Licence |
| --- | --- | --- |
| spec.txt | 7d8e5814befec287ac116786d81ff14e0adc9b13295b4494649e995408fd871c | CC BY-SA 4.0 (its front matter), GitHub Flavored Markdown Spec 0.29, after John MacFarlane's CommonMark spec |
| extensions.txt | a2a45e98be9fca95f564f927265a0f63beea6cae5369d1cf4bde44caa51b2a3a | CC BY-SA 4.0 (its front matter), Yuki Izumi |
| regression.txt | 0920d7e8134d5e4ec3693f3f52d904b42de45e23a66c29f54fe714dc54213fd9 | BSD-2-Clause, cmark-gfm's `COPYING` |
| COPYING | c22e885f33b821bddb24cf007145e5540655b6c0f403e49e6c76a93c28e6d9a9 | cmark-gfm's licence notices, kept for `regression.txt` |

CC BY-SA 4.0: https://creativecommons.org/licenses/by-sa/4.0/. The two spec
files are kept unchanged with their attribution and licence in place. Only
`tests/adapters_qt/markdown_cmark_spec_tests.cpp` reads them, at test time;
nothing is built from them, linked into the application or installed, and
no part of them is copied into another file. `known-differences.json` is
ours: example names and our reasons, no spec text.

Updating: replace the four files from a newer commit, update this record,
and run `qt_markdown_cmark_spec` with `BPR_CMARK_SPEC_DUMP=<file>` to see
each difference, then classify it in `known-differences.json`
([[BPR-1793]]).
