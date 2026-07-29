# Test Instructions

Tests are direct evidence, not naming evidence.

- Core tests instantiate no `QApplication`.
- Host tests use fake/in-memory ports and the manual executor.
- Qt tests cover translation and presentation mechanics without duplicating
  every core rule.
- End-to-end tests use the real composition root.
- Runtime-surface tests inspect the launched process on macOS, Linux, and
  Windows; keep platform queries process-scoped and directly unit-tested.
- Fixtures are sanitized and deterministic; never include credentials or
  private repository content.
- A test must fail or visibly disagree when its claimed behavior is false.
