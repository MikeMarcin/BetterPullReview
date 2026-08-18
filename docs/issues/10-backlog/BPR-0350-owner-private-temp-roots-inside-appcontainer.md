---
id: BPR-0350
title: Owner-private temporary roots are unusable inside an AppContainer
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0347, BPR-0360]
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

## Both halves implemented and directly verified — 2026-08-18

### CPython's temporary directories

`scripts/contained_temporary_support.py` is the shared helper this card asked
for. On Windows it creates the directory with no explicit mode, so it inherits
the parent's entries — including the container's own `(I)(OI)(CI)(M)` grant. On
POSIX it delegates to `tempfile` untouched, so the `0o700` protection is exactly
what it was. It exports a context manager, a held-object form with `name` and
`cleanup()`, and a `Path`-returning form, so every existing call shape had a
drop-in.

Twenty-two files under `tests/` and two under `scripts/` now use it, across 112
call sites. `scripts/run_network_isolated_test.py` was deliberately left alone:
it is the POSIX runner, it builds a `sandbox-exec` profile under `/tmp`, and it
never executes on Windows. `.agents/skills/bpr-attended-review/scripts/test_validate_operator_packet.py`
got a small self-contained equivalent rather than an import, because a skill pack
resolves paths only inside its own directory and reaching into the repository
would make it non-portable.

Proven inside the launcher, with the negative control that makes it evidence:

| | inside (`TokenIsAppContainer=1`) | outside (`0`) |
|---|---|---|
| `tempfile.mkdtemp()` | `PermissionError` | usable |
| `make_contained_temporary_directory()` | usable | usable |
| `contained_temporary_directory()` | usable, cleaned up | usable, cleaned up |

`core_boundary_contract` runs and passes under the launcher, having previously
failed with `PermissionError: [WinError 5]` on the directory it had just made.

### The suite's own DACL helper

`tests/sqlite_contract/platform_test_support.hpp` gained
`current_appcontainer_sid_storage`, which reads `TokenIsAppContainer` and then
copies the SID out of `TokenAppContainerSid`. `set_test_dacl` grants that SID
`FILE_ALL_ACCESS` with the user entry's inheritance, and only when the process is
actually contained. `is_owner_private` expects one ACE outside a container and
exactly two inside it, and now checks the entries as a set so it does not depend
on the order `SetEntriesInAclW` emits. World read still appears only in
permissive mode; no group and no `ALL APPLICATION PACKAGES` grant was
introduced.

The four SQLite tests stopped failing at `temporary root becomes owner-private`,
which is what this half owed.

## What this did not finish, and why

The suite went from 24 failures to 16. Eight tests moved to passing:
`core_boundary_contract`, `documentation_issue_graph_contract`,
`github_recorded_fixture_contract`, `attended_review_verdict_contract`,
`project_source_dump_contract`, `live_mutation_isolation_contract`, plus
`architecture_boundaries` and `application_icon_assets_contract` from the
sibling [[BPR-0359]].

This card's third criterion — that the seventeen tests [[BPR-0347]] grouped
together all pass — is **not** met, and the reason is that the grouping was
wrong rather than that the fix is incomplete. Those tests shared a *symptom*,
not a cause. With the temporary-directory cause removed, what was underneath it
turned out to be four further independent defects, each now carrying its own
card:

- [[BPR-0362]] — nothing can read the ancestors above the container's own `AC`
  directory, and both `Path.resolve(strict=True)` and the product's
  `validate_ancestor_chain` walk all the way to `\\?\C:\`. That is what the
  four SQLite storage tests and two RFC-0016 tests actually fail on, and they
  only reached it because this card's fix let them get that far. Measured with a
  negative control: the drive root is `win32=5` inside the container and opens
  outside it.
- [[BPR-0360]] — the **product** builds the same protected owner-only descriptor
  in `src/adapters/sqlite/src/catalog_database.cpp`. This card's diagnosis named
  only the test helper and missed it. The descriptor defect is proven by direct
  measurement, but it is latent behind [[BPR-0362]]: `open_state_directory` never
  reaches the descriptor, so a fix has no observable symptom until that card
  lands. One was written and reverted for exactly that reason.
- [[BPR-0361]] — no symbol inspector reads an MSVC PE.
- [[BPR-0363]] — the Windows preset contract names a Visual Studio the preset
  does not use.

This card stays in `10-backlog/` behind [[BPR-0360]], because its own third
criterion is a statement about the four SQLite tests and that statement cannot
be made true here.
