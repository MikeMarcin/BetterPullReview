---
id: BPR-0439
title: The syntax classification contract expects a POSIX-shaped Qt test sandbox
status: Ready
rfc: RFC-BPR-0006
depends_on: []
---

# The syntax classification contract expects a POSIX-shaped Qt test sandbox

Uncontained on the Windows host, at `22b991c`:

```
syntax_classification_contract
FAILED: the generic data location 'C:/Users/mikem/AppData/Local/qttest' is outside the Qt test sandbox 'C:/Users/mikem/.qttest'
```

The contract checks that `QStandardPaths` test mode keeps the classifier's
data inside a sandbox, and it computes the sandbox as `<home>/.qttest`. That
is where Qt puts it on macOS and Linux. On Windows Qt's test mode puts it at
`%LOCALAPPDATA%/qttest`, which is exactly where the generic data location
was found. The classifier is sandboxed; the expectation named the wrong
sandbox.

Contained, the same assertion names `…/Packages/bprautomatedtest/AC/qttest`
against the same `C://.qttest`, because the container redirects
`%LOCALAPPDATA%` — so the repair has to derive the sandbox from Qt rather
than from a home directory.

## Required final state

- The contract asks `QStandardPaths` for the sandbox root it will use under
  test mode on the running platform and checks containment against that,
  rather than composing a POSIX path.
- The check still fails when test mode is off or the location escapes the
  sandbox, which is the property it exists for.
- `syntax_classification_contract` passes on Windows, contained and not,
  and is unchanged on macOS and Linux.

## Verification

```sh
ctest --preset windows-dev --output-on-failure -R '^syntax_classification_contract$'
```
