---
schema: "implement-rfc.card.v1"
id: "BPR-0430"
title: "Every Qt-linked test on Windows exits before main with 0xc0000135"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# Every Qt-linked test on Windows exits before main with 0xc0000135

The first `windows-dev` CTest run under the AppContainer boundary
([[BPR-0412]]) failed every test that links Qt the same way:

```
qt_foundation_window ........ Exit code 0xc0000135***Exception
test_network_isolation_application ........ Exit code 0xc0000135***Exception
```

`0xc0000135` is `STATUS_DLL_NOT_FOUND`: the loader could not find
`Qt6Cored.dll` and its siblings before `main` ran. Nothing put them where a
Windows loader looks. The provider resolves Qt to a prefix outside the build
tree (`…/bprqt/6.10.3/6.10.3/msvc2022_64`), the tests link its import
libraries, no step deploys the DLLs beside the executables, and no
registration adds the prefix's `bin` directory to `PATH`. On macOS and Linux
the same binaries carry the library path in their link (`@rpath`, `RUNPATH`),
so the question never comes up. Every Qt-free test in the same run passed,
which is what separates a loader failure from a boundary failure.

## Required final state

- On Windows, every test registered through `bpr_add_network_isolated_test`
  receives the Qt runtime directory — the directory of `Qt6::Core`'s DLL —
  prepended to `PATH` through `ENVIRONMENT_MODIFICATION`, when the Qt graph
  is configured. The launcher passes its environment to the contained child
  unchanged, so the prepend reaches it.
- The AppContainer can read that directory: the isolation setup already
  grants the resolved Qt prefix as an outside-project read scope.
- Nothing changes on macOS or Linux, and the Qt-free graph registers no
  `PATH` modification at all.
- The manifest gate still accepts every registration: `PATH` is not one of
  the pre-boundary injection variables it scrubs.
- A Qt-linked test starts and runs under the boundary, which [[BPR-0412]]
  records test by test.

## Verification

```sh
ctest --preset windows-dev --output-on-failure -R '^(qt_foundation_window|test_network_isolation_application|test_network_isolation_manifest)$'
```

## Result

The registration now carries
`PATH=path_list_prepend:…/bprqt/6.10.3/6.10.3/msvc2022_64/bin` on all 187
configured `windows-dev` tests and on none of the 89 `core-only` ones. Under
the boundary, after the change:

```
qt_foundation_window .................   Passed    0.12 sec
test_network_isolation_application ...   Passed    2.05 sec
test_network_isolation_manifest ......   Passed    2.04 sec
```

The first is a Qt widget test reaching `main` inside the container for the
first time on Windows; the second is the shipped application's own network
counterexample, which both loads Qt and is refused the network. The
test-by-test matrix is [[BPR-0412]]'s.
