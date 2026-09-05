---
schema: "implement-rfc.card.v1"
id: "BPR-0435"
title: "A developer build's STARTUP_FAILED names nothing"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: []
---

# A developer build's STARTUP_FAILED names nothing

Thirty-five of the fifty failures in the first Windows Qt-graph run
([[BPR-0412]]) were the application printing `STARTUP_FAILED` and exiting in
a tenth of a second — every public-path, composition, appearance, epoch and
render contract, and `runtime_process_surface`, which reported "application
exited before runtime inspection". Run directly, outside the container, with
Qt on `PATH`, `--self-test` and `--github-application-status` did the same.

`main` ends in `catch (...) { STARTUP_FAILED }`. That one word is right for
the attended-approval refusals above it, which deliberately say no more
than a malformed approval would. It is wrong as the only trace of an
exception thrown anywhere in composition: a whole platform's application
tests failed for a reason nothing on this board could name, and the card
that finally named it ([[BPR-0436]]) had to be reasoned from source rather
than read from a log.

## Required final state

- In a `BPR_BUILD_TESTING` build, the catch-all additionally writes
  `STARTUP_FAILED_CAUSE: <what()>` to **stderr**. The verdict on stdout
  stays exactly one word, so no contract that matches it changes, and a
  tests-disabled build prints nothing more than before.
- Only `std::exception` gains a cause; anything else still ends in the bare
  word, as it did.
- The diagnostic is read from a real failure on this host, not inferred.

## Verification

```sh
cmake --build --preset windows-dev
build/windows-dev/src/app/Debug/better_pull_review.exe --self-test --automated-run-budget-ms 30000
```

## Result

The relinked developer build, run directly on the host at 09:04:

```
STARTUP_FAILED_CAUSE: Application data or installed Git is unavailable
STARTUP_FAILED
```

One line on stderr, the same one word on stdout. That line is the message
`github_application` throws when `prepare_application_data_root` returns
empty, and it is what pointed [[BPR-0436]] at the root the product refuses
on Windows. A tests-disabled build compiles the `#if` away and prints
nothing more than before.
