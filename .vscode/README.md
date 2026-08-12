# VS Code CMake workflow

The workspace forces Microsoft's CMake Tools extension to use the checked-in
`CMakePresets.json`. Select a configure preset from the CMake status bar or run
`CMake: Select Configure Preset` from the command palette.

- `dev` is the tests-enabled Debug graph used for implementation and tests.
- `core-only` is the tests-enabled Qt-free Debug graph.
- `app-release` is the normal production-shaped application build: Qt enabled,
  `Release`, and `BUILD_TESTING=OFF`. Its build preset selects only
  `app_better_pull_review`.

## Building and launching

`tasks.json` exposes each preset as a VS Code task. `CMake: build
(app-release)` is the default build task, so `⇧⌘B` builds the production-shaped
application. The remaining tasks are listed under `Tasks: Run Task`, and the
`configure + build` entries run the matching workflow preset in one step.

`launch.json` exposes the runnable configurations in the Run and Debug view:

- `Run BetterPullReview (app-release)` launches the `Release` application and
  rebuilds it first. Use `Run Without Debugging` (`⌃F5`); `Release` carries no
  debug information.
- `Debug BetterPullReview (dev)` launches the tests-enabled Debug binary, which
  is the configuration to attach a debugger to.
- `Run BetterPullReview --self-test (app-release)` runs the self-test and exits.
- `Run CMake Tools launch target` defers to whatever preset and target are
  selected in the CMake status bar.

These configurations resolve preset output paths directly, so they work without
CMake Tools having configured the build directory. Its own `CMake: Run Without
Debugging`, `CMake: Debug`, and launch-target picker read the CMake file API
instead, and stay empty until the extension configures the directory itself:
because `cmake.configureOnOpen` is `false`, a build tree configured from the
terminal has no file API reply for the extension to read. Run `CMake: Configure`
once from the command palette to populate those pickers.

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
