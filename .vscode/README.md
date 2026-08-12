# VS Code CMake workflow

The workspace forces Microsoft's CMake Tools extension to use the checked-in
`CMakePresets.json`. Select a configure preset from the CMake status bar or run
`CMake: Select Configure Preset` from the command palette.

- `dev` is the tests-enabled Debug graph used for implementation and tests.
- `core-only` is the tests-enabled Qt-free Debug graph.
- `app-release` is the normal production-shaped application build: Qt enabled,
  `Release`, and `BUILD_TESTING=OFF`. Its build preset selects only
  `app_better_pull_review`.

After selecting `app-release`, select `app_better_pull_review` as the launch
target, then use `CMake: Run Without Debugging` or `CMake: Debug`.

The equivalent terminal commands are:

```sh
cmake --preset app-release
cmake --build --preset app-release
```

On macOS the result is
`build/app-release/src/app/better_pull_review.app`. This is the closest current
shipped-application shape, but it is not a signed, notarized, dependency-
deployed, or installer-ready package; RFC-BPR-0014 owns those distribution
claims.
