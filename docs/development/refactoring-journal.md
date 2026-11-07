# Refactoring journal

One line per behavior-preserving restructuring round; linked cards hold scope, line counts and checks.

- 2026-11-06 — [[BPR-1905]]: Move the signed-out public-preview wire transport, its single-attempt self-check and the bounded theme document file reader out of `github_application.cpp` into three focused sources under `src/app/src/github_application/` (42,564 to 41,674 lines; new files at most 403), with the same composition, counters and file handling.
- 2026-11-06 — [[BPR-1906]]: Split `github_application.cpp`'s composition adapters into ten responsibility files under `src/app/src/github_application/` (41,674 to 38,467 lines; every new file under 420), generated from the AST with conditions, comments and behavior unchanged.
- 2026-11-06 — [[BPR-1907]]: Split the GitHub profile runtime out of `github_application.cpp` into a declaration header and five responsibility sources, and give the root's shared helpers their own files (38,470 to 35,440 lines); the tests-off Release self-test now guards the CLI offer's compile-time switch.
- 2026-11-06 — [[BPR-1908]]: Split the window composition (`github_application::implementation`) into a declaration header and sixteen responsibility sources, keeping the security-pinned mutation span in the root (35,440 to 17,481 lines); the mutation-branch pin moved only by one indentation tab.
- 2026-11-06 — [[BPR-1909]]: Move the tests-only fixture accessors into fourteen course files plus two shared fixture-support headers (17,481 to 3,161 lines), all under `BPR_BUILD_TESTING`; a 360-row integration pass shows no regression.
- 2026-11-06 — [[BPR-1910]]: Finish `github_application.cpp`: startup, desktop and window members move to their own sources and the root keeps only the boundary reader and the security-pinned review-mutation span with the 43 includes it uses (3,161 to 1,502 lines; 42,564 at the start).
- 2026-11-06 — [[BPR-1911]]: Split `epoch_application_host.cpp`'s pimpl into a declaration header and seven responsibility sources with one shared support header (11,625 to 411 lines; every new file under 1,650).
- 2026-11-07 — [[BPR-1920]]: Split `workbench_window.cpp`'s out-of-line members into thirteen responsibility sources and one support header (21,476 to 340 lines); only the single 1,873-line `build_central_surface` file exceeds 2,000.
