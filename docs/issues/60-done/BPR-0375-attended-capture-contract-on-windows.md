---
schema: "implement-rfc.card.v1"
id: "BPR-0375"
title: "The attended capture contract fails on Windows in two ways"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: []
---

# The attended capture contract fails on Windows in two ways

`rfc0016_device_capture_security` failed with one failure and two errors, from
two unrelated causes.

## One — the review page could never match its own rebuild

```
ContractError: attended review page does not match official rebuild
```

The message names a tampering check, and nothing was tampered with.

`build_attended_review.py:826` wrote the page with
`write_text(render_html(...), encoding="utf-8")` and no `newline`, so on Windows
it emitted CRLF. `_validate_review_page` rebuilds the page from the same inputs
and compares bytes, and it reads the CSS and runtime with `read_text`, whose
universal-newline handling always yields LF. So the rebuild was LF, the artifact
was CRLF, and they could never agree.

This is worse than a failing test. The page and its manifest are **digest-bound**:
an operator verdict is recorded against their exact bytes. A builder that emits
different bytes per platform makes the verdict bind to something no other
machine can reproduce.

`newline=""` on both writes. The manifest gets it too, for the same reason and
by the same argument.

`.gitattributes` alone would not have fixed this: these files are generated at
run time, not checked out, so the writer has to be explicit. [[BPR-0374]] covers
the checked-in half.

## Two — a symlink adversary the platform will not allow

```
OSError: [WinError 1314] A required privilege is not held by the client
```

Two cases stage a symbolic link as an adversary the validator must reject.
Creating one on Windows needs `SeCreateSymbolicLinkPrivilege`, which an
unelevated account does not hold unless Developer Mode is on. The adversary
could not be constructed, so the case errored as though the product had failed.

`link_or_skip` raises `unittest.SkipTest` with a message naming what is no
longer covered, and re-raises anything that is not `winerror == 1314`. That is
the same disposition
`tests/sqlite_contract/epoch_catalog_storage_tests.cpp:795` already takes, in
almost the same words — a skip that says what it gave up, rather than a silent
pass or a misleading failure.

## Result

```
py -3 tests/scripts/rfc0016_device_capture_tests.py
OK (skipped=3)
RFC0016_DEVICE_CAPTURE_VALID:attended_human_accepted_capture
```

`rfc0016_device_capture_security` still fails **under the launcher**, at
`REPOSITORY_ROOT.resolve(strict=True)` in
`scripts/capture_rfc0016_device_contracts.py:2076`. That is [[BPR-0362]]'s
ancestor-read denial and is the only thing left between this test and green.

## Not in scope

`rfc0016_device_live_runner_security`, which fails for a third reason entirely:
`scripts/run_rfc0016_device_capture_attended.py` calls `os.getuid()` and imports
`pwd` in four places. [[BPR-0376]] carries it.
