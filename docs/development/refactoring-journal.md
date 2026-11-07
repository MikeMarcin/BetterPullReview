# Refactoring journal

One line per behavior-preserving restructuring round; linked cards hold scope, line counts and checks.

- 2026-11-06 — [[BPR-1905]]: Move the signed-out public-preview wire transport, its single-attempt self-check and the bounded theme document file reader out of `github_application.cpp` into three focused sources under `src/app/src/github_application/` (42,564 to 41,674 lines; new files at most 403), with the same composition, counters and file handling.
