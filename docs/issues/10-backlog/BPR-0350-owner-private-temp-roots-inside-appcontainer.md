---
id: BPR-0350
title: Owner-private temporary roots are unusable inside an AppContainer
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0347]
---

# Owner-private temporary roots are unusable inside an AppContainer

[[BPR-0347]] put the Windows suite inside an AppContainer and 60 of 84 tests
now pass. 17 of the 24 remaining failures are one defect with one cause.

A Windows DACL that grants only the invoking user is not sufficient for a
process running under a lowbox token. That access check additionally requires
the object to grant the container's own package SID, or `ALL APPLICATION
PACKAGES`. A directory made "private to the owner" the ordinary way therefore
becomes private from the owner as well, and the contained test cannot use the
directory it just created.

Measured on the failing host: a directory created by the contained process with
plain `os.mkdir` inherits `(I)(OI)(CI)(M)` for the container SID and works,
while one created alongside it as owner-private carries a **protected** DACL of
`SYSTEM`, `Administrators`, and `OWNER RIGHTS` with no inherited entries, and
every later access to it fails with `WinError 5`. The directory's owner is the
user rather than the package SID, so the `OWNER RIGHTS` entry cannot satisfy
the restricted pass either.

This lands twice, from two independent directions.

## CPython's temporary directories

`tempfile.mkdtemp` — and so `tempfile.TemporaryDirectory` — calls
`os.mkdir(path, 0o700)`, and modern CPython translates that POSIX mode into
exactly the protected owner-only DACL above. Roughly 25 Python test scripts use
this idiom, and the 13 Python failures in this group all reduce to it.

This cannot be fixed where it happens, and it must not be fixed by injecting
code before the boundary: the manifest gate deliberately scrubs `PYTHONSTARTUP`
and its siblings so nothing can execute ahead of the sandbox. The remaining
option is a small shared helper that contained tests use instead, which creates
its directory so that the container retains access.

## The suite's own DACL helper

`tests/sqlite_contract/platform_test_support.hpp` builds the same shape
deliberately: `set_test_dacl` grants `FILE_ALL_ACCESS` to the token user, adds
World read only in its permissive mode, and applies the result with
`PROTECTED_DACL_SECURITY_INFORMATION`. The four failing SQLite contract tests
assert `temporary root becomes owner-private` and then cannot use the root.

This half is our own code and takes a direct fix: when the process is running
in an AppContainer, an owner-private DACL must also grant that container's
package SID, read back from `TokenAppContainerSid`. That preserves what the
tests are actually asserting — no other user gains access, and World read
appears only in permissive mode — while making the object usable by the only
identity that is supposed to own it. `is_owner_private` has to accept the extra
entry for the same reason.

## Required final state

- A contained Windows test can create a temporary root and use it.
- Owner-private still means owner-private: no second user, group, or
  `ALL APPLICATION PACKAGES` grant is introduced, and the POSIX `0700`
  behaviour on macOS and Linux is untouched.
- The 17 tests named in [[BPR-0347]]'s first group pass on Windows, and the
  macOS and Linux suites are unchanged.

## Not in scope

The other seven Windows failures [[BPR-0347]] records: the binary symbol
inspector reading no symbols in `live_mutation_isolation`, and the six
undiagnosed ones. They have different causes and should not be folded in here.
