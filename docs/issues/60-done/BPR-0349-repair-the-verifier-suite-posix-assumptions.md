---
id: BPR-0349
title: Repair the isolation verifier suite's POSIX assumptions
status: Done
rfc: RFC-BPR-0008
depends_on: [BPR-0336]
---

# Repair the isolation verifier suite's POSIX assumptions

`tests/scripts/verify_test_network_isolation_tests.py` is registered on every
platform as `test_network_isolation_structure_contract`. Run on Windows it
reports `Ran 24 tests ... FAILED (failures=1, errors=1)`, and it did so at
`545f38e` as well, so this predates [[BPR-0347]] rather than following from it.
Both failures are the suite asserting POSIX properties of macOS-only
machinery, and in both the production code was right and the test was wrong.

## The derived seatbelt profile

`test_derived_profile_contains_only_exact_runner_authority` built its fixture
root from the host's own temporary directory and handed it to `write_profile`,
which is the macOS sandbox-profile writer. On Windows that is a path like
`C:\Users\...\fixture`, and `seatbelt_string` refuses it — it accepts only a
value starting with `/` and containing no quote or backslash, because anything
else could close the quoted string it is about to emit into a seatbelt policy.

The refusal was correct. A Windows path in a seatbelt profile is meaningless,
and production never produces one: this writer runs only on macOS, where the
runner's fixture root is always POSIX. Only the string form of the argument
reaches the profile, so the fixture is now a `PurePosixPath`, which states the
production shape identically on every host.

A separate control now asserts the refusal itself, using two roots that are
unsafe everywhere — one containing a quote and one not anchored at all — so
the property that used to be observed only as a Windows accident is asserted
on purpose on every platform.

## The live supervisor's execute permission

`test_fixture_alternate_stale_and_tests_enabled_supervisors_fail` chmods its
fixture supervisor to `0o600` and requires the verifier to report `live
supervisor is not an executable regular file`. The verifier asks that question
with `os.access(path, os.X_OK)`, and **Windows has no execute permission for
that call to read**: it ignores `X_OK` and answers as it would for `R_OK`. The
`chmod` changes nothing, the file stays as executable as it was, and the
refusal cannot fire.

The assertion is now made only where the permission exists. The other two
assertions in the same case — the `BUILD_TESTING` disagreement and the stale
`predates` result — are unconditional and still run everywhere.

This deliberately does not invent a Windows notion of executability for the
verifier. Doing so would implement part of the Windows designated live lane,
which [[BPR-0235]] records as explicitly fail-closed and unimplemented. The
vacuous `X_OK` result on Windows is therefore a known gap belonging to that
lane, named here so it is not rediscovered as a mystery.

## Evidence

- Before, on Windows: `Ran 24 tests in 0.094s / FAILED (failures=1, errors=1)`,
  reproduced at `545f38e` with the working tree stashed, which is what proves
  the failures predate the AppContainer work.
- After, on Windows: `Ran 25 tests in 0.098s / OK`.
- The two new refusal cases print `TEST_NETWORK_ISOLATION_RUNNER_FAILED:
  unsafe-fixture-root` twice, so the control is observed failing closed rather
  than merely passing.
- No production file changed; both edits are in the test suite.

## What this card does not claim

- **No POSIX host ran this suite for this card.** The changes are constructed
  so that macOS and Linux behaviour is unchanged — `EXECUTE_PERMISSION_IS_
  EXPRESSIBLE` is true there, and `/private/tmp/bpr-fixture` satisfies
  `seatbelt_string` exactly as the old temporary directory did — but that is
  reasoning, not a run, and it should be replayed on the next POSIX pass.
- This card does not touch the Windows network boundary itself, which is
  [[BPR-0347]], nor the Windows designated live lane, which stays unimplemented.
