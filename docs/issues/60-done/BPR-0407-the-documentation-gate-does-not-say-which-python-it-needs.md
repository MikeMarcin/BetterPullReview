---
id: BPR-0407
title: The documentation gate does not say which Python it needs
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# The documentation gate does not say which Python it needs

Found by [[BPR-0256]]'s capability-critical review while it was reproducing this
repository's own recorded evidence, which is the right way for it to have been
found.

`AGENTS.md` and nearly every issue card name the gate as:

```sh
python3 scripts/verify_documentation.py --root .
```

`python3` resolves to whatever is first on `PATH`. On macOS the system
interpreter is `/usr/bin/python3`, which is 3.9.6, and the gate does this:

```
File "scripts/verify_documentation.py", line 552, in closeout_table
    rows.append(dict(zip(CLOSEOUT_COLUMNS, values, strict=True)))
TypeError: zip() takes no keyword arguments
```

`zip(strict=True)` is 3.10. The script has no `python_requires`, no version
guard, and no stated requirement anywhere.

## What is and is not wrong here

**It fails closed**, and that matters. The real exit status is `1`, not `0` — I
checked with the pipe removed, because a pipeline reports the pipe's status and
would have shown `0`. So no session has ever been told `DOCUMENTATION_OK` by an
interpreter that could not run the gate.

What is wrong is the diagnostic and the silence. A contributor or agent whose
`PATH` puts the system Python first gets a `TypeError` from inside a table
parser, which reads like a malformed closeout card rather than a wrong
interpreter. The correct reaction — install or select a newer Python — is not
recoverable from the message.

There is a second-order cost specific to this repository. The gate is the
evidence a card cites when it moves to `60-done/`, and evidence whose result
depends on an unstated property of the invoking environment is weaker evidence
than it looks. Nothing here was actually recorded wrongly; the point is that the
board could not have told.

## Required final state

- Running the gate under an interpreter older than it needs prints one clear
  line naming the required and the found version, and exits nonzero.
- The same holds for `tests/scripts/verify_documentation_tests.py`, which is the
  other command every card names.
- The requirement is stated in `AGENTS.md` beside the command.
- The guard cannot itself use syntax newer than the version it is guarding
  against, or it fails before it can explain anything.
- Behaviour under a supported interpreter is unchanged: `DOCUMENTATION_OK` and
  94 tests, exactly as before.

## Non-goals

- Do not lower the requirement by rewriting `zip(strict=True)` and whatever else
  depends on 3.10+. The strictness is doing real work in a file whose job is
  catching mismatched tables.
- Do not audit every script in `scripts/`. This card covers the two commands the
  board actually cites; a wider sweep is separate work if anyone wants it.
- Do not touch anything under `src/`. See the dev-binary hold in the board
  README.

## Verification

```sh
/usr/bin/python3 scripts/verify_documentation.py --root .          # clear refusal, nonzero
/usr/bin/python3 tests/scripts/verify_documentation_tests.py       # clear refusal, nonzero
python3 scripts/verify_documentation.py --root .                   # DOCUMENTATION_OK
python3 tests/scripts/verify_documentation_tests.py                # 94 tests, OK
git diff --check
```

The first two must be run without a pipe. Piping a gate through `tail` reports
the pipe's exit status, not the gate's, which is how this defect's severity would
have been misread.

## Result — 2026-08-19

| claim | surface | command | proves it | result | evidence |
|---|---|---|---|---|---|
| an unsupported interpreter is refused clearly, not with a traceback | the gate | `/usr/bin/python3 scripts/verify_documentation.py --root .` | the guard runs before any 3.10 syntax is reached | `requires Python 3.10 or newer; this is 3.9.6 at /Applications/Xcode.app/…/python3`, exit **2** | this commit |
| the same holds for the contract suite | the test entry point | `/usr/bin/python3 tests/scripts/verify_documentation_tests.py` | it imports the guarded module, so it needs its own guard to report before the import | same message, exit **2** | this commit |
| supported behaviour is unchanged | the gate | `python3 scripts/verify_documentation.py --root .` under 3.14.6 | it still validates the whole board | `DOCUMENTATION_OK` | this commit |
| the suite is unchanged | `tests/scripts/verify_documentation_tests.py` | `python3 tests/scripts/verify_documentation_tests.py` | 94 cases | 94 tests, OK | this commit |
| the requirement is written where the command is | `AGENTS.md` | read it | the Commands block is what a session copies from | stated, with both commands | this commit |
| no whitespace damage | Git | `git diff --check` | it fails on trailing whitespace and conflict markers | clean | this commit |

Exit `2` rather than `1` on purpose: `1` is what the gate returns when the board
is wrong, and "your interpreter is too old" is a different answer from "your
documentation is invalid".

### The severity, stated accurately

The defect was never a wrong `DOCUMENTATION_OK`. Measured with the pipe removed,
the old failure exited `1` — fail-closed, so no card was ever admitted on
evidence from an interpreter that could not run the gate. What was wrong was that
the message pointed at the board instead of at the interpreter.

That distinction was only visible because the check was run without a pipe. The
first reading of it, through `| tail -6`, reported `exit=0` — which is the pipe's
status and would have made this look like a silent pass. The card's verification
block says to run it bare for that reason.

### Scope deliberately left

Nothing else in `scripts/` was audited. Any other script with the same exposure
is real and separate; this card covers the two commands the board actually cites
as evidence.
