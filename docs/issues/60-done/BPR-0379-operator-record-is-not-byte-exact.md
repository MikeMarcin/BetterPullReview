---
id: BPR-0379
title: The operator validation record is not byte-exact on Windows
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# The operator validation record is not byte-exact on Windows

`operator_response_validation_contract` failed in two places, both in the
attended-review skill, and both about evidence that is supposed to be
byte-identical to what was committed.

## The fixture's own artifacts

```
ERROR: working-tree artifact differs from reviewed commit:
       synthetic-gate-v001/decision-subject.txt
```

`test_validate_operator_packet.py`'s `fixture()` writes the decision subject,
the operator response, and the claims contract with `write_text` and no
`newline`, then commits them. Python wrote CRLF; git normalised to LF on the way
in; the validator compared the working tree against the commit and correctly
reported a difference in a file the fixture had just written and committed
itself.

The operator packet in the same function already used `write_bytes` and was
never affected, which is what made the other three stand out.

## The validator's own record

```
FAIL: existing record revalidates:
      ERROR: working-tree validation record differs from record commit
```

`validate_operator_packet.py:531` wrote `validation-record.json` the same way.
That is worse than a test fixture: the record **is** the evidence. It is
committed and then revalidated against its own commit, so on Windows the tool
rejected the record it had itself produced one step earlier.

Both now pass `encoding="utf-8", newline=""`. The encoding is explicit for the
same reason as the newline — evidence that varies with the writer's locale is
not evidence.

`.gitattributes` cannot cover either case: these files are produced at run time
into a scratch repository, not checked out. [[BPR-0374]] covers the checked-in
half of this class, and this is the sixth and seventh place the run-time half
has needed fixing.

## Result

```
py -3 .agents/skills/bpr-attended-review/scripts/test_validate_operator_packet.py
OPERATOR_PACKET_VALIDATION_TESTS_OK
```

## Still fails under the launcher, and not for anything above

```
subprocess.CalledProcessError: Command '['git', 'init', '-q']' returned 128
fatal: unable to get current working directory: Permission denied
```

That is [[BPR-0362]], reached through `git` rather than through our own code.
The finding is recorded there because it changes what that card's options can
achieve.
